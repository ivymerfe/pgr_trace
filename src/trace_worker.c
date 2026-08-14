#include "postgres.h"

#include "port.h"
#include "trace_worker.h"

#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "postmaster/interrupt.h"
#include "storage/latch.h"
#include "storage/proc.h"
#include "utils/elog.h"
#include "utils/timestamp.h"
#include "utils/wait_classes.h"

#include <stdio.h>
#include <sys/stat.h>

#include "shmem.h"

static FILE *TraceFile = NULL;

static void pgr_trace_close() {
	if (TraceFile != NULL) {
		fclose(TraceFile);
		TraceFile = NULL;
	}
}

static bool pgr_trace_create() {
	pgr_trace_close();

	char dir[MAXPGPATH];
	snprintf(dir, sizeof(dir), "%s/pgr_trace", DataDir);
	mkdir(dir, 0700);

	char path[MAXPGPATH];
	snprintf(path, sizeof(path), "%s/%ld.trace", dir,
			 (long)GetCurrentTimestamp());

	FILE *f = fopen(path, "w");
	if (f == NULL) {
		ereport(ERROR,
				(errmsg("pgr_trace: could not open trace file %s", path)));
		return false;
	}
	setvbuf(f, NULL, _IOFBF, 64 * 1024);
	TraceFile = f;
	return true;
}

static int worker_write_events() {
	if (TraceFile == NULL) {
		return 0;
	}
	PgrEvent ev;
	int count = 0;
	while (pgr_read_event(&ev)) {
		fwrite(&ev.id, sizeof(uint32), 1, TraceFile);
		fwrite(&ev.index, sizeof(uint32), 1, TraceFile);
		fwrite(&ev.duration_us, sizeof(uint32), 1, TraceFile);
		fwrite(&ev.type, sizeof(char), 1, TraceFile);
		count += 1;
	}
	return count;
}

PGDLLEXPORT void pgr_trace_worker_main(Datum main_arg) {
	pqsignal(SIGTERM, SignalHandlerForShutdownRequest);
	pqsignal(SIGHUP, SignalHandlerForConfigReload);
	BackgroundWorkerUnblockSignals();

	Shmem->worker_latch = &MyProc->procLatch;

	while (!ShutdownRequestPending) {
		if (pg_atomic_exchange_u32(&Shmem->trace_reset, 0)) {
			pgr_trace_close();
		}
		if (pgr_trace_is_running() && TraceFile == NULL) {
			if (!pgr_trace_create()) {
				break;
			}
		}
		if (worker_write_events() > 0) {
			pg_usleep(PGR_WORKER_SLEEP_US);
			continue;
		}
		ResetLatch(MyLatch);
		if (worker_write_events() > 0) {
			pg_usleep(PGR_WORKER_SLEEP_US);
			continue;
		}
		int rc =
			WaitLatch(MyLatch, WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
					  1000L, PG_WAIT_EXTENSION);

		if (rc & WL_POSTMASTER_DEATH) {
			break;
		}
		CHECK_FOR_INTERRUPTS();
	}
	worker_write_events();
	pgr_trace_close();
	Shmem->worker_latch = NULL;
}

void pgr_trace_worker_register() {
	BackgroundWorker worker;

	memset(&worker, 0, sizeof(worker));
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
	worker.bgw_start_time = BgWorkerStart_ConsistentState;
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	snprintf(worker.bgw_library_name, BGW_MAXLEN, "pgr_trace");
	snprintf(worker.bgw_function_name, BGW_MAXLEN, "pgr_trace_worker_main");
	snprintf(worker.bgw_name, BGW_MAXLEN, "pgr trace worker");
	snprintf(worker.bgw_type, BGW_MAXLEN, "pgr trace worker");
	RegisterBackgroundWorker(&worker);
}
