/*
    Image modifier implementation: (un)distortion functions
    Copyright (C) 2007 by Andrew Zabolotny

    Most of the math in this file has been borrowed from PanoTools.
    Thanks to the PanoTools team for their pioneering work.
*/

#include "config.h"

#ifdef VECTORIZATION_SSE

#include "lensfun.h"
#include "lensfunprv.h"
#include <xmmintrin.h>

void lfExtModifier::ModifyCoord_Dist_PTLens_SSE (void *data, float *iocoord, int count)
{
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
    __m128 c0 = _mm_loadu_ps (&iocoord[8*i]);
    __m128 c1 = _mm_loadu_ps (&iocoord[8*i+4]);
    __m128 x = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (2, 0, 2, 0));
    __m128 y = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (3, 1, 3, 1));
    __m128 ru = _mm_add_ps (_mm_mul_ps (x, x), _mm_mul_ps (y, y));

    // We don't check for zero, but set it to a very small value instead
    ru = _mm_max_ps (ru, very_small);
    ru = _mm_rcp_ps (_mm_rsqrt_ps (ru));
    __m128 rd = ru;
    for (int step = 0; step < 4; step++)
    {
      // frd = rd * (a * rd^2 * rd + b * rd^2 + c * rd + d) - ru
      __m128 rd_sq =  _mm_mul_ps (rd, rd);
      __m128 frd = _mm_mul_ps (_mm_mul_ps (a, rd), rd_sq);
      __m128 t = _mm_add_ps (_mm_mul_ps (b, rd_sq), _mm_add_ps (d, _mm_mul_ps (c, rd)));
      frd = _mm_sub_ps (_mm_mul_ps (_mm_add_ps (t, frd), rd), ru);

      // This is most likely faster than loading form L1 cache
      __m128 two = _mm_add_ps (one, one);
      __m128 three = _mm_add_ps (one, two);
      __m128 four = _mm_add_ps (two, two);

      // corr =  4 * a * rd * rd^2 + 3 * b * rd^2 + 2 * c * rd + d
      __m128 corr = _mm_mul_ps (c, rd);
      corr = _mm_add_ps (d, _mm_add_ps (corr, corr));
      t = _mm_mul_ps (rd_sq, _mm_mul_ps (three, b));
      corr = _mm_add_ps (corr, _mm_mul_ps (_mm_mul_ps (rd, rd_sq), _mm_mul_ps (four, a)));
      corr = _mm_rcp_ps (_mm_add_ps (corr, t));

      // rd -= frd * corr
      rd = _mm_sub_ps (rd, _mm_mul_ps (frd, corr));
    }
    // We don't check for zero, but set it to a very small value instead
    rd = _mm_max_ps (rd, very_small);

    // rd /= ru
    rd = _mm_mul_ps (rd, _mm_rcp_ps(ru));
    c0 = _mm_mul_ps (c0, rd);
    c1 = _mm_mul_ps (c1, rd);
    _mm_storeu_ps (&iocoord [8 * i], c0);
    _mm_storeu_ps (&iocoord [8 * i + 4], c1);
  }

  loop_count *= 4;
  int remain = count - loop_count;
  if (remain) 
    ModifyCoord_Dist_PTLens (data, &iocoord [loop_count * 2], remain);
}

#if defined (_MSC_VER)
typedef size_t uintptr_t;
#else
typedef __SIZE_TYPE__ uintptr_t;
#endif

void lfExtModifier::ModifyCoord_UnDist_PTLens_SSE (void *data, float *iocoord, int count)
{
  float *param = (float *)data;

  // Ru = Rd * (a * Rd^3 + b * Rd^2 + c * Rd + d)
  __m128 a = _mm_set_ps1 (param [0]);
  __m128 b = _mm_set_ps1 (param [1]);
  __m128 c = _mm_set_ps1 (param [2]);
  __m128 d = _mm_sub_ps (_mm_set_ps1 (1.0f), _mm_add_ps (a, _mm_add_ps (b, c)));
  int aligned = !((uintptr_t)(iocoord)&0xf);

  // SSE Loop processes 4 pixels/loop
  int loop_count = count / 4;
  if (aligned)
    for (int i = 0; i < loop_count ; i++)
    {
      __m128 c0 = _mm_load_ps (&iocoord [8 * i]);
      __m128 c1 = _mm_load_ps (&iocoord [8 * i + 4]);
      __m128 x = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (2, 0, 2, 0));
      __m128 y = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (3, 1, 3, 1));
      __m128 r2 = _mm_add_ps (_mm_mul_ps (x, x), _mm_mul_ps (y, y));
      __m128 r = _mm_rcp_ps (_mm_rsqrt_ps (r2));

      // Calculate poly3 = a * r2 * r + b * r2 + c * r + d;
      __m128 t = _mm_mul_ps (r2, b);
      __m128 poly3 = _mm_mul_ps (_mm_mul_ps (a, r2), r);
      t = _mm_add_ps (t, _mm_mul_ps (r, c));
      poly3 = _mm_add_ps (t, _mm_add_ps (poly3, d));
      _mm_store_ps (&iocoord [8 * i], _mm_mul_ps (poly3, c0));
      _mm_store_ps (&iocoord [8 * i + 4], _mm_mul_ps (poly3, c1));
    }
  else
    for (int i = 0; i < loop_count ; i++)
    {
      __m128 c0 = _mm_loadu_ps (&iocoord [8 * i]);
      __m128 c1 = _mm_loadu_ps (&iocoord [8 * i + 4]);
      __m128 x = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (2, 0, 2, 0));
      __m128 y = _mm_shuffle_ps (c0, c1, _MM_SHUFFLE (3, 1, 3, 1));
      __m128 r2 = _mm_add_ps (_mm_mul_ps (x, x), _mm_mul_ps (y, y));
      __m128 r = _mm_rcp_ps (_mm_rsqrt_ps (r2));

      // Calculate poly3 = a * r2 * r + b * r2 + c * r + d;
      __m128 poly3 = _mm_mul_ps (_mm_mul_ps (a, r2), r);
      __m128 t = _mm_mul_ps (r2, b);
      poly3 = _mm_add_ps (poly3, _mm_mul_ps (r, c));
      poly3 = _mm_add_ps (poly3, _mm_add_ps (t, d));
      _mm_storeu_ps (&iocoord [8 * i], _mm_mul_ps (poly3, c0));
      _mm_storeu_ps (&iocoord [8 * i + 4], _mm_mul_ps (poly3, c1));
    }

  loop_count *= 4;
  int remain = count - loop_count;
  if (remain)
    ModifyCoord_UnDist_PTLens (data, &iocoord [loop_count * 2], remain);
}

#endif
