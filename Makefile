MODULE_big = pgr_trace

SOURCES = src/init.c src/shmem.c src/funcs.c src/hooks.c src/statement_index.c

OBJS = $(patsubst %.c, build/%.o, $(SOURCES))

EXTENSION = pgr_trace
DATA = pgr_trace--0.1.sql

PG_CONFIG = pg_config

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

build/%.o: %.c | build/src
	@$(CC) $(PG_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/%.bc: %.c | build/src
	@$(COMPILE.c.bc) $< -o $@

build/src:
	mkdir -p $@

clean-build:
	rm -rf build

clean: clean-build
