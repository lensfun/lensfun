/*
    Image modifier implementation
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

void lfModifier::AddColorCallback (
    lfModifyColorFunc callback, int priority, void *data, size_t data_size)
{
    lfExtModifier *This = static_cast<lfExtModifier *> (this);
    lfColorCallbackData *d = new lfColorCallbackData ();
    d->callback = callback;
    This->AddCallback (This->ColorCallbacks, d, priority, data, data_size);
}

bool lfModifier::AddColorCallbackVignetting (
    lfLensCalibVignetting &model, lfPixelFormat format, bool reverse)
{
    float tmp [5];
    lfExtModifier *This = static_cast<lfExtModifier *> (this);

#define ADD_CALLBACK(func, format, type, prio) \
    case LF_PF_ ## format: \
        AddColorCallback ( \
            (lfModifyColorFunc)(void (*)(void *, float, float, type *, int, int)) \
            lfExtModifier::func, prio, tmp, 5 * sizeof (float)); \
        break;

    memcpy (tmp, model.Terms, 3 * sizeof (float));

    // Damn! Hugin uses two different "normalized" coordinate systems:
    // for distortions it uses 1.0 = min(half width, half height) and
    // for vignetting it uses 1.0 = half diagonal length. We have
    // to compute a transition coefficient as we always work in
    // the first coordinate system.
    double ns = 2.0 / sqrt (float (square (This->Width) + square (This->Height)));
    tmp [3] = ns;
    tmp [4] = ns / This->NormScale;

    if (reverse)
        switch (model.Model)
        {
            case LF_VIGNETTING_MODEL_PA:
                switch (format)
                {
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, U8, lf_u8, 250);
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, U16, lf_u16, 250);
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, U32, lf_u32, 250);
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, F32, lf_f32, 250);
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, F64, lf_f64, 250);
                    default:
                        return false;
                }
                break;

            default:
                return false;
        }
    else
        switch (model.Model)
        {
            case LF_VIGNETTING_MODEL_PA:
                switch (format)
                {
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, U8, lf_u8, 750);
#if defined(__SSE2__)
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA_Select, U16, lf_u16, 750);
#else
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, U16, lf_u16, 750);
#endif
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, U32, lf_u32, 750);
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, F32, lf_f32, 750);
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, F64, lf_f64, 750);
                    default:
                        return false;
                }
                break;

            default:
                return false;
        }

#undef ADD_CALLBACK

    return true;
}

/**
 * Uh-oh... I was not able to find the standard describing how exactly
 * CCI is computed or even what exactly it means. This is ISO 6728-83
 * or alternatively GOST R 50678-94 (Russian equivalent standard).
 * Perhaps even CCI might be removed from lensfun at one point in future
 * if there won't be enough interest in it and the standards information
 * couldn't be gathered.
 */
bool lfModifier::AddColorCallbackCCI (
    const lfLens *lens, lfPixelFormat format, bool reverse)
{
    (void)lens; (void)format; (void)reverse;
    return false;
}

bool lfModifier::ApplyColorModification (
    void *pixels, float x, float y, int width, int height, int comp_role, int row_stride) const
{
    const lfExtModifier *This = static_cast<const lfExtModifier *> (this);

    if (This->ColorCallbacks->len <= 0 || height <= 0)
        return false; // nothing to do

    x = x * This->NormScale - This->CenterX;
    y = y * This->NormScale - This->CenterY;

    for (; height; y += This->NormScale, height--)
    {
        for (int i = 0; i < (int)This->ColorCallbacks->len; i++)
        {
            lfColorCallbackData *cd =
                (lfColorCallbackData *)g_ptr_array_index (This->ColorCallbacks, i);
            cd->callback (cd->data, x, y, pixels, comp_role, width);
        }
        pixels = ((char *)pixels) + row_stride;
    }

    return true;
}

// Helper template to return the maximal value for a type.
// By default returns 0.0, which means to not clamp by upper boundary.
template<typename T>inline double type_max (T x)
{ return 0.0; }

template<>inline double type_max (lf_u16 x)
{ return 65535.0; }

template<>inline double type_max (lf_u32 x)
{ return 4294967295.0; }

