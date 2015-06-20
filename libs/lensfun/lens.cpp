/*
    Private constructors and destructors
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <limits.h>
#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <locale.h>
#include <math.h>
#include <algorithm>

static struct
{
    const char *regex;
    guchar matchidx [3];
    bool compiled;
    regex_t rex;
} lens_name_regex [] =
{
    {
        // [min focal]-[max focal]mm f/[min aperture]-[max aperture]
        "([[:space:]]+|^)([0-9]+[0-9.]*)(-[0-9]+[0-9.]*)?(mm)?[[:space:]]+(f/|f|1/|1:)?([0-9.]+)(-[0-9.]+)?",
        { 2, 3, 6 },
        false
    },
    {
        // 1:[min aperture]-[max aperture] [min focal]-[max focal]mm
        "[[:space:]]+1:([0-9.]+)(-[0-9.]+)?[[:space:]]+([0-9.]+)(-[0-9.]+)?(mm)?",
        { 3, 4, 1 },
        false
    },
    {
        // [min aperture]-[max aperture]/[min focal]-[max focal]
        "([0-9.]+)(-[0-9.]+)?[[:space:]]*/[[:space:]]*([0-9.]+)(-[0-9.]+)?",
        { 3, 4, 1 },
        false
    },
};

static float _lf_parse_float (const char *model, const regmatch_t &match)
{
    char tmp [100];
    const char *src = model + match.rm_so;
    int len = match.rm_eo - match.rm_so;

    // Skip '-' since it's not a minus sign but rather the separator
    if (*src == '-')
        src++, len--;
    strncpy (tmp, src, len);
    tmp [len] = 0;

    return atof (tmp);
}

static bool _lf_parse_lens_name (const char *model,
                                 float &minf, float &maxf,
                                 float &mina)
{
    if (!model)
        return false;

    for (size_t i = 0; i < ARRAY_LEN (lens_name_regex); i++)
    {
        if (!lens_name_regex [i].compiled)
        {
            regcomp (&lens_name_regex [i].rex, lens_name_regex [i].regex,
                     REG_EXTENDED | REG_ICASE);
            lens_name_regex [i].compiled = true;
        }

        regmatch_t matches [10];
        if (regexec (&lens_name_regex [i].rex, model, 10, matches, 0))
            continue;

        guchar *matchidx = lens_name_regex [i].matchidx;
        if (matches [matchidx [0]].rm_so != -1)
            minf = _lf_parse_float (model, matches [matchidx [0]]);
        if (matches [matchidx [1]].rm_so != -1)
            maxf = _lf_parse_float (model, matches [matchidx [1]]);
        if (matches [matchidx [2]].rm_so != -1)
            mina = _lf_parse_float (model, matches [matchidx [2]]);
        return true;
    }

    return false;
}

static void _lf_free_lens_regex ()
{
    for (size_t i = 0; i < ARRAY_LEN (lens_name_regex); i++)
        if (lens_name_regex [i].compiled)
        {
            regfree (&lens_name_regex [i].rex);
            lens_name_regex [i].compiled = false;
        }
}

//------------------------------------------------------------------------//

static int _lf_lens_regex_refs = 0;

lfLens::lfLens ()
{
    // Defaults for attributes are "unknown" (mostly 0).  Otherwise, ad hoc
    // lfLens instances used for searches could not be matched against database
    // lenses easily.  If you need defaults for database tags, set them when
    // reading the database.
    memset (this, 0, sizeof (*this));
    Type = LF_UNKNOWN;
    _lf_lens_regex_refs++;
}

lfLens::~lfLens ()
{
    lf_free (Maker);
    lf_free (Model);
    _lf_list_free ((void **)Mounts);
    _lf_list_free ((void **)CalibDistortion);
    _lf_list_free ((void **)CalibTCA);
    _lf_list_free ((void **)CalibVignetting);
    _lf_list_free ((void **)CalibCrop);
    _lf_list_free ((void **)CalibFov);
    _lf_list_free ((void **)CalibRealFocal);
    if (!--_lf_lens_regex_refs)
        _lf_free_lens_regex ();
}

lfLens &lfLens::operator = (const lfLens &other)
{
    lf_free (Maker);
    Maker = lf_mlstr_dup (other.Maker);
    lf_free (Model);
    Model = lf_mlstr_dup (other.Model);
    MinFocal = other.MinFocal;
    MaxFocal = other.MaxFocal;
    MinAperture = other.MinAperture;
    MaxAperture = other.MaxAperture;

    lf_free (Mounts); Mounts = NULL;
    if (other.Mounts)
        for (int i = 0; other.Mounts [i]; i++)
            AddMount (other.Mounts [i]);

    CenterX = other.CenterX;
    CenterY = other.CenterY;
    CropFactor = other.CropFactor;
    AspectRatio = other.AspectRatio;
    Type = other.Type;

    lf_free (CalibDistortion); CalibDistortion = NULL;
    if (other.CalibDistortion)
        for (int i = 0; other.CalibDistortion [i]; i++)
            AddCalibDistortion (other.CalibDistortion [i]);
    lf_free (CalibTCA); CalibTCA = NULL;
    if (other.CalibTCA)
        for (int i = 0; other.CalibTCA [i]; i++)
            AddCalibTCA (other.CalibTCA [i]);
    lf_free (CalibVignetting); CalibVignetting = NULL;
    if (other.CalibVignetting)
        for (int i = 0; other.CalibVignetting [i]; i++)
            AddCalibVignetting (other.CalibVignetting [i]);
    lf_free (CalibCrop); CalibCrop = NULL;
    if (other.CalibCrop)
        for (int i = 0; other.CalibCrop [i]; i++)
            AddCalibCrop (other.CalibCrop [i]);
    lf_free (CalibFov); CalibFov = NULL;
    if (other.CalibFov)
        for (int i = 0; other.CalibFov [i]; i++)
            AddCalibFov (other.CalibFov [i]);
    lf_free (CalibRealFocal); CalibRealFocal = NULL;
    if (other.CalibRealFocal)
        for (int i = 0; other.CalibRealFocal [i]; i++)
            AddCalibRealFocal (other.CalibRealFocal [i]);

    return *this;
}

