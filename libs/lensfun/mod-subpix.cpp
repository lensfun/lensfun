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
                    tmp [i] = model.Terms [i];
                AddSubpixelCallback (lfExtModifier::ModifyCoord_TCA_Linear, 500,
                                     tmp, 2 * sizeof (float));
                return true;
        }
    else
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
                AddSubpixelCallback (lfExtModifier::ModifyCoord_UnTCA_Linear, 500,
                                     tmp, 2 * sizeof (float));
                return true;
        }
    return false;
}

bool lfModifier::ApplySubpixelDistortion (
    float xu, float yu, int width, int height, float *res) const
{
    const lfExtModifier *This = static_cast<const lfExtModifier *> (this);

    if (This->SubpixelCallbacks->len <= 0)
        return false; // nothing to do

    // All callbacks work with normalized coordinates
    xu = xu * This->NormScale - This->CenterX;
    yu = yu * This->NormScale - This->CenterY;
    float y_end = yu + height * This->NormScale;

    for (float y = yu; y < y_end; y += This->NormScale)
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

    if (This->SubpixelCallbacks->len <= 0 && This->CoordCallbacks->len <= 0)
        return false; // nothing to do

    // All callbacks work with normalized coordinates
    xu = xu * This->NormScale - This->CenterX;
    yu = yu * This->NormScale - This->CenterY;
    float y_end = yu + height * This->NormScale;

    for (float y = yu; y < y_end; y += This->NormScale)
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

        for (i = 0; i < (int)This->CoordCallbacks->len; i++)
        {
            lfCoordCallbackData *cd =
                (lfCoordCallbackData *)g_ptr_array_index (This->CoordCallbacks, i);
            cd->callback (cd->data, res, width * 3);
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
