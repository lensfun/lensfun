/*
    Image modifier implementation
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>

lfModifier *lfModifier::Create (const lfLens *lens, float crop, int width, int height)
{
    return new lfExtModifier (lens, crop, width, height);
}

int lfModifier::Initialize (
    const lfLens *lens, lfPixelFormat format, float focal, float aperture,
    float distance, float scale, lfLensType targeom, int flags, bool reverse)
{
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

    if (flags & LF_MODIFY_CCI)
        if (AddColorCallbackCCI (lens, format, reverse))
            oflags |= LF_MODIFY_CCI;

    if (flags & LF_MODIFY_DISTORTION)
    {
        lfLensCalibDistortion lcd;
        if (lens->InterpolateDistortion (focal, lcd))
            if (AddCoordCallbackDistortion (lcd, reverse))
                oflags |= LF_MODIFY_DISTORTION;
    }

    // Multiply focal length by lens crop for geometry modifications as these use a different
    // coordinate system. Usually the pixel coordinates are divided by lens crop and scaled by
    // camera crop. Geometry modification coordinates are only scaled by camera crop. So multiplying
    // focal length by lens crop compensates this.
    double focal_with_lens_crop = lens && lens->CropFactor ? focal * lens->CropFactor : focal;

    if (flags & LF_MODIFY_GEOMETRY &&
        lens->Type != targeom)
        if (reverse ?
            AddCoordCallbackGeometry (targeom, lens->Type, focal_with_lens_crop) :
            AddCoordCallbackGeometry (lens->Type, targeom, focal_with_lens_crop))
            oflags |= LF_MODIFY_GEOMETRY;

    if (flags & LF_MODIFY_SCALE &&
        scale != 1.0)
        if (AddCoordCallbackScale (scale, reverse))
            oflags |= LF_MODIFY_SCALE;

    return oflags;
}

void lfModifier::Destroy ()
{
    delete static_cast<lfExtModifier *> (this);
}

//---------------------------------------------------------------------------//

lfExtModifier::lfExtModifier (const lfLens *lens, float crop, int width, int height)
{
    SubpixelCallbacks = g_ptr_array_new ();
    ColorCallbacks = g_ptr_array_new ();
    CoordCallbacks = g_ptr_array_new ();

    // Avoid divide overflows on singular cases
    Width = (width >= 2 ? width : 2);
    Height = (height >= 2 ? height : 2);

    // Image "size"
    float size = float ((width < height) ? width : height);

    // Take crop factor into account
    if (lens && lens->CropFactor)
        size *= crop / lens->CropFactor;

    // The scale to transform {-size/2 .. 0 .. size/2-1} to {-1 .. 0 .. +1}
    NormScale = 2.0 / (size - 1);

    // The scale to transform {-1 .. 0 .. +1} to {-size/2 .. 0 .. size/2-1}
    NormUnScale = (size - 1) * 0.5;

    // Geometric lens center in normalized coordinates
    CenterX = width / size + (lens ? lens->CenterX : 0.0);
    CenterY = height / size + (lens ? lens->CenterY : 0.0);
}

static void free_callback_list (GPtrArray *arr)
{
    for (unsigned i = 0; i < arr->len; i++)
    {
        lfCallbackData *d = (lfCallbackData *)g_ptr_array_index (arr, i);
        if (d)
        {
            if (d->data_size)
                g_free (d->data);
            delete d;
        }
    }
    g_ptr_array_free (arr, TRUE);
}

lfExtModifier::~lfExtModifier ()
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

void lfExtModifier::AddCallback (GPtrArray *arr, lfCallbackData *d,
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
    _lf_ptr_array_insert_sorted (arr, d, _lf_coordcb_compare);
}

//---------------------------// The C interface //---------------------------//

lfModifier *lf_modifier_new (
    const lfLens *lens, float crop, int width, int height)
{
    return lfModifier::Create (lens, crop, width, height);
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
