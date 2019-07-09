#ifndef __LBIO_H_
#define __LBIO_H_

#include <linux/dma-mapping.h>
#include <linux/buffer_head.h>
#include <linux/blk_types.h>
#include <linux/jbd2.h>
#include <linux/nvme.h>

#include <asm/bitops.h>

struct lbio_vec {
	struct page *page;
	dma_addr_t dma_addr;
};

#define LBIO_INLINE_VECS	4

struct lbio;
typedef void (lbio_end_io_t) (struct lbio *);

struct lbio {
	unsigned long int flags;
	struct lbio *next;
	sector_t sector;
	int lazy_cpu;
	unsigned short max_vcnt;
	unsigned short vcnt;
	__le64 *prp_list;
	dma_addr_t prp_dma;
	struct lbio_vec *vec;
	struct lbio_vec inlined_vec[LBIO_INLINE_VECS];
	struct list_head list;
	wait_queue_head_t lbio_waitqueue;
	atomic_t lbio_waiters;
	blk_status_t status;
	lbio_end_io_t *bi_end_io;
	void *bi_private;
} ____cacheline_internodealigned_in_smp;

extern struct lbio *lbios_global;

#define lbio_tag(lbio)	((lbio) - lbios_global)
#define tag_to_lbio(tag)	(&lbios_global[(tag)])
#define lbio_to_cpu(lbio)	(lbio_tag(lbio) / NR_LBIO_PER_CPU)

struct lbio_queue {
	unsigned int head;
	struct lbio *lbios;
};

#define MAX_LAZY_LBIOS	1024

struct lbio_lazy_list {
	struct lbio* lbios[MAX_LAZY_LBIOS];
	int nr_lbios;
};
DECLARE_PER_CPU(struct lbio_lazy_list, lbio_lazy_lists);

struct lbio_final_list {
	struct lbio* lbio;
	spinlock_t lock;
};
DECLARE_PER_CPU(struct lbio_final_list, lbio_final_lists);

#define NR_LBIO_PER_CPU 1024
#define LBIO_MAX_FREE_PAGES	64

struct lbio_free_page_pool {
	struct device *dev;
	struct list_head head;
	unsigned int nr_pages;
};

DECLARE_PER_CPU(struct lbio_free_page_pool, lbio_free_page_pools);

enum lbioflags {
	LBIO_WRITE,
	LBIO_BUSY,
	LBIO_COMPLETED,
	LBIO_PAGECACHED,
	LBIO_FUA,
};

#define lbio_is_write(lbio)	test_bit(LBIO_WRITE, &lbio->flags)
#define lbio_set_write(lbio) set_bit(LBIO_WRITE, &lbio->flags)
#define lbio_clear_write(lbio) clear_bit(LBIO_WRITE, &lbio->flags)
		
#define lbio_is_busy(lbio)	test_bit(LBIO_BUSY, &lbio->flags)
#define lbio_set_busy(lbio) set_bit(LBIO_BUSY, &lbio->flags)
#define lbio_clear_busy(lbio) clear_bit(LBIO_BUSY, &lbio->flags)

#define lbio_is_completed(lbio)	test_bit(LBIO_COMPLETED, &lbio->flags)
#define lbio_set_completed(lbio) set_bit(LBIO_COMPLETED, &lbio->flags)
#define lbio_clear_completed(lbio) clear_bit(LBIO_COMPLETED, &lbio->flags)

#define lbio_is_pagecached(lbio)	test_bit(LBIO_PAGECACHED, &lbio->flags)
#define lbio_set_pagecached(lbio) set_bit(LBIO_PAGECACHED, &lbio->flags)
#define lbio_clear_pagecached(lbio) clear_bit(LBIO_PAGECACHED, &lbio->flags)

#define lbio_is_fua(lbio)	test_bit(LBIO_FUA, &lbio->flags)
#define lbio_set_fua(lbio) set_bit(LBIO_FUA, &lbio->flags)
#define lbio_clear_fua(lbio) clear_bit(LBIO_FUA, &lbio->flags)

