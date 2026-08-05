MODULE_big = tx_commit_stats

SOURCES = src/funcs.c src/hooks.c src/init.c src/shmem.c

OBJS = $(patsubst %.c, build/%.o, $(SOURCES))

EXTENSION = tx_commit_stats
DATA = tx_commit_stats--0.1.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)

build/%.o: %.c | build/src
	$(CC) $(PG_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

build/src:
	@mkdir -p $@

clean-build:
	rm -rf build

clean: clean-build
