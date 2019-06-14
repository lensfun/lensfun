/*
    Image modifier implementation
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>
#include <stdexcept>
#include "windows/mathconstants.h"

lfModifier *lfModifier::Create (const lfLens *lens, float crop, int width, int height)
{
    return new lfModifier (lens, crop, width, height);
}

int lfModifier::Initialize (
    const lfLens *lens, lfPixelFormat format, float focal, float aperture,
    float distance, float scale, lfLensType targeom, int flags, bool reverse)
{

    Lens = lens;
    Focal = focal;
    PixelFormat = format;
    Reverse = reverse;

    if (flags & LF_MODIFY_TCA)
        EnableTCACorrection();

    if (flags & LF_MODIFY_VIGNETTING)
        EnableVignettingCorrection(aperture, distance);

    if (flags & LF_MODIFY_DISTORTION)
        EnableDistortionCorrection();

    if (flags & LF_MODIFY_GEOMETRY && lens->Type != targeom)
        EnableProjectionTransform(targeom);

    if (flags & LF_MODIFY_SCALE && scale != 1.0)
        EnableScaling(scale);

    return EnabledMods;
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
  between them: NormScale and NormUnScale = 1/NormScale.

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

lfModifier::lfModifier (const lfLens*, float crop, int width, int height)
{
    Crop = crop;

    // Avoid divide overflows on singular cases.  The "- 1" is due to the fact
    // that `Width` and `Height` are measured at the pixel centres (they are
    // actually transformed) instead at their outer rims.
    Width = double (width >= 2 ? width - 1 : 1);
    Height = double (height >= 2 ? height - 1 : 1);

    // Image "size"
    double size = Width < Height ? Width : Height;

    // The scale to transform {-size/2 .. 0 .. size/2-1} to {-1 .. 0 .. +1}
    NormScale = 2.0 / size;

    // The scale to transform {-1 .. 0 .. +1} to {-size/2 .. 0 .. size/2-1}
    NormUnScale = size * 0.5;

    // Geometric lens center in normalized coordinates
    CenterX = Width / size;
    CenterY = Height / size;

    EnabledMods = 0;
}

float hugin_distortion_scale (float width, float height, float imgcrop, float real_focal)
{
    const float image_aspect_ratio = (width < height) ? height / width : width / height;
    const float hugin_scale_in_millimeters =
        hypot (36.0, 24.0) / imgcrop / sqrt (image_aspect_ratio * image_aspect_ratio + 1) / 2.0;
    return real_focal / hugin_scale_in_millimeters;
}

float hugin_vignetting_scale (float width, float height, float imgcrop, float real_focal)
{
    const float hugin_scale_in_millimeters = hypot (36.0, 24.0) / imgcrop / 2;
    return real_focal / hugin_scale_in_millimeters;
}

lfModifier::lfModifier (const lfLens *lens, float imgfocal, float imgcrop, int imgwidth, int imgheight,
                        lfPixelFormat pixel_format, bool reverse /* = false */)
    : Crop(imgcrop), Focal(imgfocal), Reverse(reverse), PixelFormat(pixel_format), Lens(lens)
{
    // Avoid divide overflows on singular cases.  The "- 1" is due to the fact
    // that `Width` and `Height` are measured at the pixel centres (they are
    // actually transformed) instead at their outer rims.
    Width = double (imgwidth >= 2 ? imgwidth - 1 : 1);
    Height = double (imgheight >= 2 ? imgheight - 1 : 1);

    lfLensCalibDistortion lcd;
    if (lens->InterpolateDistortion (imgcrop, imgfocal, lcd))
        RealFocal = lcd.RealFocal;
    else
        RealFocal = imgfocal;

    NormScale = hypot (36.0, 24.0) / imgcrop / hypot (Width, Height) / RealFocal;
    NormUnScale = 1.0 / NormScale;

    // Geometric lens center in normalized coordinates
    CenterX = Width * NormScale * 0.5;
    CenterY = Height * NormScale * 0.5;

    EnabledMods = 0;
}

int lfModifier::EnableScaling (float scale)
{
    if (scale == 1.0)
        return false;

    // Inverse scale factor
    if (scale == 0.0)
    {
        scale = GetAutoScale (Reverse);
        if (scale == 0.0)
            return false;
    }

    lfCoordScaleCallbackData* cd = new lfCoordScaleCallbackData;

    cd->callback = ModifyCoord_Scale;
    cd->priority = Reverse ? 900 : 100;
    cd->scale_factor = Reverse ? scale : 1.0 / scale;

    CoordCallbacks.insert(cd);

    EnabledMods |= LF_MODIFY_SCALE;
    return EnabledMods;
}


int lfModifier::GetModFlags()
{
    return EnabledMods;
}

lfModifier::~lfModifier ()
{
    for (auto cb : CoordCallbacks)
        delete cb;
    for (auto cb : SubpixelCallbacks)
        delete cb;
    for (auto cb : ColorCallbacks)
        delete cb;
}

float lfModifier::GetNormalizedFocalLength (float focal) const
{
    const double normalized_in_millimeters = 12.0 / Crop;
    return static_cast<float>(static_cast<double>(focal) / normalized_in_millimeters);
}

//---------------------------// The C interface //---------------------------//

lfModifier *lf_modifier_create (
    const lfLens* lens, float imgfocal, float imgcrop, int imgwidth, int imgheight, lfPixelFormat pixel_format, bool reverse)
{
    return new lfModifier(lens, imgfocal, imgcrop, imgwidth, imgheight, pixel_format, reverse);
}

lfModifier *lf_modifier_new (
    const lfLens *lens, float crop, int width, int height)
{
    try
    {
        return new lfModifier (lens, crop, width, height);
    }
    catch (const std::exception& e)
    {
        return NULL;
    }
}

void lf_modifier_destroy (lfModifier *modifier)
{
    delete modifier;
}

int lf_modifier_initialize (
    lfModifier *modifier, const lfLens *lens, lfPixelFormat format,
    float focal, float aperture, float distance, float scale, lfLensType targeom,
    int flags, cbool reverse)
{
    return modifier->Initialize (lens, format, focal, aperture, distance,
                                 scale, targeom, flags, reverse);
}

int lf_modifier_enable_scaling (lfModifier *modifier, float scale)
{
    return modifier->EnableScaling(scale);
}

int lf_modifier_get_mod_flags (lfModifier *modifier)
{
    return modifier->GetModFlags();
}
