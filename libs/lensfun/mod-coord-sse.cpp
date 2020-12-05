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
  // See "Note about PT-based distortion models" at the top of mod-coord.cpp.
  /*
   * If buffer is not aligned, fall back to plain code
   */
  if((uintptr_t)(iocoord) & 0xf)
  {
    return ModifyCoord_UnDist_PTLens(data, iocoord, count);
  }

  lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;

  __m128 a_ = _mm_set_ps1 (cddata->terms [0]);
  __m128 b_ = _mm_set_ps1 (cddata->terms [1]);
  __m128 c_ = _mm_set_ps1 (cddata->terms [2]);
  __m128 very_small = _mm_set_ps1 (1e-15);
  __m128 one = _mm_set_ps1 (1.0f);

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
      // fru = ru * (a_ * ru^2 * ru + b_ * ru^2 + c_ * ru + 1) - rd
      __m128 ru_sq =  _mm_mul_ps (ru, ru);
      __m128 fru = _mm_mul_ps (_mm_mul_ps (a_, ru), ru_sq);
      __m128 t = _mm_add_ps (_mm_mul_ps (b_, ru_sq), _mm_add_ps (one, _mm_mul_ps (c_, ru)));
      fru = _mm_sub_ps (_mm_mul_ps (_mm_add_ps (t, fru), ru), rd);

      // This is most likely faster than loading form L1 cache
      __m128 two = _mm_add_ps (one, one);
      __m128 three = _mm_add_ps (one, two);
      __m128 four = _mm_add_ps (two, two);

      // corr =  4 * a_ * ru * ru^2 + 3 * b_ * ru^2 + 2 * c_ * ru + 1
      __m128 corr = _mm_mul_ps (c_, ru);
      corr = _mm_add_ps (one, _mm_add_ps (corr, corr));
      t = _mm_mul_ps (ru_sq, _mm_mul_ps (three, b_));
      corr = _mm_add_ps (corr, _mm_mul_ps (_mm_mul_ps (ru, ru_sq), _mm_mul_ps (four, a_)));
      corr = _mm_rcp_ps (_mm_add_ps (corr, t));

      // ru -= fru * corr
      ru = _mm_sub_ps (ru, _mm_mul_ps (fru, corr));
    }
    // We don't check for zero, but set it to a very small value instead
    ru = _mm_max_ps (ru, very_small);

    // ru /= rd
    ru = _mm_mul_ps (ru, _mm_rcp_ps(rd));
    x = _mm_mul_ps (x, ru);
    y = _mm_mul_ps (y, ru);

    c0 = _mm_unpacklo_ps(x, y);
    c1 = _mm_unpackhi_ps(x, y);
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
  // See "Note about PT-based distortion models" at the top of mod-coord.cpp.
  /*
   * If buffer is not aligned, fall back to plain code
   */
  if((uintptr_t)(iocoord) & 0xf)
  {
    return ModifyCoord_Dist_PTLens(data, iocoord, count);
  }

  lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;

  // Rd = Ru * (a_ * Ru^3 + b_ * Ru^2 + c_ * Ru + 1)
  __m128 a_ = _mm_set_ps1 (cddata->terms [0]);
  __m128 b_ = _mm_set_ps1 (cddata->terms [1]);
  __m128 c_ = _mm_set_ps1 (cddata->terms [2]);
  __m128 one = _mm_set_ps1 (1.0f);

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

    // Calculate poly3 = a_ * ru2 * ru + b_ * ru2 + c_ * ru + 1;
    __m128 t = _mm_mul_ps (ru2, b_);
    __m128 poly3 = _mm_mul_ps (_mm_mul_ps (a_, ru2), ru);
    t = _mm_add_ps (t, _mm_mul_ps (ru, c_));
    poly3 = _mm_add_ps (t, _mm_add_ps (poly3, one));

    x = _mm_mul_ps (x, poly3);
    y = _mm_mul_ps (y, poly3);

    c0 = _mm_unpacklo_ps(x, y);
    c1 = _mm_unpackhi_ps(x, y);

    _mm_store_ps (&iocoord [8 * i], c0);
    _mm_store_ps (&iocoord [8 * i + 4], c1);
  }

  loop_count *= 4;
  int remain = count - loop_count;
  if (remain)
    ModifyCoord_Dist_PTLens (data, &iocoord [loop_count * 2], remain);
}

void lfModifier::ModifyCoord_Dist_Poly3_SSE (void *data, float *iocoord, int count)
{
  // See "Note about PT-based distortion models" at the top of mod-coord.cpp.
  /*
   * If buffer is not aligned, fall back to plain code
   */
  if((uintptr_t)(iocoord) & 0xf)
  {
    return ModifyCoord_Dist_Poly3(data, iocoord, count);
  }

  lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;

  // Rd = Ru * (d_ + k1 * Ru^2)
  __m128 k1_ = _mm_set_ps1 (cddata->terms [0]);
  __m128 one = _mm_set_ps1 (1.0f);

  // SSE Loop processes 4 pixels/loop
  int loop_count = count / 4;
  for (int i = 0; i < loop_count ; i++)
  {
    __m128 c0 = _mm_load_ps (&iocoord [8 * i]);
    __m128 c1 = _mm_load_ps (&iocoord [8 * i + 4]);
    __m128 x = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (2, 0, 2, 0));
    __m128 y = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (3, 1, 3, 1));

    // Calculate poly3 = k1_ * ru * ru + 1;
    __m128 poly3 = _mm_add_ps (_mm_mul_ps (_mm_add_ps (_mm_mul_ps (x, x), _mm_mul_ps (y, y)), k1_), one);

    x = _mm_mul_ps (x, poly3);
    y = _mm_mul_ps (y, poly3);

    c0 = _mm_unpacklo_ps(x, y);
    c1 = _mm_unpackhi_ps(x, y);

    _mm_store_ps (&iocoord [8 * i], c0);
    _mm_store_ps (&iocoord [8 * i + 4], c1);
  }

  loop_count *= 4;
  int remain = count - loop_count;
  if (remain)
    ModifyCoord_Dist_Poly3 (data, &iocoord [loop_count * 2], remain);
}

#endif
