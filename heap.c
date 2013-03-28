#include "heap.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int 
left(int i) 
{
  return 2*i + 1;
}

static int 
right(int i) 
{
  return 2*i + 2;
}
static int 
parent(int i) 
{
  return (i-1)/2;
}

static void
grow(Heap* h)
{
  void* p = realloc(h->array, h->arraySize*2*sizeof(void*));
  assert (p != NULL);
  h->arraySize = h->arraySize*2;
  h->array = p;
}

static void
shrink(Heap* h)
{
  void* p = realloc(h->array, (h->arraySize>>1)*sizeof(void*));
  assert (p != NULL);
  h->arraySize = h->arraySize>>1;
  h->array = p;
}

static void
swap(Heap* h, int i, int j)
{
  void* p = h->array[i];
  h->array[i] = h->array[j];
  h->array[j] = p;
}

static void 
bubbleUp(Heap* h, int i) 
{
  int p = parent(i);
  while (i > 0 && h->compare(h->array[i], h->array[p]) < 0) {
    swap(h, i,p);
    i = p;
    p = parent(i);
  }
}

static void 
bubbleDown(Heap* h, int i) {
  do {
    int j = -1;
    int r = right(i);
    if (r < h->n && h->compare(h->array[r], h->array[i]) < 0) {
      int l = left(i);
      if (h->compare(h->array[l], h->array[r]) < 0) {
	j = l;
      } else {
	j = r;
      }
    } else {
      int l = left(i);
      if (l < h->n && h->compare(h->array[l], h->array[i]) < 0) {
	j = l;
      }
    }
    if (j >= 0) swap(h, i, j);
    i = j;
  } while (i >= 0);
}

////////////////////////////////////////////////////////////////
// public API

Heap*
newHeap(int initialSize, ComparisonFunction comp)
{
  Heap* h = calloc(1, sizeof(Heap));
  int as = 1;
  while (initialSize > 0) {
    initialSize = initialSize>>1;
    as++;
  }
  h->arraySize = 1 << as;
  h->array = calloc(h->arraySize, sizeof(void*));
  h->n = 0;
  h->compare = comp;
  return h;
}

void
heapAdd(Heap* h, void* item) 
{
  if (h->n+1 >= h->arraySize) grow(h);
  h->array[h->n++] = item;
  bubbleUp(h, h->n-1);
}

// item's value has changed
void*
heapUpdate(Heap* h, void* item)
{
  int i;
  for (i=0; i<h->n; i++) {
    if (h->array[i] == item) break;
  }
  if (i == h->n) return NULL;
  if (i > 0) {
    int p = parent(i);
    if (h->compare(h->array[p], h->array[i]) > 0) {
      swap(h, p, i);
      bubbleUp(h, p);
      return item;
    }
  }
  // choose a child and bubbleUp
  int c = left(i);
  if ((c < h->n)&&(h->compare(h->array[i], h->array[c]) > 0)) {
    swap(h, c, i);
    bubbleDown(h, c);
    return item;
  } else {
    c = right(i);
    if ((c < h->n)&&(h->compare(h->array[i], h->array[c]) > 0)) {
      swap(h, c, i);
      bubbleDown(h, c);
      return item;
    }
  }
  return item;
}

void*
heapRemove(Heap* h) 
{
  void* x = h->array[0];
  h->array[0] = h->array[--h->n];
  bubbleDown(h, 0);
  if (4*(h->n+1) < h->arraySize) shrink(h);
  return x;
}

void* 
heapPeek(Heap* h)
{
  if (h->n == 0) return NULL;
  return h->array[0];
}

#ifdef TEST_HEAP
#include <ctype.h>

main(int argc, char** argv)
{
  int i;
  int j;
  Heap* h = newHeap(3, (ComparisonFunction)strcmp);
  int cnt = argc;
  for (i=1; i<argc; i++) {
    heapAdd(h, argv[i]);
    printf("adding: %s\n", (char*)heapPeek(h));
  }
  for (i=1; i<(argc-3); i++) {
    argv[i][0] = toupper(argv[i][0]);
    heapUpdate(h, argv[i]);
    printf("update UP: %s\n", (char*)heapPeek(h));
  }
  for (i=1; i<(argc-2); i++) {
    argv[i][0] = tolower(argv[i][0]);
    heapUpdate(h, argv[i]);
    printf("update DOWN: %s\n", (char*)heapPeek(h));
  }
  while (h->n > 0) {
    char* p = heapRemove(h);
    printf("%s (%d %d)\n", p, h->n, h->arraySize);
  }
}

#endif
