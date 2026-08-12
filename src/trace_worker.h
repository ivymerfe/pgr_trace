#ifndef PGR_EVENT_WORKER_H
#define PGR_EVENT_WORKER_H

#include "postgres.h"

bool pgr_trace_worker_launch();
void pgr_trace_worker_main(Datum main_arg);

#endif
