#include <stdio.h>
#include <stdlib.h>

#include "vector.h"

void vector_init(vector *v)
{
    v->capacity = VECTOR_INIT_CAPACITY;
    v->total = 0;
    v->items = malloc(sizeof(void *) * v->capacity);
}
void cvector_init(cvector *v)
{
    v->capacity = VECTOR_INIT_CAPACITY;
    v->total = 0;
    v->items = malloc(sizeof(int) * v->capacity);
}


int vector_total(vector *v)
{
    return v->total;
}
int cvector_total(cvector *v)
{
    return v->total;
}


void vector_resize(vector *v, int capacity)
{
    #ifdef DEBUG_ON
    printf("vector_resize: %d to %d\n", v->capacity, capacity);
    #endif

    void **items = realloc(v->items, sizeof(void *) * capacity);
    if (items) {
        v->items = items;
        v->capacity = capacity;
    }
}
void cvector_resize(cvector *v, int capacity)
{
    #ifdef DEBUG_ON
    printf("vector_resize: %d to %d\n", v->capacity, capacity);
    #endif

    void *items = realloc(v->items, sizeof(int) * capacity);
    if (items) {
        v->items = items;
        v->capacity = capacity;
    }
}


void vector_add(vector *v, void *item)
{
    if (v->capacity == v->total)
        vector_resize(v, v->capacity * 2);
    v->items[v->total++] = item;
}
void cvector_add(cvector *v, int item)
{
    if (v->capacity == v->total)
        cvector_resize(v, v->capacity * 2);
    v->items[v->total++] = item;
}


void vector_set(vector *v, int index, void *item)
{
    if (index >= 0 && index < v->total)
        v->items[index] = item;
}
void cvector_set(cvector *v, int index, int item)
{
    if (index >= 0 && index < v->total)
        v->items[index] = item;
}


void *vector_get(vector *v, int index)
{
    if (index >= 0 && index < v->total)
        return v->items[index];
    return NULL;
}
int cvector_get(cvector *v, int index)
{
    if (index >= 0 && index < v->total)
        return v->items[index];
    return -2147483648;
}


void vector_delete(vector *v, int index)
{
    if (index < 0 || index >= v->total)
        return;

    v->items[index] = NULL;

    for (int i = index; i < v->total - 1; i++) {
        v->items[i] = v->items[i + 1];
        v->items[i + 1] = NULL;
    }

    v->total--;

    if (v->total > 0 && v->total == v->capacity / 4)
        vector_resize(v, v->capacity / 2);
}
void cvector_delete(cvector *v, int index)
{
    if (index < 0 || index >= v->total)
        return;

    v->items[index] = -2147483648;

    for (int i = index; i < v->total - 1; i++) {
        v->items[i] = v->items[i + 1];
        v->items[i + 1] = -2147483648;
    }

    v->total--;

    if (v->total > 0 && v->total == v->capacity / 4)
        cvector_resize(v, v->capacity / 2);
}


void vector_free(vector *v)
{
    free(v->items);
}
void cvector_free(cvector *v)
{
    free(v->items);
}
