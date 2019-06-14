/*
    Image modifier implementation
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>

lfLensCalibVignetting rescale_polynomial_coefficients (const lfLensCalibVignetting& lcv_)
{
    lfLensCalibVignetting lcv = lcv_;
    const float hugin_scale_in_millimeters = hypot (36.0, 24.0) / lcv.CalibAttr.CropFactor / 2.0;
    const float hugin_scaling = lcv.RealFocal / hugin_scale_in_millimeters;
    switch (lcv.Model)
    {
        case LF_VIGNETTING_MODEL_PA:
            lcv.Terms [0] /= pow (hugin_scaling, 2);
            lcv.Terms [1] /= pow (hugin_scaling, 4);
            lcv.Terms [2] /= pow (hugin_scaling, 6);
            break;
        }
    }
    return lcv;
}

int lfModifier::EnableVignettingCorrection(const lfLensCalibVignetting& lcv_)
{
#define ADD_CALLBACK(lcv, func, type, prio) \
    AddColorVignCallback ( lcv, \
        (lfModifyColorFunc)(void (*)(void *, float, float, type *, int, int)) \
        lfModifier::func, prio) \

    const lfLensCalibVignetting lcv = rescale_polynomial_coefficients (lcv_);
    if (Reverse)
        switch (lcv.Model)
        {
            case LF_VIGNETTING_MODEL_PA:
            case LF_VIGNETTING_MODEL_ACM:
                switch (PixelFormat)
                {
                    case LF_PF_U8:
                        ADD_CALLBACK(lcv, ModifyColor_Vignetting_PA, lf_u8, 250);
                        break;

                    case LF_PF_U16:
                        ADD_CALLBACK (lcv, ModifyColor_Vignetting_PA, lf_u16, 250);
                        break;

                    case LF_PF_U32:
                        ADD_CALLBACK (lcv, ModifyColor_Vignetting_PA, lf_u32, 250);
                        break;

                    case LF_PF_F32:
                        ADD_CALLBACK (lcv, ModifyColor_Vignetting_PA, lf_f32, 250);
                        break;

                    case LF_PF_F64:
                        ADD_CALLBACK (lcv, ModifyColor_Vignetting_PA, lf_f64, 250);
                        break;

                    default:
                        return EnabledMods;
                }
                break;

            default:
                return EnabledMods;
        }
    else
        switch (lcv.Model)
        {
            case LF_VIGNETTING_MODEL_PA:
            case LF_VIGNETTING_MODEL_ACM:
                switch (PixelFormat)
                {
                    case LF_PF_U8:
                        ADD_CALLBACK (lcv, ModifyColor_DeVignetting_PA, lf_u8, 750);
                        break;

                    case LF_PF_U16:
#ifdef VECTORIZATION_SSE2
                        if (_lf_detect_cpu_features () & LF_CPU_FLAG_SSE2)
                            ADD_CALLBACK (lcv, ModifyColor_DeVignetting_PA_SSE2, lf_u16, 750);
                        else
#endif
                        ADD_CALLBACK (lcv, ModifyColor_DeVignetting_PA, lf_u16, 750);
                        break;

                    case LF_PF_U32:
                        ADD_CALLBACK (lcv, ModifyColor_DeVignetting_PA, lf_u32, 750);
                        break;

                    case LF_PF_F32:
#ifdef VECTORIZATION_SSE
                        if (_lf_detect_cpu_features () & LF_CPU_FLAG_SSE)
                            ADD_CALLBACK (lcv, ModifyColor_DeVignetting_PA_SSE, lf_f32, 750);
                        else
#endif
                        ADD_CALLBACK (lcv, ModifyColor_DeVignetting_PA, lf_f32, 750);
                        break;

                    case LF_PF_F64:
                        ADD_CALLBACK (lcv, ModifyColor_DeVignetting_PA, lf_f64, 750);
                        break;

                    default:
                        return EnabledMods;
                }
                break;

            default:
                return EnabledMods;
        }

#undef ADD_CALLBACK

    EnabledMods |= LF_MODIFY_VIGNETTING;
    return true;
}


int lfModifier::EnableVignettingCorrection (float aperture, float distance)
{
    lfLensCalibVignetting lcv;

    if (Lens->InterpolateVignetting (Crop, Focal, aperture, distance, lcv))
    {
        EnableVignettingCorrection(lcv);
    }

    return EnabledMods;
}

void lfModifier::AddColorVignCallback (const lfLensCalibVignetting& lcv, lfModifyColorFunc func, int priority)
{
    lfColorVignCallbackData* cd = new lfColorVignCallbackData;

    cd->callback = func;
    cd->priority = priority;

    cd->norm_scale = NormScale;
    memcpy(cd->terms, lcv.Terms, sizeof(lcv.Terms));

    ColorCallbacks.insert(cd);
}

bool lfModifier::ApplyColorModification (
    void *pixels, float x, float y, int width, int height, int comp_role, int row_stride) const
{
    if (ColorCallbacks.size() <= 0 || height <= 0)
        return false; // nothing to do

    x = x * NormScale - CenterX;
    y = y * NormScale - CenterY;

    for (; height; y += NormScale, height--)
    {
        for (auto cb : ColorCallbacks)
            cb->callback (cb, x, y, pixels, comp_role, width);
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

template<typename T> void lfModifier::ModifyColor_Vignetting_PA (
    void *data, float x, float y, T *pixels, int comp_role, int count)
{
    lfColorVignCallbackData* cddata = (lfColorVignCallbackData*) data;

    // For faster computation we will compute r^2 here, and
    // further compute just the delta:
    // ((x+1)*(x+1)+y*y) - (x*x + y*y) = 2 * x + 1
    // But since we work in a normalized coordinate system, a step of
    // 1.0 pixels should be multiplied by NormScale, so it's really:
    // ((x+ns)*(x+ns)+y*y) - (x*x + y*y) = 2 * ns * x + ns^2
    float r2 = x * x + y * y;
    float d1 = 2.0 * cddata->norm_scale;
    float d2 = cddata->norm_scale * cddata->norm_scale;

    int cr = 0;
    while (count--)
    {
        float r4 = r2 * r2;
        float r6 = r4 * r2;
        float c = 1.0 + cddata->terms [0] * r2 + cddata->terms [1] * r4 + cddata->terms [2] * r6;
        if (!cr)
            cr = comp_role;

        pixels = apply_multiplier<T> (pixels, c, cr);

        r2 += d1 * x + d2;
        x += cddata->norm_scale;
    }
}

template<typename T> void lfModifier::ModifyColor_DeVignetting_PA (
    void *data, float x, float y, T *pixels, int comp_role, int count)
{
    lfColorVignCallbackData* cddata = (lfColorVignCallbackData*) data;

    // For faster computation we will compute r^2 here, and
    // further compute just the delta:
    // ((x+1)*(x+1)+y*y) - (x*x + y*y) = 2 * x + 1
    // But since we work in a normalized coordinate system, a step of
    // 1.0 pixels should be multiplied by NormScale, so it's really:
    // ((x+ns)*(x+ns)+y*y) - (x*x + y*y) = 2 * ns * x + ns^2
    float r2 = x * x + y * y;
    float d1 = 2.0 * cddata->norm_scale;
    float d2 = cddata->norm_scale * cddata->norm_scale;

    int cr = 0;
    while (count--)
    {
        float r4 = r2 * r2;
        float r6 = r4 * r2;
        float c = 1.0 + cddata->terms [0] * r2 + cddata->terms [1] * r4 + cddata->terms [2] * r6;
        if (!cr)
            cr = comp_role;

        pixels = apply_multiplier<T> (pixels, 1.0f / c, cr);

        r2 += d1 * x + d2;
        x += cddata->norm_scale;
    }
}

//---------------------------// The C interface //---------------------------//

cbool lf_modifier_apply_color_modification (
    lfModifier *modifier, void *pixels, float x, float y, int width, int height,
    int comp_role, int row_stride)
{
    return modifier->ApplyColorModification (
        pixels, x, y, width, height, comp_role, row_stride);
}

int lf_modifier_enable_vignetting_correction (
    lfModifier *modifier, float aperture, float distance)
{
    return modifier->EnableVignettingCorrection(aperture, distance);
}
