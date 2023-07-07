/*
    Image modifier implementation: (un)distortion functions
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>

/* See modifier.cpp for general info about the coordinate systems. */

lfLensCalibTCA rescale_polynomial_coefficients (const lfLensCalibTCA& lctca_, double real_focal, cbool Reverse)
{
    // FixMe: The ACM probably bases on the nominal focal length.  This needs
    // to be found out.  It this is true, we have to scale its coefficient by
    // the ratio of real and nominal focal length.
    lfLensCalibTCA lctca = lctca_;
    const float hugin_scale_in_millimeters =
        hypot (36.0, 24.0) / lctca.CalibAttr.CropFactor / hypot (lctca.CalibAttr.AspectRatio, 1) / 2.0;
    const float hugin_scaling = real_focal / hugin_scale_in_millimeters;
    switch (lctca.Model)
    {
        case LF_TCA_MODEL_LINEAR:
            if (Reverse)
                for (int i = 0; i < 2; i++)
                    lctca.Terms [i] = 1.0 / lctca.Terms [i];
            break;
        case LF_TCA_MODEL_POLY3:
            lctca.Terms [2] *= hugin_scaling;
            lctca.Terms [3] *= hugin_scaling;
            lctca.Terms [4] *= hugin_scaling * hugin_scaling;
            lctca.Terms [5] *= hugin_scaling * hugin_scaling;
            break;
        default:
            // keep gcc 4.4+ happy
            break;
    }
    return lctca;
}

int lfModifier::EnableTCACorrection (const lfLensCalibTCA& lctca_)
{
    const lfLensCalibTCA lctca = rescale_polynomial_coefficients (lctca_, RealFocal, Reverse);
    if (Reverse)
        switch (lctca.Model)
        {
            case LF_TCA_MODEL_NONE:
                break;

            case LF_TCA_MODEL_LINEAR:
                AddSubpixTCACallback(lctca, ModifyCoord_TCA_Linear, 500);
                EnabledMods |= LF_MODIFY_TCA;
                return EnabledMods;

            case LF_TCA_MODEL_POLY3:
                AddSubpixTCACallback(lctca, ModifyCoord_UnTCA_Poly3, 500);
                EnabledMods |= LF_MODIFY_TCA;
                return EnabledMods;

            case LF_TCA_MODEL_ACM:
                g_warning ("[lensfun] \"acm\" TCA model is not yet implemented "
                           "for reverse correction");
                return EnabledMods;

            default:
                // keep gcc 4.4+ happy
                break;
        }
    else
        switch (lctca.Model)
        {
            case LF_TCA_MODEL_NONE:
                break;

            case LF_TCA_MODEL_LINEAR:
                AddSubpixTCACallback(lctca, ModifyCoord_TCA_Linear, 500);
                EnabledMods |= LF_MODIFY_TCA;
                return EnabledMods;

            case LF_TCA_MODEL_POLY3:
                AddSubpixTCACallback(lctca, ModifyCoord_TCA_Poly3, 500);
                EnabledMods |= LF_MODIFY_TCA;
                return EnabledMods;

            case LF_TCA_MODEL_ACM:
                AddSubpixTCACallback(lctca, ModifyCoord_TCA_ACM, 500);
                EnabledMods |= LF_MODIFY_TCA;
                return EnabledMods;

            default:
                // keep gcc 4.4+ happy
                break;
        }

    return EnabledMods;
}

int lfModifier::EnableTCACorrection ()
{
    lfLensCalibTCA lctca;
    if (Lens->InterpolateTCA (Crop, Focal, lctca))
    {
        return EnableTCACorrection(lctca);
    }

    return EnabledMods;
}

void lfModifier::AddSubpixTCACallback (const lfLensCalibTCA& lctca, lfModifySubpixCoordFunc func, int priority)
{
    lfSubpixTCACallback* cd = new lfSubpixTCACallback;

    cd->callback = func;
    cd->priority = priority;

    memcpy(cd->terms, lctca.Terms, sizeof(lctca.Terms));

    SubpixelCallbacks.insert(cd);
}

bool lfModifier::ApplySubpixelDistortion (
    float xu, float yu, int width, int height, float *res) const
{
    if (SubpixelCallbacks.size() <= 0 || height <= 0)
        return false; // nothing to do

    // All callbacks work with normalized coordinates
    xu = xu * NormScale - CenterX;
    yu = yu * NormScale - CenterY;

    for (float y = yu; height; y += NormScale, height--)
    {
        int i;
        float x = xu;
        float *out = res;
        for (i = 0; i < width; i++, x += NormScale)
        {
            out [0] = out [2] = out [4] = x;
            out [1] = out [3] = out [5] = y;
            out += 6;
        }

        for (auto cb : SubpixelCallbacks)
            cb->callback (cb, res, width);

        // Convert normalized coordinates back into natural coordinates
        for (i = width * 3; i > 0; i--)
        {
            res [0] = (res [0] + CenterX) * NormUnScale;
            res [1] = (res [1] + CenterY) * NormUnScale;
            res += 2;
        }
    }

    return true;
}

