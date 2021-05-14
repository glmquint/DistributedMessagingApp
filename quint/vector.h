#define VECTOR_INIT_CAPACITY 4

#define VECTOR_INIT(vec) vector vec; vector_init(&vec)
#define CVECTOR_INIT(vec) cvector vec; cvector_init(&vec)

#define VECTOR_ADD(vec, item) vector_add(&vec, (void *) item)
#define CVECTOR_ADD(vec, item) cvector_add(&vec, (int) item)

#define VECTOR_SET(vec, id, item) vector_set(&vec, id, (void *) item)
#define CVECTOR_SET(vec, id, item) cvector_set(&vec, id, (int) item)

#define VECTOR_GET(vec, type, id) (type) vector_get(&vec, id)
#define CVECTOR_GET(vec, type, id) (type) cvector_get(&vec, id)

#define VECTOR_DELETE(vec, id) vector_delete(&vec, id)
#define CVECTOR_DELETE(vec, id) cvector_delete(&vec, id)

#define VECTOR_TOTAL(vec) vector_total(&vec)
#define CVECTOR_TOTAL(vec) cvector_total(&vec)

#define VECTOR_FREE(vec) vector_free(&vec)
#define CVECTOR_FREE(vec) cvector_free(&vec)

typedef struct vector {
    void **items;
    int capacity;
    int total;
} vector;

typedef struct cvector {
    int *items;
    int capacity;
    int total;
} cvector;

void vector_init(vector *);
void cvector_init(cvector *);

int vector_total(vector *);
int cvector_total(cvector *);

void vector_resize(vector *, int);
void cvector_resize(cvector *, int);

void vector_add(vector *, void *);
void cvector_add(cvector *, int);

void vector_set(vector *, int, void *);
void cvector_set(cvector *, int, int);

void *vector_get(vector *, int);
int cvector_get(cvector *, int);

void vector_delete(vector *, int);
void cvector_delete(cvector *, int);

void vector_free(vector *);
void cvector_free(cvector *);

