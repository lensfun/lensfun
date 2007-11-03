/*
    Image modifier implementation
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"

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
    float tmp [4];
    lfExtModifier *This = static_cast<lfExtModifier *> (this);

#define ADD_CALLBACK(func, format, type, prio) \
    case LF_PF_ ## format: \
        AddColorCallback ( \
            (lfModifyColorFunc)(void (*)(void *, float, float, lf_##type *, int, int)) \
            lfExtModifier::func, prio, tmp, 4 * sizeof (float)); \
        break;

    tmp [0] = This->NormScale;
    memcpy (tmp + 1, model.Terms, 3 * sizeof (float));

    if (reverse)
        switch (model.Model)
        {
            case LF_VIGNETTING_MODEL_PA:
                switch (format)
                {
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, U8, u8, 250);
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, U16, u16, 250);
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, U32, u32, 250);
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, F32, f32, 250);
                    ADD_CALLBACK (ModifyColor_Vignetting_PA, F64, f64, 250);
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
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, U8, u8, 750);
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, U16, u16, 750);
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, U32, u32, 750);
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, F32, f32, 750);
                    ADD_CALLBACK (ModifyColor_DeVignetting_PA, F64, f64, 750);
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
    void *rgb, float x, float y, int width, int height, int pixel_stride, int row_stride) const
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
            cd->callback (cd->data, x, y, rgb, pixel_stride, width);
        }
        rgb = ((char *)rgb) + row_stride;
    }

    return true;
}

template<typename T> void lfExtModifier::ModifyColor_Vignetting_PA (
    void *data, float x, float y, T *rgb, int pixel_stride, int count)
{
    float *param = (float *)data;
    // For faster computation we will compute r^2 here, and
    // further compute just the delta:
    // ((x+1)*(x+1)+y*y) - (x*x + y*y) = 2 * x + 1
    double r2 = x * x + y * y;
    double d = param [0] * 2.0;
    double d2 = param [0] * param [0];
    while (count--)
    {
        double r4 = r2 * r2;
        double r6 = r4 * r2;
        double c = 1.0 + param [1] * r2 + param [2] * r4 + param [3] * r6;

        if (sizeof (T) <= 2)
        {
            // This branch is taken ONLY for lf_u8 and lf_u16
            int c15 = int (c * 32768.0);
            rgb [0] = clamp ((int (rgb [0]) * c15) >> 15, int ((1 << ((sizeof (T) * 8) & 15)) - 1));
            rgb [1] = clamp ((int (rgb [1]) * c15) >> 15, int ((1 << ((sizeof (T) * 8) & 15)) - 1));
            rgb [2] = clamp ((int (rgb [2]) * c15) >> 15, int ((1 << ((sizeof (T) * 8) & 15)) - 1));
        }
        else
        {
            rgb [0] = T (rgb [0] * c);
            rgb [1] = T (rgb [1] * c);
            rgb [2] = T (rgb [2] * c);
        }

        rgb = (T *)(((char *)rgb) + pixel_stride);
        r2 += d * x + d2;
        x += param [0];
    }
}

template<typename T> void lfExtModifier::ModifyColor_DeVignetting_PA (
    void *data, float x, float y, T *rgb, int pixel_stride, int count)
{
    float *param = (float *)data;

    // For faster computation we will compute r^2 here, and
    // further compute just the delta:
    // ((x+1)*(x+1)+y*y) - (x*x + y*y) = 2 * x + 1
    double r2 = x * x + y * y;
    double d = param [0] * 2.0;
    double d2 = param [0] * param [0];

    while (count--)
    {
        double r4 = r2 * r2;
        double r6 = r4 * r2;
        double c = 1.0 + param [1] * r2 + param [2] * r4 + param [3] * r6;

        // Not pretty elegant, but we have to clamp uint8 and uint16,
        // and don't have to clamp u32, f32 and f64 values.
        if (sizeof (T) <= 2)
        {
            // This branch is taken ONLY for lf_u8 and lf_u16
            // Use 17.15 fixed-point math; avoid losing the sign bit
            int c15 = int (32768.0 / c);
            rgb [0] = clamp ((int (rgb [0]) * c15) >> 15, int ((1 << ((sizeof (T) * 8) & 15)) - 1));
            rgb [1] = clamp ((int (rgb [1]) * c15) >> 15, int ((1 << ((sizeof (T) * 8) & 15)) - 1));
            rgb [2] = clamp ((int (rgb [2]) * c15) >> 15, int ((1 << ((sizeof (T) * 8) & 15)) - 1));
        }
        else
        {
            c = 1.0 / c;
            rgb [0] = T (rgb [0] * c);
            rgb [1] = T (rgb [1] * c);
            rgb [2] = T (rgb [2] * c);
        }

        rgb = (T *)(((char *)rgb) + pixel_stride);
        r2 += d * x + d2;
        x += param [0];
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
    lfModifier *modifier, void *rgb, float x, float y, int width, int height, int pixel_stride, int row_stride)
{
    return modifier->ApplyColorModification (rgb, x, y, width, height, pixel_stride, row_stride);
}
