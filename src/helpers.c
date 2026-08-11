#include "postgres.h"

#include "helpers.h"

#include "libpq/libpq-be.h"

int32 pgr_extract_client_id(Port *port) {
	ListCell *lc;
	for (lc = list_head(port->guc_options); lc != NULL;
		 lc = lnext(port->guc_options, lc)) {
		char *name = (char *)lfirst(lc);
		ListCell *lc2 = lnext(port->guc_options, lc);
		char *value;

		if (lc2 == NULL) {
			break;
		}

		value = (char *)lfirst(lc2);

		if (strcmp(name, "pgr.client_id") == 0) {
			char *endptr;
			long parsed;

			errno = 0;
			parsed = strtol(value, &endptr, 10);
			if (errno == 0 && endptr != value && *endptr == '\0' &&
				parsed >= INT32_MIN && parsed <= INT32_MAX) {
				return (int32)parsed;
			}
		}

		lc = lc2;
	}
	return -1;
}
