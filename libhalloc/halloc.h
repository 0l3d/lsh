#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct block {
  int free;
  size_t size;
  struct block *next;
  struct block *prev;
} block;

void *halloc(size_t alsiz);
void *zalloc(size_t n, size_t size);
void *ralloc(void *ptr, size_t alsiz);
void *halloc_safe(size_t size);
void hfree_safe(void *ptr);
void hfree(void *ptr);
