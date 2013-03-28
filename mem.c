#define __DEFINE_MALLOC_HERE_ 1
#include <stdlib.h>
#include "mymalloc.h"
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t memmutex = PTHREAD_MUTEX_INITIALIZER; 

static void
lock() 
{
  pthread_mutex_lock(&memmutex);
}

static void
unlock() 
{
  pthread_mutex_unlock(&memmutex);
}

void* 
mymalloc(size_t size, char* fn, int ln)
{
  lock();
  fprintf(stderr, "%s:%d malloc %d\n", fn, ln, (int)size);
  void* p = malloc(size);
  unlock();
  return p;
}

void*
mycalloc(size_t nmemb, size_t size, char* fn, int ln)
{
  lock();
  fprintf(stderr, "%s:%d calloc %d, %d\n", fn, ln, (int)nmemb, (int)size);
  void* p = calloc(nmemb, size);
  unlock();
  return p;
}

void *
myrealloc(void *ptr, size_t size, char* fn, int ln)
{
  lock();
  fprintf(stderr, "%s:%d realloc %p, %d\n", fn, ln, ptr, (int)size);
  void* p = realloc(ptr, size);
  unlock();
  return p;
}

void 
myfree(void *ptr, char* fn, int ln)
{
  lock();
  fprintf(stderr, "%s:%d free %p\n", fn, ln, ptr);
  free(ptr);
  unlock();
}

