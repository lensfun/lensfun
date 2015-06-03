/*
    Image modifier implementation
    Copyright (C) 2007 by Andrew Zabolotny
    Copyright (C) 2014 by Roman Lebedev
*/

#include "config.h"

#ifdef VECTORIZATION_SSE2

#include "lensfun.h"
#include "lensfunprv.h"
#include <emmintrin.h>

#if defined (_MSC_VER)
#define PREFIX_ALIGN __declspec(align(16)) 
#define SUFFIX_ALIGN
#else
#define PREFIX_ALIGN
#define SUFFIX_ALIGN __attribute__ ((aligned (16)))
#endif
static PREFIX_ALIGN gint _15_bit_epi32[4] SUFFIX_ALIGN =
{ 32768, 32768, 32768, 32768 };
static PREFIX_ALIGN guint _16_bit_sign[4] SUFFIX_ALIGN =
{ 0x80008000, 0x80008000, 0x80008000, 0x80008000 };

#if defined (_MSC_VER)
typedef size_t uintptr_t;
#else
typedef __SIZE_TYPE__ uintptr_t;
#endif

void lfModifier::ModifyColor_DeVignetting_PA_SSE2 (
    void *data, float _x, float _y, lf_u16 *pixels, int comp_role, int count)
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
    float p4 = param [4];
    float p3 = param [3];

    float x = _x * p4;
    float y = _y * p4;

    __m128 x2 = _mm_set_ps1 (x);

    float t0 = x * x + y * y;
    x += p3;
    float t1 = x * x + y * y;
    x += p3;
    float t2 = x * x + y * y;
    x += p3;
    float t3 = x * x + y * y;
    x += p3;
 
    __m128 r2 = _mm_set_ps (t3, t2, t1, t0);
    __m128 d1_4 = _mm_set_ps1 (4.0f * 2.0 * p3);
    __m128 p0 = _mm_set_ps1 (param [0]);
    __m128 p1 = _mm_set_ps1 (param [1]);
    __m128 p2 = _mm_set_ps1 (param [2]);
    __m128 p3_4 = _mm_set_ps1 (p3 * 4.0f);
    __m128 one = _mm_set_ps1 (1.0f);
    __m128 frac = _mm_set_ps1 (1024.0f);
    __m128i rounder = _mm_set1_epi32 (512);
    // add_four = 4 * d2 + 6 * d1 * p3
    __m128 add_four = _mm_set_ps1 (16.0f * p3 * p3);

    // SSE Loop processes 4 pixels/loop
    int loop_count = count / 4;
    for (int i = 0; i < loop_count ; i++)
    {
        // First two pixels
        __m128i pix0 = _mm_load_si128 ((__m128i*)&pixels [i * 16]);
        // Second pair
        __m128i pix1 = _mm_load_si128 ((__m128i*)&pixels [i * 16 + 8]);
        __m128 r4 = _mm_mul_ps (r2, r2);
        __m128 r6 = _mm_mul_ps (r4, r2);
        // c = 1.0 + param [0] * r2 + param [1] * r4 + param [2] * r6;
        __m128 c = _mm_add_ps (_mm_add_ps (_mm_add_ps (one, _mm_mul_ps (p0, r2)),
            _mm_mul_ps (r4, p1)), _mm_mul_ps (r6, p2));

        // Calculate 5.10 fixed point coefficients.
        __m128i fraction = _mm_cvttps_epi32 (_mm_mul_ps (_mm_rcp_ps (c), frac));
        // Pack to 16 bit signed
        fraction = _mm_packs_epi32 (fraction, fraction);

        __m128i pix_frac0 = _mm_shufflelo_epi16 (fraction, _MM_SHUFFLE (0, 0, 0, 0));
        pix_frac0 = _mm_shufflehi_epi16 (pix_frac0, _MM_SHUFFLE (1, 1, 1, 1));
        __m128i pix_frac1 = _mm_shufflelo_epi16 (fraction, _MM_SHUFFLE (2, 2, 2, 2));
        pix_frac1 = _mm_shufflehi_epi16 (pix_frac1, _MM_SHUFFLE (3, 3, 3, 3));

        __m128i pix0_lo = _mm_mullo_epi16 (pix0, pix_frac0);
        __m128i pix0_hi = _mm_mulhi_epu16 (pix0, pix_frac0);
        __m128i pix1_lo = _mm_mullo_epi16 (pix1, pix_frac1);
        __m128i pix1_hi = _mm_mulhi_epu16 (pix1, pix_frac1);

        __m128i sub_32 = _mm_load_si128 ((__m128i*)_15_bit_epi32);
        __m128i signxor = _mm_load_si128 ((__m128i*)_16_bit_sign);

        // Recombine multiplied values and shift down by fraction
        pix0 = _mm_srai_epi32 (_mm_add_epi32 (
            _mm_unpacklo_epi16 (pix0_lo, pix0_hi), rounder), 10);
        pix1 = _mm_srai_epi32 (_mm_add_epi32 (
            _mm_unpackhi_epi16 (pix0_lo, pix0_hi), rounder), 10);
        __m128i pix2 = _mm_srai_epi32 (_mm_add_epi32 (
            _mm_unpacklo_epi16 (pix1_lo, pix1_hi), rounder), 10);
        __m128i pix3 = _mm_srai_epi32 (_mm_add_epi32 (
            _mm_unpackhi_epi16 (pix1_lo, pix1_hi), rounder), 10);

        // Subtract 32768 to avoid saturation
        pix0 = _mm_sub_epi32 (pix0, sub_32);
        pix1 = _mm_sub_epi32 (pix1, sub_32);
        pix2 = _mm_sub_epi32 (pix2, sub_32);
        pix3 = _mm_sub_epi32 (pix3, sub_32);

        // 32 bit signed -> 16 bit signed conversion, including saturation
        pix0 = _mm_packs_epi32 (pix0, pix1);
        pix1 = _mm_packs_epi32 (pix2, pix3);

        // Convert sign back
        pix0 = _mm_xor_si128 (pix0, signxor);
        pix1 = _mm_xor_si128 (pix1, signxor);
        // First two pixels
        _mm_store_si128 ((__m128i*)&pixels [i * 16], pix0);
        // Second pair
        _mm_store_si128 ((__m128i*)&pixels [i * 16 + 8], pix1);

        // r2_new = ((((r2) + d1 * (x + (p3*0)) + d2) + d1 * (x + (p3*1)) + d2) + d1 * (x + (p3*2)) + d2) + d1 * (x + (p3*3)) + d2
        // r2_new = 4 * d2 + 6 * d1 * p3 + r2 + 4*d1*x
        // r2_new =  add_four + r2 + 4*d1*x
        // Prepare for next iteration
        r2 = _mm_add_ps (_mm_add_ps (add_four, r2), _mm_mul_ps (d1_4, x2));
        x2 = _mm_add_ps (x2, p3_4);
    }

    loop_count *= 4;
    count -= loop_count;
    if (count)
        ModifyColor_DeVignetting_PA (data, _x + loop_count, _y + loop_count,
            &pixels [loop_count * 4], comp_role, count);
}

#endif
