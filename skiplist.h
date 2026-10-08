#ifndef SKIPLIST_H
#define SKIPLIST_H

#include "postgres.h"
#include "storage/itemptr.h"

#define SL_ALLOC(sz) palloc(sz)
#define SL_FREE(p) pfree(p)

#define SKIPLIST_MAX_LEVEL 32
#define SKIPLIST_P 0.5
typedef struct SkipListNode {int32 key; ItemPointerData tid; int level; struct SkipListNode **forward;} SkipListNode;
typedef struct SkipListHeader { SkipListNode *head; int max_level; int current_level; double p; uint64 length;} SkipListHeader;
typedef struct SkipListResult {ItemPointerData *tids; uint64 count; uint64 capacity;} SkipListResult;
SkipListHeader *skiplist_create(int max_level, double p);
void skiplist_destroy(SkipListHeader *hdr);
SkipListNode *skiplist_node_create(int32 key, ItemPointerData tid, int level);
void skiplist_node_free(SkipListNode *node);
int skiplist_random_level(const SkipListHeader *hdr);
SkipListHeader *skiplist_build(const int32 *keys,const ItemPointerData *tids,uint64 n);
bool skiplist_search(const SkipListHeader *hdr,int32 key,ItemPointerData *out_tid);
bool skiplist_insert(SkipListHeader *hdr,int32 key,ItemPointerData tid);
bool skiplist_delete(SkipListHeader *hdr,int32 key);
SkipListResult *skiplist_range_search(const SkipListHeader *hdr,int32 lo,int32 hi);
void skiplist_result_free(SkipListResult *res);
bool skiplist_validate(const SkipListHeader *hdr);
void skiplist_debug_print(const SkipListHeader *hdr);
#endif
