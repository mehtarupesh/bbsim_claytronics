#ifndef _HEAP_H_
#define _HEAP_H_

typedef int (*ComparisonFunction)(void* x, void* y);

typedef struct {
  void **array;
  int arraySize;
  int n;
  ComparisonFunction compare;
} Heap;

Heap* newHeap(int initialSize, ComparisonFunction comp);
void heapAdd(Heap* h, void* item);
void* heapUpdate(Heap* h, void* item);
void* heapRemove(Heap* h);
void* heapPeek(Heap* h);

#endif
