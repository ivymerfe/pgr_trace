#ifndef PGR_SHMEM_H
#define PGR_SHMEM_H

#include "postgres.h"
#include "port/atomics.h"

typedef struct {
	pg_atomic_uint64 commits;
	pg_atomic_uint64 aborts;
	pg_atomic_uint64 rollbacks;
} SharedMemory;

extern SharedMemory *Shmem;

void pgr_init_memory();

uint64 pgr_stats_get_commits();
uint64 pgr_stat_aborts();
uint64 pgr_stats_get_rollbacks();
uint64 pgr_stats_get_failed_commits();

void pgr_event_commit();
void pgr_event_abort();
void pgr_event_rollback();
void pgr_stats_reset();

#endif
