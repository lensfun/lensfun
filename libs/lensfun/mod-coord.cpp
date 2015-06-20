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
#include "windows/mathconstants.h"

void lfModifier::AddCoordCallback (
    lfModifyCoordFunc callback, int priority, void *data, size_t data_size)
{
    lfCoordCallbackData *d = new lfCoordCallbackData ();
    d->callback = callback;
    AddCallback (CoordCallbacks, d, priority, data, data_size);
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
                AddCoordCallback (ModifyCoord_UnDist_Poly3, 250,
                                  tmp, sizeof (float));
                break;

            case LF_DIST_MODEL_POLY5:
                AddCoordCallback (ModifyCoord_UnDist_Poly5, 250,
                                  model.Terms, sizeof (float) * 2);
                break;

            case LF_DIST_MODEL_PTLENS:
#ifdef VECTORIZATION_SSE
                if (_lf_detect_cpu_features () & LF_CPU_FLAG_SSE)
                    AddCoordCallback (ModifyCoord_UnDist_PTLens_SSE, 250,
                                      model.Terms, sizeof (float) * 3);
                else
#endif
                AddCoordCallback (ModifyCoord_UnDist_PTLens, 250,
                                  model.Terms, sizeof (float) * 3);
                break;

            default:
                return false;
        }
    else
        switch (model.Model)
        {
            case LF_DIST_MODEL_POLY3:
#ifdef VECTORIZATION_SSE
                if (_lf_detect_cpu_features () & LF_CPU_FLAG_SSE)
                    AddCoordCallback (ModifyCoord_Dist_Poly3_SSE, 750,
                                      model.Terms, sizeof (float));
                else
#endif
                AddCoordCallback (ModifyCoord_Dist_Poly3, 750,
                                  model.Terms, sizeof (float));
                break;

            case LF_DIST_MODEL_POLY5:
                AddCoordCallback (ModifyCoord_Dist_Poly5, 750,
                                  model.Terms, sizeof (float) * 2);
                break;

            case LF_DIST_MODEL_PTLENS:
#ifdef VECTORIZATION_SSE
                if (_lf_detect_cpu_features () & LF_CPU_FLAG_SSE)
                    AddCoordCallback (ModifyCoord_Dist_PTLens_SSE, 750,
                                      model.Terms, sizeof (float) * 3);
                else
#endif
                AddCoordCallback (ModifyCoord_Dist_PTLens, 750,
                                  model.Terms, sizeof (float) * 3);
                break;

            default:
                return false;
        }

    return true;
}

double lfModifier::AutoscaleResidualDistance (float *coord) const
{
    double result = coord [0] - MaxX;
    double intermediate = -MaxX - coord [0];
    if (intermediate > result) result = intermediate;
    intermediate = coord [1] - MaxY;
    if (intermediate > result) result = intermediate;
    intermediate = -MaxY - coord [1];
    return intermediate > result ? intermediate : result;
}

float lfModifier::GetTransformedDistance (lfPoint point) const
{
    double dist = point.dist;
    double sa = sin (point.angle);
    double ca = cos (point.angle);

    // We have to find the radius ru in the direction of the given point which
    // distorts to the original (distorted) image edge.  We will use Newton's
    // method for minimizing the distance between the distorted point at ru and
    // the original edge.
    float ru = dist; // Initial approximation
    float dx = 0.0001F;
    for (int countdown = 50; ; countdown--)
    {
        float res [2];

        res [0] = ca * ru; res [1] = sa * ru;
        for (int j = 0; j < (int)((GPtrArray *)CoordCallbacks)->len; j++)
        {
            lfCoordCallbackData *cd =
                (lfCoordCallbackData *)g_ptr_array_index ((GPtrArray *)CoordCallbacks, j);
            cd->callback (cd->data, res, 1);
        }
        double rd = AutoscaleResidualDistance (res);
        if (rd > -NEWTON_EPS * 100 && rd < NEWTON_EPS * 100)
            break;

        if (!countdown)
            // e.g. for some ultrawide fisheyes corners extend to infinity
            // so function never converge ...
            return -1;

        // Compute approximative function prime in (x,y)
        res [0] = ca * (ru + dx); res [1] = sa * (ru + dx);
        for (int j = 0; j < (int)((GPtrArray *)CoordCallbacks)->len; j++)
        {
            lfCoordCallbackData *cd =
                (lfCoordCallbackData *)g_ptr_array_index ((GPtrArray *)CoordCallbacks, j);
            cd->callback (cd->data, res, 1);
        }
        double rd1 = AutoscaleResidualDistance (res);

        // If rd1 is very close to rd, this means our delta is too small
        // and we can hit the precision limit of the float format...
        if (absolute (rd1 - rd) < 0.00001)
        {
            dx *= 2;
            continue;
        }

        // dy/dx;
        double prime = (rd1 - rd) / dx;

        ru -= rd / prime;
    }

    return ru;
}

