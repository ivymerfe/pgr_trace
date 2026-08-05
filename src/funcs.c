#include "postgres.h"

#include "access/htup_details.h"
#include "funcapi.h"

#include "shmem.h"

PG_FUNCTION_INFO_V1(tx_commit_stats_get);
PG_FUNCTION_INFO_V1(tx_commit_stats_reset);

Datum tx_commit_stats_get(PG_FUNCTION_ARGS) {
	TupleDesc tupdesc;
	Datum values[3];
	bool nulls[3] = {false, false, false};

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE) {
		ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
						errmsg("function returning record called in context "
							   "that cannot accept type record")));
	}

	tupdesc = BlessTupleDesc(tupdesc);

	values[0] =
		Int64GetDatum(pg_atomic_read_u64(&Shmem->successful_commit_count));
	values[1] = Int64GetDatum(pg_atomic_read_u64(&Shmem->failed_commit_count));
	values[2] = Int64GetDatum(pg_atomic_read_u64(&Shmem->rollback_count));

	PG_RETURN_DATUM(HeapTupleGetDatum(heap_form_tuple(tupdesc, values, nulls)));
}

Datum tx_commit_stats_reset(PG_FUNCTION_ARGS) {
	pg_atomic_write_u64(&Shmem->successful_commit_count, 0);
	pg_atomic_write_u64(&Shmem->failed_commit_count, 0);
	pg_atomic_write_u64(&Shmem->rollback_count, 0);

	PG_RETURN_VOID();
}
