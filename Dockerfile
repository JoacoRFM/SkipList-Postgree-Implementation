FROM postgres:18.6-bookworm
RUN apt-get update \
 && apt-get install -y --no-install-recommends build-essential postgresql-server-dev-18 \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /opt/skiplist
COPY Makefile skiplist.control skiplist--1.0.sql skiplist.c skiplist.h skiplist_pg.c ./
RUN make && make install && make clean
