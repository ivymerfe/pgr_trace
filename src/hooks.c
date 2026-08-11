#include "postgres.h"

#include "hooks.h"

#include "access/xact.h"
#include "executor/executor.h"
#include "fmgr.h"
#include "libpq/auth.h"
#include "miscadmin.h"
#include "nodes/parsenodes.h"
#include "storage/ipc.h"
#include "storage/shmem.h"
#include "tcop/utility.h"
#include "utils/elog.h"

#include "helpers.h"
#include "shmem.h"

SharedMemory *Shmem = NULL;

static shmem_startup_hook_type prev_shmem_startup = NULL;
static shmem_request_hook_type prev_shmem_request = NULL;

static ClientAuthentication_hook_type prev_ClientAuthentication = NULL;
static ProcessUtility_hook_type prev_ProcessUtility = NULL;
static ExecutorStart_hook_type prev_ExecutorStart = NULL;

static int32 PgrClientId = -1;
static bool IsUserTransaction = false;

static void pgr_ClientAuthentication_hook(struct Port *port, int status) {
	if (prev_ClientAuthentication) {
		prev_ClientAuthentication(port, status);
	}
	PgrClientId = pgr_extract_client_id(port);
	if (PgrClientId != -1) {
		ereport(LOG, errmsg("pgr connected with id = %d", PgrClientId));
	}
}

static void pgr_ExecutorStart_hook(QueryDesc *queryDesc, int eflags) {
	IsUserTransaction = true;

	if (prev_ExecutorStart) {
		prev_ExecutorStart(queryDesc, eflags);
	} else {
		standard_ExecutorStart(queryDesc, eflags);
	}
}

static void pgr_ProcessUtility_hook(PlannedStmt *pstmt, const char *queryString,
									bool readOnlyTree,
									ProcessUtilityContext context,
									ParamListInfo params,
									QueryEnvironment *queryEnv,
									DestReceiver *dest, QueryCompletion *qc) {
	IsUserTransaction = true;

	if (IsTransactionBlock()) {
		Node *parsetree = pstmt->utilityStmt;
		if (IsA(parsetree, TransactionStmt)) {
			TransactionStmt *stmt = (TransactionStmt *)parsetree;
			if (stmt->kind == TRANS_STMT_ROLLBACK) {
				pg_atomic_fetch_add_u64(&Shmem->rollbacks, 1);
			}
		}
	}
	if (prev_ProcessUtility) {
		prev_ProcessUtility(pstmt, queryString, readOnlyTree, context, params,
							queryEnv, dest, qc);
	} else {
		standard_ProcessUtility(pstmt, queryString, readOnlyTree, context,
								params, queryEnv, dest, qc);
	}
}

static void pgr_xact_callback(XactEvent event, void *arg) {
	if (!Shmem) {
		return;
	}
	if (MyBackendType != B_BACKEND) {
		return;
	}
	if (event == XACT_EVENT_COMMIT) {
		if (IsUserTransaction) {
			pgr_event_commit();
		}
		IsUserTransaction = false;
	}
	if (event == XACT_EVENT_ABORT) {
		if (IsUserTransaction) {
			pgr_event_abort();
		}
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

	prev_ProcessUtility = ProcessUtility_hook;
	ProcessUtility_hook = pgr_ProcessUtility_hook;

	prev_ExecutorStart = ExecutorStart_hook;
	ExecutorStart_hook = pgr_ExecutorStart_hook;

	RegisterXactCallback(pgr_xact_callback, NULL);
}
