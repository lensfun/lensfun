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
#include <locale.h>
#include <math.h>

lfLens::lfLens ()
{
    memset (this, 0, sizeof (*this));
    //RedCCI = 0;
    GreenCCI = 5;
    BlueCCI = 4;
    Type = LF_UNKNOWN;
    CropFactor = 1.0;
}

lfLens::~lfLens ()
{
    lf_free (Maker);
    lf_free (Model);
    _lf_list_free ((void **)Mounts);
    _lf_list_free ((void **)CalibDistortion);
    _lf_list_free ((void **)CalibTCA);
    _lf_list_free ((void **)CalibVignetting);
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
    RedCCI = other.RedCCI;
    GreenCCI = other.GreenCCI;
    BlueCCI = other.BlueCCI;
    CropFactor = other.CropFactor;
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
    _lf_addstr (&Mounts, val);
}

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
                                 float &mina, float &maxa)
{
    static struct
    {
        const char *regex;
        guchar matchidx [4];
        bool compiled;
        regex_t rex;
    } lens_name_regex [] =
    {
        {
            // 1:[min aperture]-[max aperture] [min focal]-[max focal]mm
            "[[:space:]]+1:([0-9.]+)(-[0-9.]+)?[[:space:]]+([0-9.]+)(-[0-9.]+)?(mm)?",
            { 3, 4, 1, 2 },
            false
        },
        {
            // [min aperture]-[max aperture]/[min focal]-[max focal]
            "([0-9.]+)(-[0-9.]+)?[[:space:]]*/[[:space:]]*([0-9.]+)(-[0-9.]+)?",
            { 3, 4, 1, 2 },
            false
        },
        {
            // [min focal]-[max focal]mm f/[min aperture]-[max aperture]
            "([0-9.]+)(-[0-9.]+)?(mm)?[[:space:]]+(f/?)?([0-9.]+)(-[0-9.]+)?",
            { 1, 2, 5, 6 },
            false
        }
    };

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
        if (matches [matchidx [3]].rm_so != -1)
            maxa = _lf_parse_float (model, matches [matchidx [3]]);
        return true;
    }

    return false;
}

