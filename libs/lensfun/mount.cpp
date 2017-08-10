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
    Name = NULL;
    Compat = NULL;
}

lfMount::~lfMount ()
{
    lf_free (Name);
}

lfMount::lfMount (const lfMount &other)
{
    Name = lf_mlstr_dup (other.Name);
    Compat = NULL;
    MountCompat = other.GetCompats();
}

lfMount &lfMount::operator = (const lfMount &other)
{
    lf_free (Name);
    Name = lf_mlstr_dup (other.Name);
    Compat = NULL;
    MountCompat = other.GetCompats();

    return *this;
}

bool lfMount::operator == (const lfMount& other)
{
    return _lf_strcmp (Name, other.Name) == 0;
}
bool lfMount::operator != (const lfMount& other)
{
    return _lf_strcmp (Name, other.Name) != 0;
}

void lfMount::SetName (const char *val, const char *lang)
{
    Name = lf_mlstr_add (Name, lang, val);
}

void lfMount::AddCompat (const char *val)
{
    MountCompat.emplace(std::string(val));
    // TODO: update legacy Compat pointers
}

const std::set<std::string> lfMount::GetCompats () const
{
    return MountCompat;
}

bool lfMount::Check ()
{
    if (!Name)
        return false;

    return true;
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
