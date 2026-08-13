#include "postgres.h"

#include "statement_start_hook.h"

#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

uint64 StatementIndex = 0;

static unsigned char saved_bytes[16];
static unsigned char patch_bytes[16];
static void *target_addr = NULL;

static void pgr_statement_start_hook() {
	StatementIndex += 1;

	memcpy(target_addr, saved_bytes, 16);
	((void (*)(void))target_addr)();
	memcpy(target_addr, patch_bytes, 16);
}

static int find_vma_bounds(void *addr, uintptr_t *start, uintptr_t *end) {
	FILE *f = fopen("/proc/self/maps", "r");
	char line[512];
	if (!f) {
		return -1;
	}
	while (fgets(line, sizeof(line), f)) {
		uintptr_t s, e;
		if (sscanf(line, "%lx-%lx", &s, &e) == 2) {
			if ((uintptr_t)addr >= s && (uintptr_t)addr < e) {
				*start = s;
				*end = e;
				fclose(f);
				return 0;
			}
		}
	}
	fclose(f);
	return -1;
}

void pgr_statement_start_hook_setup() {
	target_addr = dlsym(RTLD_DEFAULT, "SetCurrentStatementStartTimestamp");
	if (target_addr == NULL) {
		ereport(
			FATAL,
			(errmsg("could not resolve SetCurrentStatementStartTimestamp: %s",
					dlerror())));
	}

	/*
	 * Если делать mprotect только на две страницы то мы разделим регион на три
	 * части что не нравится некоторым профилировщикам
	 */
	uintptr_t vma_start, vma_end;
	if (find_vma_bounds(target_addr, &vma_start, &vma_end) != 0) {
		ereport(FATAL, errmsg("failed to find vma bounds"));
	}
	if (mprotect((void *)vma_start, vma_end - vma_start,
				 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
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
		void *fn = (void *)pgr_statement_start_hook;
		memcpy(patch_bytes + 6, &fn, 8);
	}

	memcpy(target_addr, patch_bytes, 16);
}
