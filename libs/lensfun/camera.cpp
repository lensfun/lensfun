/*
    Private constructors and destructors
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"

lfCamera::lfCamera ()
{
    memset (this, 0, sizeof (*this));
    CropFactor = 1.0;
}

lfCamera::~lfCamera ()
{
    g_free (Maker);
    g_free (Model);
    g_free (Variant);
    g_free (Mount);
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
    if (!Maker || !Model || !Mount)
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
