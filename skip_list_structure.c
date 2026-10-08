/*
 * skiplist_pg.c  --  ESQUELETO (cuerpos vacíos a propósito)
 */
#include "skiplist.h"

#ifndef SKIPLIST_STANDALONE
PG_MODULE_MAGIC;
#endif

/* ================= Ciclo de vida ================= */

SkipListHeader *skiplist_create(int max_level, double p)
{
    SkipListHeader *head;
    SkipListNode *nodo;

    head = malloc(sizeof(SkipListHeader));

    nodo = malloc(sizeof(SkipListNode));

    nodo->level = max_level;

    nodo->forward = malloc(max_level * sizeof(SkipListNode *));

    for (int i = 0; i < max_level; i++)
    {
        nodo->forward[i] = NULL;
    }

    head->head = nodo;
    head->max_level = max_level;
    head->current_level = 1;
    head->p = p;
    head->length = 0;

    return head;
}


void skiplist_destroy(SkipListHeader *hdr)
{
    while (hdr->head != NULL)
    {
        SkipListNode *temp = hdr->head;
        hdr->head = hdr->head->forward[0];

        free(temp->forward);
        free(temp);
    }

    free(hdr);
}


/* ================= Nodos ================= */

SkipListNode *skiplist_node_create(int32 key, ItemPointerData tid, int level)
{
    SkipListNode *node = malloc(sizeof(SkipListNode));

    node->key = key;
    node->tid = tid;
    node->level = level;

    node->forward = malloc(level * sizeof(SkipListNode *));

    for (int i = 0; i < level; i++)
    {
        node->forward[i] = NULL;
    }

    return node;
}


void skiplist_node_free(SkipListNode *node)
{
    if (!node)
    {
        return;
    }

    free(node->forward);
    free(node);
}


/* ================= Niveles ================= */

int skiplist_random_level(const SkipListHeader *hdr)
{
    int nivel = 1;
    double rnd;

    rnd = (double) rand() / RAND_MAX;

    while (rnd < hdr->p && nivel < hdr->max_level)
    {
        nivel++;
        rnd = (double) rand() / RAND_MAX;
    }

    return nivel;
}
/* ================= Operaciones ================= */

SkipListHeader *skiplist_build(const int32 *keys,
                               const ItemPointerData *tids,
                               uint64 n)
{
    /* TODO: crear lista vacía e insertar los n pares (clave, tid) */
    (void) keys; (void) tids; (void) n;
    return NULL;
}

bool skiplist_search(const SkipListHeader *hdr, int32 key, ItemPointerData *out_tid)
{
    SkipListNode *current = hdr->head;
    for (int level = hdr->current_level - 1; level >= 0; level--)
    {
        while (current->forward[level] != NULL &&
               current->forward[level]->key < key)
        {
            current = current->forward[level];
        }
    }
    SkipListNode *next = current->forward[0];
    if (next != NULL && next->key == key)
    {
        *out_tid = next->tid;
        return true;
    }

    return false;
}

bool skiplist_insert(SkipListHeader *hdr, int32 key, ItemPointerData tid)
{
    SkipListNode *update[hdr->max_level];

    SkipListNode *current = hdr->head;

    for (int level = hdr->current_level - 1; level >= 0; level--)
    {
        while (current->forward[level] != NULL &&
               current->forward[level]->key < key)
        {
            current = current->forward[level];
        }

        update[level] = current;
    }
    
    if (current->forward[0] != NULL &&
        current->forward[0]->key == key)
    {
        return false;
    }

    int new_level = skiplist_random_level(hdr);

    if (new_level > hdr->current_level)
    {
        for (int level = hdr->current_level; level < new_level; level++)
        {
            update[level] = hdr->head;
        }

        hdr->current_level = new_level;
    }

    SkipListNode *n_node = skiplist_node_create(key, tid, new_level);

    for (int level = 0; level < new_level; level++)
    {
        n_node->forward[level] = update[level]->forward[level];
        update[level]->forward[level] = n_node;
    }

    hdr->length++;

    return true;
}

bool skiplist_delete(SkipListHeader *hdr, int32 key)
{
    /* TODO: arreglo update[], desenlazar, liberar, bajar current_level si toca */
    (void) hdr; (void) key;
    return false;
}

SkipListResult *skiplist_range_search(const SkipListHeader *hdr, int32 lo, int32 hi)
{
    /* TODO: ubicar primer nodo >= lo y recorrer nivel 0 hasta pasar hi */
    (void) hdr; (void) lo; (void) hi;
    return NULL;
}

/* ================= Resultados y depuración ================= */

void skiplist_result_free(SkipListResult *res)
{
    /* TODO */
    (void) res;
}

bool skiplist_validate(const SkipListHeader *hdr)
{
    /* TODO: verificar orden ascendente en cada nivel y que cada nivel
     *       superior sea subconjunto del inferior */
    (void) hdr;
    return true;
}

void skiplist_debug_print(const SkipListHeader *hdr)
{
    /* TODO: imprimir cada nivel (útil para pruebas de la Fase 3) */
    (void) hdr;
}

/* ================= FASE 4: Integración PostgreSQL =================
 * Aquí irán las funciones SQL-callable, por ejemplo:
 *
 *   PG_FUNCTION_INFO_V1(skiplist_build_sql);
 *   Datum skiplist_build_sql(PG_FUNCTION_ARGS) { ... }
 *
 * y el .control / .sql / Makefile con PGXS.
 * =============================================================== */
