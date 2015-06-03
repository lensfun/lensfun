/*
    Image modifier implementation: (un)distortion functions
    Copyright (C) 2007 by Andrew Zabolotny
    Copyright (C) 2014 by Roman Lebedev

    Most of the math in this file has been borrowed from PanoTools.
    Thanks to the PanoTools team for their pioneering work.
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

void lfModifier::ModifyCoord_UnDist_PTLens_SSE (void *data, float *iocoord, int count)
{
  /*
   * If buffer is not aligned, fall back to plain code
   */
  if((uintptr_t)(iocoord) & 0xf)
  {
    return ModifyCoord_UnDist_PTLens(data, iocoord, count);
  }

  float *param = (float *)data;

  __m128 a = _mm_set_ps1 (param [0]);
  __m128 b = _mm_set_ps1 (param [1]);
  __m128 c = _mm_set_ps1 (param [2]);
  __m128 very_small = _mm_set_ps1 (1e-15);
  __m128 one = _mm_set_ps1 (1.0f);
  __m128 d = _mm_sub_ps (one, _mm_add_ps (a, _mm_add_ps (b, c)));

  // SSE Loop processes 4 pixels/loop
  int loop_count = count / 4;
  for (int i = 0; i < loop_count ; i++)
  {
    // Load 4 sets of coordinates
    __m128 c0 = _mm_load_ps (&iocoord[8*i]);
    __m128 c1 = _mm_load_ps (&iocoord[8*i+4]);
    __m128 x = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (2, 0, 2, 0));
    __m128 y = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (3, 1, 3, 1));
    __m128 rd = _mm_add_ps (_mm_mul_ps (x, x), _mm_mul_ps (y, y));

    // We don't check for zero, but set it to a very small value instead
    rd = _mm_max_ps (rd, very_small);
    rd = _mm_rcp_ps (_mm_rsqrt_ps (rd));
    __m128 ru = rd;
    for (int step = 0; step < 4; step++)
    {
      // fru = ru * (a * ru^2 * ru + b * ru^2 + c * ru + d) - rd
      __m128 ru_sq =  _mm_mul_ps (ru, ru);
      __m128 fru = _mm_mul_ps (_mm_mul_ps (a, ru), ru_sq);
      __m128 t = _mm_add_ps (_mm_mul_ps (b, ru_sq), _mm_add_ps (d, _mm_mul_ps (c, ru)));
      fru = _mm_sub_ps (_mm_mul_ps (_mm_add_ps (t, fru), ru), rd);

      // This is most likely faster than loading form L1 cache
      __m128 two = _mm_add_ps (one, one);
      __m128 three = _mm_add_ps (one, two);
      __m128 four = _mm_add_ps (two, two);

      // corr =  4 * a * ru * ru^2 + 3 * b * ru^2 + 2 * c * ru + d
      __m128 corr = _mm_mul_ps (c, ru);
      corr = _mm_add_ps (d, _mm_add_ps (corr, corr));
      t = _mm_mul_ps (ru_sq, _mm_mul_ps (three, b));
      corr = _mm_add_ps (corr, _mm_mul_ps (_mm_mul_ps (ru, ru_sq), _mm_mul_ps (four, a)));
      corr = _mm_rcp_ps (_mm_add_ps (corr, t));

      // ru -= fru * corr
      ru = _mm_sub_ps (ru, _mm_mul_ps (fru, corr));
    }
    // We don't check for zero, but set it to a very small value instead
    ru = _mm_max_ps (ru, very_small);

    // ru /= rd
    ru = _mm_mul_ps (ru, _mm_rcp_ps(rd));
    c0 = _mm_mul_ps (c0, ru);
    c1 = _mm_mul_ps (c1, ru);
    _mm_store_ps (&iocoord [8 * i], c0);
    _mm_store_ps (&iocoord [8 * i + 4], c1);
  }

  loop_count *= 4;
  int remain = count - loop_count;
  if (remain) 
    ModifyCoord_UnDist_PTLens (data, &iocoord [loop_count * 2], remain);
}

