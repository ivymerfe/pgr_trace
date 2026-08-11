#include "postgres.h"

#include "fmgr.h"
#include "miscadmin.h"

#include "hooks.h"
#include "statement_index.h"

PG_MODULE_MAGIC;

void _PG_init(void) {
	if (!process_shared_preload_libraries_in_progress) {
		ereport(
			ERROR,
			(errmsg("pgr_trace must be loaded via shared_preload_libraries")));
	}
	pgr_setup_hooks();
	pgr_statement_index_setup_hook();
}
