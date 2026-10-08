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
#include "skiplist.h"

PG_FUNCTION_INFO_V1(sl_build);
PG_FUNCTION_INFO_V1(sl_search);
PG_FUNCTION_INFO_V1(sl_insert);
PG_FUNCTION_INFO_V1(sl_range);
PG_FUNCTION_INFO_V1(sl_clear);
PG_FUNCTION_INFO_V1(sl_count);

static SkipListHeader *lista = NULL;
static MemoryContext memoriaLista = NULL;

static void
crear_lista(void)
{
    MemoryContext anterior;
    if (lista != NULL)
        return;
    if (memoriaLista == NULL)
        memoriaLista = AllocSetContextCreate(TopMemoryContext, "skiplist_backend", ALLOCSET_DEFAULT_SIZES);
    anterior = MemoryContextSwitchTo(memoriaLista);
    lista = skiplist_create(SKIPLIST_MAX_LEVEL, SKIPLIST_P);
    MemoryContextSwitchTo(anterior);
    if (lista == NULL)
        ereport(ERROR, (errmsg("no se pudo crear la Skip List")));
}

Datum
sl_clear(PG_FUNCTION_ARGS)
{
    lista = NULL;
    if (memoriaLista != NULL)
    {
        MemoryContextDelete(memoriaLista);
        memoriaLista = NULL;
    }
    PG_RETURN_VOID();
}

Datum
sl_count(PG_FUNCTION_ARGS)
{
    PG_RETURN_INT64(lista ? (int64) lista->length : 0);
}

Datum
sl_build(PG_FUNCTION_ARGS)
{
    Oid tablaId = PG_GETARG_OID(0);
    char *columna = text_to_cstring(PG_GETARG_TEXT_PP(1));
    char *nombreTabla = get_rel_name(tablaId);
    char *esquema;
    char *consulta;
    Portal cursor;
    MemoryContext anterior;
    int numColumna;

    if (nombreTabla == NULL)
        ereport(ERROR, (errmsg("la relacion no existe")));
    numColumna = get_attnum(tablaId, columna);
    if (numColumna == InvalidAttrNumber || get_atttype(tablaId, numColumna) != INT4OID)
        ereport(ERROR, (errmsg("la columna debe existir y ser de tipo integer")));
    esquema = get_namespace_name(get_rel_namespace(tablaId));
    if (esquema == NULL)
        ereport(ERROR, (errmsg("esquema de relacion no encontrado")));

    consulta = psprintf("SELECT %s, ctid FROM %s",
                     quote_identifier(columna),
                     quote_qualified_identifier(esquema, nombreTabla));

    
    sl_clear(fcinfo);
    crear_lista();

    if (SPI_connect() != SPI_OK_CONNECT)
        ereport(ERROR, (errmsg("SPI_connect fallo")));
    cursor = SPI_cursor_open_with_args(NULL, consulta, 0, NULL, NULL, NULL, true, 0);
    if (cursor == NULL)
        ereport(ERROR, (errmsg("no se pudo abrir cursor SPI")));

    for (;;)
    {
        uint64 i, filas;
        SPI_cursor_fetch(cursor, true, 1000);
        filas = SPI_processed;
        if (filas == 0)
        {
            if (SPI_tuptable) SPI_freetuptable(SPI_tuptable);
            break;
        }
        for (i = 0; i < filas; i++)
        {
            HeapTuple fila = SPI_tuptable->vals[i];
            TupleDesc descripcion = SPI_tuptable->tupdesc;
            bool claveNula, tidNulo;
            Datum claveDato = SPI_getbinval(fila, descripcion, 1, &claveNula);
            Datum tidDato = SPI_getbinval(fila, descripcion, 2, &tidNulo);
            if (!claveNula && !tidNulo)
            {
                ItemPointerData tid = *DatumGetItemPointer(tidDato);
                anterior = MemoryContextSwitchTo(memoriaLista);
                if (!skiplist_insert(lista, DatumGetInt32(claveDato), tid))
                    ereport(ERROR, (errmsg("fallo insertando nodo en Skip List")));
                MemoryContextSwitchTo(anterior);
            }
        }
        SPI_freetuptable(SPI_tuptable);
    }
    SPI_cursor_close(cursor);
    SPI_finish();
    PG_RETURN_INT64((int64) lista->length);
}

Datum
sl_search(PG_FUNCTION_ARGS)
{
    ItemPointerData encontrado;
    ItemPointerData *salida;
    int32 key = PG_GETARG_INT32(0);
    if (lista == NULL || !skiplist_search(lista, key, &encontrado))
        PG_RETURN_NULL();
    salida = (ItemPointerData *) palloc(sizeof(ItemPointerData));
    *salida = encontrado;
    PG_RETURN_ITEMPOINTER(salida);
}

Datum
sl_insert(PG_FUNCTION_ARGS)
{
    int32 key = PG_GETARG_INT32(0);
    ItemPointerData tid = *PG_GETARG_ITEMPOINTER(1);
    MemoryContext anterior;
    bool ok;
    crear_lista();
    anterior = MemoryContextSwitchTo(memoriaLista);
    ok = skiplist_insert(lista, key, tid);
    MemoryContextSwitchTo(anterior);
    PG_RETURN_BOOL(ok);
}

Datum
sl_range(PG_FUNCTION_ARGS)
{
    FuncCallContext *contexto;
    SkipListResult *resultado;
    if (SRF_IS_FIRSTCALL())
    {
        MemoryContext memoriaAnterior;
        contexto = SRF_FIRSTCALL_INIT();
        memoriaAnterior = MemoryContextSwitchTo(contexto->multi_call_memory_ctx);
        resultado = lista ? skiplist_range_search(lista,
                        PG_GETARG_INT32(0), PG_GETARG_INT32(1)) : NULL;
        contexto->user_fctx = resultado;
        contexto->max_calls = resultado ? resultado->count : 0;
        MemoryContextSwitchTo(memoriaAnterior);
    }
    contexto = SRF_PERCALL_SETUP();
    resultado = (SkipListResult *) contexto->user_fctx;
    if (resultado && contexto->call_cntr < contexto->max_calls)
    {
        ItemPointerData *out = palloc(sizeof(ItemPointerData));
        *out = resultado->tids[contexto->call_cntr];
        SRF_RETURN_NEXT(contexto, ItemPointerGetDatum(out));
    }
    SRF_RETURN_DONE(contexto);
}