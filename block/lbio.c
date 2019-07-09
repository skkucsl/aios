#include <linux/kernel.h>
#include <linux/lbio.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/pagemap.h>

#include <asm/barrier.h>

struct lbio *lbios_global;
EXPORT_SYMBOL(lbios_global);

DEFINE_PER_CPU(struct lbio_free_page_pool, lbio_free_page_pools);
EXPORT_PER_CPU_SYMBOL(lbio_free_page_pools);

DEFINE_PER_CPU(struct lbio_free_page_pool, lbio_free_write_page_pools);
EXPORT_PER_CPU_SYMBOL(lbio_free_write_page_pools);

DEFINE_PER_CPU(struct lbio_lazy_list, lbio_lazy_lists);
EXPORT_PER_CPU_SYMBOL(lbio_lazy_lists);

DEFINE_PER_CPU(struct lbio_final_list, lbio_final_lists);
EXPORT_PER_CPU_SYMBOL(lbio_final_lists);

DEFINE_PER_CPU(struct lbio_queue, lbio_queues);
EXPORT_PER_CPU_SYMBOL(lbio_queues);


/* 
 * This function is called because prps prepare failed
 *
 * Just release each lbio
 * Remember that lbios whose nvme cmds not submitted are linked
 */
void lbio_req_completion_error(struct lbio *lbio)
{
	int i;
	int nr_free = 0;
	LIST_HEAD(free_pages);

	while (lbio) {
		printk(KERN_ERR "[ERROR] lbio error case %s:%s:%d, lbio:%p\n",
									__func__, __FILE__, __LINE__, lbio);
		for (i = 0; i < lbio->vcnt; ++i) {
			struct lbio_vec *bv = &lbio->vec[i];
			struct page *page = bv->page;

			ClearPageUptodate(page);
			ClearPageMappedToDisk(page);
			/* It's safe to call this function because the page never 
			 * belongs to page cache */
			__ClearPageLocked(page);

			page->private = bv->dma_addr;
			page->index = 0;

			list_add(&page->lru, &free_pages);
			nr_free++;
		}
		if (lbio->vcnt > LBIO_INLINE_VECS) 
			kfree(lbio->vec);

		/* No need to release prp list because this error case is 
		 * caused by -ENOMEM at prp list allocation */

		BUG_ON(lbio_is_completed(lbio));
		BUG_ON(lbio_is_pagecached(lbio));

		lbio_clear_busy(lbio); /* clear busy at the very last */
		lbio = lbio->next;
	}

	/* refill free pages to free page pool */
	refill_free_pages_lbio(&free_pages, nr_free);
}
EXPORT_SYMBOL(lbio_req_completion_error);

static DEFINE_PER_CPU(struct work_struct, lbio_lazy_work);

static void lbio_lazy_dma_unmapping_per_cpu(struct work_struct *dummy)
{
	AIOS_lazy_dma_unmapping();
}

static void lbio_free_free_page_pool_per_cpu(struct work_struct *dummy)
{
	struct lbio_free_page_pool* lbio_free_page_pool = 
		get_cpu_ptr(&lbio_free_page_pools);
	struct device* dev = lbio_free_page_pool->dev;
	struct page* page, *next;
	int nr_freed = 0;

	list_for_each_entry_safe(page, next, &lbio_free_page_pool->head, lru) {
		dma_addr_t dma_addr = page->private;
		dma_unmap_page(dev, dma_addr, PAGE_SIZE, DMA_FROM_DEVICE);
		page->private = 0UL;
		__free_pages(page, 0);
		nr_freed++;
	}

	BUG_ON(nr_freed != lbio_free_page_pool->nr_pages);
	lbio_free_page_pool->nr_pages = 0;

	put_cpu_ptr(&lbio_free_page_pools);
}

extern void debug_lbio(struct lbio* lbio);

struct lbio *lbio_alloc(gfp_t gfp_mask, unsigned int nr_iovecs)
{
	struct lbio_queue *lbio_queue;
	struct lbio *lbio;

	lbio_queue = &get_cpu_var(lbio_queues);
	lbio = &lbio_queue->lbios[lbio_queue->head];

