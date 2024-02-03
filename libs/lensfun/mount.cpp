/*
    Private constructors and destructors
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <stdlib.h>

namespace {
    std::vector<char*> AddCompatBulk(const std::vector<char*>& other)
    {
        std::vector<char*> result;
        //+1 for _lf_terminate_vec
        result.reserve(other.size() + 1);

        for (auto* otherMounts : other)
        {
            if (otherMounts)
            {
                result.emplace_back(new char[strlen(otherMounts) + 1]);
                strcpy(result.back(), otherMounts);
            }
        }

        // add terminating NULL
        _lf_terminate_vec(result);
        return result;
    }
}

lfMount::lfMount () :
     Name{nullptr},
     Compat{nullptr}
{
}

lfMount::~lfMount ()
{
    lf_free (Name);

    for (char* m: MountCompat)
        delete[] m;
}

lfMount::lfMount (const lfMount &other) :
     Name{lf_mlstr_dup (other.Name)},
     Compat{nullptr},
     MountCompat{AddCompatBulk(other.MountCompat)}
{
    // legacy compat pointer
    Compat = MountCompat.data();
}

lfMount::lfMount (lfMount &&other) noexcept :
     Name{other.Name},
     Compat{other.Compat},
     MountCompat{std::move(other.MountCompat)}
{
    other.Name = nullptr;
    other.Compat = nullptr;
}

lfMount &lfMount::operator = (const lfMount &other)
{
    lf_free (Name);
    Name = lf_mlstr_dup (other.Name);

    for (char* m: MountCompat)
        delete[] m;

    MountCompat = AddCompatBulk(other.MountCompat);

    // legacy compat pointer
    Compat = MountCompat.data();

    return *this;
}

lfMount &lfMount::operator = (lfMount &&other) noexcept
{
    Name = other.Name;
    Compat = other.Compat;
    MountCompat = std::move(other.MountCompat);

    other.Name = nullptr;
    other.Compat = nullptr;

    return *this;
}

bool lfMount::operator == (const lfMount& other) // const noexcept
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
        MountCompat.emplace_back(new char[strlen(val) + 1]);
        strcpy(MountCompat.back(), val);

        // add terminating NULL
        _lf_terminate_vec(MountCompat);

        // legacy compat pointer
        Compat = MountCompat.data();
    }
}

const char* const* lfMount::GetCompats() const //noexcept
{
    return MountCompat.data();
}

bool lfMount::Check () // const noexcept
{
    return Name != nullptr;
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

void lf_mount_copy (lfMount *dest, const lfMount *source)
{
    *dest = *source;
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
