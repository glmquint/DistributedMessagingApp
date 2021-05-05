#include <stdlib.h>
#include <stdio.h>

/*dynamic vector of unsigned ints*/
typedef struct uivector {
  unsigned* data;
  size_t size; /*size in number of unsigned longs*/
  size_t allocsize; /*allocated size in bytes*/
} uivector;

static void uivector_cleanup(void* p) {
  ((uivector*)p)->size = ((uivector*)p)->allocsize = 0;
  free(((uivector*)p)->data);
  ((uivector*)p)->data = NULL;
}

/*returns 1 if success, 0 if failure ==> nothing done*/
static unsigned uivector_resize(uivector* p, size_t size) {
  size_t allocsize = size * sizeof(unsigned);
  if(allocsize > p->allocsize) {
    size_t newsize = allocsize + (p->allocsize >> 1u);
    void* data = realloc(p->data, newsize);
    if(data) {
      p->allocsize = newsize;
      p->data = (unsigned*)data;
    }
    else return 0; /*error: not enough memory*/
  }
  p->size = size;
  return 1; /*success*/
}

static void uivector_init(uivector* p) {
  p->data = NULL;
  p->size = p->allocsize = 0;
}

/*returns 1 if success, 0 if failure ==> nothing done*/
static unsigned uivector_push_back(uivector* p, unsigned c) {
  if(!uivector_resize(p, p->size + 1)) return 0;
  p->data[p->size - 1] = c;
  return 1;
}

int main(){
	uivector uiv;
	uivector_init(&uiv);
	uivector_push_back(&uiv, 1337);
	uivector_push_back(&uiv, 2337);
	uivector_push_back(&uiv, 3337);
	uivector_push_back(&uiv, 4337);
	uivector_push_back(&uiv, 5337);
	uivector_push_back(&uiv, 6337);
	uivector_push_back(&uiv, 7337);
	for (int i = 0; i < (&uiv)->size; i++){
		printf("%d\n", (&uiv)->data[i]);
	}
    printf("\n");
    uivector_resize(&uiv, (&uiv)->size - 1);
    uivector_push_back(&uiv, 42);
	for (int i = 0; i < (&uiv)->size; i++){
		printf("%d\n", (&uiv)->data[i]);
	}
	return(0);
}