float lfModifier::GetAutoScale (bool reverse)
{
    // Compute the scale factor automatically
    const float subpixel_scale = ((GPtrArray *)SubpixelCallbacks)->len == 0 ? 1.0 : 1.001;

    if (((GPtrArray *)CoordCallbacks)->len == 0)
        return subpixel_scale;

    // 3 2 1
    // 4   0
    // 5 6 7
    lfPoint point [8];

    point [1].angle = atan2 (float (Height), float (Width));
    point [3].angle = M_PI - point [1].angle;
    point [5].angle = M_PI + point [1].angle;
    point [7].angle = 2 * M_PI - point [1].angle;

    point [0].angle = 0.0F;
    point [2].angle = float (M_PI / 2.0);
    point [4].angle = float (M_PI);
    point [6].angle = float (M_PI * 3.0 / 2.0);

    point [1].dist = point [3].dist = point [5].dist = point [7].dist =
        sqrt (float (square (Width) + square (Height))) * 0.5 * NormScale;
    point [0].dist = point [4].dist = Width * 0.5 * NormScale;
    point [2].dist = point [6].dist = Height * 0.5 * NormScale;

    float scale = 0.01F;
    for (int i = 0; i < 8; i++)
    {
        float transformed_distance = GetTransformedDistance (point [i]);
        float point_scale = point [i].dist / transformed_distance;
        if (point_scale > scale)
            scale = point_scale;
    }
    // 1 permille is our limit of accuracy (in rare cases, we may be even
    // worse, depending on what happens between the test points), so assure
    // that we really have no black borders left.
    scale *= 1.001;
    scale *= subpixel_scale;

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
    int priority = reverse ? 900 : 100;
    AddCoordCallback (ModifyCoord_Scale, priority, tmp, sizeof (tmp));
    return true;
}

