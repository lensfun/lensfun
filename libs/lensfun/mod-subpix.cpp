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
    lfExtModifier *This = static_cast<lfExtModifier *> (this);
    lfSubpixelCallbackData *d = new lfSubpixelCallbackData ();
    d->callback = callback;
    This->AddCallback (This->SubpixelCallbacks, d, priority, data, data_size);
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
                AddSubpixelCallback (lfExtModifier::ModifyCoord_TCA_Linear, 500,
                                     tmp, 2 * sizeof (float));
                return true;

            case LF_TCA_MODEL_POLY3:
                AddSubpixelCallback (lfExtModifier::ModifyCoord_TCA_Poly3, 500,
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
                AddSubpixelCallback (lfExtModifier::ModifyCoord_UnTCA_Linear, 500,
                                     model.Terms, 2 * sizeof (float));
                return true;

            case LF_TCA_MODEL_POLY3:
                AddSubpixelCallback (lfExtModifier::ModifyCoord_UnTCA_Poly3, 500,
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
    const lfExtModifier *This = static_cast<const lfExtModifier *> (this);

    if (This->SubpixelCallbacks->len <= 0 || height <= 0)
        return false; // nothing to do

    // All callbacks work with normalized coordinates
    xu = xu * This->NormScale - This->CenterX;
    yu = yu * This->NormScale - This->CenterY;

    for (float y = yu; height; y += This->NormScale, height--)
    {
        int i;
        float x = xu;
        float *out = res;
        for (i = 0; i < width; i++, x += This->NormScale)
        {
            out [0] = out [2] = out [4] = x;
            out [1] = out [3] = out [5] = y;
            out += 6;
        }

        for (i = 0; i < (int)This->SubpixelCallbacks->len; i++)
        {
            lfSubpixelCallbackData *cd =
                (lfSubpixelCallbackData *)g_ptr_array_index (This->SubpixelCallbacks, i);
            cd->callback (cd->data, res, width);
        }

        // Convert normalized coordinates back into natural coordiates
        for (i = width * 3; i > 0; i--)
        {
            res [0] = (res [0] + This->CenterX) * This->NormUnScale;
            res [1] = (res [1] + This->CenterY) * This->NormUnScale;
            res += 2;
        }
    }

    return true;
}

bool lfModifier::ApplySubpixelGeometryDistortion (
    float xu, float yu, int width, int height, float *res) const
{
    const lfExtModifier *This = static_cast<const lfExtModifier *> (this);

    if ((This->SubpixelCallbacks->len <= 0 && This->CoordCallbacks->len <= 0)
     || height <= 0)
        return false; // nothing to do

    // All callbacks work with normalized coordinates
    xu = xu * This->NormScale - This->CenterX;
    yu = yu * This->NormScale - This->CenterY;

    for (float y = yu; height; y += This->NormScale, height--)
    {
        int i;
        float x = xu;
        float *out = res;
        for (i = 0; i < width; i++, x += This->NormScale)
        {
            out [0] = out [2] = out [4] = x;
            out [1] = out [3] = out [5] = y;
            out += 6;
        }

        for (i = 0; i < (int)This->CoordCallbacks->len; i++)
        {
            lfCoordCallbackData *cd =
                (lfCoordCallbackData *)g_ptr_array_index (This->CoordCallbacks, i);
            cd->callback (cd->data, res, width * 3);
        }

        for (i = 0; i < (int)This->SubpixelCallbacks->len; i++)
        {
            lfSubpixelCallbackData *cd =
                (lfSubpixelCallbackData *)g_ptr_array_index (This->SubpixelCallbacks, i);
            cd->callback (cd->data, res, width);
        }

        // Convert normalized coordinates back into natural coordiates
        for (i = width * 3; i > 0; i--)
        {
            res [0] = (res [0] + This->CenterX) * This->NormUnScale;
            res [1] = (res [1] + This->CenterY) * This->NormUnScale;
            res += 2;
        }
    }

    return true;
}

void lfExtModifier::ModifyCoord_TCA_Linear (void *data, float *iocoord, int count)
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

void lfExtModifier::ModifyCoord_UnTCA_Linear (void *data, float *iocoord, int count)
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

void lfExtModifier::ModifyCoord_TCA_Poly3 (void *data, float *iocoord, int count)
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
        double ru, rd, rd2;

        x = iocoord [0];
        y = iocoord [1];
        ru = sqrt (x * x + y * y);
        if (ru == 0.0)
            goto next_subpixel_r;

        // Original equation: Ru = b * Rd^3 + c * Rd^2 + v * Rd
        // We have Ru, need to find Rd. We will use Newton's method
        // for finding the root of the equation:
        //
        // Target function: b * Rd^3 + c * Rd^2 + v * Rd - Ru = 0
        // Derivative:      3 * b * Rd^2 + 2 * c * Rd + v
        rd = ru;
        for (int step = 0; ; step++)
        {
            rd2 = rd * rd;
            double frd = br * rd2 * rd + cr * rd2 + vr * rd - ru;
            if (frd >= -NEWTON_EPS && frd < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_subpixel_r;

            rd -= frd / (3 * br * rd2 + 2 * cr * rd + vr);
        }
        // Negative radius does not make sense at all
        if (rd > 0.0)
        {
            rd /= ru;
            iocoord [0] = x * rd;
            iocoord [1] = y * rd;
        }
next_subpixel_r:

        x = iocoord [4];
        y = iocoord [5];
        ru = sqrt (x * x + y * y);
        if (ru == 0.0)
            goto next_subpixel_b;

        rd = ru;
        for (int step = 0; ; step++)
        {
            rd2 = rd * rd;
            double frd = bb * rd2 * rd + cb * rd2 + vb * rd - ru;
            if (frd >= -NEWTON_EPS && frd < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_subpixel_b;

            rd -= frd / (3 * bb * rd2 + 2 * cb * rd + vb);
        }
        // Negative radius does not make sense at all
        if (rd > 0.0)
        {
            rd /= ru;
            iocoord [4] = x * rd;
            iocoord [5] = y * rd;
        }
next_subpixel_b:;
    }
}

void lfExtModifier::ModifyCoord_UnTCA_Poly3 (void *data, float *iocoord, int count)
{
    // Ru = Rd * (b * Rd^2 + c * Rd + v)
    const float *param = (float *)data;
    const float vr = param [0];
    const float vb = param [1];
    const float cr = param [2];
    const float cb = param [3];
    const float br = param [4];
    const float bb = param [5];

    float x, y, r2, poly2;
    // Optimize for the case when c == 0 (avoid two square roots per pixel)
    if (cr == 0.0 && cb == 0.0)
        for (float *end = iocoord + count * 2 * 3; iocoord < end; iocoord += 6)
        {
            x = iocoord [0];
            y = iocoord [1];
            r2 = x * x + y * y;
            poly2 = br * r2 + vr;
            iocoord [0] = x * poly2;
            iocoord [1] = y * poly2;

            x = iocoord [4];
            y = iocoord [5];
            r2 = x * x + y * y;
            poly2 = bb * r2 + vb;
            iocoord [4] = x * poly2;
            iocoord [5] = y * poly2;
        }
    else
        for (float *end = iocoord + count * 2 * 3; iocoord < end; iocoord += 6)
        {
            x = iocoord [0];
            y = iocoord [1];
            r2 = x * x + y * y;
            poly2 = br * r2 + cr * sqrt (r2) + vr;
            iocoord [0] = x * poly2;
            iocoord [1] = y * poly2;

            x = iocoord [4];
            y = iocoord [5];
            r2 = x * x + y * y;
            poly2 = bb * r2 + cb * sqrt (r2) + vb;
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