void lfLens::SetMaker (const char *val, const char *lang)
{
    Maker = lf_mlstr_add (Maker, lang, val);
}

void lfLens::SetModel (const char *val, const char *lang)
{
    Model = lf_mlstr_add (Model, lang, val);
}

void lfLens::AddMount (const char *val)
{
    if (val)
        _lf_addstr (&Mounts, val);
}

void lfLens::GuessParameters ()
{
    float minf = float (INT_MAX), maxf = float (INT_MIN);
    float mina = float (INT_MAX), maxa = float (INT_MIN);

    char *old_numeric = setlocale (LC_NUMERIC, NULL);
    old_numeric = strdup (old_numeric);
    setlocale (LC_NUMERIC, "C");

    if (Model && (!MinAperture || !MinFocal) &&
        !strstr (Model, "adapter") &&
        !strstr (Model, "reducer") &&
        !strstr (Model, "booster") &&
        !strstr (Model, "extender") &&
        !strstr (Model, "converter"))
        _lf_parse_lens_name (Model, minf, maxf, mina);

    if (!MinAperture || !MinFocal)
    {
        // Try to find out the range of focal lengths using calibration data
        if (CalibDistortion)
            for (int i = 0; CalibDistortion [i]; i++)
            {
                float f = CalibDistortion [i]->Focal;
                if (f < minf)
                    minf = f;
                if (f > maxf)
                    maxf = f;
            }
        if (CalibTCA)
            for (int i = 0; CalibTCA [i]; i++)
            {
                float f = CalibTCA [i]->Focal;
                if (f < minf)
                    minf = f;
                if (f > maxf)
                    maxf = f;
            }
        if (CalibVignetting)
            for (int i = 0; CalibVignetting [i]; i++)
            {
                float f = CalibVignetting [i]->Focal;
                float a = CalibVignetting [i]->Aperture;
                if (f < minf)
                    minf = f;
                if (f > maxf)
                    maxf = f;
                if (a < mina)
                    mina = a;
                if (a > maxa)
                    maxa = a;
            }
        if (CalibCrop)
            for (int i=0; CalibCrop [i]; i++)
            {
                float f = CalibCrop [i]->Focal;
                if (f < minf)
                    minf = f;
                if (f > maxf)
                    maxf = f;
            }
        if (CalibFov)
            for (int i=0; CalibFov [i]; i++)
            {
                float f = CalibFov [i]->Focal;
                if (f < minf)
                    minf = f;
                if (f > maxf)
                    maxf = f;
            }
        if (CalibRealFocal)
            for (int i=0; CalibRealFocal [i]; i++)
            {
                float f = CalibRealFocal [i]->Focal;
                if (f < minf)
                    minf = f;
                if (f > maxf)
                    maxf = f;
            }

    }

    if (minf != INT_MAX && !MinFocal)
        MinFocal = minf;
    if (maxf != INT_MIN && !MaxFocal)
        MaxFocal = maxf;
    if (mina != INT_MAX && !MinAperture)
        MinAperture = mina;
    if (maxa != INT_MIN && !MaxAperture)
        MaxAperture = maxa;

    if (!MaxFocal)
        MaxFocal = MinFocal;

    setlocale (LC_NUMERIC, old_numeric);
    free (old_numeric);
}

bool lfLens::Check ()
{
    GuessParameters ();

    if (!Model || !Mounts || CropFactor <= 0 ||
        MinFocal > MaxFocal || (MaxAperture && MinAperture > MaxAperture) ||
        AspectRatio < 1)
        return false;

    return true;
}

const char *lfLens::GetDistortionModelDesc (
    lfDistortionModel model, const char **details, const lfParameter ***params)
{
    static const lfParameter *param_none [] = { NULL };

    static const lfParameter param_poly3_k1 = { "k1", -0.2F, 0.2F, 0.0F };
    static const lfParameter *param_poly3 [] = { &param_poly3_k1, NULL };

    static const lfParameter param_poly5_k2 = { "k2", -0.2F, 0.2F, 0.0F };
    static const lfParameter *param_poly5 [] = { &param_poly3_k1, &param_poly5_k2, NULL };

    static const lfParameter param_ptlens_a = { "a", -0.2F, 0.2F, 0.0F };
    static const lfParameter param_ptlens_b = { "b", -0.2F, 0.2F, 0.0F };
    static const lfParameter param_ptlens_c = { "c", -0.2F, 0.2F, 0.0F };
    static const lfParameter *param_ptlens [] = {
        &param_ptlens_a, &param_ptlens_b, &param_ptlens_c, NULL };

    switch (model)
    {
        case LF_DIST_MODEL_NONE:
            if (details)
                *details = "No distortion model";
            if (params)
                *params = param_none;
            return "None";

        case LF_DIST_MODEL_POLY3:
            if (details)
                *details = "Rd = Ru * (1 - k1 + k1 * Ru^2)\n"
                    "Ref: http://www.imatest.com/docs/distortion.html";
            if (params)
                *params = param_poly3;
            return "3rd order polynomial";

        case LF_DIST_MODEL_POLY5:
            if (details)
                *details = "Rd = Ru * (1 + k1 * Ru^2 + k2 * Ru^4)\n"
                    "Ref: http://www.imatest.com/docs/distortion.html";
            if (params)
                *params = param_poly5;
            return "5th order polynomial";

        case LF_DIST_MODEL_PTLENS:
            if (details)
                *details = "Rd = Ru * (a * Ru^3 + b * Ru^2 + c * Ru + 1 - (a + b + c))\n"
                    "Ref: http://wiki.panotools.org/Lens_correction_model";
            if (params)
                *params = param_ptlens;
            return "PanoTools lens model";

        default:
            // keep gcc 4.4 happy
            break;
    }

    if (details)
        *details = NULL;
    if (params)
        *params = NULL;
    return NULL;
}

