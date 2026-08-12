#ifndef PGR_EVENT_WORKER_H
#define PGR_EVENT_WORKER_H

#include "postgres.h"

// sorry
#define PGR_WORKER_SLEEP_US 1000

void pgr_trace_worker_register(void);
void pgr_trace_worker_main(Datum main_arg);

#endif
