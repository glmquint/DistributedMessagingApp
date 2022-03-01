#include <stdio.h>
#include <stdlib.h>
#define DEBUG_OFF

#include "vector.h"

#define INIT_COMMON(type)\
    v->capacity = VECTOR_INIT_CAPACITY;\
    v->total = 0;\
    v->items = malloc(sizeof(type) * v->capacity);

#define RESIZE_COMMON\
    if (items) {\
        v->items = items;\
        v->capacity = capacity;\
    }

#define ADD_COMMON(resize_func)\
    if (v->capacity == v->total)\
        resize_func (v, v->capacity * 2);\
    v->items[v->total++] = item;

#define SET_COMMON\
    if (index >= 0 && index < v->total)\
        v->items[index] = item;

#define GET_COMMON(error)\
    if (index >= 0 && index < v->total)\
        return v->items[index];\
    else if (index < 0 && index + v->total >= 0)\
        return v->items[v->total + index];\
    return error;

#define DELETE_COMMON(resize_func,retval)\
    if (index >= v->total)\
        return;\
    if (index < 0){\
        if (v->total + index >= 0 && v->total + index < v->total)\
            index += v->total;\
        else\
            return;\
    }\
    v->items[index] = retval;\
    for (int i = index; i < v->total - 1; i++) {\
        v->items[i] = v->items[i + 1];\
        v->items[i + 1] = retval;\
    }\
    v->total--;\
    if (v->total > 0 && v->total == v->capacity / 4)\
        resize_func (v, v->capacity / 2);

void vector_init(vector *v)
{
    INIT_COMMON(void *)
}
void cvector_init(cvector *v)
{
    INIT_COMMON(int)
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
    RESIZE_COMMON
}
void cvector_resize(cvector *v, int capacity)
{
    #ifdef DEBUG_ON
    printf("vector_resize: %d to %d\n", v->capacity, capacity);
    #endif
    void *items = realloc(v->items, sizeof(int) * capacity);
    RESIZE_COMMON
}

void vector_add(vector *v, void *item)
{
    ADD_COMMON(vector_resize);
}
void cvector_add(cvector *v, int item)
{
    ADD_COMMON(cvector_resize);
}

void vector_set(vector *v, int index, void *item)
{
    SET_COMMON
}
void cvector_set(cvector *v, int index, int item)
{
    SET_COMMON
}

void *vector_get(vector *v, int index)
{
    GET_COMMON(NULL)
}
int cvector_get(cvector *v, int index)
{
    GET_COMMON((int)0x80000000)
}

void vector_delete(vector *v, int index)
{
    DELETE_COMMON(vector_resize, NULL)
}
void cvector_delete(cvector *v, int index)
{
    DELETE_COMMON(cvector_resize, (int)0x8000000000)
}

void vector_free(vector *v)
{
    free(v->items);
}
void cvector_free(cvector *v)
{
    free(v->items);
}