const char *lfLens::GetTCAModelDesc (
    lfTCAModel model, const char **details, const lfParameter ***params)
{
    static const lfParameter *param_none [] = { NULL };

    static const lfParameter param_linear_kr = { "kr", 0.99F, 1.01F, 1.0F };
    static const lfParameter param_linear_kb = { "kb", 0.99F, 1.01F, 1.0F };
    static const lfParameter *param_linear [] =
    { &param_linear_kr, &param_linear_kb, NULL };

    static const lfParameter param_poly3_br = { "br", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_poly3_cr = { "cr", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_poly3_vr = { "vr",  0.99F, 1.01F, 1.0F };
    static const lfParameter param_poly3_bb = { "bb", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_poly3_cb = { "cb", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_poly3_vb = { "vb",  0.99F, 1.01F, 1.0F };
    static const lfParameter *param_poly3 [] =
    {
        &param_poly3_vr, &param_poly3_vb,
        &param_poly3_cr, &param_poly3_cb,
        &param_poly3_br, &param_poly3_bb,
        NULL
    };

    switch (model)
    {
        case LF_TCA_MODEL_NONE:
            if (details)
                *details = "No transversal chromatic aberration model";
            if (params)
                *params = param_none;
            return "None";

        case LF_TCA_MODEL_LINEAR:
            if (details)
                *details = "Cd = Cs * k\n"
                    "Ref: http://cipa.icomos.org/fileadmin/papers/Torino2005/403.pdf";
            if (params)
                *params = param_linear;
            return "Linear";

        case LF_TCA_MODEL_POLY3:
            if (details)
                *details = "Cd = Cs^3 * b + Cs^2 * c + Cs * v\n"
                    "Ref: http://wiki.panotools.org/Tca_correct";
            if (params)
                *params = param_poly3;
            return "3rd order polynomial";

        default:
            // keep gcc 4.4 happy
            break;
    }

    if (details)
        *details = NULL;
    if (params)
        *params = NULL;
    return NULL;
}

const char *lfLens::GetVignettingModelDesc (
    lfVignettingModel model, const char **details, const lfParameter ***params)
{
    static const lfParameter *param_none [] = { NULL };

    static const lfParameter param_pa_k1 = { "k1", -1.0, 2.0, 0.0 };
    static const lfParameter param_pa_k2 = { "k2", -1.0, 2.0, 0.0 };
    static const lfParameter param_pa_k3 = { "k3", -1.0, 2.0, 0.0 };
    static const lfParameter *param_pa [] =
    { &param_pa_k1, &param_pa_k2, &param_pa_k3, NULL };

    switch (model)
    {
        case LF_VIGNETTING_MODEL_NONE:
            if (details)
                *details = "No vignetting model";
            if (params)
                *params = param_none;
            return "None";

        case LF_VIGNETTING_MODEL_PA:
            if (details)
                *details = "Pablo D'Angelo vignetting model\n"
                    "(which is a more general variant of the cos^4 law):\n"
                    "Cd = Cs * (1 + k1 * R^2 + k2 * R^4 + k3 * R^6)\n"
                    "Ref: http://hugin.sourceforge.net/tech/";
            if (params)
                *params = param_pa;
            return "6th order polynomial";

        default:
            // keep gcc 4.4 happy
            break;
    }

    if (details)
        *details = "";
    if (params)
        *params = NULL;
    return NULL;
}

const char *lfLens::GetCropDesc (
    lfCropMode mode , const char **details, const lfParameter ***params)
{
    static const lfParameter *param_none [] = { NULL };

    static const lfParameter param_crop_left = { "left", -1.0F, 1.0F, 0.0F };
    static const lfParameter param_crop_right = { "right", 0.0F, 2.0F, 0.0F };
    static const lfParameter param_crop_top = { "top", -1.0F, 1.0F, 0.0F };
    static const lfParameter param_crop_bottom = { "bottom", 0.0F, 2.0F, 0.0F };
    static const lfParameter *param_crop [] = { &param_crop_left, &param_crop_right, &param_crop_top, &param_crop_bottom, NULL };

    switch (mode)
    {
        case LF_NO_CROP:
            if (details)
                *details = "No crop";
            if (params)
                *params = param_none;
            return "No crop";

        case LF_CROP_RECTANGLE:
            if (details)
                *details = "Rectangular crop area";
            if (params)
                *params = param_crop;
            return "rectangular crop";

        case LF_CROP_CIRCLE:
            if (details)
                *details = "Circular crop area";
            if (params)
                *params = param_crop;
            return "circular crop";

        default:
            // keep gcc 4.4 happy
            break;
    }

    if (details)
        *details = NULL;
    if (params)
        *params = NULL;
    return NULL;
}

const char *lfLens::GetLensTypeDesc (lfLensType type, const char **details)
{
    switch (type)
    {
        case LF_UNKNOWN:
            if (details)
                *details = "";
            return "Unknown";

        case LF_RECTILINEAR:
            if (details)
                *details = "Ref: http://wiki.panotools.org/Rectilinear_Projection";
            return "Rectilinear";

        case LF_FISHEYE:
            if (details)
                *details = "Ref: http://wiki.panotools.org/Fisheye_Projection";
            return "Fish-Eye";

        case LF_PANORAMIC:
            if (details)
                *details = "Ref: http://wiki.panotools.org/Cylindrical_Projection";
            return "Panoramic";

        case LF_EQUIRECTANGULAR:
            if (details)
                *details = "Ref: http://wiki.panotools.org/Equirectangular_Projection";
            return "Equirectangular";

        case LF_FISHEYE_ORTHOGRAPHIC:
            if (details)
                *details = "Ref: http://wiki.panotools.org/Fisheye_Projection";
            return "Fisheye, orthographic";

        case LF_FISHEYE_STEREOGRAPHIC:
            if (details)
                *details = "Ref: http://wiki.panotools.org/Stereographic_Projection";
            return "Fisheye, stereographic";

        case LF_FISHEYE_EQUISOLID:
            if (details)
                *details = "Ref: http://wiki.panotools.org/Fisheye_Projection";
            return "Fisheye, equisolid";

        case LF_FISHEYE_THOBY:
            if (details)
                *details = "Ref: http://groups.google.com/group/hugin-ptx/browse_thread/thread/bd822d178e3e239d";
            return "Thoby-Fisheye";

        default:
            // keep gcc 4.4 happy
            break;
    }

    if (details)
        *details = "";
    return NULL;
}

static bool cmp_distortion (const void *x1, const void *x2)
{
    const lfLensCalibDistortion *d1 = static_cast<const lfLensCalibDistortion *> (x1);
    const lfLensCalibDistortion *d2 = static_cast<const lfLensCalibDistortion *> (x2);
    return (d1->Focal == d2->Focal);
}

void lfLens::AddCalibDistortion (const lfLensCalibDistortion *dc)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibDistortion ***cd;
        void ***arr;
    } x = { &CalibDistortion };
    _lf_addobj (x.arr, dc, sizeof (*dc), cmp_distortion);
}

bool lfLens::RemoveCalibDistortion (int idx)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibDistortion ***cd;
        void ***arr;
    } x = { &CalibDistortion };
    return _lf_delobj (x.arr, idx);
}

