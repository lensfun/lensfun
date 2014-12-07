#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__DragonFly__)
#include <malloc.h>
#endif
#ifdef __APPLE__
#include <sys/malloc.h>
#endif

void *lf_alloc_align(size_t alignment, size_t size)
{
#if defined(__WIN32__)
  return _aligned_malloc(size, alignment);
#else
  void *ptr = NULL;
  if(posix_memalign(&ptr, alignment, size) != 0)
    return NULL;
  return ptr;
#endif
}
#ifdef __WIN32__
void lf_free_align(void *mem)
{
  _aligned_free(mem);
}
#else
#define lf_free_align(A) free(A)
#endif