	while (lbio_is_busy(lbio)) {
		if (lbio_is_completed(lbio) && lbio_is_pagecached(lbio)) {
			int cpu = lbio->lazy_cpu;
			struct work_struct *work = &per_cpu(lbio_lazy_work, cpu);
			put_cpu_var(lbio_queues);

			if (cpu == smp_processor_id())
				AIOS_lazy_dma_unmapping();
			else {
				schedule_work_on(cpu, work);
				flush_work(work);
			}
		} else {
			DEFINE_WAIT(wait);
			atomic_inc(&lbio->lbio_waiters);
			prepare_to_wait(&lbio->lbio_waitqueue, &wait, TASK_UNINTERRUPTIBLE);
			put_cpu_var(lbio_queues);
			schedule();
			get_cpu();
			atomic_dec(&lbio->lbio_waiters);
			finish_wait(&lbio->lbio_waitqueue, &wait);
		}
	}

	lbio_set_busy(lbio);
	lbio_queue->head = (lbio_queue->head + 1) % NR_LBIO_PER_CPU;
	put_cpu_var(lbio_queues);

	if (nr_iovecs > LBIO_INLINE_VECS) {
		lbio->vec = kmalloc(sizeof(struct lbio_vec) * nr_iovecs, gfp_mask);
		if (lbio->vec == NULL) {
			printk(KERN_ERR "[AIOS ERROR] %s:%d nr_iovecs %d\n", __func__, __LINE__, nr_iovecs);
			return NULL;
		}
	} else
		lbio->vec = &(lbio->inlined_vec[0]);

	lbio->vcnt = 0;
	lbio->sector = 0;
	lbio->next = NULL;
	lbio->prp_list = NULL;
	lbio->prp_dma = 0;
	lbio->max_vcnt = nr_iovecs;
	lbio->bi_end_io = NULL;
	lbio->bi_private = NULL;
	INIT_LIST_HEAD(&lbio->list);
	atomic_set(&lbio->lbio_waiters, 0);
	init_waitqueue_head(&lbio->lbio_waitqueue);

	return lbio;
}

void AIOS_lazy_page_cache(struct address_space *mapping, struct lbio *lbio)
{
	int i, error;
	struct page *page;
	struct lbio *temp = NULL;

	while (lbio) {
		if (!lbio_is_busy(lbio))
			goto next_lbio;

		for (i = 0; i < lbio->vcnt; ++i) {
			page = lbio->vec[i].page;
			error = add_to_page_cache_lru_nolock(page, mapping, page->index,
					readahead_gfp_mask(mapping));
			if (error) {
				if (error != -EEXIST)
					printk(KERN_ERR "[AIOS ERROR] %s:%d page cache insertion error %d\n",
															__func__, __LINE__, error);
				else
					count_vm_event(AIOS_PAGE_EEXIST);
				SetPageReuse(page);
			} else /* -EEXIST pages should be freed after interrupt completion */
				put_page(page);
		}

next_lbio:
		temp = lbio->next;
		lbio_set_pagecached(lbio);
		lbio = temp;
	}
}
EXPORT_SYMBOL(AIOS_lazy_page_cache);

static DEFINE_PER_CPU(struct work_struct, lbio_exit_work);

void exit_lbio(void)
{
	struct cpumask has_work;
	int cpu;

	get_online_cpus();

	cpumask_clear(&has_work);
	for_each_online_cpu(cpu) {
		struct work_struct *work = &per_cpu(lbio_exit_work, cpu);

		INIT_WORK(work, lbio_lazy_dma_unmapping_per_cpu);
		schedule_work_on(cpu, work);
		cpumask_set_cpu(cpu, &has_work);
	}

	for_each_cpu(cpu, &has_work)
		flush_work(&per_cpu(lbio_exit_work, cpu));

	cpumask_clear(&has_work);
	for_each_online_cpu(cpu) {
		struct work_struct *work = &per_cpu(lbio_exit_work, cpu);

		INIT_WORK(work, lbio_free_free_page_pool_per_cpu);
		schedule_work_on(cpu, work);
		cpumask_set_cpu(cpu, &has_work);

	}
	for_each_cpu(cpu, &has_work)
		flush_work(&per_cpu(lbio_exit_work, cpu));

	put_online_cpus();
}

