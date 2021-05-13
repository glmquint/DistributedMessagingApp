#include <stdio.h>
#include <stdlib.h>

#include "../vector.h"

int main(void)
{
    int i;
    int* val;

    VECTOR_INIT(v);

    VECTOR_ADD(v, "Bonjour");
    VECTOR_ADD(v, "tout");
    VECTOR_ADD(v, "le");
    VECTOR_ADD(v, "monde");

    for (i = 0; i < VECTOR_TOTAL(v); i++)
        printf("%s ", VECTOR_GET(v, char*, i));
    printf("\n");

    VECTOR_DELETE(v, 3);
    VECTOR_DELETE(v, 2);
    VECTOR_DELETE(v, 1);

    val = malloc(sizeof(int));
    *val = 12;
    VECTOR_SET(v, 0, val);
    val = malloc(sizeof(int));
    *val = 42;
    VECTOR_ADD(v, val);

    for (i = 0; i < VECTOR_TOTAL(v); i++)
        printf("%d ", *VECTOR_GET(v, int*, i));
    printf("\n");

    CVECTOR_INIT(cv);
    CVECTOR_ADD(cv, 13);
    CVECTOR_ADD(cv, 14);
    CVECTOR_ADD(cv, 15);
    CVECTOR_ADD(cv, 13);
    CVECTOR_ADD(cv, 13);
    CVECTOR_ADD(cv, 13);
    CVECTOR_ADD(cv, 13);
    CVECTOR_ADD(cv, 13);
    CVECTOR_ADD(cv, 13);
    CVECTOR_ADD(cv, 14);
    CVECTOR_ADD(cv, 15);
    CVECTOR_ADD(cv, 14);
    CVECTOR_ADD(cv, 15);
    CVECTOR_ADD(cv, 14);
    CVECTOR_ADD(cv, 15);
    CVECTOR_ADD(cv, 14);
    CVECTOR_ADD(cv, 15);
    CVECTOR_ADD(cv, 14);
    CVECTOR_ADD(cv, 15);
    CVECTOR_ADD(cv, 14);
    CVECTOR_ADD(cv, 15);

    CVECTOR_DELETE(cv, 1);

    for (i = 0; i < CVECTOR_TOTAL(cv); i++)
        printf("%d ", CVECTOR_GET(cv, int, i+1));
    printf("\n");

    VECTOR_FREE(v);
    CVECTOR_FREE(cv);
}