static bool cmp_tca (const void *x1, const void *x2)
{
    const lfLensCalibTCA *t1 = static_cast<const lfLensCalibTCA *> (x1);
    const lfLensCalibTCA *t2 = static_cast<const lfLensCalibTCA *> (x2);
    return (t1->Focal == t2->Focal);
}

void lfLens::AddCalibTCA (const lfLensCalibTCA *tcac)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibTCA ***ctca;
        void ***arr;
    } x = { &CalibTCA };
    _lf_addobj (x.arr, tcac, sizeof (*tcac), cmp_tca);
}

bool lfLens::RemoveCalibTCA (int idx)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibTCA ***ctca;
        void ***arr;
    } x = { &CalibTCA };
    return _lf_delobj (x.arr, idx);
}

static bool cmp_vignetting (const void *x1, const void *x2)
{
    const lfLensCalibVignetting *v1 = static_cast<const lfLensCalibVignetting *> (x1);
    const lfLensCalibVignetting *v2 = static_cast<const lfLensCalibVignetting *> (x2);
    return (v1->Focal == v2->Focal) &&
           (v1->Distance == v2->Distance) &&
           (v1->Aperture == v2->Aperture);
}

void lfLens::AddCalibVignetting (const lfLensCalibVignetting *vc)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibVignetting ***cv;
        void ***arr;
    } x = { &CalibVignetting };
    _lf_addobj (x.arr, vc, sizeof (*vc), cmp_vignetting);
}

bool lfLens::RemoveCalibVignetting (int idx)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibVignetting ***cv;
        void ***arr;
    } x = { &CalibVignetting };
    return _lf_delobj (x.arr, idx);
}

static bool cmp_lenscrop (const void *x1, const void *x2)
{
    const lfLensCalibCrop *d1 = static_cast<const lfLensCalibCrop *> (x1);
    const lfLensCalibCrop *d2 = static_cast<const lfLensCalibCrop *> (x2);
    return (d1->Focal == d2->Focal);
}

void lfLens::AddCalibCrop (const lfLensCalibCrop *lcc)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibCrop ***cd;
        void ***arr;
    } x = { &CalibCrop };
    _lf_addobj (x.arr, lcc, sizeof (*lcc), cmp_lenscrop);
}

bool lfLens::RemoveCalibCrop (int idx)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibCrop ***cd;
        void ***arr;
    } x = { &CalibCrop };
    return _lf_delobj (x.arr, idx);
}

static bool cmp_lensfov (const void *x1, const void *x2)
{
    const lfLensCalibFov *d1 = static_cast<const lfLensCalibFov *> (x1);
    const lfLensCalibFov *d2 = static_cast<const lfLensCalibFov *> (x2);
    return (d1->Focal == d2->Focal);
}

void lfLens::AddCalibFov (const lfLensCalibFov *lcf)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibFov ***cd;
        void ***arr;
    } x = { &CalibFov };
    _lf_addobj (x.arr, lcf, sizeof (*lcf), cmp_lensfov);
}

bool lfLens::RemoveCalibFov (int idx)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibFov ***cd;
        void ***arr;
    } x = { &CalibFov };
    return _lf_delobj (x.arr, idx);
}

static bool cmp_real_focal (const void *x1, const void *x2)
{
    const lfLensCalibRealFocal *d1 = static_cast<const lfLensCalibRealFocal *> (x1);
    const lfLensCalibRealFocal *d2 = static_cast<const lfLensCalibRealFocal *> (x2);
    return (d1->Focal == d2->Focal);
}

void lfLens::AddCalibRealFocal (const lfLensCalibRealFocal *lcf)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibRealFocal ***cd;
        void ***arr;
    } x = { &CalibRealFocal };
    _lf_addobj (x.arr, lcf, sizeof (*lcf), cmp_real_focal);
}

