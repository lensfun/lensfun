/*
    Private constructors and destructors
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"

lfCamera::lfCamera ()
{
    Maker = NULL;
    Model = NULL;
    Variant = NULL;
    Mount = NULL;
    CropFactor = 0.0f;
}

lfCamera::~lfCamera ()
{
    lf_free (Maker);
    lf_free (Model);
    lf_free (Variant);
    lf_free (Mount);
}

lfCamera::lfCamera (const lfCamera &other)
{
    Maker = lf_mlstr_dup (other.Maker);
    Model = lf_mlstr_dup (other.Model);
    Variant = lf_mlstr_dup (other.Variant);
    Mount = g_strdup (other.Mount);
    CropFactor = other.CropFactor;
}

lfCamera &lfCamera::operator = (const lfCamera &other)
{
    lf_free (Maker);
    Maker = lf_mlstr_dup (other.Maker);
    lf_free (Model);
    Model = lf_mlstr_dup (other.Model);
    lf_free (Variant);
    Variant = lf_mlstr_dup (other.Variant);
    _lf_setstr (&Mount, other.Mount);
    CropFactor = other.CropFactor;
    return *this;
}

void lfCamera::SetMaker (const char *val, const char *lang)
{
    Maker = lf_mlstr_add (Maker, lang, val);
}

void lfCamera::SetModel (const char *val, const char *lang)
{
    Model = lf_mlstr_add (Model, lang, val);
}

void lfCamera::SetVariant (const char *val, const char *lang)
{
    Variant = lf_mlstr_add (Variant, lang, val);
}

void lfCamera::SetMount (const char *val)
{
    _lf_setstr (&Mount, val);
}

bool lfCamera::Check ()
{
    if (!Maker || !Model || !Mount || CropFactor <= 0)
        return false;

    return true;
}

//---------------------------// The C interface //---------------------------//

lfCamera *lf_camera_create ()
{
    return new lfCamera ();
}

void lf_camera_destroy (lfCamera *camera)
{
    delete camera;
}

void lf_camera_copy (lfCamera *dest, const lfCamera *source)
{
    *dest = *source;
}

cbool lf_camera_check (lfCamera *camera)
{
    return camera->Check ();
}
