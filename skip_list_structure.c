/*
 * skiplist_pg.c  --  ESQUELETO (cuerpos vacíos a propósito)
 */
#include "skip_list_structure.h"
#ifndef SKIPLIST_STANDALONE
int PG_MODULE_MAGIC;
#endif

/* ================= Ciclo de vida ================= */

SkipListHeader *skiplist_create(int max_level, double p)
{
    SkipListHeader *head;
    SkipListNode *nodo;

    if (max_level <= 0 || p < 0.0 || p > 1.0)
        return NULL;

    head = SL_ALLOC(sizeof(SkipListHeader));
    nodo = SL_ALLOC(sizeof(SkipListNode));

    nodo->level = max_level;
    nodo->forward = SL_ALLOC(max_level * sizeof(SkipListNode *));

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
    /* TODO (PERSONA 3): implementar liberacion completa de memoria */

    SkipListNode* current;
    SkipListNode* next;

    if (hdr == NULL) { return; }

    if (hdr->head != NULL) {
        current = hdr->head->forward[0];
        while (current != NULL) {
            next = current->forward[0];
            skiplist_node_free(current);
            current = next;
        }

        skiplist_node_free(hdr->head);
    }

    SL_FREE(hdr);
}


/* ================= Nodos ================= */

SkipListNode *skiplist_node_create(int32 key, ItemPointerData tid, int level)
{
    SkipListNode *node;

    if (level <= 0)
        return NULL;

    node = SL_ALLOC(sizeof(SkipListNode));

    node->key = key;
    node->tid = tid;
    node->level = level;

    node->forward = SL_ALLOC(level * sizeof(SkipListNode *));

    for (int i = 0; i < level; i++)
    {
        node->forward[i] = NULL;
    }

    return node;
}


void skiplist_node_free(SkipListNode *node)
{
    if (node == NULL)
        return;

    SL_FREE(node->forward);
    SL_FREE(node);
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
    SkipListHeader* hdr = skiplist_create(SKIPLIST_MAX_LEVEL, SKIPLIST_P);

    if (hdr == NULL)
        return NULL;

    for (uint64 i = 0; i < n; i++) {
        if (!skiplist_insert(hdr, keys[i], tids[i])) {
            skiplist_destroy(hdr);
            return NULL;
        }
    }

    return hdr;
}

bool
skiplist_search(const SkipListHeader *hdr, int32 key,
                ItemPointerData *out_tid)
{
    SkipListNode *current;
    SkipListNode *next;
    int level;

    if (hdr == NULL || hdr->head == NULL || out_tid == NULL)
        return false;

    current = hdr->head;

    for (level = hdr->current_level - 1; level >= 0; level--)
    {
        while (current->forward[level] != NULL &&
               current->forward[level]->key < key)
        {
            current = current->forward[level];
        }
    }

    next = current->forward[0];

    if (next != NULL && next->key == key)
    {
        *out_tid = next->tid;
        return true;
    }

    return false;
}

bool
skiplist_insert(SkipListHeader *hdr, int32 key,
                ItemPointerData tid)
{
    SkipListNode **update;
    SkipListNode *current;
    SkipListNode *n_node;
    int level;
    int new_level;

    if (hdr == NULL || hdr->head == NULL)
        return false;

    /*
     * Reservamos dinámicamente el arreglo update.
     * Así evitamos un Variable Length Array (VLA).
     */
    update = (SkipListNode **) SL_ALLOC(
        sizeof(SkipListNode *) * hdr->max_level
    );

    if (update == NULL)
        return false;

    current = hdr->head;

    for (level = hdr->current_level - 1;
         level >= 0;
         level--)
    {
        while (current->forward[level] != NULL &&
               current->forward[level]->key < key)
        {
            current = current->forward[level];
        }

        update[level] = current;
    }

    /*
     * Verificamos si la clave ya existe.
     */
    if (current->forward[0] != NULL &&
        current->forward[0]->key == key)
    {
        SL_FREE(update);
        return false;
    }

    new_level = skiplist_random_level(hdr);

    /*
     * Si el nuevo nodo tiene más niveles que los actuales,
     * el head será el anterior en esos niveles.
     */
    if (new_level > hdr->current_level)
    {
        for (level = hdr->current_level;
             level < new_level;
             level++)
        {
            update[level] = hdr->head;
        }

        hdr->current_level = new_level;
    }

    n_node = skiplist_node_create(key, tid, new_level);

    if (n_node == NULL)
    {
        SL_FREE(update);
        return false;
    }

    /*
     * Insertamos el nuevo nodo en cada nivel.
     */
    for (level = 0; level < new_level; level++)
    {
        n_node->forward[level] = update[level]->forward[level];
        update[level]->forward[level] = n_node;
    }

    hdr->length++;

    /*
     * update ya no es necesario.
     */
    SL_FREE(update);

    return true;
}
bool skiplist_delete(SkipListHeader *hdr, int32 key)
{
    /* TODO: arreglo update[], desenlazar, liberar, bajar current_level si toca */
    SkipListNode* update[SKIPLIST_MAX_LEVEL];
    SkipListNode* x;

    if (hdr == NULL) { return false; }

    x = hdr->head;
    for (int i = hdr->current_level - 1; i >= 0; i--)
    {
        while (x->forward[i] != NULL && x->forward[i]->key < key) {
            x = x->forward[i];
        }
        update[i] = x;
    }

    x = x->forward[0];
    if (x == NULL || x->key != key) { return false; }

    for (int i = 0; i < hdr->current_level; i++)
    {
        if (update[i]->forward[i] != x) { break; }
        update[i]->forward[i] = x->forward[i];
    }

    skiplist_node_free(x);

    while (hdr->current_level > 1 && hdr->head->forward[hdr->current_level - 1] == NULL) {
        hdr->current_level--;
    }

    hdr->length--;
    return true;
}