void lfLens::GuessParameters ()
{
    float minf = INT_MAX, maxf = INT_MIN;
    float mina = INT_MAX, maxa = INT_MIN;

    char *old_numeric = setlocale (LC_NUMERIC, "C");

    if (!MinAperture || !MinFocal)
        _lf_parse_lens_name (Model, minf, maxf, mina, maxa);

    if (!MinAperture || !MinFocal)
    {
        // Try to find out the range of focal distance using calibration data
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
    if (!MaxAperture)
        MaxAperture = MinAperture;

    setlocale (LC_NUMERIC, old_numeric);
}

bool lfLens::Check ()
{
    GuessParameters ();

    if (!Model || !Mounts ||
        MinFocal > MaxFocal || MinAperture > MaxAperture)
        return false;

    return true;
}

const char *lfLens::GetDistortionModelDesc (
    lfDistortionModel model, const char **details, const lfParameter ***params)
{
    static const lfParameter *param_none [] = { NULL };

    static const lfParameter param_poly3_k1 = { "k1", -0.2, 0.2, 0.0 };
    static const lfParameter *param_poly3 [] = { &param_poly3_k1, NULL };

    static const lfParameter param_poly5_k2 = { "k2", -0.2, 0.2, 0.0 };
    static const lfParameter *param_poly5 [] = { &param_poly3_k1, &param_poly5_k2, NULL };

    static const lfParameter param_fov1_omega = { "omega", 0.0, 1.0, 0.0 };
    static const lfParameter *param_fov1 [] = { &param_fov1_omega, NULL };

    static const lfParameter param_ptlens_a = { "a", -0.2, 0.2, 0.0 };
    static const lfParameter param_ptlens_b = { "b", -0.2, 0.2, 0.0 };
    static const lfParameter param_ptlens_c = { "c", -0.2, 0.2, 0.0 };
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
                *details = "Ru = Rd * (1 + k1 * Rd^2)\n"
                    "Ref: http://www.imatest.com/docs/distortion.html";
            if (params)
                *params = param_poly3;
            return "3rd order polynomial";

        case LF_DIST_MODEL_POLY5:
            if (details)
                *details = "Ru = Rd * (1 + k1 * Rd^2 + k2 * Rd^4)\n"
                    "Ref: http://www.imatest.com/docs/distortion.html";
            if (params)
                *params = param_poly5;
            return "5th order polynomial";

        case LF_DIST_MODEL_FOV1:
            if (details)
                *details = "Ru = tg (Rd * omega) / (2 * tg (omega/2))\n"
                    "Ref: ftp://ftp-sop.inria.fr/chir/publis/devernay-faugeras:01.pdf";
            if (params)
                *params = param_fov1;
            return "1st-order field-of-view";

        case LF_DIST_MODEL_PTLENS:
            if (details)
                *details = "Ru = Rd * (a * Rd^3 + b * Rd^2 + c * Rd + 1)\n"
                    "Ref: http://wiki.panotools.org/Lens_correction_model";
            if (params)
                *params = param_ptlens;
            return "PanoTools lens model";
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

    static const lfParameter param_linear_kr = { "kr", 0.8, 1.2, 1.0 };
    static const lfParameter param_linear_kg = { "kg", 0.8, 1.2, 1.0 };
    static const lfParameter param_linear_kb = { "kb", 0.8, 1.2, 1.0 };
    static const lfParameter *param_linear [] =
    { &param_linear_kr, &param_linear_kg, &param_linear_kb, NULL };

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
                *details = "Rd = Ru * k\n"
                    "Ref: http://cipa.icomos.org/fileadmin/papers/Torino2005/403.pdf";
            if (params)
                *params = param_linear;
            return "Linear";
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

    static const lfParameter param_pa_k1 = { "k1", -0.2, 0.2, 0.0 };
    static const lfParameter param_pa_k2 = { "k2", -0.2, 0.2, 0.0 };
    static const lfParameter param_pa_k3 = { "k3", -0.2, 0.2, 0.0 };
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
                    "c = 1 + k1 * R^2 + k2 * R^4 + k3 * R^6\n"
                    "Ref: http://hugin.sourceforge.net/tech/";
            if (params)
                *params = param_pa;
            return "6th order polynomial";
    }

    if (details)
        *details = "";
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
    void **x = (void **)CalibDistortion;
    _lf_addobj (&x, dc, sizeof (*dc), cmp_distortion);
    CalibDistortion = (lfLensCalibDistortion **)x;
}

static bool cmp_tca (const void *x1, const void *x2)
{
    const lfLensCalibTCA *t1 = static_cast<const lfLensCalibTCA *> (x1);
    const lfLensCalibTCA *t2 = static_cast<const lfLensCalibTCA *> (x2);
    return (t1->Focal == t2->Focal);
}

