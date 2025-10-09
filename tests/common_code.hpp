#ifndef COMMON_CODE_HPP
#define COMMON_CODE_HPP
#include <cstdlib>

void* lf_alloc_align(size_t alignment, size_t size) {
#ifdef _WIN32  // std::aligned_alloc isn't supported in Microsoft C Runtime library
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
}

void lf_free_align(void* mem) {
#ifdef _WIN32
    _aligned_free(mem);
#else
    std::free(mem);
#endif
}

#endif // COMMON_CODE_HPP