template<typename T>static inline T *apply_multiplier (T *pixels, double c, int &cr)
{
    for (;;)
    {
        switch (cr & 15)
        {
            case LF_CR_END:
                return pixels;

            case LF_CR_NEXT:
                cr >>= 4;
                return pixels;

            case LF_CR_UNKNOWN:
                break;

            default:
                *pixels = clampd<T> (*pixels * c, 0.0, type_max (T (0)));
                break;
        }
        pixels++;
        cr >>= 4;
    }
    return pixels;
}

inline guint clampbits (gint x, guint n)
{ guint32 _y_temp; if ((_y_temp = x >> n)) x = ~_y_temp >> (32 - n); return x; }

// For lf_u8 pixel type do a more efficient arithmetic using fixed point
template<>inline lf_u8 *apply_multiplier (lf_u8 *pixels, double c, int &cr)
{
    // Use 20.12 fixed-point math. That leaves 11 bits (factor 2048)  as max multiplication
    int c12 = int (c * 4096.0);
    if (c12 > (2047 << 12))
        c12 = 2047 << 12;

    for (;;)
    {
        switch (cr & 15)
        {
            case LF_CR_END:
                return pixels;

            case LF_CR_NEXT:
                cr >>= 4;
                return pixels;

            case LF_CR_UNKNOWN:
                break;

            default:
                if ((cr & 15) > LF_CR_UNKNOWN)
                {
                    int r = (int (*pixels) * c12 + 2048) >> 12;
                    *pixels = clampbits (r, 8);
                }
                break;
        }
        pixels++;
        cr >>= 4;
    }
    return pixels;
}

// For lf_u16 pixel type do a more efficient arithmetic using fixed point
template<>inline lf_u16 *apply_multiplier (lf_u16 *pixels, double c, int &cr)
{
    // Use 22.10 fixed-point math. That leaves 6 bits (factor 32)  as max multiplication
    int c10 = int (c * 1024.0);
    if (c10 > (31 << 10))
        c10 = 31 << 10;

    for (;;)
    {
        switch (cr & 15)
        {
            case LF_CR_END:
                return pixels;

            case LF_CR_NEXT:
                cr >>= 4;
                return pixels;

            case LF_CR_UNKNOWN:
                break;

            default:
                if ((cr & 15) > LF_CR_UNKNOWN)
                {
                    int r = (int (*pixels) * c10 + 512) >> 10;
                    *pixels = clampbits (r, 16);
                }
                break;
        }
        pixels++;
        cr >>= 4;
    }
    return pixels;
}

template<typename T> void lfExtModifier::ModifyColor_Vignetting_PA (
    void *data, float x, float y, T *pixels, int comp_role, int count)
{
    float *param = (float *)data;

    x *= param [4];
    y *= param [4];

    // For faster computation we will compute r^2 here, and
    // further compute just the delta:
    // ((x+1)*(x+1)+y*y) - (x*x + y*y) = 2 * x + 1
    // But since we work in a normalized coordinate system, a step of
    // 1.0 pixels should be multiplied by NormScale, so it's really:
    // ((x+ns)*(x+ns)+y*y) - (x*x + y*y) = 2 * ns * x + ns^2
    float r2 = x * x + y * y;
    float d1 = 2.0 * param [3];
    float d2 = param [3] * param [3];

    int cr = 0;
    while (count--)
    {
        float r4 = r2 * r2;
        float r6 = r4 * r2;
        float c = 1.0 + param [0] * r2 + param [1] * r4 + param [2] * r6;
        if (!cr)
            cr = comp_role;

        pixels = apply_multiplier<T> (pixels, c, cr);

        r2 += d1 * x + d2;
        x += param [3];
    }
}

template<typename T> void lfExtModifier::ModifyColor_DeVignetting_PA (
    void *data, float x, float y, T *pixels, int comp_role, int count)
{
    float *param = (float *)data;

    x *= param [4];
    y *= param [4];

    // For faster computation we will compute r^2 here, and
    // further compute just the delta:
    // ((x+1)*(x+1)+y*y) - (x*x + y*y) = 2 * x + 1
    // But since we work in a normalized coordinate system, a step of
    // 1.0 pixels should be multiplied by NormScale, so it's really:
    // ((x+ns)*(x+ns)+y*y) - (x*x + y*y) = 2 * ns * x + ns^2
    float r2 = x * x + y * y;
    float d1 = 2.0 * param [3];
    float d2 = param [3] * param [3];

    int cr = 0;
    while (count--)
    {
        float r4 = r2 * r2;
        float r6 = r4 * r2;
        float c = 1.0 + param [0] * r2 + param [1] * r4 + param [2] * r6;
        if (!cr)
            cr = comp_role;

        pixels = apply_multiplier<T> (pixels, 1.0f / c, cr);

        r2 += d1 * x + d2;
        x += param [3];
    }
}