bool lfModifier::ApplySubpixelGeometryDistortion (
    float xu, float yu, int width, int height, float *res) const
{
    if ((SubpixelCallbacks.size() <= 0 && CoordCallbacks.size() <= 0) || height <= 0)
        return false; // nothing to do

    // All callbacks work with normalized coordinates
    xu = xu * NormScale - CenterX;
    yu = yu * NormScale - CenterY;

    for (float y = yu; height; y += NormScale, height--)
    {
        int i;
        float x = xu;
        float *out = res;
        for (i = 0; i < width; i++, x += NormScale)
        {
            out [0] = out [2] = out [4] = x;
            out [1] = out [3] = out [5] = y;
            out += 6;
        }

        for (auto cb : CoordCallbacks)
            cb->callback (cb, res, width * 3);

        for (auto cb : SubpixelCallbacks)
            cb->callback (cb, res, width);

        // Convert normalized coordinates back into natural coordinates
        for (i = width * 3; i > 0; i--)
        {
            res [0] = (res [0] + CenterX) * NormUnScale;
            res [1] = (res [1] + CenterY) * NormUnScale;
            res += 2;
        }
    }

    return true;
}

void lfModifier::ModifyCoord_TCA_Linear (void *data, float *iocoord, int count)
{
    lfSubpixTCACallback* cddata = (lfSubpixTCACallback*) data;
    const float k_r = cddata->terms [0];
    const float k_b = cddata->terms [1];

    for (float *end = iocoord + count * 2 * 3; iocoord < end; iocoord += 6)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];

        iocoord [0] = x * k_r;
        iocoord [1] = y * k_r;
        iocoord [4] = x * k_b;
        iocoord [5] = y * k_b;
    }
}