bool lfModifier::AddCoordCallbackGeometry (lfLensType from, lfLensType to, float focal)
{
    float tmp [2];
    tmp [0] = focal / NormalizedInMillimeters;
    tmp [1] = 1.0 / tmp [0];

    if(from == to)
        return false;
    if(from == LF_UNKNOWN)
        return false;
    if(to == LF_UNKNOWN)
        return false;
    // handle special cases
    switch (from)
    {
        case LF_RECTILINEAR:
            switch (to)
            {
                case LF_FISHEYE:
                    AddCoordCallback (ModifyCoord_Geom_FishEye_Rect,
                                      500, tmp, sizeof (tmp));
                    return true;

                case LF_PANORAMIC:
                    AddCoordCallback (ModifyCoord_Geom_Panoramic_Rect,
                                      500, tmp, sizeof (tmp));
                    return true;

                case LF_EQUIRECTANGULAR:
                    AddCoordCallback (ModifyCoord_Geom_ERect_Rect,
                                      500, tmp, sizeof (tmp));
                    return true;

                default:
                    // keep gcc 4.4+ happy
                    break;
            }
            break;

        case LF_FISHEYE:
            switch (to)
            {
                case LF_RECTILINEAR:
                    AddCoordCallback (ModifyCoord_Geom_Rect_FishEye,
                                      500, tmp, sizeof (tmp));
                    return true;

                case LF_PANORAMIC:
                    AddCoordCallback (ModifyCoord_Geom_Panoramic_FishEye,
                                      500, tmp, sizeof (tmp));
                    return true;

                case LF_EQUIRECTANGULAR:
                    AddCoordCallback (ModifyCoord_Geom_ERect_FishEye,
                                      500, tmp, sizeof (tmp));
                    return true;

                default:
                    // keep gcc 4.4+ happy
                    break;
            }
            break;

        case LF_PANORAMIC:
            switch (to)
            {
                case LF_RECTILINEAR:
                    AddCoordCallback (ModifyCoord_Geom_Rect_Panoramic,
                                      500, tmp, sizeof (tmp));
                    return true;

                case LF_FISHEYE:
                    AddCoordCallback (ModifyCoord_Geom_FishEye_Panoramic,
                                      500, tmp, sizeof (tmp));
                    return true;

                case LF_EQUIRECTANGULAR:
                    AddCoordCallback (ModifyCoord_Geom_ERect_Panoramic,
                                      500, tmp, sizeof (tmp));
                    return true;

                default:
                    // keep gcc 4.4+ happy
                    break;
            }
            break;

        case LF_EQUIRECTANGULAR:
            switch (to)
            {
                case LF_RECTILINEAR:
                    AddCoordCallback (ModifyCoord_Geom_Rect_ERect,
                                      500, tmp, sizeof (tmp));
                    return true;

                case LF_FISHEYE:
                    AddCoordCallback (ModifyCoord_Geom_FishEye_ERect,
                                      500, tmp, sizeof (tmp));
                    return true;

                case LF_PANORAMIC:
                    AddCoordCallback (ModifyCoord_Geom_Panoramic_ERect,
                                      500, tmp, sizeof (tmp));
                    return true;

                default:
                    // keep gcc 4.4+ happy
                    break;
            }
        case LF_FISHEYE_ORTHOGRAPHIC:
        case LF_FISHEYE_STEREOGRAPHIC:
        case LF_FISHEYE_EQUISOLID:
        case LF_FISHEYE_THOBY:
        case LF_UNKNOWN:
        default:
            break;
    };

    //convert from input projection to target projection via equirectangular projection
    switch(to)
    {
        case LF_RECTILINEAR:
            AddCoordCallback (ModifyCoord_Geom_Rect_ERect,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE:
            AddCoordCallback (ModifyCoord_Geom_FishEye_ERect,
                                500, tmp, sizeof (tmp));
            break;
        case LF_PANORAMIC:
            AddCoordCallback (ModifyCoord_Geom_Panoramic_ERect,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE_ORTHOGRAPHIC:
            AddCoordCallback (ModifyCoord_Geom_Orthographic_ERect,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE_STEREOGRAPHIC:
            AddCoordCallback (ModifyCoord_Geom_Stereographic_ERect,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE_EQUISOLID:
            AddCoordCallback (ModifyCoord_Geom_Equisolid_ERect,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE_THOBY:
            AddCoordCallback (ModifyCoord_Geom_Thoby_ERect,
                                500, tmp, sizeof (tmp));
            break;
        case LF_EQUIRECTANGULAR:
        default:
            //nothing to do
            break;
    };
    switch(from)
    {
        case LF_RECTILINEAR:
            AddCoordCallback (ModifyCoord_Geom_ERect_Rect,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE:
            AddCoordCallback (ModifyCoord_Geom_ERect_FishEye,
                                500, tmp, sizeof (tmp));
            break;
        case LF_PANORAMIC:
            AddCoordCallback (ModifyCoord_Geom_ERect_Panoramic,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE_ORTHOGRAPHIC:
            AddCoordCallback (ModifyCoord_Geom_ERect_Orthographic,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE_STEREOGRAPHIC:
            AddCoordCallback (ModifyCoord_Geom_ERect_Stereographic,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE_EQUISOLID:
            AddCoordCallback (ModifyCoord_Geom_ERect_Equisolid,
                                500, tmp, sizeof (tmp));
            break;
        case LF_FISHEYE_THOBY:
            AddCoordCallback (ModifyCoord_Geom_ERect_Thoby,
                                500, tmp, sizeof (tmp));
            break;
        case LF_EQUIRECTANGULAR:
        default:
            //nothing to do
            break;
    };
    return true;
}

bool lfModifier::ApplyGeometryDistortion (
    float xu, float yu, int width, int height, float *res) const
{
    if (((GPtrArray *)CoordCallbacks)->len <= 0 || height <= 0)
        return false; // nothing to do

    // All callbacks work with normalized coordinates
    xu = xu * NormScale - CenterX;
    yu = yu * NormScale - CenterY;

    for (float y = yu; height; y += NormScale, height--)
    {
        int i;
        float x = xu;
        for (i = 0; i < width; i++, x += NormScale)
        {
            res [i * 2] = x;
            res [i * 2 + 1] = y;
        }

        for (i = 0; i < (int)((GPtrArray *)CoordCallbacks)->len; i++)
        {
            lfCoordCallbackData *cd =
                (lfCoordCallbackData *)g_ptr_array_index ((GPtrArray *)CoordCallbacks, i);
            cd->callback (cd->data, res, width);
        }

        // Convert normalized coordinates back into natural coordiates
        for (i = 0; i < width; i++)
        {
            res [0] = (res [0] + CenterX) * NormUnScale;
            res [1] = (res [1] + CenterY) * NormUnScale;
            res += 2;
        }
    }

    return true;
}

void lfModifier::ModifyCoord_Scale (void *data, float *iocoord, int count)
{
    float scale = *(float *)data;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        iocoord [0] *= scale;
        iocoord [1] *= scale;
    }
}

void lfModifier::ModifyCoord_UnDist_Poly3 (void *data, float *iocoord, int count)
{
    const float inv_k1 = *(float *)data;
    const float one_minus_k1_div_k1 = (1 - 1.0 / inv_k1) * inv_k1;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];
        double rd = sqrt (x * x + y * y);
        if (rd == 0.0)
            continue;

        float rd_div_k1 = rd * inv_k1;

        // Use Newton's method to avoid dealing with complex numbers
        // When carefully tuned this works almost as fast as Cardano's
        // method (and we don't use complex numbers in it, which is
        // required for a full solution!)
        //
        // Original function: Rd = k1 * Ru^3 + (1 - k1) * Ru
        // Target function:   k1 * Ru^3 + (1 - k1) * Ru - Rd = 0
        // Divide by k1:      Ru^3 + Ru * (1 - k1)/k1 - Rd/k1 = 0
        // Derivative:        3 * Ru^2 + (1 - k1)/k1
        double ru = rd;
        for (int step = 0; ; step++)
        {
            double fru = ru * ru * ru + ru * one_minus_k1_div_k1 - rd_div_k1;
            if (fru >= -NEWTON_EPS && fru < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_pixel;

            ru -= fru / (3 * ru * ru + one_minus_k1_div_k1);
        }
        if (ru < 0.0)
            continue; // Negative radius does not make sense at all

        ru /= rd;
        iocoord [0] = x * ru;
        iocoord [1] = y * ru;

    next_pixel:
        ;
    }
}

void lfModifier::ModifyCoord_Dist_Poly3 (void *data, float *iocoord, int count)
{
    // Rd = Ru * (1 - k1 + k1 * Ru^2)
    const float k1 = *(float *)data;
    const float one_minus_k1 = 1.0 - k1;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const float poly2 = one_minus_k1 + k1 * (x * x + y * y);

        iocoord [0] = x * poly2;
        iocoord [1] = y * poly2;
    }
}

void lfModifier::ModifyCoord_UnDist_Poly5 (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    float k1 = param [0];
    float k2 = param [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];
        double rd = sqrt (x * x + y * y);
        if (rd == 0.0)
            continue;

        // Use Newton's method
        double ru = rd;
        for (int step = 0; ; step++)
        {
            double ru2 = ru * ru;
            double fru = ru * (1.0 + k1 * ru2 + k2 * ru2 * ru2) - rd;
            if (fru >= -NEWTON_EPS && fru < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_pixel;

            ru -= fru / (1.0 + 3 * k1 * ru2 + 5 * k2 * ru2 * ru2);
        }
        if (ru < 0.0)
            continue; // Negative radius does not make sense at all

        ru /= rd;
        iocoord [0] = x * ru;
        iocoord [1] = y * ru;

    next_pixel:
        ;
    }
}

void lfModifier::ModifyCoord_Dist_Poly5 (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    // Rd = Ru * (1 + k1 * Ru^2 + k2 * Ru^4)
    const float k1 = param [0];
    const float k2 = param [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const float ru2 = x * x + y * y;
        const float poly4 = (1.0 + k1 * ru2 + k2 * ru2 * ru2);

        iocoord [0] = x * poly4;
        iocoord [1] = y * poly4;
    }
}

void lfModifier::ModifyCoord_UnDist_PTLens (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    float a = param [0];
    float b = param [1];
    float c = param [2];
    float d = 1.0 - a - b - c;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];
        double rd = sqrt (x * x + y * y);
        if (rd == 0.0)
            continue;

        // Use Newton's method
        double ru = rd;
        for (int step = 0; ; step++)
        {
            double fru = ru * (a * ru * ru * ru + b * ru * ru + c * ru + d) - rd;
            if (fru >= -NEWTON_EPS && fru < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_pixel;

            ru -= fru / (4 * a * ru * ru * ru + 3 * b * ru * ru + 2 * c * ru + d);
        }
        if (ru < 0.0)
            continue; // Negative radius does not make sense at all

        ru /= rd;
        iocoord [0] = x * ru;
        iocoord [1] = y * ru;

    next_pixel:
        ;
    }
}

void lfModifier::ModifyCoord_Dist_PTLens (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    // Rd = Ru * (a * Ru^3 + b * Ru^2 + c * Ru + d)
    const float a = param [0];
    const float b = param [1];
    const float c = param [2];
    const float d = 1.0 - a - b - c;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const float ru2 = x * x + y * y;
        const float r = sqrtf (ru2);
        const float poly3 = a * ru2 * r + b * ru2 + c * r + d;

        iocoord [0] = x * poly3;
        iocoord [1] = y * poly3;
    }
}

void lfModifier::ModifyCoord_Geom_FishEye_Rect (void *data, float *iocoord, int count)
{
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        float r = sqrt (x * x + y * y);
        float rho, theta = r * inv_dist;

        if (theta >= M_PI / 2.0)
            rho = 1.6e16F;
        else if (theta == 0.0)
            rho = 1.0;
        else
            rho = tan (theta) / theta;

        iocoord [0] = rho * x;
        iocoord [1] = rho * y;
    }
}

void lfModifier::ModifyCoord_Geom_Rect_FishEye (void *data, float *iocoord, int count)
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

void lfModifier::ModifyCoord_Geom_Panoramic_Rect (
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


void lfModifier::ModifyCoord_Geom_Rect_Panoramic (
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

void lfModifier::ModifyCoord_Geom_FishEye_Panoramic (
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

void lfModifier::ModifyCoord_Geom_Panoramic_FishEye (
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
        double theta = 0.0;

        if (r==0.0)
            theta = 0.0;
        else
            theta = dist * atan2 (r, dist * cos (phi)) / r;

        iocoord [0] = theta * s;
        iocoord [1] = theta * y;
    }
}

void lfModifier::ModifyCoord_Geom_ERect_Rect (void *data, float *iocoord, int count)
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

void lfModifier::ModifyCoord_Geom_Rect_ERect (void *data, float *iocoord, int count)
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

void lfModifier::ModifyCoord_Geom_ERect_FishEye (void *data, float *iocoord, int count)
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

void lfModifier::ModifyCoord_Geom_FishEye_ERect (void *data, float *iocoord, int count)
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

void lfModifier::ModifyCoord_Geom_ERect_Panoramic (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float y = iocoord [1];
        iocoord [1] = dist * tan (y * inv_dist);
    }
}

void lfModifier::ModifyCoord_Geom_Panoramic_ERect (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float y = iocoord [1];
        iocoord [1] = dist * atan (y * inv_dist);
    }
}

void lfModifier::ModifyCoord_Geom_Orthographic_ERect (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double r     = sqrt(x * x + y * y);
        double theta = 0.0;

        if (r < dist)
            theta = asin (r * inv_dist);
        else
            theta = M_PI / 2.0;

        double phi   = atan2 (y, x);

        double s = (theta == 0.0) ? inv_dist : (sin (theta) / (theta * dist) );

        double vx = cos (theta);
        double vy = s * dist * theta * cos (phi);

        iocoord [0] = dist * atan2 (vy, vx);
        iocoord [1] = dist * atan (s * dist * theta * sin (phi) / sqrt (vx * vx + vy * vy));
    }
};

void lfModifier::ModifyCoord_Geom_ERect_Orthographic (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double phi   = x * inv_dist;
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

        double s  = sin (theta);
        double vx = s * sin (phi); //  y' -> x
        double vy = cos (theta);   //  z' -> y

        theta = atan2 (sqrt (vx * vx + vy * vy), s * cos (phi));
        phi   = atan2 (vy, vx);
        double rho  = dist * sin (theta);
        iocoord [0] = rho * cos (phi);
        iocoord [1] = rho * sin (phi);
     }
};

#define EPSLN   1.0e-10

void lfModifier::ModifyCoord_Geom_Stereographic_ERect (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0] * inv_dist;
        float y = iocoord [1] * inv_dist;

        double rh = sqrt (x * x + y * y);
        double c  = 2.0 * atan (rh / 2.0);
        double sinc = sin (c);
        double cosc = cos (c);

        iocoord [0] = 0;
        if(fabs (rh) <= EPSLN)
        {
            iocoord [1] = 1.6e16F;
        }
        else
        {
            iocoord [1] = asin (y * sinc / rh) * dist;
            if((fabs (cosc) >= EPSLN) || (fabs (x) >= EPSLN))
            {
                iocoord [0] = atan2 (x * sinc, cosc * rh) * dist;
            }
            else
            {
                iocoord [0] = 1.6e16F;
            };
        };
    };
};