static inline int lbio_add_page(struct lbio *lbio, struct page *page)
{
	if (lbio->vcnt == lbio->max_vcnt)
		return 0;

	lbio->vec[lbio->vcnt].page = page;
	if (!page->private) {
		struct device *dev = per_cpu_ptr(&lbio_free_page_pools, 0)->dev;
		dma_addr_t dma_addr = AIOS_dma_map_page(dev, page, 0, PAGE_SIZE, DMA_FROM_DEVICE);
		if (!dma_addr)
			printk(KERN_ERR "[AIOS ERROR %s:%d]\n", __func__, __LINE__);
		lbio->vec[lbio->vcnt].dma_addr = dma_addr;
	} else
		lbio->vec[lbio->vcnt].dma_addr = (dma_addr_t)page->private;
	page->private = 0UL;

	lbio->vcnt++;
	return 1;
}

static inline int lbio_add_write_page(struct lbio *lbio, struct page *page)
{
	if (lbio->vcnt == lbio->max_vcnt)
		return 0;

	lbio->vec[lbio->vcnt].page = page;
	lbio->vcnt++;
	return 1;
}

static inline int lbio_add_write_bh(struct lbio *lbio, struct buffer_head *bh)
{
	if (lbio->vcnt == lbio->max_vcnt)
		return 0;

	lbio->vec[lbio->vcnt].page = (struct page *)bh;
	lbio->vcnt++;
	return 1;
}

static inline struct page *AIOS_get_free_page(void)
{
	struct page *page = NULL;
	struct lbio_free_page_pool *free_page_pool = &get_cpu_var(lbio_free_page_pools);

	if (free_page_pool->nr_pages) {
		page = list_last_entry(&free_page_pool->head, struct page, lru);
		list_del(&page->lru);
		free_page_pool->nr_pages--;
	}
	put_cpu_var(lbio_free_page_pools);

	return page;
}

static inline void refill_free_pages_lbio(struct list_head *pages, int nr)
{
	struct lbio_free_page_pool *free_page_pool = &get_cpu_var(lbio_free_page_pools);
	list_splice(pages, &free_page_pool->head);
	free_page_pool->nr_pages += nr;
	put_cpu_var(lbio_free_page_pools);
}

static inline void push_lazy_list(struct lbio **from, int nr)
{
	struct lbio_lazy_list *lazy_list;
	local_irq_disable();
	lazy_list = &get_cpu_var(lbio_lazy_lists);
	BUG_ON(lazy_list->nr_lbios + nr > MAX_LAZY_LBIOS);
	memcpy(lazy_list->lbios[lazy_list->nr_lbios], from, sizeof(struct lbio *) * nr);
	lazy_list->nr_lbios += nr;
	put_cpu_var(lbio_lazy_lists);
	local_irq_enable();
}

/* this is called in interrupt context. That's why it saves irqflags */
static inline void push_lazy_single(struct lbio *lbio)
{
	if (!lbio_is_pagecached(lbio)) {
		struct lbio_lazy_list *lazy_list = (this_cpu_ptr(&lbio_lazy_lists));
		lbio->lazy_cpu = smp_processor_id();
		if (lazy_list->nr_lbios >= MAX_LAZY_LBIOS) {
			printk(KERN_ERR "[AIOS ERROR] %s:%d\n", __func__, __LINE__);
			BUG();
		}
		lazy_list->lbios[lazy_list->nr_lbios++] = lbio;
	} else {
		int cpu = lbio_to_cpu(lbio);
		struct lbio_final_list *lazy_list = &per_cpu(lbio_final_lists, cpu);
		spin_lock(&lazy_list->lock);
		lbio->next = lazy_list->lbio;
		lazy_list->lbio = lbio;
		spin_unlock(&lazy_list->lock);
	}
}

extern int lbio_lazy_dma_unmapping_with_cpu(int cpu);
extern struct lbio *lbio_alloc(gfp_t gfp_mask, unsigned int nr_iovecs);
extern void lbio_wakeup(struct lbio *lbio);
extern int init_lbio(struct device *dev);
extern void exit_lbio(void);

extern int nvme_AIOS_poll(struct lbio *lbio, void *ret_nvmeq, int count);
extern int nvme_AIOS_read(struct lbio *lbio);
extern int nvme_AIOS_write(struct lbio *lbio);
extern int nvme_lbio_submit_cmd(struct lbio *lbio, int flush_fua);
extern int nvme_lbio_dma_mapping(struct lbio *lbio);

extern void lbio_req_completion_error(struct lbio *lbio);

extern void AIOS_lazy_page_cache(struct address_space* mapping, struct lbio *lbio);
extern void AIOS_lazy_dma_unmapping(void);
extern void AIOS_refill_free_page(struct address_space *mapping);

#endif