bool lfLens::RemoveCalibRealFocal (int idx)
{
    // Avoid "dereferencing type-punned pointer will break strict-aliasing rules" warning
    union
    {
        lfLensCalibRealFocal ***cd;
        void ***arr;
    } x = { &CalibRealFocal };
    return _lf_delobj (x.arr, idx);
}

static int __insert_spline (void **spline, float *spline_dist, float dist, void *val)
{
    if (dist < 0)
    {
        if (dist > spline_dist [1])
        {
            spline_dist [0] = spline_dist [1];
            spline_dist [1] = dist;
            spline [0] = spline [1];
            spline [1] = val;
            return 1;
        }
        else if (dist > spline_dist [0])
        {
            spline_dist [0] = dist;
            spline [0] = val;
            return 0;
        }
    }
    else
    {
        if (dist < spline_dist [2])
        {
            spline_dist [3] = spline_dist [2];
            spline_dist [2] = dist;
            spline [3] = spline [2];
            spline [2] = val;
            return 2;
        }
        else if (dist < spline_dist [3])
        {
            spline_dist [3] = dist;
            spline [3] = val;
            return 3;
        }
    }
    return -1;
}

static void __parameter_scales (float values [], int number_of_values,
                                int type, int model, int index)
{
    switch (type)
    {
    case LF_MODIFY_DISTORTION:
        switch (model)
        {
        case LF_DIST_MODEL_POLY3:
        case LF_DIST_MODEL_POLY5:
        case LF_DIST_MODEL_PTLENS:
            break;
        }
        break;

    case LF_MODIFY_TCA:
        switch (model)
        {
        case LF_TCA_MODEL_LINEAR:
        case LF_TCA_MODEL_POLY3:
            if (index < 2)
                for (int i=0; i < number_of_values; i++)
                    values [i] = 1.0;
            break;
        }
        break;
    }
}

bool lfLens::InterpolateDistortion (float focal, lfLensCalibDistortion &res) const
{
    if (!CalibDistortion)
        return false;

    union
    {
        lfLensCalibDistortion *spline [4];
        void *spline_ptr [4];
    };
    float spline_dist [4] = { -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX };
    lfDistortionModel dm = LF_DIST_MODEL_NONE;

    memset (spline, 0, sizeof (spline));
    for (int i = 0; CalibDistortion [i]; i++)
    {
        lfLensCalibDistortion *c = CalibDistortion [i];
        if (c->Model == LF_DIST_MODEL_NONE)
            continue;

        // Take into account just the first encountered lens model
        if (dm == LF_DIST_MODEL_NONE)
            dm = c->Model;
        else if (dm != c->Model)
        {
            g_warning ("[Lensfun] lens %s/%s has multiple distortion models defined\n",
                       Maker, Model);
            continue;
        }

        float df = focal - c->Focal;
        if (df == 0.0)
        {
            // Exact match found, don't care to interpolate
            res = *c;
            return true;
        }

        __insert_spline (spline_ptr, spline_dist, df, c);
    }

    if (!spline [1] || !spline [2])
    {
        if (spline [1])
            res = *spline [1];
        else if (spline [2])
            res = *spline [2];
        else
            return false;

        return true;
    }

    // No exact match found, interpolate the model parameters
    res.Model = dm;
    res.Focal = focal;

    float t = (focal - spline [1]->Focal) / (spline [2]->Focal - spline [1]->Focal);

    for (size_t i = 0; i < ARRAY_LEN (res.Terms); i++)
    {
        float values [5] = {spline [0] ? spline [0]->Focal : NAN, spline [1]->Focal,
                            spline [2]->Focal, spline [3] ? spline [3]->Focal : NAN,
                            focal};
        __parameter_scales (values, 5, LF_MODIFY_DISTORTION, dm, i);
        res.Terms [i] = _lf_interpolate (
            spline [0] ? spline [0]->Terms [i] * values [0] : FLT_MAX,
            spline [1]->Terms [i] * values [1], spline [2]->Terms [i] * values [2],
            spline [3] ? spline [3]->Terms [i] * values [3] : FLT_MAX,
            t) / values [4];
    }

    return true;
}

bool lfLens::InterpolateTCA (float focal, lfLensCalibTCA &res) const
{
    if (!CalibTCA)
        return false;

    union
    {
        lfLensCalibTCA *spline [4];
        void *spline_ptr [4];
    };
    float spline_dist [4] = { -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX };
    lfTCAModel tcam = LF_TCA_MODEL_NONE;

    memset (&spline, 0, sizeof (spline));
    for (int i = 0; CalibTCA [i]; i++)
    {
        lfLensCalibTCA *c = CalibTCA [i];
        if (c->Model == LF_TCA_MODEL_NONE)
            continue;

        // Take into account just the first encountered lens model
        if (tcam == LF_TCA_MODEL_NONE)
            tcam = c->Model;
        else if (tcam != c->Model)
        {
            g_warning ("[Lensfun] lens %s/%s has multiple TCA models defined\n",
                       Maker, Model);
            continue;
        }

        float df = focal - c->Focal;
        if (df == 0.0)
        {
            // Exact match found, don't care to interpolate
            res = *c;
            return true;
        }

        __insert_spline (spline_ptr, spline_dist, df, c);
    }

    if (!spline [1] || !spline [2])
    {
        if (spline [1])
            res = *spline [1];
        else if (spline [2])
            res = *spline [2];
        else
            return false;

        return true;
    }

    // No exact match found, interpolate the model parameters
    res.Model = tcam;
    res.Focal = focal;

    float t = (focal - spline [1]->Focal) / (spline [2]->Focal - spline [1]->Focal);

    for (size_t i = 0; i < ARRAY_LEN (res.Terms); i++)
    {
        float values [5] = {spline [0] ? spline [0]->Focal : NAN, spline [1]->Focal,
                            spline [2]->Focal, spline [3] ? spline [3]->Focal : NAN,
                            focal};
        __parameter_scales (values, 5, LF_MODIFY_TCA, tcam, i);
        res.Terms [i] = _lf_interpolate (
            spline [0] ? spline [0]->Terms [i] * values [0] : FLT_MAX,
            spline [1]->Terms [i] * values [1], spline [2]->Terms [i] * values [2],
            spline [3] ? spline [3]->Terms [i] * values [3] : FLT_MAX,
            t) / values [4];
    }

    return true;
}

