/*
    Image modifier implementation: (un)distortion functions
    Copyright (C) 2007 by Andrew Zabolotny

    Most of the math in this file has been borrowed from PanoTools.
    Thanks to the PanoTools team for their pioneering work.


    Note about PT-based distortion models: Due to the "d" parameter in the
    PanoTools/Hugin formula for the modelling of distortion, its application
    shrinks the centre of the image by the factor d = (1 - a - b - c).  This
    means a change of the 35mm-equiv. focal length of the image.  Since this
    focal length is needed for proper projection transformation and perspective
    correction, we either have to assume the changed focal length, or we have
    to re-scale the image after distortion correction so that the focal length
    is correct again.

    Lensfun's code takes the latter option: When adding the callback functions,
    the parameters a, b, c are transformed into a' = a/d⁴, b' = b/d³, c' =
    c/d², d' = 1.  (They are named a_, b_, c_ in the code.)  This effectively
    scales the undistorted coordinates by 1/d, enlarging the corrected image by
    d.  This way, the ugliness is isolated in a few lines of code, and Lensfun
    can assume focal-length-preserving transformations all along.

    The same applies to the poly3 model, where a, b, c are 0, k1, 0.
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>
#include "windows/mathconstants.h"

lfLensCalibDistortion rescale_polynomial_coefficients (const lfLensCalibDistortion& lcd_,
                                                       double real_focal, cbool Reverse)
{
    // FixMe: The ACM probably bases on the nominal focal length.  This needs
    // to be found out.  It this is true, we have to scale its coefficient by
    // the ratio of real and nominal focal length.
    lfLensCalibDistortion lcd = lcd_;
    const float hugin_scale_in_millimeters =
        hypot (36.0, 24.0) / lcd.CalibAttr.CropFactor / hypot (lcd.CalibAttr.AspectRatio, 1) / 2.0;
    const float hugin_scaling = real_focal / hugin_scale_in_millimeters;
    switch (lcd.Model)
    {
        case LF_DIST_MODEL_POLY3:
            {
                // See "Note about PT-based distortion models" at the top of
                // this file.
                float d = 1 - lcd.Terms [0];
                if (Reverse)
                {
                    if (lcd.Terms [0] != 0)
                        lcd.Terms [0] = pow (d * hugin_scaling, 3) / lcd.Terms [0];
                }
                else
                    lcd.Terms [0] /= pow (d * hugin_scaling, 3);
            }
            break;
        case LF_DIST_MODEL_POLY5:
            lcd.Terms [0] /= pow (hugin_scaling, 3);
            lcd.Terms [1] /= pow (hugin_scaling, 5);
            break;
        case LF_DIST_MODEL_PTLENS:
        {
            // See "Note about PT-based distortion models" at the top of
            // this file.
            float d = 1 - lcd.Terms [0] - lcd.Terms [1] - lcd.Terms [2];
            lcd.Terms [0] /= pow (d * hugin_scaling, 4);
            lcd.Terms [1] /= pow (d * hugin_scaling, 3);
            lcd.Terms [2] /= pow (d * hugin_scaling, 2);
            break;
        }
    }
    return lcd;
}

int lfModifier::EnableDistortionCorrection (const lfLensCalibDistortion& lcd_)
{
    const lfLensCalibDistortion lcd = rescale_polynomial_coefficients (lcd_, RealFocal, Reverse);
    if (Reverse)
        switch (lcd.Model)
        {
            case LF_DIST_MODEL_POLY3:
                if (lcd.Terms [0] == 0)
                    return EnabledMods;
                AddCoordDistCallback (lcd, ModifyCoord_UnDist_Poly3, 250);
                break;

            case LF_DIST_MODEL_POLY5:
                AddCoordDistCallback (lcd, ModifyCoord_UnDist_Poly5, 250);
                break;

            case LF_DIST_MODEL_PTLENS:
            {
#ifdef VECTORIZATION_SSE
                if (_lf_detect_cpu_features () & LF_CPU_FLAG_SSE)
                    AddCoordDistCallback (lcd, ModifyCoord_UnDist_PTLens_SSE, 250);
                else
#endif
                AddCoordDistCallback (lcd, ModifyCoord_UnDist_PTLens, 250);
                break;
            }
            case LF_DIST_MODEL_ACM:
                g_warning ("[lensfun] \"acm\" distortion model is not yet implemented "
                           "for reverse correction");
                return EnabledMods;

            default:
                return EnabledMods;
        }
    else
        switch (lcd.Model)
        {
            case LF_DIST_MODEL_POLY3:
                {
    #ifdef VECTORIZATION_SSE
                    if (_lf_detect_cpu_features () & LF_CPU_FLAG_SSE)
                        AddCoordDistCallback (lcd, ModifyCoord_Dist_Poly3_SSE, 750);
                    else
    #endif
                    AddCoordDistCallback (lcd, ModifyCoord_Dist_Poly3, 750);
                }
                break;

            case LF_DIST_MODEL_POLY5:
                AddCoordDistCallback (lcd, ModifyCoord_Dist_Poly5, 750);
                break;

            case LF_DIST_MODEL_PTLENS:
            {
                {
    #ifdef VECTORIZATION_SSE
                    if (_lf_detect_cpu_features () & LF_CPU_FLAG_SSE)
                        AddCoordDistCallback (lcd, ModifyCoord_Dist_PTLens_SSE, 750);
                    else
    #endif
                    AddCoordDistCallback (lcd, ModifyCoord_Dist_PTLens, 750);
                }
                break;
            }
            case LF_DIST_MODEL_ACM:
                AddCoordDistCallback (lcd, ModifyCoord_Dist_ACM, 750);
                break;

            default:
                return EnabledMods;
        }

    EnabledMods |= LF_MODIFY_DISTORTION;
    return EnabledMods;
}

int lfModifier::EnableDistortionCorrection ()
{
    lfLensCalibDistortion lcd;
    if (Lens->InterpolateDistortion (Crop, Focal, lcd))
    {
        EnableDistortionCorrection (lcd);
    }

    return EnabledMods;
}

int lfModifier::EnableProjectionTransform (lfLensType target_projection)
{
    if(target_projection == LF_UNKNOWN || target_projection == Lens->Type)
        return EnabledMods;
    if(Lens->Type == LF_UNKNOWN)
        return EnabledMods;

    lfLensType from = Lens->Type;
    lfLensType to = target_projection;

    if (Reverse)
    {
        from = target_projection;
        to = Lens->Type;
    }

    // handle special cases
    switch (from)
    {
        case LF_RECTILINEAR:
            switch (to)
            {
                case LF_FISHEYE:
                    AddCoordGeomCallback (ModifyCoord_Geom_FishEye_Rect, 500);
                    return true;

                case LF_PANORAMIC:
                    AddCoordGeomCallback (ModifyCoord_Geom_Panoramic_Rect, 500);
                    return true;

                case LF_EQUIRECTANGULAR:
                    AddCoordGeomCallback (ModifyCoord_Geom_ERect_Rect, 500);
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
                    AddCoordGeomCallback (ModifyCoord_Geom_Rect_FishEye, 500);
                    return true;

                case LF_PANORAMIC:
                    AddCoordGeomCallback (ModifyCoord_Geom_Panoramic_FishEye, 500);
                    return true;

                case LF_EQUIRECTANGULAR:
                    AddCoordGeomCallback (ModifyCoord_Geom_ERect_FishEye, 500);
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
                    AddCoordGeomCallback (ModifyCoord_Geom_Rect_Panoramic, 500);
                    return true;

                case LF_FISHEYE:
                    AddCoordGeomCallback (ModifyCoord_Geom_FishEye_Panoramic, 500);
                    return true;

                case LF_EQUIRECTANGULAR:
                    AddCoordGeomCallback (ModifyCoord_Geom_ERect_Panoramic, 500);
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
                    AddCoordGeomCallback (ModifyCoord_Geom_Rect_ERect, 500);
                    return true;

                case LF_FISHEYE:
                    AddCoordGeomCallback (ModifyCoord_Geom_FishEye_ERect, 500);
                    return true;

                case LF_PANORAMIC:
                    AddCoordGeomCallback (ModifyCoord_Geom_Panoramic_ERect, 500);
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
            AddCoordGeomCallback (ModifyCoord_Geom_Rect_ERect, 500);
            break;
        case LF_FISHEYE:
            AddCoordGeomCallback (ModifyCoord_Geom_FishEye_ERect, 500);
            break;
        case LF_PANORAMIC:
            AddCoordGeomCallback (ModifyCoord_Geom_Panoramic_ERect, 500);
            break;
        case LF_FISHEYE_ORTHOGRAPHIC:
            AddCoordGeomCallback (ModifyCoord_Geom_Orthographic_ERect, 500);
            break;
        case LF_FISHEYE_STEREOGRAPHIC:
            AddCoordGeomCallback (ModifyCoord_Geom_Stereographic_ERect, 500);
            break;
        case LF_FISHEYE_EQUISOLID:
            AddCoordGeomCallback (ModifyCoord_Geom_Equisolid_ERect, 500);
            break;
        case LF_FISHEYE_THOBY:
            AddCoordGeomCallback (ModifyCoord_Geom_Thoby_ERect, 500);
            break;
        case LF_EQUIRECTANGULAR:
        default:
            //nothing to do
            break;
    };
    switch(from)
    {
        case LF_RECTILINEAR:
            AddCoordGeomCallback (ModifyCoord_Geom_ERect_Rect, 500);
            break;
        case LF_FISHEYE:
            AddCoordGeomCallback (ModifyCoord_Geom_ERect_FishEye, 500);
            break;
        case LF_PANORAMIC:
            AddCoordGeomCallback (ModifyCoord_Geom_ERect_Panoramic, 500);
            break;
        case LF_FISHEYE_ORTHOGRAPHIC:
            AddCoordGeomCallback (ModifyCoord_Geom_ERect_Orthographic, 500);
            break;
        case LF_FISHEYE_STEREOGRAPHIC:
            AddCoordGeomCallback (ModifyCoord_Geom_ERect_Stereographic, 500);
            break;
        case LF_FISHEYE_EQUISOLID:
            AddCoordGeomCallback (ModifyCoord_Geom_ERect_Equisolid, 500);
            break;
        case LF_FISHEYE_THOBY:
            AddCoordGeomCallback (ModifyCoord_Geom_ERect_Thoby, 500);
            break;
        case LF_EQUIRECTANGULAR:
        default:
            //nothing to do
            break;
    };
    return true;
}

void lfModifier::AddCoordDistCallback (const lfLensCalibDistortion& lcd, lfModifyCoordFunc func, int priority)
{
    lfCoordDistCallbackData* cd = new lfCoordDistCallbackData;

    cd->callback = func;
    cd->priority = priority;

    memcpy(cd->terms, lcd.Terms, sizeof(lcd.Terms));

    CoordCallbacks.insert(cd);
}

void lfModifier::AddCoordGeomCallback (lfModifyCoordFunc func, int priority)
{
    lfCoordGeomCallbackData* cd = new lfCoordGeomCallbackData;

    cd->callback = func;
    cd->priority = priority;

    CoordCallbacks.insert(cd);
}

double lfModifier::AutoscaleResidualDistance (float *coord) const
{

    // Maximal x and y value in normalized coordinates for the original image
    double max_x = Width / 2.0 * NormScale;
    double max_y = Height / 2.0 * NormScale;

    double result = coord [0] - max_x;
    double intermediate = -max_x - coord [0];
    if (intermediate > result) result = intermediate;
    intermediate = coord [1] - max_y;
    if (intermediate > result) result = intermediate;
    intermediate = -max_y - coord [1];
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
        for (auto cb : CoordCallbacks)
            cb->callback (cb, res, 1);

        double rd = AutoscaleResidualDistance (res);
        if (rd > -NEWTON_EPS * 100 && rd < NEWTON_EPS * 100)
            break;

        if (!countdown)
            // e.g. for some ultrawide fisheyes corners extend to infinity
            // so function never converge ...
            return -1;

        // Compute approximative function prime in (x,y)
        res [0] = ca * (ru + dx); res [1] = sa * (ru + dx);
        for (auto cb : CoordCallbacks)
            cb->callback (cb, res, 1);

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
    const float subpixel_scale = SubpixelCallbacks.size() == 0 ? 1.0 : 1.001;

    if (CoordCallbacks.size() == 0)
        return subpixel_scale;

    // 3 2 1
    // 4   0
    // 5 6 7
    lfPoint point [8];

    point [1].angle = atan2 (Height, Width);
    point [3].angle = M_PI - point [1].angle;
    point [5].angle = M_PI + point [1].angle;
    point [7].angle = 2 * M_PI - point [1].angle;

    point [0].angle = 0.0F;
    point [2].angle = float (M_PI / 2.0);
    point [4].angle = float (M_PI);
    point [6].angle = float (M_PI * 3.0 / 2.0);

    point [1].dist = point [3].dist = point [5].dist = point [7].dist =
        sqrt (pow (Width, 2) + pow (Height, 2)) * 0.5 * NormScale;
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

bool lfModifier::ApplyGeometryDistortion (
    float xu, float yu, int width, int height, float *res) const
{
    if (CoordCallbacks.size() <= 0 || height <= 0)
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

        for (auto cb : CoordCallbacks)
            cb->callback (cb, res, width);

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
    lfCoordScaleCallbackData* cddata = (lfCoordScaleCallbackData*) data;

    const float scale = ((lfCoordScaleCallbackData *)data)->scale_factor;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        iocoord [0] = iocoord [0] * scale;
        iocoord [1] = iocoord [1] * scale;
    }
}

void lfModifier::ModifyCoord_UnDist_Poly3 (void *data, float *iocoord, int count)
{
    lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;

    // See "Note about PT-based distortion models" at the top of this file.
    const float inv_k1_ = cddata->terms[0];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const double rd = sqrt (x * x + y * y);
        if (rd == 0.0)
            continue;

        float rd_div_k1_ = rd * inv_k1_;

        // Use Newton's method to avoid dealing with complex numbers
        // When carefully tuned this works almost as fast as Cardano's
        // method (and we don't use complex numbers in it, which is
        // required for a full solution!)
        //
        // Original function: Rd = k1_ * Ru^3 + Ru
        // Target function:   k1_ * Ru^3 + Ru - Rd = 0
        // Divide by k1_:     Ru^3 + Ru/k1_ - Rd/k1_ = 0
        // Derivative:        3 * Ru^2 + 1/k1_
        double ru = rd;
        for (int step = 0; ; step++)
        {
            double fru = ru * ru * ru + ru * inv_k1_ - rd_div_k1_;
            if (fru >= -NEWTON_EPS && fru < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_pixel;

            ru -= fru / (3 * ru * ru + inv_k1_);
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
    lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;

    // See "Note about PT-based distortion models" at the top of this file.
    // Rd = Ru * (1 + k1_ * Ru^2)
    const float k1_ = cddata->terms [0];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const float poly2 = 1 + k1_ * (x * x + y * y);

        iocoord [0] = x * poly2;
        iocoord [1] = y * poly2;
    }
}

void lfModifier::ModifyCoord_UnDist_Poly5 (void *data, float *iocoord, int count)
{
    lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;

    float k1 = cddata->terms [0];
    float k2 = cddata->terms [1];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
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
    lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;
    // Rd = Ru * (1 + k1 * Ru^2 + k2 * Ru^4)
    const float k1 = cddata->terms [0];
    const float k2 = cddata->terms [1];

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
    lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;

    // See "Note about PT-based distortion models" at the top of this file.
    float a_ = cddata->terms [0];
    float b_ = cddata->terms [1];
    float c_ = cddata->terms [2];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        double rd = sqrt (x * x + y * y);
        if (rd == 0.0)
            continue;

        // Use Newton's method
        double ru = rd;
        for (int step = 0; ; step++)
        {
            double fru = ru * (a_ * ru * ru * ru + b_ * ru * ru + c_ * ru + 1) - rd;
            if (fru >= -NEWTON_EPS && fru < NEWTON_EPS)
                break;
            if (step > 5)
                // Does not converge, no real solution in this area?
                goto next_pixel;

            ru -= fru / (4 * a_ * ru * ru * ru + 3 * b_ * ru * ru + 2 * c_ * ru + 1);
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
    lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;

    // See "Note about PT-based distortion models" at the top of this file.
    // Rd = Ru * (a_ * Ru^3 + b_ * Ru^2 + c_ * Ru + 1)
    const float a_ = cddata->terms [0];
    const float b_ = cddata->terms [1];
    const float c_ = cddata->terms [2];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const float ru2 = x * x + y * y;
        const float r = sqrtf (ru2);
        const float poly3 = a_ * ru2 * r + b_ * ru2 + c_ * r + 1;

        iocoord [0] = x * poly3;
        iocoord [1] = y * poly3;
    }
}

void lfModifier::ModifyCoord_Dist_ACM (void *data, float *iocoord, int count)
{
    lfCoordDistCallbackData* cddata = (lfCoordDistCallbackData*) data;

    const float k1 = cddata->terms [0];
    const float k2 = cddata->terms [1];
    const float k3 = cddata->terms [2];
    const float k4 = cddata->terms [3];
    const float k5 = cddata->terms [4];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const float ru2 = x * x + y * y;
        const float ru4 = ru2 * ru2;
        const float common_term = 1.0 + k1 * ru2 + k2 * ru4 + k3 * ru4 * ru2 + 2 * (k4 * y + k5 * x);

        iocoord [0] = x * common_term + k5 * ru2;
        iocoord [1] = y * common_term + k4 * ru2;
    }
}

void lfModifier::ModifyCoord_Geom_FishEye_Rect (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        float r = sqrt (x * x + y * y);
        float rho;

        if (r >= M_PI / 2.0)
            rho = 1.6e16F;
        else if (r == 0.0)
            rho = 1.0;
        else
            rho = tan (r) / r;

        iocoord [0] = rho * x;
        iocoord [1] = rho * y;
    }
}

void lfModifier::ModifyCoord_Geom_Rect_FishEye (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        float theta, r = sqrt (x * x + y * y);
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
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        iocoord [0] = tan (x);
        iocoord [1] = y / cos (x);
    }
}


void lfModifier::ModifyCoord_Geom_Rect_Panoramic (
    void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        iocoord [0] = atan (x);
        iocoord [1] = y * cos (iocoord [0]);
    }
}

void lfModifier::ModifyCoord_Geom_FishEye_Panoramic (
    void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double r = sqrt (x * x + y * y);
        double s = (r == 0.0) ? 1.0 : (sin (r) / r);

        double vx = cos (r);      //  z' -> x
        double vy = s * x;        //  x' -> y

        iocoord [0] = atan2 (vy, vx);
        iocoord [1] = s * y / sqrt (vx * vx + vy * vy);
    }
}

void lfModifier::ModifyCoord_Geom_Panoramic_FishEye (
    void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double s = sin (x);   // y' -> x
        double r = sqrt (s * s + y * y);
        double theta = 0.0;

        if (r==0.0)
            theta = 0.0;
        else
            theta = atan2 (r, cos (x)) / r;

        iocoord [0] = theta * s;
        iocoord [1] = theta * y;
    }
}

void lfModifier::ModifyCoord_Geom_ERect_Rect (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double theta = -y + M_PI / 2.0;
        if (theta < 0)
        {
            theta = -theta;
            x += M_PI;
        }
        if (theta > M_PI)
        {
            theta = 2 * M_PI - theta;
            x += M_PI;
        }

        iocoord [0] = tan (x);
        iocoord [1] = 1.0 / (tan (theta) * cos (x));
    }
}

void lfModifier::ModifyCoord_Geom_Rect_ERect (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        iocoord [0] = atan2 (x, 1);
        iocoord [1] = atan2 (y, sqrt (1 + x * x));
    }
}

void lfModifier::ModifyCoord_Geom_ERect_FishEye (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double theta = -y + M_PI / 2;
        if (theta < 0)
        {
            theta = -theta;
            x += M_PI;
        }
        if (theta > M_PI)
        {
            theta = 2 * M_PI - theta;
            x += M_PI;
        }

        double s = sin (theta);
        double vx = s * sin (x); //  y' -> x
        double vy = cos (theta);   //  z' -> y

        double r = sqrt (vx * vx + vy * vy);

        theta = atan2 (r, s * cos (x));

        r = 1.0 / r;
        iocoord [0] = theta * vx * r;
        iocoord [1] = theta * vy * r;
    }
}

void lfModifier::ModifyCoord_Geom_FishEye_ERect (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double r = sqrt (x * x + y * y);
        double s = (r == 0.0) ? 1.0 : (sin (r) / r);

        double vx = cos (r);
        double vy = s * x;

        iocoord [0] = atan2 (vy, vx);
        iocoord [1] = atan (s * y / sqrt (vx * vx + vy * vy));
    }
}

void lfModifier::ModifyCoord_Geom_ERect_Panoramic (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
        iocoord [1] = tan (iocoord [1]);
}

void lfModifier::ModifyCoord_Geom_Panoramic_ERect (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        iocoord [1] = atan (iocoord [1]);
    }
}

void lfModifier::ModifyCoord_Geom_Orthographic_ERect (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double r     = sqrt(x * x + y * y);
        double theta = 0.0;

        if (r < 1)
            theta = asin (r);
        else
            theta = M_PI / 2.0;

        double phi   = atan2 (y, x);

        double s = (theta == 0.0) ? 1.0 : (sin (theta) / theta);

        double vx = cos (theta);
        double vy = s * theta * cos (phi);

        iocoord [0] = atan2 (vy, vx);
        iocoord [1] = atan (s * theta * sin (phi) / sqrt (vx * vx + vy * vy));
    }
};

void lfModifier::ModifyCoord_Geom_ERect_Orthographic (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double theta = -y + M_PI / 2;
        if (theta < 0)
        {
            theta = -theta;
            x += M_PI;
        }
        if (theta > M_PI)
        {
            theta = 2 * M_PI - theta;
            x += M_PI;
        }

        double s  = sin (theta);
        double vx = s * sin (x); //  y' -> x
        double vy = cos (theta);   //  z' -> y

        theta = atan2 (sqrt (vx * vx + vy * vy), s * cos (x));
        x     = atan2 (vy, vx);
        double rho  = sin (theta);
        iocoord [0] = rho * cos (x);
        iocoord [1] = rho * sin (x);
     }
};

#define EPSLN   1.0e-10

void lfModifier::ModifyCoord_Geom_Stereographic_ERect (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

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
            iocoord [1] = asin (y * sinc / rh);
            if((fabs (cosc) >= EPSLN) || (fabs (x) >= EPSLN))
            {
                iocoord [0] = atan2 (x * sinc, cosc * rh);
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
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float lon = iocoord [0];
        float lat = iocoord [1];

        double cosphi = cos (lat);
        double ksp = 2.0 / (1.0 + cosphi * cos (lon));

        iocoord [0] = ksp * cosphi * sin (lon);
        iocoord [1] = ksp * sin (lat);
    }
};

void lfModifier::ModifyCoord_Geom_Equisolid_ERect (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double r = sqrt (x * x + y * y);
        double theta = 0.0;

        if (r < 2.0)
            theta = 2.0 * asin (r / 2.0);
        else
            theta = M_PI / 2.0;

        double phi = atan2 (y, x);
        double s = (theta == 0.0) ? 1.0 : (sin (theta) / theta);

        double vx = cos (theta);
        double vy = s * theta * cos (phi);

        iocoord [0] = atan2 (vy, vx);
        iocoord [1] = atan (s * theta * sin (phi) / sqrt (vx * vx + vy * vy));
    };
};

void lfModifier::ModifyCoord_Geom_ERect_Equisolid (void *data, float *iocoord, int count)
{    
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        double lambda = iocoord [0];
        double phi = iocoord [1];

        if (fabs (cos(phi) * cos(lambda) + 1.0) <= EPSLN)
        {
            iocoord [0] = 1.6e16F;
            iocoord [1] = 1.6e16F;
        }
        else
        {
            double k1 = sqrt (2.0 / (1 + cos(phi) * cos(lambda)));

            iocoord [0] = k1 * cos (phi) * sin (lambda);
            iocoord [1] = k1 * sin (phi);
        };
    };
};

#define THOBY_K1_PARM 1.47F
#define THOBY_K2_PARM 0.713F

void lfModifier::ModifyCoord_Geom_Thoby_ERect (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double rho = sqrt (x * x + y * y);
        if(rho<-THOBY_K1_PARM || rho > THOBY_K1_PARM)
        {
            iocoord [0] = 1.6e16F;
            iocoord [1] = 1.6e16F;
        }
        else
        {
            double theta = asin (rho / THOBY_K1_PARM) / THOBY_K2_PARM;
            double phi   = atan2 (y, x);
            double s     = (theta == 0.0) ? 1.0 : (sin (theta) / theta);

            double vx = cos (theta);
            double vy = s * theta * cos (phi);

            iocoord [0] = atan2 (vy, vx);
            iocoord [1] = atan (s * theta * sin (phi) / sqrt (vx * vx + vy * vy));
        };
    };
};

void lfModifier::ModifyCoord_Geom_ERect_Thoby (void *data, float *iocoord, int count)
{
    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x = iocoord [0];
        float y = iocoord [1];

        double theta = -y + M_PI / 2;
        if (theta < 0)
        {
            theta = -theta;
            x += M_PI;
        }
        if (theta > M_PI)
        {
            theta = 2 * M_PI - theta;
            x += M_PI;
        }

        double s  = sin (theta);
        double vx = s * sin (x); //  y' -> x
        double vy = cos (theta);   //  z' -> y
        theta = atan2 (sqrt (vx * vx + vy * vy), s * cos (x));
        x     = atan2 (vy, vx);
        double rho = THOBY_K1_PARM * sin (theta * THOBY_K2_PARM);

        iocoord [0] = rho * cos (x);
        iocoord [1] = rho * sin (x);
    };
};

//---------------------------// The C interface //---------------------------//

float lf_modifier_get_auto_scale (lfModifier *modifier, cbool reverse)
{
    return modifier->GetAutoScale (reverse);
}

cbool lf_modifier_apply_geometry_distortion (
    lfModifier *modifier, float xu, float yu, int width, int height, float *res)
{
    return modifier->ApplyGeometryDistortion (xu, yu, width, height, res);
}

int lf_modifier_enable_distortion_correction (lfModifier *modifier)
{
    return modifier->EnableDistortionCorrection();
}

int lf_modifier_enable_projection_transform (
    lfModifier *modifier, lfLensType target_projection)
{
    return modifier->EnableProjectionTransform(target_projection);
}
