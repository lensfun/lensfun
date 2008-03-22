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

#define ADD_CALLBACK(func, format, type, prio) \
    case LF_PF_ ## format: \
        AddColorCallback ( \
            (lfModifyColorFunc)(void (*)(void *, float, float, type *, int, int)) \
            lfExtModifier::func, prio, tmp, 5 * sizeof (float)); \
        break;

    memcpy (tmp, model.Terms, 3 * sizeof (float));

    // Damn! Hugin uses two different "normalized" coordinate systems:
    // for distortions it uses 1.0 = max(half width, half height) and
    // for vignetting it uses 1.0 = half diagonal length. We have
    // to compute a transition coefficient as we always work in
    // the first coordinate system.
    double ns = 2.0 / sqrt (square (This->Width) + square (This->Height));
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
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, U16, lf_u16, 750);
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

    if (This->ColorCallbacks->len <= 0)
        return false; // nothing to do

    x = x * This->NormScale - This->CenterX;
    y = y * This->NormScale - This->CenterY;
    float y_end = y + height * This->NormScale;

    for (; y < y_end; y += This->NormScale)
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

template<typename T>static T *apply_multiplier (T *pixels, double c, int &cr)
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
                *pixels = clamp (T (*pixels * c), T (0));
                break;
        }
        pixels++;
        cr >>= 4;
    }
    return pixels;
}

template<> static lf_u8 *apply_multiplier<lf_u8> (lf_u8 *pixels, double c, int &cr)
{
    // Use 20.12 fixed-point math
    int c12 = int (c * 4096.0);
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
                    *pixels = clamp ((int (*pixels) * c12) >> 12, 0, 0xff);
                break;
        }
        pixels++;
        cr >>= 4;
    }
    return pixels;
}

template<> static lf_u16 *apply_multiplier<lf_u16> (lf_u16 *pixels, double c, int &cr)
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
                if ((cr & 15) > LF_CR_UNKNOWN)
                    *pixels = clamp (lf_u16 (*pixels * c), lf_u16 (0), lf_u16 (0xffff));
                break;
        }
        pixels++;
        cr >>= 4;
    }
    return pixels;
}

template<> static lf_u32 *apply_multiplier<lf_u32> (lf_u32 *pixels, double c, int &cr)
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
                if ((cr & 15) > LF_CR_UNKNOWN)
                    *pixels = clamp (lf_u32 (*pixels * c), lf_u32 (0), lf_u32 (0xffffffff));
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
    double r2 = x * x + y * y;
    double d1 = 2.0 * param [3];
    double d2 = param [3] * param [3];

    int cr = 0;
    while (count--)
    {
        double r4 = r2 * r2;
        double r6 = r4 * r2;
        double c = 1.0 + param [0] * r2 + param [1] * r4 + param [2] * r6;
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

    double r2 = x * x + y * y;
    double d1 = 2.0 * param [3];
    double d2 = param [3] * param [3];

    int cr = 0;
    while (count--)
    {
        double r4 = r2 * r2;
        double r6 = r4 * r2;
        double c = 1.0 + param [0] * r2 + param [1] * r4 + param [2] * r6;
        if (!cr)
            cr = comp_role;

        pixels = apply_multiplier<T> (pixels, 1.0 / c, cr);

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
