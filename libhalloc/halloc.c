#include "halloc.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <valgrind/valgrind.h>
block *head = NULL;

#define SPLIT_THRESHOLD 16

// MALLOC ALTERNATIVE
void *halloc(size_t alsiz) {
  if (head == NULL) {
    head = (block *)sbrk(sizeof(block) + alsiz);
    if (head == (void *)-1)
      return NULL;
    head->size = alsiz;
    head->free = 0;
    head->next = NULL;
    return (void *)(head + 1);
  }

  block *best = NULL;
  block *current = head;
  while (current != NULL) {

    if (current->free == 1 && current->size >= alsiz) {
      if (!best || current->size < best->size) {
        best = current;
      }
    }

    current = current->next;
  }

  if (best) {
    if (best->size >= alsiz + sizeof(block) + SPLIT_THRESHOLD) {
      block *new = (block *)((char *)(best + 1) + alsiz);
      new->size = best->size - alsiz - sizeof(block);
      new->free = 1;
      best->size = alsiz;
      new->next = best->next;
      new->prev = best;
      if (best->next) {
        best->next->prev = new;
      }
      best->next = new;
    }
    best->free = 0;
    return (void *)(best + 1);
  }

  block *newm = (block *)sbrk(sizeof(block) + alsiz);
  if (newm == (void *)-1)
    return NULL;
  newm->size = alsiz;
  newm->free = 0;
  newm->next = NULL;

  current = head;
  while (current->next)
    current = current->next;

  current->next = newm;

  return (void *)(newm + 1);
}

void *zalloc(size_t n, size_t size) {
  void *ptr = halloc(n * size);
  if (ptr) {
    memset(ptr, 0, n * size);
  }
  return ptr;
}

void hfree(void *addr) {
  if (addr == NULL)
    return;
  block *curr = head;
  block *back = NULL;

  while (curr != NULL && (void *)(curr + 1) != addr) {
    back = curr;
    curr = curr->next;
  }

  if (curr != NULL) {
    curr->free = 1;
    if (back != NULL && back->free == 1) {
      back->size += curr->size + sizeof(block);
      back->next = curr->next;
      if (curr->next)
        curr->next->prev = back;
      curr = back;
    }
    if (curr->next != NULL && curr->next->free == 1) {
      block *next = curr->next;
      curr->size += next->size + sizeof(block);
      curr->next = next->next;
      if (curr->next)
        curr->next->prev = curr;
    }
  } else {

    printf(" Pointer does not belong to any allocated block.");
  }
}

void *ralloc(void *addr, size_t alsiz) {
  if (!addr)
    return halloc(alsiz);

  block *new = (block *)addr - 1;
  if (new->size >= alsiz)
    return addr;

  void *newaddr;
  newaddr = halloc(alsiz);
  if (!newaddr)
    return NULL;

  memcpy(newaddr, addr, new->size);
  hfree(addr);
  return newaddr;
}

// NOTE: VALGRIND implementation

void *halloc_safe(size_t size) {
  void *ptr = halloc(size);
  if (ptr) {
    VALGRIND_MALLOCLIKE_BLOCK(ptr, size, 0, 0);
  }
  return ptr;
}

void hfree_safe(void *ptr) {
  if (ptr) {
    VALGRIND_FREELIKE_BLOCK(ptr, 0);
    hfree(ptr);
  }
}
