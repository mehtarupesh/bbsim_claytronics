#ifndef _MYMALLOC_H_
#define _MYMALLOC_H_

#if 0
#ifndef __DEFINE_MALLOC_HERE_
# define malloc(__size) mymalloc(__size, __FILE__, __LINE__)
# define calloc(__n, __s) mycalloc(__n, __s, __FILE__, __LINE__)
# define realloc(__p, __s) myrealloc(__p, __s,  __FILE__, __LINE__)
# define free(__p) myfree(__p,  __FILE__, __LINE__)

extern void* mymalloc(size_t size, char* fn, int ln);
extern void* mycalloc(size_t nmemb, size_t size, char* fn, int ln);
extern void * myrealloc(void *ptr, size_t size, char* fn, int ln);
extern void myfree(void *ptr, char* fn, int ln);
#endif
#endif

#endif
