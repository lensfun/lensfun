/*
    Image modifier implementation
    Copyright (C) 2014 by Roman Lebedev
*/

#include "config.h"

#ifdef VECTORIZATION_SSE

#include "lensfun.h"
#include "lensfunprv.h"
#include <xmmintrin.h>

#if defined (_MSC_VER)
typedef size_t uintptr_t;
#else
typedef __SIZE_TYPE__ uintptr_t;
#endif

void lfModifier::ModifyColor_DeVignetting_PA_SSE (
    void *data, float _x, float _y, lf_f32 *pixels, int comp_role, int count)
{
    int cr = comp_role;

    /*
     * If there are not four components per pixel, or buffer is not aligned,
     * fall back to plain code
     */
    if (!(((cr & 15) > LF_CR_NEXT) &&
          (((cr >> 4) & 15) > LF_CR_NEXT) &&
          (((cr >> 8) & 15) > LF_CR_NEXT) &&
          (((cr >> 12) & 15) > LF_CR_NEXT) &&
          (((cr >> 16) & 15) == LF_CR_END)) ||
        ((uintptr_t)(pixels) & 0xf))
    {
        return ModifyColor_DeVignetting_PA(data, _x, _y, pixels, comp_role, count);
    }

    float *param = (float *)data;

    float x = _x * param [4];
    float y = _y * param [4];

    __m128 x2 = _mm_set_ps1 (x);

    __m128 r2 = _mm_set_ps1 (x * x + y * y);
    __m128 d1 = _mm_set_ps1 (2.0 * param [3]);
    __m128 p0 = _mm_set_ps1 (param [0]);
    __m128 p1 = _mm_set_ps1 (param [1]);
    __m128 p2 = _mm_set_ps1 (param [2]);
    __m128 p3 = _mm_set_ps1 (param [3]);
    __m128 one = _mm_set_ps1 (1.0f);
    __m128 d2 = _mm_set_ps1 (param [3] * param [3]);

    // SSE Loop processes 1 pixel/loop
    for (int i = 0; i < count; i++)
    {
        __m128 pix = _mm_load_ps (&pixels [i * 4]);

        __m128 r4 = _mm_mul_ps (r2, r2);
        __m128 r6 = _mm_mul_ps (r4, r2);

        // c = 1.0 + param [0] * r2 + param [1] * r4 + param [2] * r6;
        __m128 c = _mm_add_ps (_mm_add_ps (_mm_add_ps (
            one, _mm_mul_ps (p0, r2)), _mm_mul_ps (r4, p1)), _mm_mul_ps (r6, p2));

        pix = _mm_div_ps(pix, c);

        pix = _mm_max_ps(pix, _mm_setzero_ps());

        _mm_store_ps (&pixels [i * 4], pix);

        // Prepare for next iteration
        r2 = _mm_add_ps (_mm_add_ps (d2, r2), _mm_mul_ps (d1, x2));
        x2 = _mm_add_ps (x2, p3);
    }
}

#endif
