#include "postgres.h"

#include "client_id.h"
#include "shmem.h"
#include "statement_index.h"

void pgr_init_memory() {
	pg_atomic_init_u64(&Shmem->commits, 0);
	pg_atomic_init_u64(&Shmem->aborts, 0);
	pg_atomic_init_u64(&Shmem->rollbacks, 0);

	pg_atomic_init_u64(&Shmem->head, 0);
	Shmem->tail = 0;
	for (int i = 0; i < PGR_RING_SIZE; i++) {
		pg_atomic_init_u32(&Shmem->events[i].ready, 0);
	}
	pg_atomic_init_u64(&Shmem->dropped_events, 0);

	pg_atomic_init_u32(&Shmem->trace_running, 0);
	pg_atomic_init_u32(&Shmem->trace_reset, 0);
	Shmem->worker_latch = NULL;
}

uint64 pgr_stats_get_commits() {
	return pg_atomic_read_u64(&Shmem->commits);
}

uint64 pgr_stats_get_aborts() {
	return pg_atomic_read_u64(&Shmem->aborts);
}

uint64 pgr_stats_get_rollbacks() {
	return pg_atomic_read_u64(&Shmem->rollbacks);
}

uint64 pgr_stats_get_failed_commits() {
	return pgr_stats_get_aborts() - pgr_stats_get_rollbacks();
}

uint64 pgr_stats_get_dropped_events() {
	return pg_atomic_read_u64(&Shmem->dropped_events);
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

static void pgr_wakeup_worker() {
	if (Shmem->worker_latch) {
		SetLatch(Shmem->worker_latch);
	}
}

void pgr_trace_start() {
	pg_atomic_write_u32(&Shmem->trace_running, 1);
	pgr_trace_reset();
}

void pgr_trace_stop() {
	if (pg_atomic_read_u32(&Shmem->trace_running) == 0) {
		return;
	}
	pg_atomic_write_u32(&Shmem->trace_running, 0);
	pgr_trace_reset();
}

void pgr_trace_reset() {
	pg_atomic_write_u32(&Shmem->trace_reset, 1);
	pgr_wakeup_worker();
}

bool pgr_trace_is_running() {
	if (!Shmem) {
		return false;
	}
	return pg_atomic_read_u32(&Shmem->trace_running) == 1;
}

void pgr_send_event(char event_type, uint32 duration_us) {
	if (!Shmem || !pgr_trace_is_running()) {
		return;
	}
	uint64 slot = pg_atomic_fetch_add_u64(&Shmem->head, 1) % PGR_RING_SIZE;
	PgrEvent *ev = &Shmem->events[slot];

	if (pg_atomic_read_u32(&ev->ready) == 1) {
		pg_atomic_fetch_add_u64(&Shmem->dropped_events, 1);
		return;
	}
	ev->id = (uint32)PgrClientId;
	ev->index = (uint32)StatementIndex;
	ev->type = event_type;
	ev->duration_us = duration_us;

	pg_write_barrier();
	pg_atomic_write_u32(&ev->ready, 1);

	pgr_wakeup_worker();
}

bool pgr_read_event(PgrEvent *out) {
	PgrEvent *ev = &Shmem->events[Shmem->tail % PGR_RING_SIZE];

	if (pg_atomic_read_u32(&ev->ready) == 0) {
		return false;
	}

	pg_read_barrier();
	out->id = ev->id;
	out->index = ev->index;
	out->type = ev->type;
	out->duration_us = ev->duration_us;

	pg_atomic_write_u32(&ev->ready, 0);
	Shmem->tail++;

	return true;
}
