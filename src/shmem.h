#ifndef PGR_SHMEM_H
#define PGR_SHMEM_H

#include "postgres.h"
#include "port/atomics.h"
#include "storage/latch.h"

#define PGR_RING_SIZE 128 * 1024

typedef struct {
	uint32 id;
	uint32 index;
	char event_type;
	uint32 duration_us;
	pg_atomic_uint32 ready;
} PgrEvent;

typedef struct {
	pg_atomic_uint64 commits;
	pg_atomic_uint64 aborts;
	pg_atomic_uint64 rollbacks;

	pg_atomic_uint64 head;
	uint64 tail;
	PgrEvent events[PGR_RING_SIZE];
	pg_atomic_uint64 dropped_events;

	pg_atomic_uint32 trace_running;
	Latch *worker_latch;
} SharedMemory;

extern SharedMemory *Shmem;

void pgr_init_memory();

uint64 pgr_stats_get_commits();
uint64 pgr_stats_get_aborts();
uint64 pgr_stats_get_rollbacks();
uint64 pgr_stats_get_failed_commits();
uint64 pgr_stats_get_dropped_events();

void pgr_event_commit();
void pgr_event_abort();
void pgr_event_rollback();
void pgr_stats_reset();

void pgr_trace_start();
void pgr_trace_stop();
bool pgr_trace_is_running();

void pgr_send_event(char event_type, uint32 duration_us);
bool pgr_read_event(PgrEvent *out);

#endif
