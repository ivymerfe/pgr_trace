#ifndef PG_TX_STATS_SHMEM_H
#define PG_TX_STATS_SHMEM_H

#include "postgres.h"
#include "port/atomics.h"

typedef struct SharedMemory {
	pg_atomic_uint64 successful_commit_count;
	pg_atomic_uint64 failed_commit_count;
	pg_atomic_uint64 rollback_count;
} SharedMemory;

extern SharedMemory *Shmem;

void setup_shmem_hooks(void);

#endif // PG_TX_STATS_SHMEM_H
