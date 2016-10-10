/*
    Image modifier implementation
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>
#include "windows/mathconstants.h"

lfModifier *lfModifier::Create (const lfLens *lens, float crop, int width, int height)
{
    return new lfModifier (lens, crop, width, height);
}

int lfModifier::Initialize (
    const lfLens *lens, lfPixelFormat format, float focal, float aperture,
    float distance, float scale, lfLensType targeom, int flags, bool reverse)
{
    FocalLengthNormalized = GetRealFocalLength (lens, focal) / NormalizedInMillimeters;

    int oflags = 0;

    if (flags & LF_MODIFY_TCA)
    {
        lfLensCalibTCA lctca;
        if (lens->InterpolateTCA (focal, lctca))
            if (AddSubpixelCallbackTCA (lctca, reverse))
                oflags |= LF_MODIFY_TCA;
    }

    if (flags & LF_MODIFY_VIGNETTING)
    {
        lfLensCalibVignetting lcv;
        if (lens->InterpolateVignetting (focal, aperture, distance, lcv))
            if (AddColorCallbackVignetting (lcv, format, reverse))
                oflags |= LF_MODIFY_VIGNETTING;
    }

    if (flags & LF_MODIFY_DISTORTION)
    {
        lfLensCalibDistortion lcd;
        if (lens->InterpolateDistortion (focal, lcd))
            if (AddCoordCallbackDistortion (lcd, reverse))
                oflags |= LF_MODIFY_DISTORTION;
    }

    if (flags & LF_MODIFY_GEOMETRY &&
        lens->Type != targeom)
    {
        if (reverse ?
            AddCoordCallbackGeometry (targeom, lens->Type) :
            AddCoordCallbackGeometry (lens->Type, targeom))
            oflags |= LF_MODIFY_GEOMETRY;
    }

    if (flags & LF_MODIFY_SCALE &&
        scale != 1.0)
        if (AddCoordCallbackScale (scale, reverse))
            oflags |= LF_MODIFY_SCALE;

    Reverse = reverse;

    return oflags;
}

float lfModifier::GetRealFocalLength (const lfLens *lens, float focal)
{
    float result = focal;
    lfLensCalibFov fov_raw;
    if (lens && lens->InterpolateFov (focal, fov_raw))
    {
        float fov = fov_raw.FieldOfView * M_PI / 180.0;
        float half_width_in_millimeters = NormalizedInMillimeters * lens->AspectRatio;
        // See also SrcPanoImage::calcFocalLength in Hugin.
        switch (lens->Type)
        {
            case LF_UNKNOWN:
                break;

            case LF_RECTILINEAR:
                result = half_width_in_millimeters / tan (fov / 2.0);
                break;

            case LF_FISHEYE:
            case LF_PANORAMIC:
            case LF_EQUIRECTANGULAR:
                result = half_width_in_millimeters / (fov / 2.0);
                break;

            case LF_FISHEYE_ORTHOGRAPHIC:
                result = half_width_in_millimeters / sin (fov / 2.0);
                break;

            case LF_FISHEYE_STEREOGRAPHIC:
                result = half_width_in_millimeters / (2 * tan (fov / 4.0));
                break;

            case LF_FISHEYE_EQUISOLID:
                result = half_width_in_millimeters / (2 * sin (fov / 4.0));
                break;

            case LF_FISHEYE_THOBY:
                result = half_width_in_millimeters / (1.47 * sin (0.713 * fov / 2.0));
                break;

            default:
                // This should never happen
                result = NAN;
        }
        return result;
    }
    lfLensCalibDistortion lcd;
    if (lens->InterpolateDistortion (focal, lcd))
        if (lcd.RealFocal > 0)
            return lcd.RealFocal;
    return result;
}

void lfModifier::Destroy ()
{
    delete this;
}

//---------------------------------------------------------------------------//

/*
  About coordinate systems in Lensfun

  Lensfun uses three coordinate systems.  In all of them, the centre of the
  image is the origin.  There is a single coordinate "r" which is the distance
  from the origin.

  (1) For scaling, distortion, and TCA correction, the so-called "normalised"
      coordinate system is used.  r = 1 is the middle of the long edge, in
      other words, the half height of the image (in landscape mode).

  (2) For vignetting, r = 1 is the corner of the image.

  (3) For geometry transformation and for all Adobe camera models, the unit
      length is the focal length.

  The constructor lfModifier::lfModifier is the central method that
  handles the coordinate systems.  It does so by providing the scaling factors
  between them: NormScale (and NormUnScale = 1/NormScale),
  NormalizedInMillimeters, and AspectRatioCorrection.

  Have a look at lfModifier::ApplySubpixelGeometryDistortion to see the
  coordinate systems (1) and (3) in action.  First, the original pixel
  coordinates are converted into (1) by multiplying by NormScale.  Then,
  scaling, geometry transformation, un-distortion, and un-TCA are performed, in
  this order.  Remember that this means that the coordinates are *distorted*,
  to make a proper lookup in the uncorrected, original bitmap.  For this to
  work, the coordinates are finally divided by NormScale again.  Done.

  But the devil is in the details.  Geometry transformation has to happen in
  (3), so for only this step, all coordinates are scaled by
  FocalLengthNormalized in lfModifier::AddCoordCallbackGeometry.  Moreover, it
  is important to see that the conversion into (1) is pretty irrelevant.  (It
  is performed for that the resulting image is not ridiculously small; but this
  could also be achieved with proper auto-scaling.)  Instead, really critical
  is only the *back-transformation* from (1) into the pixel coordinate system
  of the uncorrected, original bitmap.  This must be exactly correct.
  Otherwise, the strength of correction does not match with the position in the
  picture, and the correction cannot work.

  And then there is vignetting.  All callbacks work in (1), and vignetting,
  being the only colour modification so far, also gets its coordinates in (1).
  Thus, lfModifier::AddColorCallbackVignetting appends two more floats to the
  array of vignetting parameters for conversion into (2).

  Sometimes, a calibration is used that was made with another sensor size,
  i.e. different crop factor and/or aspect ratio.  Then, coordinate_correction
  in the following routine is different from 1.  It maps coordinates from (1)
  of the image sensor to (1) of the calibration sensor.  It is a product of
  three components: Converting to (2) of the image sensor, scaling by the ratio
  of the cropfactors to (2) of the calibration sensor, and finally converting
  to (1) of the calibration sensor.  The detour via (2) is necessary because
  crop factors are defined by the sensor diagonal, as is (2).

*/

