#include "postgres.h"

#include "shmem.h"

void pgr_init_memory() {
	pg_atomic_init_u64(&Shmem->commits, 0);
	pg_atomic_init_u64(&Shmem->aborts, 0);
	pg_atomic_init_u64(&Shmem->rollbacks, 0);
}

uint64 pgr_stats_get_commits() {
	return pg_atomic_read_u64(&Shmem->commits);
}

uint64 pgr_stat_aborts() {
	return pg_atomic_read_u64(&Shmem->aborts);
}

uint64 pgr_stats_get_rollbacks() {
	return pg_atomic_read_u64(&Shmem->rollbacks);
}

uint64 pgr_stats_get_failed_commits() {
	return pgr_stat_aborts() - pgr_stats_get_rollbacks();
}

void pgr_event_commit() {
	pg_atomic_fetch_add_u64(&Shmem->commits, 1);
}

void pgr_event_abort() {
	pg_atomic_fetch_add_u64(&Shmem->aborts, 1);
}

void pgr_event_rollback() {
	pg_atomic_fetch_add_u64(&Shmem->rollbacks, 1);
}

void pgr_stats_reset() {
	pg_atomic_write_u64(&Shmem->commits, 0);
	pg_atomic_write_u64(&Shmem->aborts, 0);
	pg_atomic_write_u64(&Shmem->rollbacks, 0);
}