void lfModifier::ModifyCoord_Dist_PTLens_SSE (void *data, float *iocoord, int count)
{
  /*
   * If buffer is not aligned, fall back to plain code
   */
  if((uintptr_t)(iocoord) & 0xf)
  {
    return ModifyCoord_Dist_PTLens(data, iocoord, count);
  }

  float *param = (float *)data;

  // Rd = Ru * (a * Ru^3 + b * Ru^2 + c * Ru + d)
  __m128 a = _mm_set_ps1 (param [0]);
  __m128 b = _mm_set_ps1 (param [1]);
  __m128 c = _mm_set_ps1 (param [2]);
  __m128 d = _mm_sub_ps (_mm_set_ps1 (1.0f), _mm_add_ps (a, _mm_add_ps (b, c)));

  // SSE Loop processes 4 pixels/loop
  int loop_count = count / 4;
  for (int i = 0; i < loop_count ; i++)
  {
    __m128 c0 = _mm_load_ps (&iocoord [8 * i]);
    __m128 c1 = _mm_load_ps (&iocoord [8 * i + 4]);
    __m128 x = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (2, 0, 2, 0));
    __m128 y = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (3, 1, 3, 1));
    __m128 ru2 = _mm_add_ps (_mm_mul_ps (x, x), _mm_mul_ps (y, y));
    __m128 ru = _mm_rcp_ps (_mm_rsqrt_ps (ru2));

    // Calculate poly3 = a * ru2 * ru + b * ru2 + c * ru + d;
    __m128 t = _mm_mul_ps (ru2, b);
    __m128 poly3 = _mm_mul_ps (_mm_mul_ps (a, ru2), ru);
    t = _mm_add_ps (t, _mm_mul_ps (ru, c));
    poly3 = _mm_add_ps (t, _mm_add_ps (poly3, d));
    _mm_store_ps (&iocoord [8 * i], _mm_mul_ps (poly3, c0));
    _mm_store_ps (&iocoord [8 * i + 4], _mm_mul_ps (poly3, c1));
  }

  loop_count *= 4;
  int remain = count - loop_count;
  if (remain)
    ModifyCoord_Dist_PTLens (data, &iocoord [loop_count * 2], remain);
}

void lfModifier::ModifyCoord_Dist_Poly3_SSE (void *data, float *iocoord, int count)
{
  /*
   * If buffer is not aligned, fall back to plain code
   */
  if((uintptr_t)(iocoord) & 0xf)
  {
    return ModifyCoord_Dist_Poly3(data, iocoord, count);
  }

  float *param = (float *)data;

  // Rd = Ru * (d + k1 * Ru^2),  d = 1 - k1
  __m128 k1 = _mm_set_ps1 (param [0]);
  __m128 d = _mm_sub_ps (_mm_set_ps1 (1.0f), k1);

  // SSE Loop processes 4 pixels/loop
  int loop_count = count / 4;
  for (int i = 0; i < loop_count ; i++)
  {
    __m128 c0 = _mm_load_ps (&iocoord [8 * i]);
    __m128 c1 = _mm_load_ps (&iocoord [8 * i + 4]);
    __m128 x = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (2, 0, 2, 0));
    __m128 y = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (3, 1, 3, 1));

    // Calculate poly3 = k1 * ru * ru + d;
    __m128 poly3 = _mm_add_ps (_mm_mul_ps (_mm_add_ps (_mm_mul_ps (x, x), _mm_mul_ps (y, y)), k1), d);
    _mm_store_ps (&iocoord [8 * i], _mm_mul_ps (poly3, c0));
    _mm_store_ps (&iocoord [8 * i + 4], _mm_mul_ps (poly3, c1));
  }

  loop_count *= 4;
  int remain = count - loop_count;
  if (remain)
    ModifyCoord_Dist_Poly3 (data, &iocoord [loop_count * 2], remain);
}

#endif