static float __vignetting_dist (
    const lfLens *l, const lfLensCalibVignetting &x, float focal, float aperture, float distance)
{
    // translate every value to linear scale and normalize
    // approximatively to range 0..1
    float f1 = focal - l->MinFocal;
    float f2 = x.Focal - l->MinFocal;
    float df = l->MaxFocal - l->MinFocal;
    if (df != 0)
    {
        f1 /= df;
        f2 /= df;
    }
    float a1 = 4.0 / aperture;
    float a2 = 4.0 / x.Aperture;
    float d1 = 0.1 / distance;
    float d2 = 0.1 / x.Distance;

    return sqrt (square (f2 - f1) + square (a2 - a1) + square (d2 - d1));
}

bool lfLens::InterpolateVignetting (
    float focal, float aperture, float distance, lfLensCalibVignetting &res) const
{
    if (!CalibVignetting)
        return false;

    lfVignettingModel vm = LF_VIGNETTING_MODEL_NONE;
    res.Focal = focal;
    res.Aperture = aperture;
    res.Distance = distance;
    for (size_t i = 0; i < ARRAY_LEN (res.Terms); i++)
        res.Terms [i] = 0;

    // Use http://en.wikipedia.org/wiki/Inverse_distance_weighting with
    // p = 3.5.
    float total_weighting = 0;
    const float power = 3.5;

    float smallest_interpolation_distance = FLT_MAX;
    for (int i = 0; CalibVignetting [i]; i++)
    {
        const lfLensCalibVignetting* c = CalibVignetting [i];
        // Take into account just the first encountered lens model
        if (vm == LF_VIGNETTING_MODEL_NONE)
	    {
            vm = c->Model;
	        res.Model = vm;
        } 
        else if (vm != c->Model)
        {
            g_warning ("[Lensfun] lens %s/%s has multiple vignetting models defined\n",
                       Maker, Model);
            continue;
        }

	    float interpolation_distance = __vignetting_dist (this, *c, focal, aperture, distance);
	    if (interpolation_distance < 0.0001) {
	        res = *c;
	        return true;
	    }
	    
	    smallest_interpolation_distance = std::min(smallest_interpolation_distance, interpolation_distance);
	    float weighting = fabs (1.0 / pow (interpolation_distance, power));
	    for (size_t i = 0; i < ARRAY_LEN (res.Terms); i++)
	        res.Terms [i] += weighting * c->Terms [i];
	    total_weighting += weighting;
    }
    
    if (smallest_interpolation_distance > 1)
        return false;
    
    if (total_weighting > 0 && smallest_interpolation_distance < FLT_MAX)
    {
	    for (size_t i = 0; i < ARRAY_LEN (res.Terms); i++)
	        res.Terms [i] /= total_weighting;
	    return true;
    } else 
        return false;
}

bool lfLens::InterpolateCrop (float focal, lfLensCalibCrop &res) const
{
    if (!CalibCrop)
        return false;

    union
    {
        lfLensCalibCrop *spline [4];
        void *spline_ptr [4];
    };
    float spline_dist [4] = { -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX };
    lfCropMode cm = LF_NO_CROP;

    memset (spline, 0, sizeof (spline));
    for (int i = 0; CalibCrop [i]; i++)
    {
        lfLensCalibCrop *c = CalibCrop [i];
        if (c->CropMode == LF_NO_CROP)
            continue;

        // Take into account just the first encountered crop mode
        if (cm == LF_NO_CROP)
            cm = c->CropMode;
        else if (cm != c->CropMode)
        {
            g_warning ("[Lensfun] lens %s/%s has multiple crop modes defined\n",
                       Maker, Model);
            continue;
        }

        float df = focal - c->Focal;
        if (df == 0.0)
        {
            // Exact match found, don't care to interpolate
            res = *c;
            return true;
        }

        __insert_spline (spline_ptr, spline_dist, df, c);
    }

    if (!spline [1] || !spline [2])
    {
        if (spline [1])
            res = *spline [1];
        else if (spline [2])
            res = *spline [2];
        else
            return false;

        return true;
    }

    // No exact match found, interpolate the model parameters
    res.CropMode = cm;
    res.Focal = focal;

    float t = (focal - spline [1]->Focal) / (spline [2]->Focal - spline [1]->Focal);

    for (size_t i = 0; i < ARRAY_LEN (res.Crop); i++)
        res.Crop [i] = _lf_interpolate (
            spline [0] ? spline [0]->Crop [i] : FLT_MAX,
            spline [1]->Crop [i], spline [2]->Crop [i],
            spline [3] ? spline [3]->Crop [i] : FLT_MAX, t);

    return true;
}

