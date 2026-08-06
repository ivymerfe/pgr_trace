#include "postgres.h"

#include "access/xact.h"
#include "executor/executor.h"
#include "fmgr.h"
#include "hooks.h"
#include "miscadmin.h"
#include "nodes/parsenodes.h"
#include "tcop/utility.h"

#include "shmem.h"

static ProcessUtility_hook_type prev_ProcessUtility = NULL;
static ExecutorStart_hook_type prev_ExecutorStart = NULL;

static bool is_user_tx = false;

static void tx_stats_callback(XactEvent event, void *arg) {
	if (!Shmem) {
		return;
	}
	if (MyBackendType != B_BACKEND) {
		return;
	}
	if (event == XACT_EVENT_COMMIT) {
		if (is_user_tx) {
			pg_atomic_fetch_add_u64(&Shmem->successful_commits, 1);
		}
		is_user_tx = false;
	}
	if (event == XACT_EVENT_ABORT) {
		if (is_user_tx) {
			pg_atomic_fetch_add_u64(&Shmem->aborted, 1);
		}
		is_user_tx = false;
	}
}

static void tx_stats_ProcessUtility(PlannedStmt *pstmt, const char *queryString,
									bool readOnlyTree,
									ProcessUtilityContext context,
									ParamListInfo params,
									QueryEnvironment *queryEnv,
									DestReceiver *dest, QueryCompletion *qc) {
	is_user_tx = true;
	pg_atomic_fetch_add_u64(&Shmem->utility, 1);

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

static void tx_stats_ExecutorStart(QueryDesc *queryDesc, int eflags) {
	is_user_tx = true;
	pg_atomic_fetch_add_u64(&Shmem->exec_start, 1);

	if (prev_ExecutorStart) {
		prev_ExecutorStart(queryDesc, eflags);
	} else {
		standard_ExecutorStart(queryDesc, eflags);
	}
}

void setup_hooks() {
	prev_ProcessUtility = ProcessUtility_hook;
	ProcessUtility_hook = tx_stats_ProcessUtility;
	prev_ExecutorStart = ExecutorStart_hook;
	ExecutorStart_hook = tx_stats_ExecutorStart;

	RegisterXactCallback(tx_stats_callback, NULL);
}
