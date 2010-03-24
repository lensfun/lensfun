/*
    Image modifier implementation
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>

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

#define ADD_CALLBACK(func, type, prio) \
    AddColorCallback ( \
        (lfModifyColorFunc)(void (*)(void *, float, float, type *, int, int)) \
        lfExtModifier::func, prio, tmp, 5 * sizeof (float)) \

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
                    case LF_PF_U8:
                        ADD_CALLBACK (ModifyColor_Vignetting_PA, lf_u8, 250);
                        break;

                    case LF_PF_U16:
                        ADD_CALLBACK (ModifyColor_Vignetting_PA, lf_u16, 250);
                        break;

                    case LF_PF_U32:
                        ADD_CALLBACK (ModifyColor_Vignetting_PA, lf_u32, 250);
                        break;

                    case LF_PF_F32:
                        ADD_CALLBACK (ModifyColor_Vignetting_PA, lf_f32, 250);
                        break;

                    case LF_PF_F64:
                        ADD_CALLBACK (ModifyColor_Vignetting_PA, lf_f64, 250);
                        break;

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
                    case LF_PF_U8:
                        ADD_CALLBACK (ModifyColor_DeVignetting_PA, lf_u8, 750);
                        break;

                    case LF_PF_U16:
#ifdef VECTORIZATION_SSE2
                        if (_lf_detect_cpu_features () & LF_CPU_FLAG_SSE2)
                            ADD_CALLBACK (ModifyColor_DeVignetting_PA_Select, lf_u16, 750);
                        else
#endif
                        ADD_CALLBACK (ModifyColor_DeVignetting_PA, lf_u16, 750);
                        break;

                    case LF_PF_U32:
                        ADD_CALLBACK (ModifyColor_DeVignetting_PA, lf_u32, 750);
                        break;

                    case LF_PF_F32:
                        ADD_CALLBACK (ModifyColor_DeVignetting_PA, lf_f32, 750);
                        break;

                    case LF_PF_F64:
                        ADD_CALLBACK (ModifyColor_DeVignetting_PA, lf_f64, 750);
                        break;

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
