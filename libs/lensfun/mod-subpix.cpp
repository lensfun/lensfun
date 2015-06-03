/*
    Image modifier implementation: (un)distortion functions
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>

void lfModifier::AddSubpixelCallback (
    lfSubpixelCoordFunc callback, int priority, void *data, size_t data_size)
{
    lfSubpixelCallbackData *d = new lfSubpixelCallbackData ();
    d->callback = callback;
    AddCallback (SubpixelCallbacks, d, priority, data, data_size);
}

bool lfModifier::AddSubpixelCallbackTCA (lfLensCalibTCA &model, bool reverse)
{
    float tmp [2];

    if (reverse)
        switch (model.Model)
        {
            case LF_TCA_MODEL_NONE:
                break;

            case LF_TCA_MODEL_LINEAR:
                for (int i = 0; i < 2; i++)
                {
                    if (!model.Terms [i])
                        return false;
                    tmp [i] = 1.0 / model.Terms [i];
                }
                AddSubpixelCallback (ModifyCoord_UnTCA_Linear, 500,
                                     tmp, 2 * sizeof (float));
                return true;

            case LF_TCA_MODEL_POLY3:
                AddSubpixelCallback (ModifyCoord_UnTCA_Poly3, 500,
                                     model.Terms, 6 * sizeof (float));
                return true;

            default:
                // keep gcc 4.4+ happy
                break;
        }
    else
        switch (model.Model)
        {
            case LF_TCA_MODEL_NONE:
                break;

            case LF_TCA_MODEL_LINEAR:
                AddSubpixelCallback (ModifyCoord_TCA_Linear, 500,
                                     model.Terms, 2 * sizeof (float));
                return true;

            case LF_TCA_MODEL_POLY3:
                AddSubpixelCallback (ModifyCoord_TCA_Poly3, 500,
                                     model.Terms, 6 * sizeof (float));
                return true;

            default:
                // keep gcc 4.4+ happy
                break;
        }

    return false;
}

bool lfModifier::ApplySubpixelDistortion (
    float xu, float yu, int width, int height, float *res) const
{
    if (((GPtrArray *)SubpixelCallbacks)->len <= 0 || height <= 0)
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

        for (i = 0; i < (int)((GPtrArray *)SubpixelCallbacks)->len; i++)
        {
            lfSubpixelCallbackData *cd =
                (lfSubpixelCallbackData *)g_ptr_array_index ((GPtrArray *)SubpixelCallbacks, i);
            cd->callback (cd->data, res, width);
        }

        // Convert normalized coordinates back into natural coordiates
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
    if ((((GPtrArray *)SubpixelCallbacks)->len <= 0 && ((GPtrArray *)CoordCallbacks)->len <= 0)
     || height <= 0)
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

        for (i = 0; i < (int)((GPtrArray *)CoordCallbacks)->len; i++)
        {
            lfCoordCallbackData *cd =
                (lfCoordCallbackData *)g_ptr_array_index ((GPtrArray *)CoordCallbacks, i);
            cd->callback (cd->data, res, width * 3);
        }

        for (i = 0; i < (int)((GPtrArray *)SubpixelCallbacks)->len; i++)
        {
            lfSubpixelCallbackData *cd =
                (lfSubpixelCallbackData *)g_ptr_array_index ((GPtrArray *)SubpixelCallbacks, i);
            cd->callback (cd->data, res, width);
        }

        // Convert normalized coordinates back into natural coordiates
        for (i = width * 3; i > 0; i--)
        {
            res [0] = (res [0] + CenterX) * NormUnScale;
            res [1] = (res [1] + CenterY) * NormUnScale;
            res += 2;
        }
    }

    return true;
}

void lfModifier::ModifyCoord_UnTCA_Linear (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    float k_r = param [0];
    float k_b = param [1];

    for (float *end = iocoord + count * 2 * 3; iocoord < end; iocoord += 6)
    {
        iocoord [0] *= k_r;
        iocoord [1] *= k_r;
        iocoord [4] *= k_b;
        iocoord [5] *= k_b;
    }
}

void lfModifier::ModifyCoord_TCA_Linear (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    float k_r = param [0];
    float k_b = param [1];

    for (float *end = iocoord + count * 2 * 3; iocoord < end; iocoord += 6)
    {
        iocoord [0] *= k_r;
        iocoord [1] *= k_r;
        iocoord [4] *= k_b;
        iocoord [5] *= k_b;
    }
}

void lfModifier::ModifyCoord_UnTCA_Poly3 (void *data, float *iocoord, int count)
{
    const float *param = (float *)data;
    const float vr = param [0];
    const float vb = param [1];
    const float cr = param [2];
    const float cb = param [3];
    const float br = param [4];
    const float bb = param [5];

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
    const float *param = (float *)data;
    const float vr = param [0];
    const float vb = param [1];
    const float cr = param [2];
    const float cb = param [3];
    const float br = param [4];
    const float bb = param [5];

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

//---------------------------// The C interface //---------------------------//

void lf_modifier_add_subpixel_callback (
    lfModifier *modifier, lfSubpixelCoordFunc callback, int priority,
    void *data, size_t data_size)
{
    modifier->AddSubpixelCallback (callback, priority, data, data_size);
}

cbool lf_modifier_add_subpixel_callback_TCA (
    lfModifier *modifier, lfLensCalibTCA *model, cbool reverse)
{
    return modifier->AddSubpixelCallbackTCA (*model, reverse);
}

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