void lfModifier::ModifyCoord_UnTCA_Poly3 (void *data, float *iocoord, int count)
{
    lfSubpixTCACallback* cddata = (lfSubpixTCACallback*) data;
    const float vr = cddata->terms [0];
    const float vb = cddata->terms [1];
    const float cr = cddata->terms [2];
    const float cb = cddata->terms [3];
    const float br = cddata->terms [4];
    const float bb = cddata->terms [5];

    for (float *end = iocoord + count * 2 * 3; iocoord < end; iocoord += 6)
    {
        float x, y;
        double rd, ru, ru2;

        x = iocoord [0];
        y = iocoord [1];
        rd = sqrt (x * x + y * y);
        if (rd == 0.0)
            goto next_subpixel_r;

        // Original equation: Rd = b * Ru^3 + c * Ru^2 + v * Ru
        // We have Rd, need to find Ru. We will use Newton's method
        // for finding the root of the equation:
        //
        // Target function: b * Ru^3 + c * Ru^2 + v * Ru - Rd = 0
        // Derivative:      3 * b * Ru^2 + 2 * c * Ru + v
        ru = rd;
        for (int step = 0; ; step++)
        {
            ru2 = ru * ru;
            double fru = br * ru2 * ru + cr * ru2 + vr * ru - rd;
            if (fru >= -NEWTON_EPS && fru < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_subpixel_r;

            ru -= fru / (3 * br * ru2 + 2 * cr * ru + vr);
        }
        // Negative radius does not make sense at all
        if (ru > 0.0)
        {
            ru /= rd;
            iocoord [0] = x * ru;
            iocoord [1] = y * ru;
        }
next_subpixel_r:

        x = iocoord [4];
        y = iocoord [5];
        rd = sqrt (x * x + y * y);
        if (rd == 0.0)
            goto next_subpixel_b;

        ru = rd;
        for (int step = 0; ; step++)
        {
            ru2 = ru * ru;
            double fru = bb * ru2 * ru + cb * ru2 + vb * ru - rd;
            if (fru >= -NEWTON_EPS && fru < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_subpixel_b;

            ru -= fru / (3 * bb * ru2 + 2 * cb * ru + vb);
        }
        // Negative radius does not make sense at all
        if (ru > 0.0)
        {
            ru /= rd;
            iocoord [4] = x * ru;
            iocoord [5] = y * ru;
        }
next_subpixel_b:;
    }
}

void lfModifier::ModifyCoord_TCA_Poly3 (void *data, float *iocoord, int count)
{
    // Rd = Ru * (b * Ru^2 + c * Ru + v)
    lfSubpixTCACallback* cddata = (lfSubpixTCACallback*) data;
    const float vr = cddata->terms [0];
    const float vb = cddata->terms [1];
    const float cr = cddata->terms [2];
    const float cb = cddata->terms [3];
    const float br = cddata->terms [4];
    const float bb = cddata->terms [5];

    float x, y, ru2, poly2;
    // Optimize for the case when c == 0 (avoid two square roots per pixel)
    if (cr == 0.0 && cb == 0.0)
        for (float *end = iocoord + count * 2 * 3; iocoord < end; iocoord += 6)
        {
            x = iocoord [0];
            y = iocoord [1];
            ru2 = x * x + y * y;
            poly2 = br * ru2 + vr;
            iocoord [0] = x * poly2;
            iocoord [1] = y * poly2;

            x = iocoord [4];
            y = iocoord [5];
            ru2 = x * x + y * y;
            poly2 = bb * ru2 + vb;
            iocoord [4] = x * poly2;
            iocoord [5] = y * poly2;
        }
    else
        for (float *end = iocoord + count * 2 * 3; iocoord < end; iocoord += 6)
        {
            x = iocoord [0];
            y = iocoord [1];
            ru2 = x * x + y * y;
            poly2 = br * ru2 + cr * sqrt (ru2) + vr;
            iocoord [0] = x * poly2;
            iocoord [1] = y * poly2;

            x = iocoord [4];
            y = iocoord [5];
            ru2 = x * x + y * y;
            poly2 = bb * ru2 + cb * sqrt (ru2) + vb;
            iocoord [4] = x * poly2;
            iocoord [5] = y * poly2;
        }
}

void lfModifier::ModifyCoord_TCA_ACM (void *data, float *iocoord, int count)
{
    // Rd = Ru * (b * Ru^2 + c * Ru + v)
    lfSubpixTCACallback* cddata = (lfSubpixTCACallback*) data;
    const float alpha0 = cddata->terms [0];
    const float beta0 = cddata->terms [1];
    const float alpha1 = cddata->terms [2];
    const float beta1 = cddata->terms [3];
    const float alpha2 = cddata->terms [4];
    const float beta2 = cddata->terms [5];
    const float alpha3 = cddata->terms [6];
    const float beta3 = cddata->terms [7];
    const float alpha4 = cddata->terms [8];
    const float beta4 = cddata->terms [9];
    const float alpha5 = cddata->terms [10];
    const float beta5 = cddata->terms [11];

    float x, y, ru2, ru4, common_term;
    for (float *end = iocoord + count * 2 * 3; iocoord < end; iocoord += 6)
    {
        // In the Adobe docs, our "ru" is called "rd".  They call it so because
        // it is already distorted for the distortion correction.  However, in
        // context of TCA correction, it is undistorted, so Lensfun calls it
        // "ru".
        x = iocoord [0];
        y = iocoord [1];
        ru2 = x * x + y * y;
        ru4 = ru2 * ru2;
        common_term = 1.0 + alpha1 * ru2 + alpha2 * ru4 + alpha3 * ru4 * ru2 +
                      2 * (alpha4 * y + alpha5 * x);
        iocoord [0] = alpha0 * (x * common_term + alpha5 * ru2);
        iocoord [1] = alpha0 * (y * common_term + alpha4 * ru2);
        iocoord [0] = iocoord [0];
        iocoord [1] = iocoord [1];

        x = iocoord [4];
        y = iocoord [5];
        ru2 = x * x + y * y;
        ru4 = ru2 * ru2;
        common_term = 1.0 + beta1 * ru2 + beta2 * ru4 + beta3 * ru4 * ru2 +
                      2 * (beta4 * y + beta5 * x);
        iocoord [4] = beta0 * (x * common_term + beta5 * ru2);
        iocoord [5] = beta0 * (y * common_term + beta4 * ru2);
    }
}

//---------------------------// The C interface //---------------------------//

cbool lf_modifier_apply_subpixel_distortion (
    lfModifier *modifier, float xu, float yu, int width, int height, float *res)
{
    return modifier->ApplySubpixelDistortion (xu, yu, width, height, res);
}

cbool lf_modifier_apply_subpixel_geometry_distortion (
    lfModifier *modifier, float xu, float yu, int width, int height, float *res)
{
    return modifier->ApplySubpixelGeometryDistortion (xu, yu, width, height, res);
}

int lf_modifier_enable_tca_correction (lfModifier *modifier)
{
    return modifier->EnableTCACorrection();
}