SkipListResult *skiplist_range_search(const SkipListHeader *hdr, int32 lo, int32 hi)
{
    /* TODO: ubicar primer nodo >= lo y recorrer nivel 0 hasta pasar hi */
    SkipListResult* res;
    SkipListNode* x;
    SkipListNode* tmp;
    uint64 count = 0;
    uint64 idx = 0;

    if (hdr == NULL) { return NULL; }

    res = SL_ALLOC(sizeof(SkipListResult));
    if (res == NULL) { return NULL; }

    res->tids = NULL;
    res->count = 0;
    res->capacity = 0;

    if (lo > hi) { return res; }
    x = hdr->head;

    for (int i = hdr->current_level - 1; i >= 0; i--)
    {
        while (x->forward[i] != NULL && x->forward[i]->key < lo) {
            x = x->forward[i];
        }
    }

    x = x->forward[0];

    tmp = x;
    while (tmp != NULL && tmp->key <= hi) {
        count++;
        tmp = tmp->forward[0];
    }

    if (count == 0) { return res; }

    res->tids = SL_ALLOC(sizeof(ItemPointerData) * count);
    if (res->tids == NULL) {
        SL_FREE(res);
        return NULL;
    }

    res->capacity = count;

    while (x != NULL && x->key <= hi) {
        res->tids[idx++] = x->tid;
        x = x->forward[0];
    }
    res->count = count;

    return res;
}

/* ================= Resultados y depuración ================= */

void skiplist_result_free(SkipListResult *res)
{
    /* TODO */
    if (res == NULL) { return; }
    if (res->tids != NULL) { SL_FREE(res->tids); }
    SL_FREE(res);
}

bool skiplist_validate(const SkipListHeader *hdr)
{
    SkipListNode *current;
    SkipListNode *lower;
    int level;

    if (hdr == NULL || hdr->head == NULL)
    {
        return false;
    }

    if (hdr->current_level < 1 ||
        hdr->current_level > hdr->max_level) { return false; }

    /*Verificar que el nivel 0 esté ordenado */
    current = hdr->head->forward[0];
    while (current != NULL && current->forward[0] != NULL)
    {
        if (current->key >= current->forward[0]->key) { return false; }
        current = current->forward[0];
    }

    /*Verificar que los nodos de niveles superiores también existan en el nivel inmediatamente inferior */
    for (level = 1; level < hdr->current_level; level++)
    {
        current = hdr->head->forward[level];
        while (current != NULL)
        {
            bool found = false;
            lower = hdr->head->forward[level - 1];
            while (lower != NULL)
            {
                if (lower == current)
                {
                    found = true;
                    break;
                }
                lower = lower->forward[level - 1];
            }
            if (!found) { return false; }
            current = current->forward[level];
        }
    }
    return true;
}

void skiplist_debug_print(const SkipListHeader *hdr)
{
    SkipListNode *current;
    int level;
    if (hdr == NULL || hdr->head == NULL)
    {
        printf("Skip List: NULL\n");
        return;
    }
    printf("Skip List\n");
    printf("length = %llu, current_level = %d, max_level = %d\n",
           (unsigned long long)hdr->length,
           hdr->current_level,
           hdr->max_level);

    for (level = hdr->current_level - 1; level >= 0; level--)
    {
        printf("Nivel %d: HEAD", level);
        current = hdr->head->forward[level];
        while (current != NULL)
        {
            printf(" -> %d", current->key);
            current = current->forward[level];
        }
        printf("\n");
    }
}

/* ================= FASE 4: Integración PostgreSQL =================
 * Aquí irán las funciones SQL-callable, por ejemplo:
 *
 *   PG_FUNCTION_INFO_V1(skiplist_build_sql);
 *   Datum skiplist_build_sql(PG_FUNCTION_ARGS) { ... }
 *
 * y el .control / .sql / Makefile con PGXS.
 * =============================================================== */
