#undef TRACE_SYSTEM
#define TRACE_SYSTEM aios_breakdown

#if !defined(_TRACE_AIOS_BREAKDOWN_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_AIOS_BREAKDOWN_H
#include <linux/tracepoint.h>

TRACE_EVENT(fsync_breakdown,
	TP_PROTO(unsigned long long rdtsc0, unsigned long long rdtsc1, unsigned long long rdtsc2,
			unsigned long long rdtsc3, unsigned long long rdtsc4, unsigned long long rdtsc5,
			unsigned long long rdtsc6, unsigned long long rdtsc7, unsigned long long rdtsc8,
			unsigned long long rdtsc9, unsigned long long rdtsc10),
	TP_ARGS(rdtsc0, rdtsc1, rdtsc2, rdtsc3, rdtsc4, rdtsc5, rdtsc6, rdtsc7, rdtsc8, rdtsc9, rdtsc10),
	TP_STRUCT__entry(
		__field( unsigned long long, rdtsc0)
		__field( unsigned long long, rdtsc1)
		__field( unsigned long long, rdtsc2)
		__field( unsigned long long, rdtsc3)
		__field( unsigned long long, rdtsc4)
		__field( unsigned long long, rdtsc5)
		__field( unsigned long long, rdtsc6)
		__field( unsigned long long, rdtsc7)
		__field( unsigned long long, rdtsc8)
		__field( unsigned long long, rdtsc9)
		__field( unsigned long long, rdtsc10)
	),
	TP_fast_assign(
		__entry->rdtsc0 = rdtsc0;
		__entry->rdtsc1 = rdtsc1;
		__entry->rdtsc2 = rdtsc2;
		__entry->rdtsc3 = rdtsc3;
		__entry->rdtsc4 = rdtsc4;
		__entry->rdtsc5 = rdtsc5;
		__entry->rdtsc6 = rdtsc6;
		__entry->rdtsc7 = rdtsc7;
		__entry->rdtsc8 = rdtsc8;
		__entry->rdtsc9 = rdtsc9;
		__entry->rdtsc10 = rdtsc10;
	),
	TP_printk("%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
				(unsigned long long)__entry->rdtsc0, (unsigned long long)__entry->rdtsc1,
				(unsigned long long)__entry->rdtsc2, (unsigned long long)__entry->rdtsc3,
				(unsigned long long)__entry->rdtsc4, (unsigned long long)__entry->rdtsc5,
				(unsigned long long)__entry->rdtsc6, (unsigned long long)__entry->rdtsc7,
				(unsigned long long)__entry->rdtsc8, (unsigned long long)__entry->rdtsc9,
				(unsigned long long)__entry->rdtsc10)
);

TRACE_EVENT(fsync2_breakdown,
	TP_PROTO(unsigned long long rdtsc0, unsigned long long rdtsc1, unsigned long long rdtsc2,
			unsigned long long rdtsc3, unsigned long long rdtsc4, unsigned long long rdtsc5,
			unsigned long long rdtsc6, unsigned long long rdtsc7, unsigned long long rdtsc8,
			unsigned long long rdtsc9, unsigned long long rdtsc10),
	TP_ARGS(rdtsc0, rdtsc1, rdtsc2, rdtsc3, rdtsc4, rdtsc5, rdtsc6, rdtsc7, rdtsc8, rdtsc9, rdtsc10),
	TP_STRUCT__entry(
		__field( unsigned long long, rdtsc0)
		__field( unsigned long long, rdtsc1)
		__field( unsigned long long, rdtsc2)
		__field( unsigned long long, rdtsc3)
		__field( unsigned long long, rdtsc4)
		__field( unsigned long long, rdtsc5)
		__field( unsigned long long, rdtsc6)
		__field( unsigned long long, rdtsc7)
		__field( unsigned long long, rdtsc8)
		__field( unsigned long long, rdtsc9)
		__field( unsigned long long, rdtsc10)
	),
	TP_fast_assign(
		__entry->rdtsc0 = rdtsc0;
		__entry->rdtsc1 = rdtsc1;
		__entry->rdtsc2 = rdtsc2;
		__entry->rdtsc3 = rdtsc3;
		__entry->rdtsc4 = rdtsc4;
		__entry->rdtsc5 = rdtsc5;
		__entry->rdtsc6 = rdtsc6;
		__entry->rdtsc7 = rdtsc7;
		__entry->rdtsc8 = rdtsc8;
		__entry->rdtsc9 = rdtsc9;
		__entry->rdtsc10 = rdtsc10;
	),
	TP_printk("%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
				(unsigned long long)__entry->rdtsc0, (unsigned long long)__entry->rdtsc1,
				(unsigned long long)__entry->rdtsc2, (unsigned long long)__entry->rdtsc3,
				(unsigned long long)__entry->rdtsc4, (unsigned long long)__entry->rdtsc5,
				(unsigned long long)__entry->rdtsc6, (unsigned long long)__entry->rdtsc7,
				(unsigned long long)__entry->rdtsc8, (unsigned long long)__entry->rdtsc9,
				(unsigned long long)__entry->rdtsc10)
);

#endif
#include <trace/define_trace.h>

