#ifndef PG_TX_STATS_SHMEM_H
#define PG_TX_STATS_SHMEM_H

#include "postgres.h"
#include "port/atomics.h"

typedef struct SharedMemory {
	pg_atomic_uint64 successful_commits;
	pg_atomic_uint64 aborted;
	pg_atomic_uint64 rollbacks;
} SharedMemory;

extern SharedMemory *Shmem;

void setup_shmem_hooks(void);

#endif // PG_TX_STATS_SHMEM_H
