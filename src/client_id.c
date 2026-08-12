#include "postgres.h"

#include "client_id.h"

int32 PgrClientId = -1;

void pgr_read_client_id(Port *port) {
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
				PgrClientId = (int32)parsed;
				return;
			}
		}
		lc = lc2;
	}
}

bool pgr_is_client() {
	return PgrClientId != -1;
}
