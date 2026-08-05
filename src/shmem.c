#include "postgres.h"

#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/shmem.h"

#include "shmem.h"

SharedMemory *Shmem = NULL;

static shmem_startup_hook_type prev_shmem_startup_hook = NULL;
static shmem_request_hook_type prev_shmem_request_hook = NULL;

static void shared_memory_init(void) {
	pg_atomic_init_u64(&Shmem->successful_commits, 0);
	pg_atomic_init_u64(&Shmem->aborted, 0);
	pg_atomic_init_u64(&Shmem->rollbacks, 0);
}

static void hook_shmem_request(void) {
	if (prev_shmem_request_hook) {
		prev_shmem_request_hook();
	}

	RequestAddinShmemSpace(MAXALIGN(sizeof(SharedMemory)));
}

static void hook_shmem_startup(void) {
	bool found_mem;

	if (prev_shmem_startup_hook) {
		prev_shmem_startup_hook();
	}

	Shmem = (SharedMemory *)ShmemInitStruct("tx_commit_stats_mem",
											sizeof(SharedMemory), &found_mem);
	if (!found_mem) {
		shared_memory_init();
	}
}

void setup_shmem_hooks(void) {
	prev_shmem_request_hook = shmem_request_hook;
	shmem_request_hook = hook_shmem_request;

	prev_shmem_startup_hook = shmem_startup_hook;
	shmem_startup_hook = hook_shmem_startup;
}