void lfLens::AddCalibTCA (const lfLensCalibTCA *tcac)
{
    void **x = (void **)CalibTCA;
    _lf_addobj (&x, tcac, sizeof (*tcac), cmp_tca);
    CalibTCA = (lfLensCalibTCA **)x;
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
    void **x = (void **)CalibVignetting;
    _lf_addobj (&x, vc, sizeof (*vc), cmp_vignetting);
    CalibVignetting = (lfLensCalibVignetting **)x;
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
            g_warning ("WARNING: lens %s/%s has multiple distortion models defined\n",
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
        res.Terms [i] = _lf_interpolate (
            spline [0] ? spline [0]->Terms [i] : FLT_MAX,
            spline [1]->Terms [i], spline [2]->Terms [i],
            spline [3] ? spline [3]->Terms [i] : FLT_MAX, t);

    return true;
}

bool lfLens::InterpolateTCA (float focal, lfLensCalibTCA &res) const
{
    if (!CalibTCA)
        return false;

    union
    {
        lfLensCalibTCA *spline [4];
        void **spline_ptr;
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
            g_warning ("WARNING: lens %s/%s has multiple TCA models defined\n",
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
        res.Terms [i] = _lf_interpolate (
            spline [0] ? spline [0]->Terms [i] : FLT_MAX,
            spline [1]->Terms [i], spline [2]->Terms [i],
            spline [3] ? spline [3]->Terms [i] : FLT_MAX, t);

    return true;
}

static float __vignetting_dist (
    const lfLens *l, lfLensCalibVignetting &x, float focal, float aperture, float distance)
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
    float a1 = log (aperture) / 2.77258872223978123766;   // log(16)
    float a2 = log (x.Aperture) / 2.77258872223978123766; // log(16)
    float d1 = log (distance);
    float d2 = log (x.Distance);

    return sqrt (fsqr (f2 - f1) + fsqr (a2 - a1) + fsqr (d2 - d1));
}

static void __vignetting_interp (
    lfLensCalibVignetting **spline, lfLensCalibVignetting &res, float t)
{
    res.Focal = _lf_interpolate (
        spline [0] ? spline [0]->Focal : FLT_MAX,
        spline [1]->Focal, spline [2]->Focal,
        spline [3] ? spline [3]->Focal : FLT_MAX, t);
    res.Aperture = _lf_interpolate (
        spline [0] ? spline [0]->Aperture : FLT_MAX,
        spline [1]->Aperture, spline [2]->Aperture,
        spline [3] ? spline [3]->Aperture : FLT_MAX, t);
    res.Distance = _lf_interpolate (
        spline [0] ? spline [0]->Distance : FLT_MAX,
        spline [1]->Distance, spline [2]->Distance,
        spline [3] ? spline [3]->Distance : FLT_MAX, t);
}

bool lfLens::InterpolateVignetting (
    float focal, float aperture, float distance, lfLensCalibVignetting &res) const
{
    if (!CalibVignetting)
        return false;

    /* This is a pretty complex problem, despite its apparent simplicity.
     * What we have is to find the intensity of a 3D force field in given
     * point (let's call it p) determined by coordinates (X=focal,
     * Y=aperture, Z=distance). For this we have a lot of measurements
     * (e.g. calibration data) taken at other random points with their
     * own X/Y/Z coordinates.
     *
     * We must find the line that intersects at least two calibration
     * points and comes as close to the interpolation point as possible.
     * Even better if we find 4 such points, in this case we can approximate
     * by a real spline instead of doing linear interpolation.
     *
     * We will do this as follows. For every point find the other points
     * on the line that intersects this point and the point p. We need
     * just four points: two on one side of p, and two on other side of it.
     * Then build a Hermit spline that passes through these four points.
     * Finally, compute a estimate of the "quality" of this spline, which
     * is made of: total spline length (the shorter is spline, the higher
     * is quality), how close the resulting X,Y,Z is to our target.
     * When we get a estimate of a high enough quality, we return
     * the found point to the caller.
     *
     * Otherwise, the found point is added to the pool and the loop goes
     * over and over again.
     */

    bool rc = false;

    GPtrArray *vc = g_ptr_array_new ();
    int cvc;
    for (cvc = 0; CalibVignetting [cvc]; cvc++)
        g_ptr_array_add (vc, CalibVignetting [cvc]);

    lfVignettingModel vm = LF_VIGNETTING_MODEL_NONE;

    float min_dist = 0.01;
    for (guint i = 0; i < vc->len; i++)
    {
        lfLensCalibVignetting *c =
            (lfLensCalibVignetting *)g_ptr_array_index (vc, i);

        // Take into account just the first encountered lens model
        if (vm == LF_VIGNETTING_MODEL_NONE)
            vm = c->Model;
        else if (vm != c->Model)
        {
            g_warning ("WARNING: lens %s/%s has multiple vignetting models defined\n",
                       Maker, Model);
            continue;
        }

        //                    __
        // Compute the vector pc
        float pcx = c->Focal - focal;
        float pcy = c->Aperture - aperture;
        float pcz = c->Distance - distance;
        float norm = sqrt (pcx * pcx + pcy * pcy + pcz * pcz);

        // If c == p, return it without any interpolation
        if (norm < 0.0001)
        {
            res = *c;
            rc = true;
            goto leave;
        }

        norm = 1.0 / norm;
        pcx *= norm;
        pcy *= norm;
        pcz *= norm;

        union
        {
            lfLensCalibVignetting *spline [4];
            void **spline_ptr;
        };
        // Don't pick up way off points
        float spline_rating [4] = { -10.0, -10.0, 1.0, 10.0 };
        float spline_dist [4] =  { FLT_MAX, FLT_MAX, 1.0, FLT_MAX };

        memset (&spline, 0, sizeof (spline));
        for (guint j = 0; j < vc->len; j++)
        {
            if (j == i)
                continue;

            lfLensCalibVignetting *x =
                (lfLensCalibVignetting *)g_ptr_array_index (vc, j);

            //                      __
            // Calculate the vector px and bring it to same scale
            float pxx = x->Focal - focal;
            float pxy = x->Aperture - aperture;
            float pxz = x->Distance - distance;
            float norm2 = 1.0 / sqrt (pxx * pxx + pxy * pxy + pxz * pxz);

            //                                                      __     __
            // Compute the cosinus of the angle between the vectors px and pc
            float cs = (pxx * pcx + pxy * pcy + pxz * pcz) * norm2;
            if (cs > -0.01 && cs < +0.01)
                continue; // +-90 degrees

            // We will judge how good this point is for us by computing
            // the rating as the relation distance/cos(angle)^3
            float dist = sqrt (fsqr (pxx * norm) + fsqr (pxy * norm) + fsqr (pxz * norm));
            float rating = dist / (cs * cs * cs);

            if (rating >= -0.00001 && dist <= +0.00001)
            {
                res = *x;
                rc = true;
                goto leave;
            }

            switch (__insert_spline (spline_ptr, spline_rating, rating, x))
            {
                case 0:
                    spline_dist [0] = dist;
                    break;
                case 1:
                    spline_dist [0] = spline_dist [1];
                    spline_dist [1] = dist;
                    break;
                case 2:
                    spline_dist [3] = spline_dist [2];
                    spline_dist [2] = dist;
                    break;
                case 3:
                    spline_dist [3] = dist;
                    break;
            }
        }

        // If we have found no points for the spline, drop
        if (!spline [1] || !spline [2])
            continue;

        // Sort the spline points according to the real distance
        // between p and the points, not by "rating".
        if (spline_dist [0] < spline_dist [1])
        {
            lfLensCalibVignetting *tmp = spline [0];
            spline [0] = spline [1];
            spline [1] = tmp;
            float tmpf = spline_dist [0];
            spline_dist [0] = spline_dist [1];
            spline_dist [1] = tmpf;
        }
        if (spline_dist [3] < spline_dist [2])
        {
            lfLensCalibVignetting *tmp = spline [2];
            spline [2] = spline [3];
            spline [3] = tmp;
            float tmpf = spline_dist [2];
            spline_dist [2] = spline_dist [3];
            spline_dist [3] = tmpf;
        }

        // Interpolate a new point given four spline points
        // For this we have to find first the 't' parameter
        // in the range 0..1 which gives the closest to p point
        lfLensCalibVignetting m;
        float t = 0.5;
        float w = 0.25;

        while (w > 0.001)
        {
            __vignetting_interp (spline, m, t);
            float md = __vignetting_dist (this, m, focal, aperture, distance);
            if (md < 0.01)
                break;

            lfLensCalibVignetting m1;
            __vignetting_interp (spline, m1, t + 0.01);
            float md1 = __vignetting_dist (this, m1, focal, aperture, distance);

            if (md1 > md)
                t -= w;
            else
                t += w;
            w /= 2;
        }

        for (size_t i = 0; i < ARRAY_LEN (res.Terms); i++)
            m.Terms [i] = _lf_interpolate (
                spline [0] ? spline [0]->Terms [i] : FLT_MAX,
                spline [1]->Terms [i], spline [2]->Terms [i],
                spline [3] ? spline [3]->Terms [i] : FLT_MAX, t);

        m.Model = vm;
        m.Focal = _lf_interpolate (
            spline [0] ? spline [0]->Focal : FLT_MAX,
            spline [1]->Focal, spline [2]->Focal,
            spline [3] ? spline [3]->Focal : FLT_MAX, t);
        m.Aperture = _lf_interpolate (
            spline [0] ? spline [0]->Aperture : FLT_MAX,
            spline [1]->Aperture, spline [2]->Aperture,
            spline [3] ? spline [3]->Aperture : FLT_MAX, t);
        m.Distance = _lf_interpolate (
            spline [0] ? spline [0]->Distance : FLT_MAX,
            spline [1]->Distance, spline [2]->Distance,
            spline [3] ? spline [3]->Distance : FLT_MAX, t);

        // If interpolated point is close enough, take it
        float dist = __vignetting_dist (this, m, focal, aperture, distance);
        // 0.005 is a carefully manually crafted value ;-D
        if (dist < 0.005)
        {
            res = m;
            rc = true;
            goto leave;
        }
        if (dist < min_dist)
        {
            res = m;
            rc = true;
        }

        g_ptr_array_add (vc, new lfLensCalibVignetting (m));
    }

leave:
    for (guint i = cvc; i < vc->len; i++)
    {
        lfLensCalibVignetting *c =
            (lfLensCalibVignetting *)g_ptr_array_index (vc, i);
        delete c;
    }
    g_ptr_array_free (vc, TRUE);
    return rc;
}

gint _lf_lens_compare (gconstpointer a, gconstpointer b)
{
    int i;
    lfLens *i1 = (lfLens *)a;
    lfLens *i2 = (lfLens *)b;

    int cmp = _lf_strcmp (i1->Maker, i2->Maker);
    if (cmp != 0)
        return cmp;

    cmp = _lf_strcmp (i1->Model, i2->Model);
    if (cmp != 0)
        return cmp;

    for (i = 0; i1->Mounts [i] || i2->Mounts [i]; i++)
    {
        cmp = _lf_strcmp (i1->Mounts [i], i2->Mounts [i]);
        if (cmp != 0)
            return cmp;
    }

    cmp = int ((i1->MinFocal - i2->MinFocal) * 100);
    if (cmp != 0)
        return cmp;

    cmp = int ((i1->MaxFocal - i2->MaxFocal) * 100);
    if (cmp != 0)
        return cmp;

    cmp = int ((i1->MinAperture - i2->MinAperture) * 100);
    if (cmp != 0)
        return cmp;

    cmp = int ((i1->MaxAperture - i2->MaxAperture) * 100);
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

    if (pattern->CropFactor > 0.01 && pattern->CropFactor < match->CropFactor - 0.01)
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

    switch (_lf_compare_num (pattern->MinFocal, match->MinFocal))
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

    // Check the lens mount, if specified
    if (match->Mounts)
    {
        int old_score = score;

        if (compat_mounts && !compat_mounts [0])
            compat_mounts = NULL;

        if (pattern->Mounts)
        {
            int nm = 0;
            for (int i = 0; pattern->Mounts [i]; i++)
                for (int j = 0; match->Mounts [j]; j++)
                    if (!_lf_strcmp (pattern->Mounts [i], match->Mounts [j]))
                    {
                        nm++;
                        break;
                    }

            if (nm)
            {
                // Count number of mounts in the match lens
                int match_nm;
                for (match_nm = 0; match->Mounts [match_nm]; match_nm++)
                    ;
                // Don't allow 0 score, make it at least 1
                int _score = (nm * 20) / match_nm;
                if (!_score)
                    _score = 1;
                score += _score;
            }
        }

        if (compat_mounts)
        {
            int nm = 0;
            for (int i = 0; compat_mounts [i]; i++)
                for (int j = 0; match->Mounts [j]; j++)
                    if (!_lf_strcmp (compat_mounts [i], match->Mounts [j]))
                    {
                        nm++;
                        break;
                    }

            if (nm)
            {
                // Count number of compatible mounts in the match lens
                int match_nm;
                for (match_nm = 0; compat_mounts [match_nm]; match_nm++)
                    ;

                // Don't allow 0 score, make it at least 1
                int _score = (nm * 10) / match_nm;
                if (!_score)
                    _score = 1;
                score += _score;
            }
        }

        // If there were no mount matches, fail the comparison
        if (old_score == score && (pattern->Mounts || compat_mounts))
            return 0;
    }

    // If maker is specified, check it using our patented _lf_strcmp(tm) technology
    if (pattern->Maker && match->Maker)
        if (_lf_strcmp (pattern->Maker, match->Maker) != 0)
            return 0; // Bah! different maker.
        else
            score += 10; // Good doggy, here's a cookie

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

const char *lf_get_lens_type_desc (enum lfLensType type, const char **details)
{
    return lfLens::GetLensTypeDesc (type, details);
}

cbool lf_interpolate_distortion (const lfLens *lens, float focal,
    lfLensCalibDistortion *res)
{
    return lens->InterpolateDistortion (focal, *res);
}

cbool lf_interpolate_tca (const lfLens *lens, float focal, lfLensCalibTCA *res)
{
    return lens->InterpolateTCA (focal, *res);
}

cbool lf_interpolate_vignetting (const lfLens *lens, float focal, float aperture,
    float distance, lfLensCalibVignetting *res)
{
    return lens->InterpolateVignetting (focal, aperture, distance, *res);
}