bool lfLens::InterpolateFov (float focal, lfLensCalibFov &res) const
{
    if (!CalibFov)
        return false;

    union
    {
        lfLensCalibFov *spline [4];
        void *spline_ptr [4];
    };
    float spline_dist [4] = { -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX };

    memset (spline, 0, sizeof (spline));
    int counter=0;
    for (int i = 0; CalibFov [i]; i++)
    {
        lfLensCalibFov *c = CalibFov [i];
        if (c->FieldOfView == 0)
            continue;

        counter++;
        float df = focal - c->Focal;
        if (df == 0.0)
        {
            // Exact match found, don't care to interpolate
            res = *c;
            return true;
        }

        __insert_spline (spline_ptr, spline_dist, df, c);
    }

    //no valid data found
    if (counter==0)
        return false;

    if (!spline [1] || !spline [2])
    {
        if (spline [1])
            res = *spline [1];
        else if (spline [2])
            res = *spline [2];
        else
            return false;

        return true;
    }

    // No exact match found, interpolate the model parameters
    res.Focal = focal;

    float t = (focal - spline [1]->Focal) / (spline [2]->Focal - spline [1]->Focal);

    res.FieldOfView = _lf_interpolate (
        spline [0] ? spline [0]->FieldOfView : FLT_MAX,
        spline [1]->FieldOfView, spline [2]->FieldOfView,
        spline [3] ? spline [3]->FieldOfView : FLT_MAX, t);

    return true;
}

bool lfLens::InterpolateRealFocal (float focal, lfLensCalibRealFocal &res) const
{
    if (!CalibRealFocal)
        return false;

    union
    {
        lfLensCalibRealFocal *spline [4];
        void *spline_ptr [4];
    };
    float spline_dist [4] = { -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX };

    memset (spline, 0, sizeof (spline));
    int counter=0;
    for (int i = 0; CalibRealFocal [i]; i++)
    {
        lfLensCalibRealFocal *c = CalibRealFocal [i];
        if (c->RealFocal == 0)
            continue;

        counter++;
        float df = focal - c->Focal;
        if (df == 0.0)
        {
            // Exact match found, don't care to interpolate
            res = *c;
            return true;
        }

        __insert_spline (spline_ptr, spline_dist, df, c);
    }

    //no valid data found
    if (counter==0)
        return false;

    if (!spline [1] || !spline [2])
    {
        if (spline [1])
            res = *spline [1];
        else if (spline [2])
            res = *spline [2];
        else
            return false;

        return true;
    }

    // No exact match found, interpolate the model parameters
    res.Focal = focal;

    float t = (focal - spline [1]->Focal) / (spline [2]->Focal - spline [1]->Focal);

    res.RealFocal = _lf_interpolate (
        spline [0] ? spline [0]->RealFocal : FLT_MAX,
        spline [1]->RealFocal, spline [2]->RealFocal,
        spline [3] ? spline [3]->RealFocal : FLT_MAX, t);

    return true;
}

gint _lf_lens_parameters_compare (const lfLens *i1, const lfLens *i2)
{
    int cmp = int ((i1->MinFocal - i2->MinFocal) * 100);
    if (cmp != 0)
        return cmp;

    cmp = int ((i1->MaxFocal - i2->MaxFocal) * 100);
    if (cmp != 0)
        return cmp;

    return int ((i1->MinAperture - i2->MinAperture) * 100);

    // MaxAperture is usually not given in database...
    // so it's a guessed value, often incorrect.
}

gint _lf_lens_name_compare (const lfLens *i1, const lfLens *i2)
{
    int cmp = _lf_strcmp (i1->Maker, i2->Maker);
    if (cmp != 0)
        return cmp;

    return _lf_strcmp (i1->Model, i2->Model);
}

gint _lf_lens_compare (gconstpointer a, gconstpointer b)
{
    lfLens *i1 = (lfLens *)a;
    lfLens *i2 = (lfLens *)b;

    int cmp = _lf_lens_name_compare (i1, i2);
    if (cmp != 0)
        return cmp;

    return int ((i1->CropFactor - i2->CropFactor) * 100);
}

static int _lf_compare_num (float a, float b)
{
    if (!a || !b)
        return 0; // neutral

    float r = a / b;
    if (r <= 0.99 || r >= 1.01)
        return -1; // strong no
    return +1; // strong yes
}

int _lf_lens_compare_score (const lfLens *pattern, const lfLens *match,
                            lfFuzzyStrCmp *fuzzycmp, const char **compat_mounts)
{
    int score = 0;

    // Compare numeric fields first since that's easy.

    if (pattern->Type != LF_UNKNOWN)
        if (pattern->Type != match->Type)
            return 0;

    if (pattern->CropFactor > 0.01 && pattern->CropFactor < match->CropFactor * 0.96)
        return 0;

    if (pattern->CropFactor >= match->CropFactor * 1.41)
        score += 2;
    else if (pattern->CropFactor >= match->CropFactor * 1.31)
        score += 4;
    else if (pattern->CropFactor >= match->CropFactor * 1.21)
        score += 6;
    else if (pattern->CropFactor >= match->CropFactor * 1.11)
        score += 8;
    else if (pattern->CropFactor >= match->CropFactor * 1.01)
        score += 10;
    else if (pattern->CropFactor >= match->CropFactor)
        score += 5;
    else if (pattern->CropFactor >= match->CropFactor * 0.96)
        score += 3;

    switch (_lf_compare_num (pattern->MinFocal, match->MinFocal))
    {
        case -1:
            return 0;

        case +1:
            score += 10;
            break;
    }

    switch (_lf_compare_num (pattern->MaxFocal, match->MaxFocal))
    {
        case -1:
            return 0;

        case +1:
            score += 10;
            break;
    }

    switch (_lf_compare_num (pattern->MinAperture, match->MinAperture))
    {
        case -1:
            return 0;

        case +1:
            score += 10;
            break;
    }

    switch (_lf_compare_num (pattern->MaxAperture, match->MaxAperture))
    {
        case -1:
            return 0;

        case +1:
            score += 10;
            break;
    }

    switch (_lf_compare_num (pattern->AspectRatio, match->AspectRatio))
    {
        case -1:
            return 0;

        case +1:
            score += 10;
            break;
    }

    if (compat_mounts && !compat_mounts [0])
        compat_mounts = NULL;

    // Check the lens mount, if specified
    if (match->Mounts && (pattern->Mounts || compat_mounts))
    {
        bool matching_mount_found = false;

        if (pattern->Mounts)
            for (int i = 0; pattern->Mounts [i]; i++)
                for (int j = 0; match->Mounts [j]; j++)
                    if (!_lf_strcmp (pattern->Mounts [i], match->Mounts [j]))
                    {
                        matching_mount_found = true;
                        score += 10;
                        goto exit_mount_search;
                    }

        if (compat_mounts)
            for (int i = 0; compat_mounts [i]; i++)
                for (int j = 0; match->Mounts [j]; j++)
                    if (!_lf_strcmp (compat_mounts [i], match->Mounts [j]))
                    {
                        matching_mount_found = true;
                        score += 5;
                        goto exit_mount_search;
                    }

    exit_mount_search:
        if (!matching_mount_found)
            return 0;
    }

    // If maker is specified, check it using our patented _lf_strcmp(tm) technology
    if (pattern->Maker && match->Maker)
    {
        if (_lf_mlstrcmp (pattern->Maker, match->Maker) != 0)
            return 0; // Bah! different maker.
        else
            score += 10; // Good doggy, here's a cookie
    }

    // And now the most complex part - compare models
    if (pattern->Model && match->Model)
    {
        int _score = fuzzycmp->Compare (match->Model);
        if (!_score)
            return 0; // Model does not match
        _score = (_score * 4) / 10;
        if (!_score)
            _score = 1;
        score += _score;
    }

    return score;
}

