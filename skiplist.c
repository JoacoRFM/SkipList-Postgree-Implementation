#include "skiplist.h"
#include "fmgr.h"  
PG_MODULE_MAGIC;



SkipListHeader* skiplist_create(int max_level, double p)
{
    if (max_level <= 0 || max_level > SKIPLIST_MAX_LEVEL)
        max_level = SKIPLIST_MAX_LEVEL;

    SkipListHeader* hdr = (SkipListHeader*)SL_ALLOC(sizeof(SkipListHeader));
    if (!hdr) return NULL;

    ItemPointerData vacio = { 0, 0 };
    hdr->head = skiplist_node_create(0, vacio, max_level);
    if (!hdr->head)
    {
        SL_FREE(hdr);
        return NULL;
    }

    hdr->max_level = max_level;
    hdr->current_level = 1;
    hdr->p = p;
    hdr->length = 0;

    return hdr;
}

void skiplist_destroy(SkipListHeader* hdr)
{
    if (!hdr) return;

    SkipListNode* curr = hdr->head;
    while (curr != NULL)
    {
        SkipListNode* next = curr->forward[0];
        skiplist_node_free(curr);
        curr = next;
    }
    SL_FREE(hdr);
}

SkipListNode* skiplist_node_create(int32 key, ItemPointerData tid, int level)
{
    SkipListNode* node = (SkipListNode*)SL_ALLOC(sizeof(SkipListNode));
    if (!node) return NULL;

    node->key = key;
    node->tid = tid;
    node->level = level;
    node->forward = (SkipListNode**)SL_ALLOC(level * sizeof(SkipListNode*));

    if (!node->forward)
    {
        SL_FREE(node);
        return NULL;
    }

    for (int i = 0; i < level; i++)
    {
        node->forward[i] = NULL;
    }

    return node;
}

void skiplist_node_free(SkipListNode* node)
{
    if (!node) return;
    if (node->forward)
        SL_FREE(node->forward);
    SL_FREE(node);
}

int skiplist_random_level(const SkipListHeader* hdr)
{
    int level = 1;
    double p = (hdr && hdr->p > 0.0) ? hdr->p : SKIPLIST_P;
    int maxNivel = (hdr && hdr->max_level > 0) ? hdr->max_level : SKIPLIST_MAX_LEVEL;

    while (((double)rand() / RAND_MAX) < p && level < maxNivel)
    {
        level++;
    }
    return level;
}



SkipListHeader* skiplist_build(const int32* keys, const ItemPointerData* tids, uint64 n)
{
    SkipListHeader* hdr = skiplist_create(SKIPLIST_MAX_LEVEL, SKIPLIST_P);
    if (!hdr) return NULL;

    for (uint64 i = 0; i < n; i++)
    {
        skiplist_insert(hdr, keys[i], tids[i]);
    }

    return hdr;
}

bool skiplist_search(const SkipListHeader* hdr, int32 key, ItemPointerData* out_tid)
{
    if (!hdr || !hdr->head) return false;

    SkipListNode* curr = hdr->head;

    for (int i = hdr->current_level - 1; i >= 0; i--)
    {
        while (curr->forward[i] != NULL && curr->forward[i]->key < key)
        {
            curr = curr->forward[i];
        }
    }

    curr = curr->forward[0];

    if (curr != NULL && curr->key == key)
    {
        if (out_tid)
            *out_tid = curr->tid;
        return true;
    }

    return false;
}