lfModifier::lfModifier (const lfLens *lens, float crop, int width, int height)
{
    SubpixelCallbacks = g_ptr_array_new ();
    ColorCallbacks = g_ptr_array_new ();
    CoordCallbacks = g_ptr_array_new ();

    // Avoid divide overflows on singular cases.  The "- 1" is due to the fact
    // that `Width` and `Height` are measured at the pixel centres (they are
    // actually transformed) instead at their outer rims.
    Width = double (width >= 2 ? width - 1 : 1);
    Height = double (height >= 2 ? height - 1 : 1);

    // Image "size"
    double size = Width < Height ? Width : Height;
    double image_aspect_ratio = Width < Height ? Height / Width : Width / Height;

    double calibration_cropfactor;
    double calibration_aspect_ratio;
    if (lens)
    {
        calibration_cropfactor = lens->CropFactor;
        calibration_aspect_ratio = lens->AspectRatio;
    }
    else
        calibration_cropfactor = calibration_aspect_ratio = NAN;
    AspectRatioCorrection = sqrt (calibration_aspect_ratio * calibration_aspect_ratio + 1);

    double coordinate_correction =
        1.0 / sqrt (image_aspect_ratio * image_aspect_ratio + 1) *
        calibration_cropfactor / crop *
        AspectRatioCorrection;

    // In NormalizedInMillimeters, we un-do all factors of
    // coordinate_correction that refer to the calibration sensor because
    // NormalizedInMillimeters is supposed to transform to image coordinates.
    NormalizedInMillimeters = sqrt (36.0*36.0 + 24.0*24.0) / 2.0 /
        AspectRatioCorrection / calibration_cropfactor;

    // The scale to transform {-size/2 .. 0 .. size/2-1} to {-1 .. 0 .. +1}
    NormScale = 2.0 / size * coordinate_correction;

    // The scale to transform {-1 .. 0 .. +1} to {-size/2 .. 0 .. size/2-1}
    NormUnScale = size * 0.5 / coordinate_correction;

    // Geometric lens center in normalized coordinates
    CenterX = Width / size * coordinate_correction + (lens ? lens->CenterX : 0.0);
    CenterY = Height / size * coordinate_correction + (lens ? lens->CenterY : 0.0);

    // Used for autoscaling
    MaxX = Width / 2.0 * NormScale;
    MaxY = Height / 2.0 * NormScale;
}

static void free_callback_list (void *arr)
{
    for (unsigned i = 0; i < ((GPtrArray *)arr)->len; i++)
    {
        lfCallbackData *d = (lfCallbackData *)g_ptr_array_index ((GPtrArray *)arr, i);
        if (d)
        {
            if (d->data_size)
                g_free (d->data);
            delete d;
        }
    }
    g_ptr_array_free ((GPtrArray *)arr, TRUE);
}

lfModifier::~lfModifier ()
{
    free_callback_list (SubpixelCallbacks);
    free_callback_list (ColorCallbacks);
    free_callback_list (CoordCallbacks);
}

static gint _lf_coordcb_compare (gconstpointer a, gconstpointer b)
{
    lfCallbackData *d1 = (lfCallbackData *)a;
    lfCallbackData *d2 = (lfCallbackData *)b;
    return d1->priority < d2->priority ? -1 :
        d1->priority > d2->priority ? +1 : 0;
}

void lfModifier::AddCallback (void *arr, lfCallbackData *d,
                              int priority, void *data, size_t data_size)
{
    d->priority = priority;
    d->data_size = data_size;
    if (data_size)
    {
        d->data = g_malloc (data_size);
        memcpy (d->data, data, data_size);
    }
    else
        d->data = data;
    _lf_ptr_array_insert_sorted ((GPtrArray *)arr, d, _lf_coordcb_compare);
}

//---------------------------// The C interface //---------------------------//

lfModifier *lf_modifier_new (
    const lfLens *lens, float crop, int width, int height)
{
    return new lfModifier (lens, crop, width, height);
}

void lf_modifier_destroy (lfModifier *modifier)
{
    modifier->Destroy ();
}

int lf_modifier_initialize (
    lfModifier *modifier, const lfLens *lens, lfPixelFormat format,
    float focal, float aperture, float distance, float scale, lfLensType targeom,
    int flags, cbool reverse)
{
    return modifier->Initialize (lens, format, focal, aperture, distance,
                                 scale, targeom, flags, reverse);
}
