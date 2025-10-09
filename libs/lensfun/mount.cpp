/*
    Private constructors and destructors
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <cstdlib>
#include <algorithm>
#include <iterator>

lfMount::lfMount () :
     Name{nullptr},
     Compat{nullptr}
{
}

lfMount::~lfMount ()
{
    lf_free (Name);
}

lfMount::lfMount (const lfMount &other) :
     Name{lf_mlstr_dup (other.Name)},
     Compat{nullptr},
     MountCompat{other.MountCompat}
{
    RebuildCAPIPointers();
}

lfMount::lfMount (lfMount &&other) noexcept :
     Name{std::move(other.Name)},
     Compat{std::move(other.Compat)},
     MountCompat{std::move(other.MountCompat)},
     MountCompatCAPIPtrs{std::move(other.MountCompatCAPIPtrs)}
{
    other.Name = nullptr;
    other.Compat = nullptr;
}

lfMount &lfMount::operator = (const lfMount &other)
{
    lf_free (Name);
    Name = lf_mlstr_dup (other.Name);

    MountCompat = other.MountCompat;

    RebuildCAPIPointers();

    return *this;
}

lfMount &lfMount::operator = (lfMount &&other) noexcept
{
    Name = std::move(other.Name);
    Compat = std::move(other.Compat);
    MountCompat = std::move(other.MountCompat);
    MountCompatCAPIPtrs = std::move(other.MountCompatCAPIPtrs);

    other.Name = nullptr;
    other.Compat = nullptr;

    return *this;
}

void lfMount::RebuildCAPIPointers()
{
    MountCompatCAPIPtrs.clear();

    // +1 for C-compatible null-terminated vector
    MountCompatCAPIPtrs.reserve(MountCompat.size() + 1);
    std::transform(MountCompat.begin(),
                   MountCompat.end(),
                   std::back_inserter(MountCompatCAPIPtrs),
                   [](const std::string& mount){ return mount.c_str(); });

    // making the vector C-compatible
    MountCompatCAPIPtrs.emplace_back(nullptr);

    //const_cast for C-compatibility sake
    //NOLINTNEXTLINE
    Compat = const_cast<char**>(MountCompatCAPIPtrs.data());
}

bool lfMount::operator == (const lfMount& other) const noexcept
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
        MountCompat.emplace_back(val);
        RebuildCAPIPointers();
    }
}

const char* const* lfMount::GetCompats() const noexcept
{
    return MountCompatCAPIPtrs.data();
}

bool lfMount::Check () const noexcept
{
    return Name != nullptr;
}


//---------------------------// The C interface //---------------------------//

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