void lfModifier::ModifyCoord_Geom_ERect_Stereographic (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float lon = iocoord [0] * inv_dist;
        float lat = iocoord [1] * inv_dist;

        double cosphi = cos (lat);
        double ksp = dist * 2.0 / (1.0 + cosphi * cos (lon));

        iocoord [0] = ksp * cosphi * sin (lon);
        iocoord [1] = ksp * sin (lat);
    }
};

void lfModifier::ModifyCoord_Geom_Equisolid_ERect (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double r = sqrt (x * x + y * y);
        double theta = 0.0;

        if (r < dist*2.0)
            theta = 2.0 * asin (r * inv_dist / 2.0);
        else
            theta = M_PI / 2.0;

        double phi = atan2 (y, x);
        double s = (theta == 0.0) ? inv_dist : (sin (theta) / (dist * theta));

        double vx = cos (theta);
        double vy = s * dist * theta * cos (phi);

        iocoord [0] = dist * atan2 (vy, vx);
        iocoord [1] = dist * atan (s * dist * theta * sin (phi) / sqrt (vx * vx + vy * vy));
    };
};

void lfModifier::ModifyCoord_Geom_ERect_Equisolid (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        double lambda = iocoord [0] / dist;
        double phi = iocoord [1] / dist;

        if (fabs (cos(phi) * cos(lambda) + 1.0) <= EPSLN)
        {
            iocoord [0] = 1.6e16F;
            iocoord [1] = 1.6e16F;
        }
        else
        {
            double k1 = sqrt (2.0 / (1 + cos(phi) * cos(lambda)));

            iocoord [0] = dist * k1 * cos (phi) * sin (lambda);
            iocoord [1] = dist * k1 * sin (phi);
        };
    };
};

