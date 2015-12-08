/*
    Private constructors and destructors
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"

lfCamera::lfCamera ()
{
    // Defaults for attributes are "unknown" (mostly 0).  Otherwise, ad hoc
    // lfLens instances used for searches could not be matched against database
    // lenses easily.  If you need defaults for database tags, set them when
    // reading the database.
    memset (this, 0, sizeof (*this));
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

lfCamera *lf_camera_new ()
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

gint _lf_camera_compare (gconstpointer a, gconstpointer b)
{
    lfCamera *i1 = (lfCamera *)a;
    lfCamera *i2 = (lfCamera *)b;

    int cmp = _lf_strcmp (i1->Maker, i2->Maker);
    if (cmp != 0)
        return cmp;

    cmp = _lf_strcmp (i1->Model, i2->Model);
    if (cmp != 0)
        return cmp;

    return _lf_strcmp (i1->Variant, i2->Variant);
}
