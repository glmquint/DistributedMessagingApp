#include <stdio.h>
#include <stdlib.h>

#include "vector.h"

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
    CVECTOR_ADD(cv, 16);
    CVECTOR_ADD(cv, 17);
    CVECTOR_ADD(cv, 18);
    CVECTOR_ADD(cv, 19);
    CVECTOR_ADD(cv, 20);
    CVECTOR_ADD(cv, 21);
    CVECTOR_ADD(cv, 22);
    CVECTOR_ADD(cv, 23);
    CVECTOR_ADD(cv, 24);
    CVECTOR_ADD(cv, 25);
    CVECTOR_ADD(cv, 26);
    CVECTOR_ADD(cv, 27);
    CVECTOR_ADD(cv, 28);
    CVECTOR_ADD(cv, 29);
    CVECTOR_ADD(cv, 30);
    CVECTOR_ADD(cv, 31);
    CVECTOR_ADD(cv, 32);
    CVECTOR_ADD(cv, 33);

    CVECTOR_DELETE(cv, 1);

    for (i = 0; i < CVECTOR_TOTAL(cv); i++)
        printf("%d) %d\n", i, CVECTOR_GET(cv, int, i));
    printf("\n");

    for (i = (-1 * CVECTOR_TOTAL(cv)); i < 0; i++)
        printf("%d) %d\n", i, CVECTOR_GET(cv, int, i));
    printf("\n");

    VECTOR_FREE(v);
    CVECTOR_FREE(cv);
}
