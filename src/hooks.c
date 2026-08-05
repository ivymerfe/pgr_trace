#include "postgres.h"

#include "access/xact.h"
#include "fmgr.h"
#include "hooks.h"
#include "miscadmin.h"
#include "nodes/parsenodes.h"
#include "tcop/utility.h"

#include "shmem.h"

static ProcessUtility_hook_type prev_ProcessUtility = NULL;

static void tx_stats_callback(XactEvent event, void *arg) {
	if (!Shmem) {
		return;
	}
	if (MyBackendType != B_BACKEND) {
		return;
	}
	if (event == XACT_EVENT_COMMIT) {
		pg_atomic_fetch_add_u64(&Shmem->successful_commits, 1);
	}
	if (event == XACT_EVENT_ABORT) {
		pg_atomic_fetch_add_u64(&Shmem->aborted, 1);
	}
}

static void tx_stats_ProcessUtility(PlannedStmt *pstmt, const char *queryString,
									bool readOnlyTree,
									ProcessUtilityContext context,
									ParamListInfo params,
									QueryEnvironment *queryEnv,
									DestReceiver *dest, QueryCompletion *qc) {
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

void setup_hooks() {
	prev_ProcessUtility = ProcessUtility_hook;
	ProcessUtility_hook = tx_stats_ProcessUtility;
	RegisterXactCallback(tx_stats_callback, NULL);
}