//---------------------------// The C interface //---------------------------//

lfLens *lf_lens_new ()
{
    return new lfLens ();
}

void lf_lens_destroy (lfLens *lens)
{
    delete lens;
}

void lf_lens_copy (lfLens *dest, const lfLens *source)
{
    *dest = *source;
}

void lf_lens_guess_parameters (lfLens *lens)
{
    lens->GuessParameters ();
}

cbool lf_lens_check (lfLens *lens)
{
    return lens->Check ();
}

const char *lf_get_distortion_model_desc (
    enum lfDistortionModel model, const char **details, const lfParameter ***params)
{
    return lfLens::GetDistortionModelDesc (model, details, params);
}

const char *lf_get_tca_model_desc (
    enum lfTCAModel model, const char **details, const lfParameter ***params)
{
    return lfLens::GetTCAModelDesc (model, details, params);
}

const char *lf_get_vignetting_model_desc (
    enum lfVignettingModel model, const char **details, const lfParameter ***params)
{
    return lfLens::GetVignettingModelDesc (model, details, params);
}

const char *lf_get_crop_desc (
    enum lfCropMode mode, const char **details, const lfParameter ***params)
{
    return lfLens::GetCropDesc (mode, details, params);
}

const char *lf_get_lens_type_desc (enum lfLensType type, const char **details)
{
    return lfLens::GetLensTypeDesc (type, details);
}

cbool lf_lens_interpolate_distortion (const lfLens *lens, float focal,
    lfLensCalibDistortion *res)
{
    return lens->InterpolateDistortion (focal, *res);
}

cbool lf_lens_interpolate_tca (const lfLens *lens, float focal, lfLensCalibTCA *res)
{
    return lens->InterpolateTCA (focal, *res);
}

cbool lf_lens_interpolate_vignetting (const lfLens *lens, float focal, float aperture,
    float distance, lfLensCalibVignetting *res)
{
    return lens->InterpolateVignetting (focal, aperture, distance, *res);
}

cbool lf_lens_interpolate_crop (const lfLens *lens, float focal,
    lfLensCalibCrop *res)
{
    return lens->InterpolateCrop (focal, *res);
}

cbool lf_lens_interpolate_fov (const lfLens *lens, float focal,
    lfLensCalibFov *res)
{
    return lens->InterpolateFov (focal, *res);
}

cbool lf_lens_interpolate_real_focal (const lfLens *lens, float focal,
    lfLensCalibRealFocal *res)
{
    return lens->InterpolateRealFocal (focal, *res);
}

void lf_lens_add_calib_distortion (lfLens *lens, const lfLensCalibDistortion *dc)
{
    lens->AddCalibDistortion (dc);
}

cbool lf_lens_remove_calib_distortion (lfLens *lens, int idx)
{
    return lens->RemoveCalibDistortion (idx);
}

void lf_lens_add_calib_tca (lfLens *lens, const lfLensCalibTCA *tcac)
{
    lens->AddCalibTCA (tcac);
}

cbool lf_lens_remove_calib_tca (lfLens *lens, int idx)
{
    return lens->RemoveCalibTCA (idx);
}

void lf_lens_add_calib_vignetting (lfLens *lens, const lfLensCalibVignetting *vc)
{
    lens->AddCalibVignetting (vc);
}

cbool lf_lens_remove_calib_vignetting (lfLens *lens, int idx)
{
    return lens->RemoveCalibVignetting (idx);
}

void lf_lens_add_calib_crop (lfLens *lens, const lfLensCalibCrop *lcc)
{
    lens->AddCalibCrop (lcc);
}

cbool lf_lens_remove_calib_crop (lfLens *lens, int idx)
{
    return lens->RemoveCalibCrop (idx);
}


void lf_lens_add_calib_fov (lfLens *lens, const lfLensCalibFov *lcf)
{
    lens->AddCalibFov (lcf);
}

cbool lf_lens_remove_calib_fov (lfLens *lens, int idx)
{
    return lens->RemoveCalibFov (idx);
}

void lf_lens_add_calib_real_focal (lfLens *lens, const lfLensCalibRealFocal *lcf)
{
    lens->AddCalibRealFocal (lcf);
}

cbool lf_lens_remove_calib_real_focal (lfLens *lens, int idx)
{
    return lens->RemoveCalibRealFocal (idx);
}

