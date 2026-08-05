#include "postgres.h"

#include "access/htup_details.h"
#include "funcapi.h"

#include "shmem.h"
#include <stdint.h>

PG_FUNCTION_INFO_V1(tx_commit_stats_get);
PG_FUNCTION_INFO_V1(tx_commit_stats_reset);

Datum tx_commit_stats_get(PG_FUNCTION_ARGS) {
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
	uint64_t success = pg_atomic_read_u64(&Shmem->successful_commits);
	uint64_t aborts = pg_atomic_read_u64(&Shmem->aborted);
	uint64_t rollbacks = pg_atomic_read_u64(&Shmem->rollbacks);
	uint64_t failed_commits = aborts - rollbacks;

	bool nulls[3] = {false, false, false};
	Datum values[3];
	values[0] = Int64GetDatum(success);
	values[1] = Int64GetDatum(failed_commits);
	values[2] = Int64GetDatum(rollbacks);

	PG_RETURN_DATUM(HeapTupleGetDatum(heap_form_tuple(tupdesc, values, nulls)));
}

Datum tx_commit_stats_reset(PG_FUNCTION_ARGS) {
	pg_atomic_write_u64(&Shmem->successful_commits, 0);
	pg_atomic_write_u64(&Shmem->aborted, 0);
	pg_atomic_write_u64(&Shmem->rollbacks, 0);

	PG_RETURN_VOID();
}
