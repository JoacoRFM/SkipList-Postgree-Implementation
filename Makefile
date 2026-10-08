EXTENSION = skiplist
MODULE_big = skiplist
OBJS = skiplist.o skiplist_pg.o
DATA = skiplist--1.0.sql
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
