#include "postgres.h"

#include "fmgr.h"
#include "miscadmin.h"

#include "hooks.h"
#include "shmem.h"

PG_MODULE_MAGIC;

void _PG_init(void) {
	if (!process_shared_preload_libraries_in_progress) {
		ereport(ERROR, (errmsg("tx_commit_stats must be loaded via "
							   "shared_preload_libraries")));
	}

	setup_shmem_hooks();
	setup_hooks();
}
