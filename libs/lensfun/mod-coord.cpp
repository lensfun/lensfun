/*
    Image modifier implementation: (un)distortion functions
    Copyright (C) 2007 by Andrew Zabolotny

    Most of the math in this file has been borrowed from PanoTools.
    Thanks to the PanoTools team for their pioneering work.
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>

// This epsilon is in image coordinate space, where 1.0 is
// half of the smallest image dimenstion (width or height)
// adjusted for the lens calibration data/camera crop factors.
#define NEWTON_EPS 0.00001

void lfModifier::AddCoordCallback (
    lfModifyCoordFunc callback, int priority, void *data, size_t data_size)
{
    lfExtModifier *This = static_cast<lfExtModifier *> (this);
    lfCoordCallbackData *d = new lfCoordCallbackData ();
    d->callback = callback;
    This->AddCallback (This->CoordCallbacks, d, priority, data, data_size);
}

bool lfModifier::AddCoordCallbackDistortion (lfLensCalibDistortion &model, bool reverse)
{
    float tmp [3];

    if (reverse)
        switch (model.Model)
        {
            case LF_DIST_MODEL_POLY3:
                if (!model.Terms [0])
                    return false;
                tmp [0] = 1.0 / model.Terms [0];
                AddCoordCallback (lfExtModifier::ModifyCoord_Dist_Poly3, 750, tmp, sizeof (float));
                break;
            case LF_DIST_MODEL_POLY5:
                tmp [0] = model.Terms [0];
                tmp [1] = model.Terms [1];
                AddCoordCallback (lfExtModifier::ModifyCoord_Dist_Poly5, 750, tmp, sizeof (float) * 2);
                break;
            case LF_DIST_MODEL_FOV1:
                if (!model.Terms [0])
                    return false;
                tmp [0] = 1.0 / model.Terms [0];
                tmp [1] = 2.0 * tan (model.Terms [0] / 2.0);
                AddCoordCallback (lfExtModifier::ModifyCoord_Dist_FOV1, 750, tmp, sizeof (float) * 2);
                break;
            case LF_DIST_MODEL_PTLENS:
                tmp [0] = model.Terms [0];
                tmp [1] = model.Terms [1];
                tmp [2] = model.Terms [2];
                AddCoordCallback (lfExtModifier::ModifyCoord_Dist_PTLens, 750, tmp, sizeof (float) * 2);
                break;
            default:
                return false;
        }
    else
        switch (model.Model)
        {
            case LF_DIST_MODEL_POLY3:
                tmp [0] = model.Terms [0];
                AddCoordCallback (lfExtModifier::ModifyCoord_UnDist_Poly3, 250, tmp, sizeof (float));
                break;
            case LF_DIST_MODEL_POLY5:
                tmp [0] = model.Terms [0];
                tmp [1] = model.Terms [1];
                AddCoordCallback (lfExtModifier::ModifyCoord_UnDist_Poly5, 250, tmp, sizeof (float) * 2);
                break;
            case LF_DIST_MODEL_FOV1:
                if (!model.Terms [0])
                    return false;
                tmp [0] = model.Terms [0];
                tmp [1] = 0.5 / tan (model.Terms [0] / 2.0);
                AddCoordCallback (lfExtModifier::ModifyCoord_UnDist_FOV1, 250, tmp, sizeof (float) * 2);
                break;
            case LF_DIST_MODEL_PTLENS:
                tmp [0] = model.Terms [0];
                tmp [1] = model.Terms [1];
                tmp [2] = model.Terms [2];
                AddCoordCallback (lfExtModifier::ModifyCoord_UnDist_PTLens, 250, tmp, sizeof (float) * 3);
                break;
            default:
                return false;
        }

    return true;
}

float lfModifier::GetAutoScale (bool reverse)
{
    // Compute the scale factor automatically
    const lfExtModifier *This = static_cast<const lfExtModifier *> (this);
    // 3 2 1
    // 4   0
    // 5 6 7
    struct { float angle, dist; } edge [8];

    edge [1].angle = atan2 (This->Height, This->Width);
    edge [3].angle = M_PI - edge [1].angle;
    edge [5].angle = M_PI + edge [1].angle;
    edge [7].angle = 2 * M_PI - edge [1].angle;

    edge [0].angle = 0.0;
    edge [2].angle = M_PI / 2.0;
    edge [4].angle = M_PI;
    edge [6].angle = M_PI * 3.0 / 2.0;

    edge [1].dist = edge [3].dist = edge [5].dist = edge [7].dist =
        sqrt (This->Width * This->Width + This->Height * This->Height) * 0.5 * This->NormScale;
    edge [0].dist = edge [4].dist = This->Width * 0.5 * This->NormScale;
    edge [2].dist = edge [6].dist = This->Height * 0.5 * This->NormScale;

    float scale = 0.01;
    for (int i = 0; i < 8; i++)
    {
        float angle = edge [i].angle;
        float dist = edge [i].dist;
        double sa = sin (angle);
        double ca = cos (angle);

        // We have to find the radius ru which distorts to given corner.
        // We will use Newton's method for this, we have to find the
        // root of the equation: distance (distorted (x,y)) - dist = 0
        float ru = dist; // Initial approximation
        float dx = 0.0001;
        for (int countdown = 50; ; countdown--)
        {
            float res [2];

            res [0] = ca * ru; res [1] = sa * ru;
            for (int j = 0; j < (int)This->CoordCallbacks->len; j++)
            {
                lfCoordCallbackData *cd =
                    (lfCoordCallbackData *)g_ptr_array_index (This->CoordCallbacks, j);
                cd->callback (cd->data, res, 1);
            }
            double rd = sqrt (res [0] * res [0] + res [1] * res [1]) - dist;
            if (rd > -NEWTON_EPS && rd < NEWTON_EPS)
                break;

            if (!countdown)
                // e.g. for some ultrawide fisheyes corners extend to infinity
                // so function never converge ...
                goto skip_edge;

            // Compute approximative function prime in (x,y)
            res [0] = ca * (ru + dx); res [1] = sa * (ru + dx);
            for (int j = 0; j < (int)This->CoordCallbacks->len; j++)
            {
                lfCoordCallbackData *cd =
                    (lfCoordCallbackData *)g_ptr_array_index (This->CoordCallbacks, j);
                cd->callback (cd->data, res, 1);
            }
            double rd1 = sqrt (res [0] * res [0] + res [1] * res [1]) - dist;

            // If rd1 is very close to rd, this means our delta is too small
            // and we can hit the precision limit of the float format...
            if (fabs (rd1 - rd) < 0.00001)
            {
                dx *= 2;
                continue;
            }

            // dy/dx;
            double prime = (rd1 - rd) / dx;

            ru -= rd / prime;
        }

        dist /= ru;
        if (dist > scale)
            scale = dist;
    skip_edge:
        ;
    }

    return reverse ? 1.0 / scale : scale;
}

bool lfModifier::AddCoordCallbackScale (float scale, bool reverse)
{
    float tmp [1];

    // Inverse scale factor
    if (scale == 0.0)
    {
        scale = GetAutoScale (reverse);
        if (scale == 0.0)
            return false;
    }

    tmp [0] = reverse ? scale : 1.0 / scale;
    AddCoordCallback (lfExtModifier::ModifyCoord_Scale, 100, tmp, sizeof (tmp));
    return true;
}

bool lfModifier::AddCoordCallbackGeometry (lfLensType from, lfLensType to, float focal)
{
    float tmp [2];
    // The magic "12" is half of 36mm frame height
    // (which is 1.0 in our normalized coordinates)
    tmp [0] = focal / 12.0;
    tmp [1] = 1.0 / tmp [0];

    switch (from)
    {
        case LF_UNKNOWN:
            // Should never happen
            return false;

        case LF_RECTILINEAR:
            switch (to)
            {
                case LF_UNKNOWN:
                    // Should never happen
                    return false;
                case LF_RECTILINEAR:
                    // Nothing to do
                    return true;
                case LF_FISHEYE:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_Rect_FishEye,
                                      500, tmp, sizeof (tmp));
                    return true;
                case LF_PANORAMIC:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_Rect_Panoramic,
                                      500, tmp, sizeof (tmp));
                    return true;
                case LF_EQUIRECTANGULAR:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_Rect_ERect,
                                      500, tmp, sizeof (tmp));
                    return true;
            }
            break;

        case LF_FISHEYE:
            switch (to)
            {
                case LF_UNKNOWN:
                    // Should never happen
                    return false;
                case LF_RECTILINEAR:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_FishEye_Rect,
                                      500, tmp, sizeof (tmp));
                    return true;
                case LF_FISHEYE:
                    // Nothing to do
                    return true;
                case LF_PANORAMIC:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_FishEye_Panoramic,
                                      500, tmp, sizeof (tmp));
                    return true;
                case LF_EQUIRECTANGULAR:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_FishEye_ERect,
                                      500, tmp, sizeof (tmp));
                    return true;
            }
            break;

        case LF_PANORAMIC:
            switch (to)
            {
                case LF_UNKNOWN:
                    // Should never happen
                    return false;
                case LF_RECTILINEAR:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_Panoramic_Rect,
                                      500, tmp, sizeof (tmp));
                    return true;
                case LF_FISHEYE:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_Panoramic_FishEye,
                                      500, tmp, sizeof (tmp));
                    return true;
                case LF_PANORAMIC:
                    // Nothing to do
                    return true;
                case LF_EQUIRECTANGULAR:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_Panoramic_ERect,
                                      500, tmp, sizeof (tmp));
                    return true;
            }
            break;

        case LF_EQUIRECTANGULAR:
            switch (to)
            {
                case LF_UNKNOWN:
                    // Should never happen
                    return false;
                case LF_RECTILINEAR:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_ERect_Rect,
                                      500, tmp, sizeof (tmp));
                    return true;
                case LF_FISHEYE:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_ERect_FishEye,
                                      500, tmp, sizeof (tmp));
                    return true;
                case LF_PANORAMIC:
                    AddCoordCallback (lfExtModifier::ModifyCoord_Geom_ERect_Panoramic,
                                      500, tmp, sizeof (tmp));
                    return true;
                case LF_EQUIRECTANGULAR:
                    // Nothing to do
                    return true;
            }
            break;
    }

    return false;
}

bool lfModifier::ApplyGeometryDistortion (
    float xu, float yu, int width, int height, float *res) const
{
    const lfExtModifier *This = static_cast<const lfExtModifier *> (this);

    if (This->CoordCallbacks->len <= 0)
        return false; // nothing to do

    // All callbacks work with normalized coordinates
    xu = xu * This->NormScale - This->CenterX;
    yu = yu * This->NormScale - This->CenterY;
    float y_end = yu + height * This->NormScale;

    for (float y = yu; y < y_end; y += This->NormScale)
    {
        int i;
        float x = xu;
        for (i = 0; i < width; i++, x += This->NormScale)
        {
            res [i * 2] = x;
            res [i * 2 + 1] = y;
        }

        for (i = 0; i < (int)This->CoordCallbacks->len; i++)
        {
            lfCoordCallbackData *cd =
                (lfCoordCallbackData *)g_ptr_array_index (This->CoordCallbacks, i);
            cd->callback (cd->data, res, width);
        }

        // Convert normalized coordinates back into natural coordiates
        for (i = 0; i < width; i++)
        {
            res [0] = (res [0] + This->CenterX) * This->NormUnScale;
            res [1] = (res [1] + This->CenterY) * This->NormUnScale;
            res += 2;
        }
    }

    return true;
}

void lfExtModifier::ModifyCoord_Scale (void *data, float *iocoord, int count)
{
    float scale = *(float *)data;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        iocoord [0] *= scale;
        iocoord [1] *= scale;
    }
}

void lfExtModifier::ModifyCoord_Dist_Poly3 (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];
        double ru = sqrt (x * x + y * y);
        if (ru == 0.0)
            continue;

#if 0
        // Works for positive k1 only, for negative it creates complex numbers
        float b = *(float *)data;
        float c = -b * ru;

        float _u = sqrt (c * c / 4.0 + b * b * b / 27.0);
        float u = _lf_rt3 (c / 2.0 + _u);
        if (u == 0.0)
            u = _lf_rt3 (c / 2.0 - _u);

        float rd = b / (3.0 * u) - u;
#else
        float c = *(float *)data;
        float d = -c * ru;

        // Use Newton's method to avoid dealing with complex numbers
        // When carefully tuned this works almost as fast as Cardano's
        // method (and we don't use complex numbers in it, which is
        // required for a full solution!)
        double rd = ru;
        for (int step = 0; ; step++)
        {
            double frd = rd * rd * rd + c * rd + d;
            if (frd >= -NEWTON_EPS && frd < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_pixel;

            rd -= frd / (3 * rd * rd + c);
        }
        if (rd < 0.0)
            continue; // Negative radius does not make sense at all
#endif

        rd /= ru;
        iocoord [0] = x * rd;
        iocoord [1] = y * rd;

    next_pixel:
        ;
    }
}

void lfExtModifier::ModifyCoord_UnDist_Poly3 (void *data, float *iocoord, int count)
{
    // Ru = Rd * (1 + k1 * Rd^2)
    const float k1 = *(float *)data;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        float r2 = x * x + y * y;

        iocoord [0] = x * (1.0 + k1 * r2);
        iocoord [1] = y * (1.0 + k1 * r2);
    }
}

void lfExtModifier::ModifyCoord_Dist_Poly5 (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    float k1 = param [0];
    float k2 = param [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];
        double ru = sqrt (x * x + y * y);
        if (ru == 0.0)
            continue;

        // Use Newton's method
        double rd = ru;
        for (int step = 0; ; step++)
        {
            double rd2 = rd * rd;
            double frd = rd * (1.0 + k1 * rd2 + k2 * rd2 * rd2) - ru;
            if (frd >= -NEWTON_EPS && frd < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_pixel;

            rd -= frd / (1.0 + 3 * k1 * rd2 + 5 * k2 * rd2 * rd2);
        }
        if (rd < 0.0)
            continue; // Negative radius does not make sense at all

        rd /= ru;
        iocoord [0] = x * rd;
        iocoord [1] = y * rd;

    next_pixel:
        ;
    }
}

void lfExtModifier::ModifyCoord_UnDist_Poly5 (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    // Ru = Rd * (1 + k1 * Rd^2 + k2 * Rd^4)
    const float k1 = param [0];
    const float k2 = param [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        float r2 = x * x + y * y;
        float r4 = r2 * r2;

        iocoord [0] = x * (1.0 + k1 * r2 + k2 * r4);
        iocoord [1] = y * (1.0 + k1 * r2 + k2 * r4);
    }
}

void lfExtModifier::ModifyCoord_Dist_FOV1 (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    // Rd = (1 / omega) * arctan (2 * Ru * tan (omega / 2))
    float c1 = param [0];
    float c2 = param [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];

        iocoord [0] = c1 * atan (x * c2);
        iocoord [1] = c1 * atan (y * c2);
    }
}

void lfExtModifier::ModifyCoord_UnDist_FOV1 (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    // Ru = tg (Rd * omega) / (2 * tg (omega/2))
    float omega = param [0];
    float dconst = param [1];

    if (omega > 0.001)
        for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
        {
            const float x = iocoord [0];
            const float y = iocoord [1];

            iocoord [0] = tan (x * omega) * dconst;
            iocoord [1] = tan (y * omega) * dconst;
        }
}

void lfExtModifier::ModifyCoord_Dist_PTLens (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    float a = param [0];
    float b = param [1];
    float c = param [2];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];
        double ru = sqrt (x * x + y * y);
        if (ru == 0.0)
            continue;

        // Use Newton's method
        double rd = ru;
        for (int step = 0; ; step++)
        {
            double frd = rd * (a * rd * rd * rd + b * rd * rd + c * rd + 1.0) - ru;
            if (frd >= -NEWTON_EPS && frd < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_pixel;

            rd -= frd / (4 * a * rd * rd * rd + 3 * b * rd * rd + 2 * c * rd + 1.0);
        }
        if (rd < 0.0)
            continue; // Negative radius does not make sense at all

        rd /= ru;
        iocoord [0] = x * rd;
        iocoord [1] = y * rd;

    next_pixel:
        ;
    }
}

void lfExtModifier::ModifyCoord_UnDist_PTLens (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    // Ru = Rd * (a * Rd^3 + b * Rd^2 + c * Rd + 1)
    const float a = param [0];
    const float b = param [1];
    const float c = param [2];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const float r2 = x * x + y * y;
        const float r = sqrt (r2);

        iocoord [0] = x * (a * r2 * r + b * r2 + c * r + 1);
        iocoord [1] = y * (a * r2 * r + b * r2 + c * r + 1);
    }
}

void lfExtModifier::ModifyCoord_Geom_Rect_FishEye (void *data, float *iocoord, int count)
{
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        float r = sqrt (x * x + y * y);
        float rho, theta = r * inv_dist;

        if (theta >= M_PI / 2.0)
            rho = 1.6e16;
        else if (theta == 0.0)
            rho = 1.0;
        else
            rho = tan (theta) / theta;

        iocoord [0] = rho * x;
        iocoord [1] = rho * y;
    }
}

void lfExtModifier::ModifyCoord_Geom_FishEye_Rect (void *data, float *iocoord, int count)
{
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        float theta, r = sqrt (x * x + y * y) * inv_dist;
        if (r == 0.0)
            theta = 1.0;
        else
            theta = atan (r) / r;

        iocoord [0] = theta * x;
        iocoord [1] = theta * y;
    }
}

void lfExtModifier::ModifyCoord_Geom_Rect_Panoramic (
    void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0] * inv_dist;
        float y = iocoord [1];

        iocoord [0] = dist * tan (x);
        iocoord [1] = y / cos (x);
    }
}


void lfExtModifier::ModifyCoord_Geom_Panoramic_Rect (
    void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        iocoord [0] = dist * atan (x * inv_dist);
        iocoord [1] = y * cos (iocoord [0] * inv_dist);
    }
}

void lfExtModifier::ModifyCoord_Geom_Panoramic_FishEye (
    void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double r = sqrt (x * x + y * y);
        double theta = r * inv_dist;
        double s = (theta == 0.0) ? inv_dist : (sin (theta) / r);

        double vx = cos (theta);  //  z' -> x
        double vy = s * x;        //  x' -> y

        iocoord [0] = dist * atan2 (vy, vx);
        iocoord [1] = dist * s * y / sqrt (vx * vx + vy * vy);
    }
}

void lfExtModifier::ModifyCoord_Geom_FishEye_Panoramic (
    void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double phi = x * inv_dist;
        double s = dist * sin (phi);   // y' -> x
        double r = sqrt (s * s + y * y);
        double theta = dist * atan2 (r, dist * cos (phi)) / r;

        iocoord [0] = theta * s;
        iocoord [1] = theta * y;
    }
}

void lfExtModifier::ModifyCoord_Geom_Rect_ERect (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double phi = x * inv_dist;
        double theta = -y * inv_dist + M_PI / 2.0;
        if (theta < 0)
        {
            theta = -theta;
            phi += M_PI;
        }
        if (theta > M_PI)
        {
            theta = 2 * M_PI - theta;
            phi += M_PI;
        }

        iocoord [0] = dist * tan (phi);
        iocoord [1] = dist / (tan (theta) * cos (phi));
    }
}

void lfExtModifier::ModifyCoord_Geom_ERect_Rect (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        iocoord [0] = dist * atan2 (x, dist);
        iocoord [1] = dist * atan2 (y, sqrt (dist * dist + x * x));
    }
}

void lfExtModifier::ModifyCoord_Geom_FishEye_ERect (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double phi = x * inv_dist;
        double theta = -y * inv_dist + M_PI / 2;
        if (theta < 0)
        {
            theta = -theta;
            phi += M_PI;
        }
        if (theta > M_PI)
        {
            theta = 2 * M_PI - theta;
            phi += M_PI;
        }

        double s = sin (theta);
        double vx = s * sin (phi); //  y' -> x
        double vy = cos (theta);   //  z' -> y

        double r = sqrt (vx * vx + vy * vy);

        theta = dist * atan2 (r, s * cos (phi));

        r = 1.0 / r;
        iocoord [0] = theta * vx * r;
        iocoord [1] = theta * vy * r;
    }
}

void lfExtModifier::ModifyCoord_Geom_ERect_FishEye (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double r = sqrt (x * x + y * y);
        double theta = r * inv_dist;
        double s = (theta == 0.0) ? inv_dist : (sin (theta) / r);

        double vx = cos (theta);
        double vy = s * x;

        iocoord [0] = dist * atan2 (vy, vx);
        iocoord [1] = dist * atan (s * y / sqrt (vx * vx + vy * vy));
    }
}

void lfExtModifier::ModifyCoord_Geom_Panoramic_ERect (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float y = iocoord [1];
        iocoord [1] = dist * tan (y * inv_dist);
    }
}

void lfExtModifier::ModifyCoord_Geom_ERect_Panoramic (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float y = iocoord [1];
        iocoord [1] = dist * atan (y * inv_dist);
    }
}

//---------------------------// The C interface //---------------------------//

void lf_modifier_add_coord_callback (
    lfModifier *modifier, lfModifyCoordFunc callback, int priority,
    void *data, size_t data_size)
{
    modifier->AddCoordCallback (callback, priority, data, data_size);
}

float lf_modifier_get_auto_scale (lfModifier *modifier, cbool reverse)
{
    return modifier->GetAutoScale (reverse);
}

cbool lf_modifier_apply_geometry_distortion (
    lfModifier *modifier, float xu, float yu, int width, int height, float *res)
{
    return modifier->ApplyGeometryDistortion (xu, yu, width, height, res);
}
