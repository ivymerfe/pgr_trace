#include "postgres.h"

#include "hooks.h"

#include "access/xact.h"
#include "executor/executor.h"
#include "libpq/auth.h"
#include "miscadmin.h"
#include "nodes/parsenodes.h"
#include "nodes/plannodes.h"
#include "optimizer/planner.h"
#include "parser/analyze.h"
#include "portability/instr_time.h"
#include "storage/ipc.h"
#include "storage/shmem.h"
#include "tcop/utility.h"
#include "utils/elog.h"

#include "client_id.h"
#include "shmem.h"
#include "utils/timestamp.h"

SharedMemory *Shmem = NULL;

static shmem_startup_hook_type prev_shmem_startup = NULL;
static shmem_request_hook_type prev_shmem_request = NULL;

static ClientAuthentication_hook_type prev_ClientAuthentication = NULL;
static post_parse_analyze_hook_type prev_post_parse_analyze = NULL;
static planner_hook_type prev_planner = NULL;
static ExecutorStart_hook_type prev_ExecutorStart = NULL;
static ExecutorRun_hook_type prev_ExecutorRun = NULL;
static ProcessUtility_hook_type prev_ProcessUtility = NULL;

static bool IsUserTransaction = false;

static instr_time pgr_trace_start_it;

static uint32 it_diff_us(instr_time start, instr_time end) {
	INSTR_TIME_SUBTRACT(end, start);
	return (uint32)INSTR_TIME_GET_MICROSEC(end);
}

static void pgr_trace_begin() {
	if (!pgr_is_client() || !pgr_trace_is_running()) {
		return;
	}
	INSTR_TIME_SET_CURRENT(pgr_trace_start_it);
}

static void pgr_trace_end(char event_type) {
	if (!pgr_is_client() || !pgr_trace_is_running()) {
		return;
	}
	instr_time now;
	INSTR_TIME_SET_CURRENT(now);
	uint32 duration_us = it_diff_us(pgr_trace_start_it, now);
	pgr_send_event(event_type, duration_us);
}

static void pgr_ClientAuthentication_hook(struct Port *port, int status) {
	if (prev_ClientAuthentication) {
		prev_ClientAuthentication(port, status);
	}
	pgr_read_client_id(port);
	if (pgr_is_client()) {
		ereport(DEBUG1,
				errmsg("pgr client connected with id = %d", PgrClientId));
	}
}

static void pgr_post_parse_analyze_hook(ParseState *pstate, Query *query,
										JumbleState *jstate) {
	if (pgr_is_client()) {
		long secs;
		int usecs;
		TimestampDifference(GetCurrentStatementStartTimestamp(),
							GetCurrentTimestamp(), &secs, &usecs);
		uint32 parse_duration = (uint32)(secs * 1000000L + usecs);
		pgr_send_event('p', parse_duration);
	}
	if (prev_post_parse_analyze) {
		prev_post_parse_analyze(pstate, query, jstate);
	}
}

static PlannedStmt *pgr_planner_hook(Query *parse, const char *query_string,
									 int cursorOptions,
									 ParamListInfo boundParams) {
	PlannedStmt *result;

	pgr_trace_begin();
	if (prev_planner) {
		result = prev_planner(parse, query_string, cursorOptions, boundParams);
	} else {
		result =
			standard_planner(parse, query_string, cursorOptions, boundParams);
	}
	pgr_trace_end('P');
	return result;
}

static void pgr_ExecutorStart_hook(QueryDesc *queryDesc, int eflags) {
	IsUserTransaction = true;

	pgr_trace_begin();
	if (prev_ExecutorStart) {
		prev_ExecutorStart(queryDesc, eflags);
	} else {
		standard_ExecutorStart(queryDesc, eflags);
	}
	pgr_trace_end('S');
}

static void pgr_ExecutorRun_hook(QueryDesc *queryDesc, ScanDirection direction,
								 uint64 count) {
	pgr_trace_begin();
	if (prev_ExecutorRun) {
		prev_ExecutorRun(queryDesc, direction, count);
	} else {
		standard_ExecutorRun(queryDesc, direction, count);
	}
	pgr_trace_end('E');
}

static void pgr_ProcessUtility_hook(PlannedStmt *pstmt, const char *queryString,
									bool readOnlyTree,
									ProcessUtilityContext context,
									ParamListInfo params,
									QueryEnvironment *queryEnv,
									DestReceiver *dest, QueryCompletion *qc) {
	IsUserTransaction = true;

	if (pgr_is_client() && IsTransactionBlock()) {
		Node *parsetree = pstmt->utilityStmt;
		if (IsA(parsetree, TransactionStmt)) {
			TransactionStmt *stmt = (TransactionStmt *)parsetree;
			if (stmt->kind == TRANS_STMT_ROLLBACK) {
				pgr_event_rollback();
			}
		}
	}
	pgr_trace_begin();
	if (prev_ProcessUtility) {
		prev_ProcessUtility(pstmt, queryString, readOnlyTree, context, params,
							queryEnv, dest, qc);
	} else {
		standard_ProcessUtility(pstmt, queryString, readOnlyTree, context,
								params, queryEnv, dest, qc);
	}
	pgr_trace_end('U');
}

static void pgr_xact_callback(XactEvent event, void *arg) {
	if (MyBackendType != B_BACKEND || !pgr_is_client()) {
		return;
	}
	if (!Shmem) {
		return;
	}
	if (event == XACT_EVENT_PRE_COMMIT && IsUserTransaction) {
		pgr_trace_begin();
	}
	if (event == XACT_EVENT_COMMIT && IsUserTransaction) {
		pgr_trace_end('C');
		pgr_event_commit();
		IsUserTransaction = false;
	}
	if (event == XACT_EVENT_ABORT && IsUserTransaction) {
		pgr_trace_end('A');
		pgr_event_abort();
		IsUserTransaction = false;
	}
}

static void pgr_shmem_request_hook() {
	if (prev_shmem_request) {
		prev_shmem_request();
	}

	RequestAddinShmemSpace(MAXALIGN(sizeof(SharedMemory)));
}

static void pgr_shmem_startup_hook() {
	if (prev_shmem_startup) {
		prev_shmem_startup();
	}

	bool found_mem;
	Shmem = (SharedMemory *)ShmemInitStruct("pgr_trace_mem",
											sizeof(SharedMemory), &found_mem);
	if (!found_mem) {
		pgr_init_memory();
	}
}

void pgr_setup_hooks() {
	prev_shmem_request = shmem_request_hook;
	shmem_request_hook = pgr_shmem_request_hook;

	prev_shmem_startup = shmem_startup_hook;
	shmem_startup_hook = pgr_shmem_startup_hook;

	prev_ClientAuthentication = ClientAuthentication_hook;
	ClientAuthentication_hook = pgr_ClientAuthentication_hook;

	prev_post_parse_analyze = post_parse_analyze_hook;
	post_parse_analyze_hook = pgr_post_parse_analyze_hook;

	prev_planner = planner_hook;
	planner_hook = pgr_planner_hook;

	prev_ExecutorStart = ExecutorStart_hook;
	ExecutorStart_hook = pgr_ExecutorStart_hook;

	prev_ExecutorRun = ExecutorRun_hook;
	ExecutorRun_hook = pgr_ExecutorRun_hook;

	prev_ProcessUtility = ProcessUtility_hook;
	ProcessUtility_hook = pgr_ProcessUtility_hook;

	RegisterXactCallback(pgr_xact_callback, NULL);
}