bool skiplist_insert(SkipListHeader* hdr, int32 key, ItemPointerData tid)
{
    if (!hdr || !hdr->head) return false;

    SkipListNode* update[SKIPLIST_MAX_LEVEL];
    SkipListNode* curr = hdr->head;

    for (int i = hdr->current_level - 1; i >= 0; i--)
    {
        while (curr->forward[i] != NULL && curr->forward[i]->key < key)
        {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }

    curr = curr->forward[0];

    if (curr != NULL && curr->key == key)
    {
        curr->tid = tid;
        return true;
    }

    int nivelNuevo = skiplist_random_level(hdr);

    if (nivelNuevo > hdr->current_level)
    {
        for (int i = hdr->current_level; i < nivelNuevo; i++)
        {
            update[i] = hdr->head;
        }
        hdr->current_level = nivelNuevo;
    }

    SkipListNode* nuevo = skiplist_node_create(key, tid, nivelNuevo);
    if (!nuevo) return false;

    for (int i = 0; i < nivelNuevo; i++)
    {
        nuevo->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = nuevo;
    }

    hdr->length++;
    return true;
}

bool skiplist_delete(SkipListHeader* hdr, int32 key)
{
    if (!hdr || !hdr->head) return false;

    SkipListNode* update[SKIPLIST_MAX_LEVEL];
    SkipListNode* curr = hdr->head;

    for (int i = hdr->current_level - 1; i >= 0; i--)
    {
        while (curr->forward[i] != NULL && curr->forward[i]->key < key)
        {
            curr = curr->forward[i];
        }
        update[i] = curr;
    }

    curr = curr->forward[0];

    if (curr == NULL || curr->key != key)
        return false;

    for (int i = 0; i < hdr->current_level; i++)
    {
        if (update[i]->forward[i] != curr)
            break;
        update[i]->forward[i] = curr->forward[i];
    }

    skiplist_node_free(curr);

    while (hdr->current_level > 1 && hdr->head->forward[hdr->current_level - 1] == NULL)
    {
        hdr->current_level--;
    }

    hdr->length--;
    return true;
}

SkipListResult* skiplist_range_search(const SkipListHeader* hdr, int32 lo, int32 hi)
{
    if (!hdr || !hdr->head || lo > hi) return NULL;

    SkipListResult* res = (SkipListResult*)SL_ALLOC(sizeof(SkipListResult));
    if (!res) return NULL;

    res->capacity = 16;
    res->count = 0;
    res->tids = (ItemPointerData*)SL_ALLOC(res->capacity * sizeof(ItemPointerData));

    if (!res->tids)
    {
        SL_FREE(res);
        return NULL;
    }

    SkipListNode* curr = hdr->head;
    for (int i = hdr->current_level - 1; i >= 0; i--)
    {
        while (curr->forward[i] != NULL && curr->forward[i]->key < lo)
        {
            curr = curr->forward[i];
        }
    }

    curr = curr->forward[0];

    while (curr != NULL && curr->key <= hi)
    {
        if (res->count >= res->capacity)
        {
            uint64 capNueva = res->capacity * 2;
            ItemPointerData* datosNuevos = (ItemPointerData*)SL_ALLOC(capNueva * sizeof(ItemPointerData));
            if (!datosNuevos) break;

            for (uint64 i = 0; i < res->count; i++)
                datosNuevos[i] = res->tids[i];

            SL_FREE(res->tids);
            res->tids = datosNuevos;
            res->capacity = capNueva;
        }

        res->tids[res->count++] = curr->tid;
        curr = curr->forward[0];
    }

    return res;
}

void skiplist_result_free(SkipListResult* res)
{
    if (!res) return;
    if (res->tids) SL_FREE(res->tids);
    SL_FREE(res);
}



bool skiplist_validate(const SkipListHeader* hdr)
{
    if (!hdr || !hdr->head) return false;

    SkipListNode* curr = hdr->head->forward[0];
    uint64 count = 0;

    while (curr != NULL)
    {
        count++;
        if (curr->forward[0] != NULL && curr->key >= curr->forward[0]->key)
        {
#ifdef SKIPLIST_STANDALONE
            printf("[VALIDATE ERROR] Error de orden entre claves %d y %d\n",
                curr->key, curr->forward[0]->key);
#endif
            return false;
        }
        curr = curr->forward[0];
    }

    if (count != hdr->length)
    {
#ifdef SKIPLIST_STANDALONE
        printf("[VALIDATE ERROR] Conteo de nodos (%lu) no coincide con length (%lu)\n",
            (unsigned long)count, (unsigned long)hdr->length);
#endif
        return false;
    }

    return true;
}

void skiplist_debug_print(const SkipListHeader* hdr)
{
    if (!hdr || !hdr->head)
    {
#ifdef SKIPLIST_STANDALONE
        printf("SkipList vacía o no inicializada.\n");
#endif
        return;
    }

#ifdef SKIPLIST_STANDALONE
    printf("\n=== ESTRUCTURA DE LA SKIP LIST (Elementos: %lu, Nivel actual: %d) ===\n",
        (unsigned long)hdr->length, hdr->current_level);

    for (int i = hdr->current_level - 1; i >= 0; i--)
    {
        printf("Nivel %2d: [HEAD]", i);
        SkipListNode* curr = hdr->head->forward[i];
        while (curr != NULL)
        {
            printf(" -> %d", curr->key);
            curr = curr->forward[i];
        }
        printf(" -> NULL\n");
    }
    printf("=====================================================================\n\n");
#endif
}


/*
int main(void)
{
    SkipListHeader* sl = skiplist_create(16, 0.5);

    ItemPointerData tid = { 1, 10 };

    int32_t keys[] = { 15, 5, 20, 10, 3, 30, 25 };
    for (int i = 0; i < 7; i++)
    {
        tid.offset = i + 1;
        skiplist_insert(sl, keys[i], tid);
    }

    skiplist_debug_print(sl);
    if (skiplist_validate(sl))
        printf("Validación de invariantes exitosa\n");

    ItemPointerData encontrado;
    if (skiplist_search(sl, 20, &encontrado))
        printf("Búsqueda exitosa: Clave 20 encontrada en (Block: %d, Offset: %d)\n",
            encontrado.block, encontrado.offset);

    printf("\nBúsqueda por Rango [8, 22]\n");
    SkipListResult* res = skiplist_range_search(sl, 8, 22);
    if (res)
    {
        printf("Elementos en rango: %lu\n", (unsigned long)res->count);
        skiplist_result_free(res);
    }

    printf("\nEliminando clave 10\n");
    skiplist_delete(sl, 10);
    skiplist_debug_print(sl);

    skiplist_destroy(sl);
    printf("Memoria liberada correctamente\n");

    return 0;
}
*/
