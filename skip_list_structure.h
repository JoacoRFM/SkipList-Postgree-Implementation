/*
 * skiplist.h  --  ESQUELETO de Skip List en C
 *
 * Fase 3 del cronograma: se desarrolla en C "puro" (modo SKIPLIST_STANDALONE).
 * Fase 4: se integra a PostgreSQL (palloc/pfree + ItemPointerData real).
 *
 * Compilar standalone:  gcc -DSKIPLIST_STANDALONE -Wall -c skiplist.c
 */
#ifndef SKIPLIST_H
#define SKIPLIST_H

/* ------------------------------------------------------------------ */
/* 1. CAPA DE PORTABILIDAD (C puro  <->  PostgreSQL)                   */
/* ------------------------------------------------------------------ */
#ifdef SKIPLIST_STANDALONE
    #include <stdint.h>
    #include <stdbool.h>
    #include <stdlib.h>

    typedef int32_t  int32;
    typedef uint16_t uint16;
    typedef uint32_t uint32;
    typedef uint64_t uint64;

    /* Reemplazo simple del CTID de PostgreSQL (bloque + posición) */
    typedef struct ItemPointerData
    {
        uint32 block;
        uint16 offset;
    } ItemPointerData;

    #define SL_ALLOC(sz)  malloc(sz)
    #define SL_FREE(p)    free(p)
#else
    #include "postgres.h"
    #include "storage/itemptr.h"     /* ItemPointerData real */

    #define SL_ALLOC(sz)  palloc(sz)
    #define SL_FREE(p)    pfree(p)
#endif

/* ------------------------------------------------------------------ */
/* 2. CONSTANTES                                                       */
/* ------------------------------------------------------------------ */
#define SKIPLIST_MAX_LEVEL   32     /* tope duro de niveles            */
#define SKIPLIST_P           0.5    /* prob. de subir un nivel         */

/* ------------------------------------------------------------------ */
/* 3. ESTRUCTURAS                                                      */
/* ------------------------------------------------------------------ */

/* Nodo: una clave, su CTID, y un arreglo de punteros "forward" */
typedef struct SkipListNode
{
    int32                 key;       /* clave indexada                  */
    ItemPointerData       tid;       /* CTID: dónde vive la tupla       */
    int                   level;     /* nº de niveles de este nodo      */
    struct SkipListNode **forward;   /* forward[0..level-1]             */
} SkipListNode;

/* Header: metadatos de toda la estructura */
typedef struct SkipListHeader
{
    SkipListNode *head;              /* nodo centinela (sin clave real) */
    int           max_level;         /* nivel máximo permitido          */
    int           current_level;     /* nivel más alto en uso ahora     */
    double        p;                 /* probabilidad de promoción       */
    uint64        length;            /* cantidad de elementos           */
} SkipListHeader;

/* Resultado de una búsqueda por rango */
typedef struct SkipListResult
{
    ItemPointerData *tids;           /* CTIDs encontrados               */
    uint64           count;          /* cuántos hay                     */
    uint64           capacity;       /* espacio reservado               */
} SkipListResult;

/* ------------------------------------------------------------------ */
/* 4. PROTOTIPOS                                                       */
/* ------------------------------------------------------------------ */

/* --- Ciclo de vida de la estructura --- */
SkipListHeader *skiplist_create(int max_level, double p);
void            skiplist_destroy(SkipListHeader *hdr);

/* --- Nodos --- */
SkipListNode   *skiplist_node_create(int32 key, ItemPointerData tid, int level);
void            skiplist_node_free(SkipListNode *node);

/* --- Niveles aleatorios --- */
int             skiplist_random_level(const SkipListHeader *hdr);

/* --- Operaciones principales (las del cronograma) --- */
SkipListHeader *skiplist_build(const int32 *keys,
                               const ItemPointerData *tids,
                               uint64 n);
bool            skiplist_search(const SkipListHeader *hdr,
                                int32 key,
                                ItemPointerData *out_tid);
bool            skiplist_insert(SkipListHeader *hdr,
                                int32 key,
                                ItemPointerData tid);
bool            skiplist_delete(SkipListHeader *hdr, int32 key);
SkipListResult *skiplist_range_search(const SkipListHeader *hdr,
                                      int32 lo, int32 hi);

/* --- Resultados y depuración --- */
void            skiplist_result_free(SkipListResult *res);
bool            skiplist_validate(const SkipListHeader *hdr);  /* invariantes */
void            skiplist_debug_print(const SkipListHeader *hdr);

#endif /* SKIPLIST_H */