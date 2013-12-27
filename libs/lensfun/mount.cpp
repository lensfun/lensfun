/*
    Private constructors and destructors
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"

lfMount::lfMount ()
{
    // Defaults for attributes are "unknown" (mostly 0).  Otherwise, ad hoc
    // lfLens instances used for searches could not be matched against database
    // lenses easily.  If you need defaults for database tags, set them when
    // reading the database.
    memset (this, 0, sizeof (*this));
}

lfMount::~lfMount ()
{
    lf_free (Name);
    _lf_list_free ((void **)Compat);
}

lfMount &lfMount::operator = (const lfMount &other)
{
    lf_free (Name);
    Name = lf_mlstr_dup (other.Name);
    lf_free (Compat); Compat = NULL;
    if (other.Compat)
        for (int i = 0; other.Compat [i]; i++)
                AddCompat (other.Compat [i]);

    return *this;
}

void lfMount::SetName (const char *val, const char *lang)
{
    Name = lf_mlstr_add (Name, lang, val);
}

void lfMount::AddCompat (const char *val)
{
    _lf_addstr (&Compat, val);
}

bool lfMount::Check ()
{
    if (!Name)
        return false;

    return true;
}

gint _lf_mount_compare (gconstpointer a, gconstpointer b)
{
    lfMount *i1 = (lfMount *)a;
    lfMount *i2 = (lfMount *)b;

    return _lf_strcmp (i1->Name, i2->Name);
}

//---------------------------// The C interface //---------------------------//

lfMount *lf_mount_new ()
{
    return new lfMount ();
}

void lf_mount_destroy (lfMount *mount)
{
    delete mount;
}

cbool lf_mount_check (lfMount *mount)
{
    return mount->Check ();
}
