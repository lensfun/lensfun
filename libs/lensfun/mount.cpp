/*
    Private constructors and destructors
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"

lfMount::lfMount ()
{
    Name = NULL;
    Compat = NULL;
}

lfMount::~lfMount ()
{
    lf_free (Name);
    for (char* m: MountCompat)
        free(m);
}

lfMount::lfMount (const lfMount &other)
{
    Name = lf_mlstr_dup (other.Name);
    Compat = NULL;

    MountCompat.clear();
    const char* const* otherMounts = other.GetCompats();
    for (int i = 0; otherMounts[i]; i++)
        AddCompat(otherMounts[i]);
}

lfMount &lfMount::operator = (const lfMount &other)
{
    lf_free (Name);
    Name = lf_mlstr_dup (other.Name);
    Compat = NULL;

    MountCompat.clear();
    const char* const* otherMounts = other.GetCompats();
    for (int i = 0; otherMounts[i]; i++)
        AddCompat(otherMounts[i]);

    return *this;
}

bool lfMount::operator == (const lfMount& other)
{
    return _lf_strcmp (Name, other.Name) == 0;
}

void lfMount::SetName (const char *val, const char *lang)
{
    Name = lf_mlstr_add (Name, lang, val);
}

void lfMount::AddCompat (const char *val)
{
    if (val)
    {
        char* p = (char*)malloc(strlen(val));
        strcpy(p, val);
        MountCompat.push_back(p);

        // add terminating NULL
        size_t s = MountCompat.size();
        MountCompat.reserve(s+1);
        MountCompat[s] = NULL;

        // legacy compat pointer
        Compat = (char**)MountCompat.data();
    }
}

const char* const* lfMount::GetCompats() const
{
    return MountCompat.data();
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

lfMount *lf_mount_create ()
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

void lf_mount_add_compat (lfMount *mount, const char *val)
{
    mount->AddCompat(val);
}

const char* const* lf_mount_get_compats (lfMount *mount)
{
    return mount->GetCompats();
}
