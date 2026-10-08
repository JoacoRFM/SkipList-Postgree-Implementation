/* Puente SQL <-> Skip List en memoria (una copia por backend). */
#include "postgres.h"
#include "fmgr.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "utils/memutils.h"
#include "utils/builtins.h"
#include "catalog/namespace.h"
#include "catalog/pg_attribute.h"
#include "catalog/pg_type_d.h"
#include "utils/lsyscache.h"
#include "storage/itemptr.h"
#include "skip_list_structure.h"

PG_FUNCTION_INFO_V1(sl_build);
PG_FUNCTION_INFO_V1(sl_search);
PG_FUNCTION_INFO_V1(sl_insert);
PG_FUNCTION_INFO_V1(sl_range);
PG_FUNCTION_INFO_V1(sl_clear);
PG_FUNCTION_INFO_V1(sl_count);

static SkipListHeader *active_list = NULL;
static MemoryContext list_ctx = NULL;

static void
ensure_list(void)
{
    MemoryContext prev;
    if (active_list != NULL)
        return;
    if (list_ctx == NULL)
        list_ctx = AllocSetContextCreate(TopMemoryContext, "skiplist_backend", ALLOCSET_DEFAULT_SIZES);
    prev = MemoryContextSwitchTo(list_ctx);
    active_list = skiplist_create(SKIPLIST_MAX_LEVEL, SKIPLIST_P);
    MemoryContextSwitchTo(prev);
    if (active_list == NULL)
        ereport(ERROR, (errmsg("no se pudo crear la Skip List")));
}

Datum
sl_clear(PG_FUNCTION_ARGS)
{
    active_list = NULL;
    if (list_ctx != NULL)
    {
        MemoryContextDelete(list_ctx);
        list_ctx = NULL;
    }
    PG_RETURN_VOID();
}

Datum
sl_count(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT64(active_list ? (int64) active_list->length : 0);
}

Datum
sl_build(PG_FUNCTION_ARGS)
{
    Oid relid = PG_GETARG_OID(0);
    char *column = text_to_cstring(PG_GETARG_TEXT_PP(1));
    char *relname = get_rel_name(relid);
    char *nspname;
    char *query;
    Portal portal;
    MemoryContext prev;
    int attnum;

    if (relname == NULL)
        ereport(ERROR, (errmsg("la relacion no existe")));
    attnum = get_attnum(relid, column);
    if (attnum == InvalidAttrNumber || get_atttype(relid, attnum) != INT4OID)
        ereport(ERROR, (errmsg("la columna debe existir y ser de tipo integer")));
    nspname = get_namespace_name(get_rel_namespace(relid));
    if (nspname == NULL)
        ereport(ERROR, (errmsg("esquema de relacion no encontrado")));

    query = psprintf("SELECT %s, ctid FROM %s",
                     quote_identifier(column),
                     quote_qualified_identifier(nspname, relname));

    /* Recrear la lista; no conserva contenido anterior. */
    sl_clear(fcinfo);
    ensure_list();

    if (SPI_connect() != SPI_OK_CONNECT)
        ereport(ERROR, (errmsg("SPI_connect fallo")));
    portal = SPI_cursor_open_with_args(NULL, query, 0, NULL, NULL, NULL, true, 0);
    if (portal == NULL)
        ereport(ERROR, (errmsg("no se pudo abrir cursor SPI")));

    for (;;)
    {
        uint64 i, rows;
        SPI_cursor_fetch(portal, true, 1000);
        rows = SPI_processed;
        if (rows == 0)
        {
            if (SPI_tuptable) SPI_freetuptable(SPI_tuptable);
            break;
        }
        for (i = 0; i < rows; i++)
        {
            HeapTuple tuple = SPI_tuptable->vals[i];
            TupleDesc desc = SPI_tuptable->tupdesc;
            bool isnull_key, isnull_tid;
            Datum keyd = SPI_getbinval(tuple, desc, 1, &isnull_key);
            Datum tidd = SPI_getbinval(tuple, desc, 2, &isnull_tid);
            if (!isnull_key && !isnull_tid)
            {
                ItemPointerData tid = *DatumGetItemPointer(tidd);
                prev = MemoryContextSwitchTo(list_ctx);
                if (!skiplist_insert(active_list, DatumGetInt32(keyd), tid))
                    ereport(ERROR, (errmsg("fallo insertando nodo en Skip List")));
                MemoryContextSwitchTo(prev);
            }
        }
        SPI_freetuptable(SPI_tuptable);
    }
    SPI_cursor_close(portal);
    SPI_finish();
    PG_RETURN_INT64((int64) active_list->length);
}

Datum
sl_search(PG_FUNCTION_ARGS)
{
    ItemPointerData found;
    ItemPointerData *output;
    int32 key = PG_GETARG_INT32(0);
    if (active_list == NULL || !skiplist_search(active_list, key, &found))
        PG_RETURN_NULL();
    output = (ItemPointerData *) palloc(sizeof(ItemPointerData));
    *output = found;
    PG_RETURN_ITEMPOINTER(output);
}

Datum
sl_insert(PG_FUNCTION_ARGS)
{
    int32 key = PG_GETARG_INT32(0);
    ItemPointerData tid = *PG_GETARG_ITEMPOINTER(1);
    MemoryContext prev;
    bool ok;
    ensure_list();
    prev = MemoryContextSwitchTo(list_ctx);
    ok = skiplist_insert(active_list, key, tid);
    MemoryContextSwitchTo(prev);
    PG_RETURN_BOOL(ok);
}

Datum
sl_range(PG_FUNCTION_ARGS)
{
    FuncCallContext *funcctx;
    SkipListResult *result;
    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext oldctx;
        funcctx = SRF_FIRSTCALL_INIT();
        oldctx = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);
        result = active_list ? skiplist_range_search(active_list,
                        PG_GETARG_INT32(0), PG_GETARG_INT32(1)) : NULL;
        funcctx->user_fctx = result;
        funcctx->max_calls = result ? result->count : 0;
        MemoryContextSwitchTo(oldctx);
    }
    funcctx = SRF_PERCALL_SETUP();
    result = (SkipListResult *) funcctx->user_fctx;
    if (result && funcctx->call_cntr < funcctx->max_calls)
    {
        ItemPointerData *out = palloc(sizeof(ItemPointerData));
        *out = result->tids[funcctx->call_cntr];
        SRF_RETURN_NEXT(funcctx, ItemPointerGetDatum(out));
    }
    SRF_RETURN_DONE(funcctx);
}