int init_lbio(struct device *dev)
{
	int cpu;

	get_online_cpus();

	lbios_global = kzalloc(sizeof(struct lbio) * num_online_cpus() * NR_LBIO_PER_CPU,
																			GFP_KERNEL);
	if (lbios_global == NULL)
		panic("[AIOS ERROR] %s:%d cannot allocate lbios_global\n", __func__, __LINE__);

	printk(KERN_ERR "[AIOS %s] num_online_cpus() %d lbios_global: %p each lbio size: %lu total size: %lu\n",
			__func__, num_online_cpus(), lbios_global, sizeof(struct lbio),
			sizeof(struct lbio) * num_online_cpus() * NR_LBIO_PER_CPU);

	/* nvme command tag field check */
	BUG_ON(num_online_cpus() * NR_LBIO_PER_CPU > 0x8000);

	for_each_online_cpu(cpu) {
		struct lbio_queue *lbio_queue = per_cpu_ptr(&lbio_queues, cpu);
		lbio_queue->head = 0;
		lbio_queue->lbios = lbios_global + (NR_LBIO_PER_CPU * cpu);
		printk(KERN_ERR "[AIOS %s] lbio_queue: %p cpu: %d head tag: %lu\n",
					__func__, lbio_queue, cpu, lbio_tag(lbio_queue->lbios));
	}

	for_each_online_cpu(cpu) {
		struct lbio_lazy_list *lbio_lazy_list = per_cpu_ptr(&lbio_lazy_lists, cpu);
		printk(KERN_ERR "[AIOS %s] lbio_lazy_list: %p cpu: %d\n", __func__, lbio_lazy_list, cpu);
		memset(lbio_lazy_list->lbios, 0, sizeof(struct lbio *) * MAX_LAZY_LBIOS);
		lbio_lazy_list->nr_lbios = 0;
	}

	for_each_online_cpu(cpu) {
		struct lbio_final_list *final_list = per_cpu_ptr(&lbio_final_lists, cpu);
		printk(KERN_ERR "[AIOS %s] final_list: %p cpu: %d\n", __func__, final_list, cpu);
		spin_lock_init(&final_list->lock);
		final_list->lbio = NULL;
	}

	for_each_online_cpu(cpu) {
		struct lbio_free_page_pool *page_pool = per_cpu_ptr(&lbio_free_page_pools, cpu);
		printk(KERN_ERR "[AIOS %s] lbio_free_page_pool: %p cpu: %d\n", __func__, page_pool, cpu);
		page_pool->dev = dev;
		INIT_LIST_HEAD(&page_pool->head);
		page_pool->nr_pages = 0;

		while (page_pool->nr_pages < LBIO_MAX_FREE_PAGES) {
			dma_addr_t dma_addr;
			struct page *page = alloc_pages(GFP_HIGHUSER_MOVABLE|__GFP_NORETRY|__GFP_NOWARN
				|__GFP_IO|__GFP_FS|__GFP_HARDWALL|__GFP_DIRECT_RECLAIM|__GFP_KSWAPD_RECLAIM, 0);
			if (!page)
				panic("[AIOS ERROR] %s:%d page pre-allocation failed\n", __func__, __LINE__);

			dma_addr = AIOS_dma_map_page(dev, page, 0, PAGE_SIZE, DMA_FROM_DEVICE);
			if (!dma_addr) {
				printk(KERN_ERR "[AIOS ERROR] %s:%d dma_map_single failed\n", __func__, __LINE__);
				__free_pages(page, 0);
				continue;
			}

			page->private = dma_addr;
			list_add(&page->lru, &page_pool->head);
			page_pool->nr_pages++;
		}
	}

	for_each_online_cpu(cpu) {
		struct work_struct *work = &per_cpu(lbio_lazy_work, cpu);
		INIT_WORK(work, lbio_lazy_dma_unmapping_per_cpu);
	}

	put_online_cpus();
	return 0;
}
