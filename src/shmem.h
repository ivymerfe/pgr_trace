#ifndef PGR_SHMEM_H
#define PGR_SHMEM_H

#include "postgres.h"
#include "port/atomics.h"
#include "storage/latch.h"

#define PGR_RING_SIZE (1 * 1024 * 1024)

typedef struct {
	pg_atomic_uint32 ready;
	uint32 id;
	uint32 index;
	uint32 duration_us;
	char type;
} PgrEvent;

typedef struct {
	pg_atomic_uint64 commits;
	pg_atomic_uint64 aborts;
	pg_atomic_uint64 rollbacks;
	pg_atomic_uint64 dropped_events;

	pg_atomic_uint32 trace_running;
	pg_atomic_uint32 trace_reset;
	Latch *worker_latch;

	pg_atomic_uint64 head;
	uint64 tail;
	PgrEvent events[PGR_RING_SIZE];
} SharedMemory;

extern SharedMemory *Shmem;

void pgr_init_memory(void);

uint64 pgr_stats_get_commits(void);
uint64 pgr_stats_get_aborts(void);
uint64 pgr_stats_get_rollbacks(void);
uint64 pgr_stats_get_failed_commits(void);
uint64 pgr_stats_get_dropped_events(void);

void pgr_event_commit(void);
void pgr_event_abort(void);
void pgr_event_rollback(void);
void pgr_stats_reset(void);

void pgr_trace_start(void);
void pgr_trace_stop(void);
void pgr_trace_reset(void);
bool pgr_trace_is_running(void);

void pgr_send_event(char event_type, uint32 duration_us);
bool pgr_read_event(PgrEvent *out);

#endif
