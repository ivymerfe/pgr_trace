#ifndef PGR_CLIENT_ID_H
#define PGR_CLIENT_ID_H

#include "libpq/libpq-be.h"

extern int32 PgrClientId;

void pgr_read_client_id(Port *port);

bool pgr_is_client(void);

#endif