#define THOBY_K1_PARM 1.47F
#define THOBY_K2_PARM 0.713F

void lfModifier::ModifyCoord_Geom_Thoby_ERect (void *data, float *iocoord, int count)
{
    const float dist = ((float *)data) [0];
    const float inv_dist = ((float *)data) [1];
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double rho = sqrt (x * x + y * y) * inv_dist;
        if(rho<-THOBY_K1_PARM || rho > THOBY_K1_PARM)
        {
            iocoord [0] = 1.6e16F;
            iocoord [1] = 1.6e16F;
        }
        else
        {
            double theta = asin (rho / THOBY_K1_PARM) / THOBY_K2_PARM;
            double phi   = atan2 (y, x);
            double s     = (theta == 0.0) ? inv_dist : (sin (theta) / (dist * theta) );

            double vx = cos (theta);
            double vy = s * dist * theta * cos (phi);

            iocoord [0] = dist * atan2 (vy, vx);
            iocoord [1] = dist * atan (s * dist * theta * sin (phi) / sqrt (vx * vx + vy * vy));
        };
    };
};

void lfModifier::ModifyCoord_Geom_ERect_Thoby (void *data, float *iocoord, int count)
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

        double s  = sin (theta);
        double vx = s * sin (phi); //  y' -> x
        double vy = cos (theta);   //  z' -> y
        theta = atan2 (sqrt (vx * vx + vy * vy), s * cos (phi));
        phi   = atan2 (vy, vx);
        double rho = THOBY_K1_PARM * dist * sin (theta * THOBY_K2_PARM);

        iocoord [0] = rho * cos (phi);
        iocoord [1] = rho * sin (phi);
    };
};

//---------------------------// The C interface //---------------------------//

void lf_modifier_add_coord_callback (
    lfModifier *modifier, lfModifyCoordFunc callback, int priority,
    void *data, size_t data_size)
{
    modifier->AddCoordCallback (callback, priority, data, data_size);
}

cbool lf_modifier_add_coord_callback_distortion (
    lfModifier *modifier, lfLensCalibDistortion *model, cbool reverse)
{
    return modifier->AddCoordCallbackDistortion (*model, reverse);
}

cbool lf_modifier_add_coord_callback_geometry (
    lfModifier *modifier, lfLensType from, lfLensType to, float focal)
{
    return modifier->AddCoordCallbackGeometry (from, to, focal);
}

cbool lf_modifier_add_coord_callback_scale (
    lfModifier *modifier, float scale, cbool reverse)
{
    return modifier->AddCoordCallbackScale (scale, reverse);
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
