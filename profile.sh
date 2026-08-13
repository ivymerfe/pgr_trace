#!/bin/bash

TARGET_PID=$(psql postgres -t -A -c \
 "SELECT pid FROM pg_stat_activity 
   WHERE backend_type = 'client backend' 
     AND state = 'active' 
     AND pid != pg_backend_pid() 
   ORDER BY query_start ASC LIMIT 1;")

if [ -n "$TARGET_PID" ]; then
  samply record -p "$TARGET_PID"
else 
  echo no process
fi
