#include "postgres.h"

#include "trace_worker.h"

#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/latch.h"
#include "storage/proc.h"
#include "utils/elog.h"
#include "utils/timestamp.h"
#include "utils/wait_classes.h"

#include <stdio.h>
#include <sys/stat.h>

#include "shmem.h"

static FILE *pgr_open_trace_file() {
	char dir[MAXPGPATH];
	snprintf(dir, sizeof(dir), "%s/pgr_trace", DataDir);
	mkdir(dir, 0700);

	char path[MAXPGPATH];
	snprintf(path, sizeof(path), "%s/trace_%ld.csv", dir,
			 (long)GetCurrentTimestamp());

	FILE *f = fopen(path, "w");
	if (f == NULL) {
		ereport(WARNING,
				(errmsg("pgr_trace: could not open trace file %s", path)));
		return NULL;
	}
	setvbuf(f, NULL, _IOFBF, 64 * 1024);
	return f;
}

static void pgr_drain_ring(FILE *f) {
	PgrEvent ev;

	while (pgr_read_event(&ev)) {
		if (f == NULL) {
			continue;
		}
		fprintf(f, "%u,%u,%c,%u\n", ev.id, ev.index, ev.event_type,
				ev.duration_us);
	}
}

static volatile sig_atomic_t got_sigterm = false;

static void pgr_trace_worker_sigterm(SIGNAL_ARGS) {
	int save_errno = errno;
	got_sigterm = true;
	SetLatch(MyLatch);
	errno = save_errno;
}

PGDLLEXPORT void pgr_trace_worker_main(Datum main_arg) {
	pqsignal(SIGTERM, pgr_trace_worker_sigterm);
	BackgroundWorkerUnblockSignals();

	Shmem->worker_latch = &MyProc->procLatch;

	FILE *trace_file = pgr_open_trace_file();

	while (!got_sigterm && pgr_trace_is_running()) {
		pgr_drain_ring(trace_file);
		int rc =
			WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
					  1000L, PG_WAIT_EXTENSION);
		ResetLatch(MyLatch);

		if (rc & WL_POSTMASTER_DEATH) {
			break;
		}

		CHECK_FOR_INTERRUPTS();
	}
	pgr_drain_ring(trace_file);

	if (trace_file != NULL) {
		fflush(trace_file);
		fclose(trace_file);
	}

	Shmem->worker_latch = NULL;
}

bool pgr_trace_worker_launch() {
	BackgroundWorker worker;

	memset(&worker, 0, sizeof(worker));
	worker.bgw_flags =
		BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_ConsistentState;
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	snprintf(worker.bgw_library_name, BGW_MAXLEN, "pgr_trace");
	snprintf(worker.bgw_function_name, BGW_MAXLEN, "pgr_trace_worker_main");
	snprintf(worker.bgw_name, BGW_MAXLEN, "pgr trace worker");
	snprintf(worker.bgw_type, BGW_MAXLEN, "pgr trace worker");
	worker.bgw_notify_pid = MyProcPid;

	BackgroundWorkerHandle *handle;
	if (!RegisterDynamicBackgroundWorker(&worker, &handle)) {
		ereport(WARNING,
				(errmsg("pgr_trace: could not register background worker")));
		return false;
	}

	pid_t pid;
	BgwHandleStatus status = WaitForBackgroundWorkerStartup(handle, &pid);
	if (status != BGWH_STARTED) {
		ereport(WARNING,
				(errmsg("pgr_trace: background worker failed to start")));
		return false;
	}

	return true;
}
