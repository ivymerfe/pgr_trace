#include "postgres.h"

#include "access/htup_details.h"
#include "fmgr.h"
#include "funcapi.h"

#include "shmem.h"
#include "statement_index.h"
#include <stdint.h>

PG_FUNCTION_INFO_V1(sql_pgr_stats_get);
PG_FUNCTION_INFO_V1(sql_pgr_stats_reset);
PG_FUNCTION_INFO_V1(sql_pgr_get_statement_index);
PG_FUNCTION_INFO_V1(sql_pgr_trace_start);
PG_FUNCTION_INFO_V1(sql_pgr_trace_stop);
PG_FUNCTION_INFO_V1(sql_pgr_trace_reset);

Datum sql_pgr_stats_get(PG_FUNCTION_ARGS) {
	TupleDesc tupdesc = (TupleDesc)fcinfo->flinfo->fn_extra;
	if (tupdesc == NULL) {
		MemoryContext oldcontext =
			MemoryContextSwitchTo(fcinfo->flinfo->fn_mcxt);

		if (get_call_result_type(fcinfo, NULL, &tupdesc) !=
			TYPEFUNC_COMPOSITE) {
			ereport(ERROR,
					(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
					 errmsg("function returning record called in context "
							"that cannot accept type record")));
		}
		fcinfo->flinfo->fn_extra = (void *)tupdesc;

		MemoryContextSwitchTo(oldcontext);
	}
	uint64_t commits = pgr_stats_get_commits();
	uint64_t rollbacks = pgr_stats_get_rollbacks();
	uint64_t failed_commits = pgr_stats_get_failed_commits();
	uint64_t dropped_events = pgr_stats_get_dropped_events();

	bool nulls[4] = {false, false, false, false};
	Datum values[4];
	values[0] = Int64GetDatum(commits);
	values[1] = Int64GetDatum(rollbacks);
	values[2] = Int64GetDatum(failed_commits);
	values[3] = Int64GetDatum(dropped_events);

	PG_RETURN_DATUM(HeapTupleGetDatum(heap_form_tuple(tupdesc, values, nulls)));
}

Datum sql_pgr_stats_reset(PG_FUNCTION_ARGS) {
	pgr_stats_reset();
	PG_RETURN_VOID();
}

Datum sql_pgr_get_statement_index(PG_FUNCTION_ARGS) {
	PG_RETURN_UINT64(StatementIndex);
}

Datum sql_pgr_trace_start(PG_FUNCTION_ARGS) {
	pgr_trace_start();
	PG_RETURN_VOID();
}

Datum sql_pgr_trace_stop(PG_FUNCTION_ARGS) {
	pgr_trace_stop();
	PG_RETURN_VOID();
}

Datum sql_pgr_trace_reset(PG_FUNCTION_ARGS) {
	pgr_trace_reset();
	PG_RETURN_VOID();
}