#if defined(__SSE2__)

static gint _15_bit_epi32[4] __attribute__ ((aligned (16))) =
{ 32768, 32768, 32768, 32768 };
static guint _16_bit_sign[4] __attribute__ ((aligned (16))) =
{ 0x80008000, 0x80008000, 0x80008000, 0x80008000 };

void lfExtModifier::ModifyColor_DeVignetting_PA_SSE2 (
    void *data, float _x, float _y, lf_u16 *pixels, int comp_role, int count)
{
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
        __m128i pix0 = _mm_loadu_si128 ((__m128i*)&pixels [i * 16]);
        // Second pair
        __m128i pix1 = _mm_loadu_si128 ((__m128i*)&pixels [i * 16 + 8]);
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
        _mm_storeu_si128 ((__m128i*)&pixels [i * 16], pix0);
        // Second pair
        _mm_storeu_si128 ((__m128i*)&pixels [i * 16 + 8], pix1);

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

void lfExtModifier::ModifyColor_DeVignetting_PA_SSE2_aligned (
    void *data, float _x, float _y, lf_u16 *pixels, int comp_role, int count)
{
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
        __m128 c = _mm_add_ps (_mm_add_ps (_mm_add_ps (
            one, _mm_mul_ps (p0, r2)), _mm_mul_ps (r4, p1)), _mm_mul_ps (r6, p2));

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
        pix0 = _mm_srai_epi32 (_mm_add_epi32 (_mm_unpacklo_epi16 (
            pix0_lo, pix0_hi), rounder), 10);
        pix1 = _mm_srai_epi32 (_mm_add_epi32 (_mm_unpackhi_epi16 (
            pix0_lo, pix0_hi), rounder), 10);
        __m128i pix2 = _mm_srai_epi32 (_mm_add_epi32 (_mm_unpacklo_epi16 (
            pix1_lo, pix1_hi), rounder), 10);
        __m128i pix3 = _mm_srai_epi32 (_mm_add_epi32 (_mm_unpackhi_epi16 (
            pix1_lo, pix1_hi), rounder), 10);

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

typedef __SIZE_TYPE__ uintptr_t;

void lfExtModifier::ModifyColor_DeVignetting_PA_Select (
    void *data, float x, float y, lf_u16 *pixels, int comp_role, int count)
{
    int cr = comp_role;

    if (((cr & 15) > LF_CR_NEXT) &&
        (((cr >> 4) & 15) > LF_CR_NEXT) &&
        (((cr >> 8) & 15) > LF_CR_NEXT) &&
        (((cr >> 12) & 15) > LF_CR_NEXT) &&
        (((cr >> 16) & 15) == LF_CR_END))
    {
        if (!((uintptr_t)(pixels) & 0xf))
            return ModifyColor_DeVignetting_PA_SSE2_aligned (data, x, y, pixels, comp_role, count);
        else 
            return ModifyColor_DeVignetting_PA_SSE2 (data, x, y, pixels, comp_role, count);
    }

    ModifyColor_DeVignetting_PA (data, x, y, pixels, comp_role, count);
}

#endif

//---------------------------// The C interface //---------------------------//

void lf_modifier_add_color_callback (
    lfModifier *modifier, lfModifyColorFunc callback, int priority,
    void *data, size_t data_size)
{
    modifier->AddColorCallback (callback, priority, data, data_size);
}

cbool lf_modifier_add_color_callback_vignetting (
    lfModifier *modifier, lfLensCalibVignetting *model,
    lfPixelFormat format, cbool reverse)
{
    return modifier->AddColorCallbackVignetting (*model, format, reverse);
}

cbool lf_modifier_add_color_callback_CCI (
    lfModifier *modifier, const lfLens *lens,
    lfPixelFormat format, cbool reverse)
{
    return modifier->AddColorCallbackCCI (lens, format, reverse);
}

cbool lf_modifier_apply_color_modification (
    lfModifier *modifier, void *pixels, float x, float y, int width, int height,
    int comp_role, int row_stride)
{
    return modifier->ApplyColorModification (
        pixels, x, y, width, height, comp_role, row_stride);
}
