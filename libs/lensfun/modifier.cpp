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

    lfLensCalibDistortion lcd;
    if (Lens->InterpolateDistortion (Crop, Focal, lcd))
        RealFocal = lcd.RealFocal;
    else
        RealFocal = Focal;

    NormScale = hypot (36.0, 24.0) / Crop / hypot (Width + 1.0, Height + 1.0) / RealFocal;
    NormUnScale = 1.0 / NormScale;

    const float size = std::min (Width, Height);
    CenterX = (Width / 2.0 + size / 2.0 * Lens->CenterX) * NormScale;
    CenterY = (Height / 2.0 + size / 2.0 * Lens->CenterY) * NormScale;

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

  Lensfun uses multiple coordinate systems.  In all of them, the centre of the
  image is the origin.  There is a single coordinate "r" which is the distance
  from the origin.

  (1) The internal coordinate system is the natural system in units of the real
      focal length of the image to be corrected.  In most cases, this equals
      the nominal focal length as given as the "focal" attribute in the XML
      files.  However, for some lenses, the real focal length is known and
      used.  This coordinate system is also used "normalized" coordinates.

  (2) The Hugin-based distortion and TCA calibration models "ptlens", "poly3",
      "poly5", and "linear" use the Hugin coordinate system.  r = 1 is the
      middle of the long edge, in other words, the half height of the image (in
      landscape mode).

  (3) For the vignetting model "pa", r = 1 is the corner of the image.

  The constructor lfModifier::lfModifier is the central method that handles the
  coordinate system.  It does so by providing the scaling factors between (1)
  and the image pixels: NormScale and NormUnScale = 1/NormScale.

  Have a look at lfModifier::ApplySubpixelGeometryDistortion to see the
  coordinate system (1) in action.  First, the original pixel coordinates are
  converted into (1) by multiplying by NormScale and shifting to the image
  centre.  Then, scaling, geometry transformation, un-distortion, and un-TCA
  are performed, in this order.  Remember that this means that the coordinates
  are *distorted*, to make a proper lookup in the uncorrected, original bitmap.
  For this to work, the coordinates are finally divided by NormScale again.
  Done.

  All non-natural coordinate systems (i.e., everything except (1)) require
  special treatment.  Luckily, all polynomial-based models can be made fit for
  (1) by re-scaling the coefficients.  This happens in functions in
  mod-coord.cpp, mod-subpix.cpp, and mod-color.cpp, each one called
  "rescale_polynomial_coefficients".

  Sometimes, a calibration is used that was made with another sensor size,
  i.e. different crop factor and/or aspect ratio.  This is also dealt with in
  the "rescale_polynomial_coefficients" functions.
*/

lfModifier::lfModifier (const lfLens*, float crop, int width, int height)
{
    Crop = crop;

    // Avoid divide overflows on singular cases.  The "- 1" is due to the fact
    // that `Width` and `Height` are measured at the pixel centres (they are
    // actually transformed) instead at their outer rims.
    Width = double (width >= 2 ? width - 1 : 1);
    Height = double (height >= 2 ? height - 1 : 1);

    EnabledMods = 0;
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
    if (Lens->InterpolateDistortion (Crop, Focal, lcd))
        RealFocal = lcd.RealFocal;
    else
        RealFocal = Focal;

    // I add 1 pixel to width and height because sensor size is given for the
    // outer rim of the pixel array.
    NormScale = hypot (36.0, 24.0) / Crop / hypot (Width + 1.0, Height + 1.0) / RealFocal;
    NormUnScale = 1.0 / NormScale;

    // Geometric lens center in normalized coordinates
    const double size = std::min (Width, Height);
    CenterX = (Width / 2.0 + size / 2.0 * Lens->CenterX) * NormScale;
    CenterY = (Height / 2.0 + size / 2.0 * Lens->CenterY) * NormScale;

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
