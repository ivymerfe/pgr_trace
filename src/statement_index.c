#include "postgres.h"

#include "statement_index.h"

#include <dlfcn.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

uint64 StatementIndex = 0;

static unsigned char saved_bytes[16];
static unsigned char patch_bytes[16];
static void *target_addr = NULL;

static void pgr_set_current_timestamp_hook() {
	StatementIndex += 1;

	memcpy(target_addr, saved_bytes, 16);
	((void (*)(void))target_addr)();
	memcpy(target_addr, patch_bytes, 16);
}

void pgr_statement_index_setup_hook() {
	long pagesize;
	void *page_addr;

	target_addr = dlsym(RTLD_DEFAULT, "SetCurrentStatementStartTimestamp");
	if (target_addr == NULL) {
		ereport(
			FATAL,
			(errmsg("could not resolve SetCurrentStatementStartTimestamp: %s",
					dlerror())));
	}

	pagesize = sysconf(_SC_PAGESIZE);
	page_addr = (void *)((uintptr_t)target_addr & ~(pagesize - 1));

	if (mprotect(page_addr, pagesize * 2, PROT_READ | PROT_WRITE | PROT_EXEC) !=
		0) {
		ereport(FATAL, (errmsg("mprotect failed: %m")));
	}

	memcpy(saved_bytes, target_addr, 16);

	patch_bytes[0] = 0xFF;
	patch_bytes[1] = 0x25;
	patch_bytes[2] = 0x00;
	patch_bytes[3] = 0x00;
	patch_bytes[4] = 0x00;
	patch_bytes[5] = 0x00;
	{
		void *fn = (void *)pgr_set_current_timestamp_hook;
		memcpy(patch_bytes + 6, &fn, 8);
	}

	memcpy(target_addr, patch_bytes, 16);
}
