/*
    Private constructors and destructors
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <limits.h>
#include <stdlib.h>
#include <regex>
#include <string.h>
#include <locale.h>
#include <math.h>
#include "windows/mathconstants.h"
#include <algorithm>
#include <cfloat>
#include <limits>

//------------------------------------------------------------------------//

lfLens::lfLens ()
{
    Maker = NULL;
    Model = NULL;
    MinFocal = 0.0;
    MaxFocal = 0.0;
    MinAperture = 0.0;
    MaxAperture = 0.0;
    Mounts = NULL;
    Type = LF_RECTILINEAR;
    Score = 0;

    // init legacy attributes
    CropFactor = 1.0;
    AspectRatio = 1.5;
    CenterX = 0.0;
    CenterY = 0.0;

    CalibDistortion = NULL;
    CalibTCA = NULL;
    CalibVignetting = NULL;
    CalibCrop = NULL;
    CalibFov = NULL;
}

lfLens::~lfLens ()
{
    lf_free (Maker);
    lf_free (Model);

    for (auto *calibset : Calibrations)
        delete calibset;

    for (char* m: MountNames)
        free(m);
}

lfLens::lfLens (const lfLens &other)
{
    Maker = lf_mlstr_dup (other.Maker);
    Model = lf_mlstr_dup (other.Model);
    MinFocal = other.MinFocal;
    MaxFocal = other.MaxFocal;
    MinAperture = other.MinAperture;
    MaxAperture = other.MaxAperture;
    Type = other.Type;

    Mounts = NULL;
    if (auto* otherMounts = other.GetMountNames())
    {
        for (int i = 0; otherMounts[i]; i++)
            AddMount(otherMounts[i]);
    }

    for (auto *calibset : other.Calibrations)
        Calibrations.push_back(new lfLensCalibrationSet(*calibset));
    _lf_terminate_vec(Calibrations);

    // Copy legacy lens attributes
    CropFactor = other.CropFactor;
    AspectRatio = other.AspectRatio;

    UpdateLegacyCalibPointers();
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
    Type = other.Type;

    Mounts = NULL;
    MountNames.clear();

    if (auto* otherMounts = other.GetMountNames())
    {
        for (int i = 0; otherMounts[i]; i++)
            AddMount(otherMounts[i]);
    }

    for (auto *calibset : Calibrations)
        delete calibset;
    Calibrations.clear();
    for (auto *calibset : other.Calibrations)
        Calibrations.push_back(new lfLensCalibrationSet(*calibset));
    _lf_terminate_vec(Calibrations);

    // Copy legacy lens attributes
    CropFactor = other.CropFactor;
    AspectRatio = other.AspectRatio;

    UpdateLegacyCalibPointers();

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
    {
        char* p = (char*)malloc(strlen(val) + 1);
        strcpy(p, val);
        MountNames.push_back(p);

        // add terminating NULL
        _lf_terminate_vec(MountNames);

        // legacy mount pointer
        Mounts = (char**)MountNames.data();
    }
}

std::regex lens_name_regexes [3] = {
    // [min focal]-[max focal]mm f/[min aperture]-[max aperture]
    std::regex("[^:]*?([0-9]+[0-9.]*)[-]?([0-9]+[0-9.]*)?(mm)[[:space:]]+(f/|f|1/|1:)?([0-9.]+)(-[0-9.]+)?.*"),

    // 1:[min aperture]-[max aperture] [min focal]-[max focal]mm
    std::regex(".*?1:([0-9.]+)[-]?([0-9.]+)?[[:space:]]+([0-9.]+)[-]?([0-9.]+)?(mm)?.*"),

    // [min aperture]-[max aperture]/[min focal]-[max focal]
    std::regex(".*?([0-9.]+)[-]?([0-9.]+)?[\\s]*/[\\s]*([0-9.]+)[-]?([0-9.]+)?.*"),
};

guchar lens_name_matches [3][3] = {
    { 1, 2, 5 },
    { 3, 4, 1 },
    { 3, 4, 1 }
};

std::regex extender_magnification_regex (".*?[0-9](\\.[0.9]+)?x.*");

void lfLens::GuessParameters ()
{
    float minf = std::numeric_limits<float>::max(), maxf = std::numeric_limits<float>::min();
    float mina = std::numeric_limits<float>::max(), maxa = std::numeric_limits<float>::min();

#if defined(PLATFORM_WINDOWS)
    _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    setlocale (LC_NUMERIC, "C");
#else
    auto loc = uselocale((locale_t) 0); // get current local
    auto nloc = newlocale(LC_NUMERIC_MASK, "C", (locale_t) 0);
    uselocale(nloc);
#endif

    if (Model && (!MinAperture || !MinFocal) &&
        !strstr (Model, "adapter") &&
        !strstr (Model, "reducer") &&
        !strstr (Model, "booster") &&
        !strstr (Model, "extender") &&
        !strstr (Model, "converter") &&
        !strstr (Model, "magnifier") &&
        !std::regex_match(Model, extender_magnification_regex))
    {
        for (size_t i = 0; i < ARRAY_LEN (lens_name_regexes); i++)
        {
            std::cmatch matches;
            if (std::regex_match(Model, matches, lens_name_regexes[i])) {
                guchar *matchidx = lens_name_matches[i];

                if (matches [matchidx [0]].matched)
                    minf = atof(matches [matchidx [0]].str().c_str());
                if (matches [matchidx [1]].matched)
                    maxf = atof(matches [matchidx [1]].str().c_str());
                if (matches [matchidx [2]].matched)
                    mina = atof(matches [matchidx [2]].str().c_str());

                break;
            }
        }
    }

    if (!MinAperture || !MinFocal)
    {
        // Try to find out the range of focal lengths using calibration data
        for (const lfLensCalibrationSet* calibset: Calibrations) {
            for (auto c: calibset->CalibDistortion)
            {
                if (c->Focal < minf)
                    minf = c->Focal;
                if (c->Focal > maxf)
                    maxf = c->Focal;
            }
            for (auto c: calibset->CalibTCA)
            {
                if (c->Focal < minf)
                    minf = c->Focal;
                if (c->Focal > maxf)
                    maxf = c->Focal;
            }
            for (auto c: calibset->CalibVignetting)
            {
                if (c->Focal < minf)
                    minf = c->Focal;
                if (c->Focal > maxf)
                    maxf = c->Focal;
                if (c->Aperture < mina)
                    mina = c->Aperture;
                if (c->Aperture > maxa)
                    maxa = c->Aperture;
            }
            for (auto c: calibset->CalibCrop)
            {
                if (c->Focal < minf)
                    minf = c->Focal;
                if (c->Focal > maxf)
                    maxf = c->Focal;
            }
            for (auto c: calibset->CalibFov)
            {
                if (c->Focal < minf)
                    minf = c->Focal;
                if (c->Focal > maxf)
                    maxf = c->Focal;
            }
        }
    }

    if (minf != std::numeric_limits<float>::max() && !MinFocal)
        MinFocal = minf;
    if (maxf != std::numeric_limits<float>::min() && !MaxFocal)
        MaxFocal = maxf;
    if (mina != std::numeric_limits<float>::max() && !MinAperture)
        MinAperture = mina;
    if (maxa != std::numeric_limits<float>::min() && !MaxAperture)
        MaxAperture = maxa;

    if (!MaxFocal) MaxFocal = MinFocal;

#if defined(PLATFORM_WINDOWS)
    _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
#else
    uselocale(loc);
    freelocale(nloc);
#endif
}

bool lfLens::Check ()
{
    GuessParameters ();

    if (!Model || MountNames.empty() || MinFocal > MaxFocal ||
        (MaxAperture && MinAperture > MaxAperture) )
        return false;

    for (auto calibset: Calibrations)
        if (calibset->Attributes.CropFactor <= 0 ||
            calibset->Attributes.AspectRatio < 1)
            return false;

    // check legacy attributes
    if (CropFactor <= 0 ||
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

    static const lfParameter param_ptlens_a = { "a", -0.5F, 0.5F, 0.0F };
    static const lfParameter param_ptlens_b = { "b", -1.0F, 1.0F, 0.0F };
    static const lfParameter param_ptlens_c = { "c", -1.0F, 1.0F, 0.0F };
    static const lfParameter *param_ptlens [] = {
        &param_ptlens_a, &param_ptlens_b, &param_ptlens_c, NULL };

    static const lfParameter param_acm_k3 = { "k3", -1.0F, 1.0F, 0.0F };
    static const lfParameter param_acm_k4 = { "k4", -1.0F, 1.0F, 0.0F };
    static const lfParameter param_acm_k5 = { "k5", -1.0F, 1.0F, 0.0F };
    static const lfParameter *param_acm [] = {
        &param_poly3_k1, &param_poly5_k2, &param_acm_k3,
        &param_acm_k4, &param_acm_k5, NULL };

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

        case LF_DIST_MODEL_ACM:
            if (details)
                *details = "x_d = x_u (1 + k_1 r^2 + k_2 r^4 + k_3 r^6) + 2x(k_4y + k_5x) + k_5 r^2\n"
                    "y_d = y_u (1 + k_1 r^2 + k_2 r^4 + k_3 r^6) + 2y(k_4y + k_5x) + k_4 r^2\n"
                    "Coordinates are in units of focal length.\n"
                    "Ref: http://download.macromedia.com/pub/labs/lensprofile_creator/lensprofile_creator_cameramodel.pdf";
            if (params)
                *params = param_acm;
            return "Adobe camera model";

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

    static const lfParameter param_acm_alpha0 = { "alpha0", 0.99F, 1.01F, 1.0F };
    static const lfParameter param_acm_beta0 = { "beta0", 0.99F, 1.01F, 1.0F };
    static const lfParameter param_acm_alpha1 = { "alpha1", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_acm_beta1 = { "beta1", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_acm_alpha2 = { "alpha2", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_acm_beta2 = { "beta2", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_acm_alpha3 = { "alpha3", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_acm_beta3 = { "beta3", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_acm_alpha4 = { "alpha4", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_acm_beta4 = { "beta4", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_acm_alpha5 = { "alpha5", -0.01F, 0.01F, 0.0F };
    static const lfParameter param_acm_beta5 = { "beta5", -0.01F, 0.01F, 0.0F };
    static const lfParameter *param_acm [] =
    {
        &param_acm_alpha0, &param_acm_beta0,
        &param_acm_alpha1, &param_acm_beta1,
        &param_acm_alpha2, &param_acm_beta2,
        &param_acm_alpha3, &param_acm_beta3,
        &param_acm_alpha4, &param_acm_beta4,
        &param_acm_alpha5, &param_acm_beta5,
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

        case LF_TCA_MODEL_ACM:
            if (details)
                *details = "x_{d,R} = α_0 ((1 + α_1 r_{u,R}^2 + α_2 r_{u,R}^4 + α_3 r_{u,R}^6) x_{u,R} +\n"
                           "          2(α_4 y_{u,R} + α_5 x_{u,R}) x_{u,R} + α_5 r_{u,R}^2)\n"
                           "y_{d,R} = α_0 ((1 + α_1 r_{u,R}^2 + α_2 r_{u,R}^4 + α_3 r_{u,R}^6) y_{u,R} +\n"
                           "          2(α_4 y_{u,R} + α_5 x_{u,R}) y_{u,R} + α_4 r_{u,R}^2)\n"
                           "x_{d,B} = β_0 ((1 + β_1 r_{u,B}^2 + β_2 r_{u,B}^4 + β_3 r_{u,B}^6) x_{u,B} +\n"
                           "          2(β_4 y_{u,B} + β_5 x_{u,B}) x_{u,B} + β_5 r_{u,B}^2)\n"
                           "y_{d,B} = β_0 ((1 + β_1 r_{u,B}^2 + β_2 r_{u,B}^4 + β_3 r_{u,B}^6) y_{u,B} +\n"
                           "          2(β_4 y_{u,B} + β_5 x_{u,B}) y_{u,B} + β_4 r_{u,B}^2)\n"
                    "Ref: http://download.macromedia.com/pub/labs/lensprofile_creator/lensprofile_creator_cameramodel.pdf";
            if (params)
                *params = param_acm;
            return "Adobe camera model";

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

    static const lfParameter param_pa_k1 = { "k1", -3.0, 1.0, 0.0 };
    static const lfParameter param_pa_k2 = { "k2", -5.0, 10.0, 0.0 };
    static const lfParameter param_pa_k3 = { "k3", -5.0, 10.0, 0.0 };
    static const lfParameter *param_pa [] =
    { &param_pa_k1, &param_pa_k2, &param_pa_k3, NULL };

    static const lfParameter param_acm_alpha1 = { "alpha1", -1.0, 1.0, 0.0 };
    static const lfParameter param_acm_alpha2 = { "alpha2", -5.0, 10.0, 0.0 };
    static const lfParameter param_acm_alpha3 = { "alpha3", -5.0, 10.0, 0.0 };
    static const lfParameter *param_acm [] =
    { &param_acm_alpha1, &param_acm_alpha2, &param_acm_alpha3, NULL };

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
            return "6th order polynomial (Pablo D'Angelo)";

        case LF_VIGNETTING_MODEL_ACM:
            if (details)
                *details = "Adobe's vignetting model\n"
                    "(which differs from D'Angelo's only in the coordinate system):\n"
                    "Cd = Cs * (1 + k1 * R^2 + k2 * R^4 + k3 * R^6)\n"
                    "Ref: http://download.macromedia.com/pub/labs/lensprofile_creator/lensprofile_creator_cameramodel.pdf";
            if (params)
                *params = param_acm;
            return "6th order polynomial (Adobe)";

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

lfLensCalibrationSet* lfLens::GetClosestCalibrationSet(const float crop) const
{
    lfLensCalibrationSet* calib_set = nullptr;
    float crop_ratio = 1e6f;
    for (auto c : Calibrations)
    {
        const float r = crop / c->Attributes.CropFactor;
        if ((r >= 0.96) && (r < crop_ratio))
        {
            crop_ratio = r;
            calib_set = c;
        }
    }
    return calib_set;
}

lfLensCalibrationSet* lfLens::GetCalibrationSetForAttributes(const lfLensCalibAttributes lcattr)
{
    // Always use Calibrations[0] for now, achieves backwards
    // compatibility to Lensfun API before 0.4.0
    {
        if (Calibrations.empty())
            Calibrations.emplace_back(new lfLensCalibrationSet(lcattr));
        _lf_terminate_vec(Calibrations);

        Calibrations[0]->Attributes.CropFactor = CropFactor;
        Calibrations[0]->Attributes.AspectRatio = AspectRatio;
        return Calibrations[0];
    }

    // try to find a matching calibset
    for (auto c: Calibrations)
    {
        if (c->Attributes == lcattr)
            return c;
    }

    // nothing found, create a new one
    Calibrations.emplace_back(new lfLensCalibrationSet(lcattr));
    _lf_terminate_vec(Calibrations);

    // return pointer
    return Calibrations.back();
}

void lfLens::AddCalibDistortion (const lfLensCalibDistortion *plcd)
{
    lfLensCalibrationSet* calibSet = GetCalibrationSetForAttributes(plcd->CalibAttr);
    calibSet->CalibDistortion.emplace_back(new lfLensCalibDistortion(*plcd));

    // Duplicate all Calibrations[0] components in legacy calibration structures
    if (calibSet == Calibrations[0])
    {
        UpdateLegacyCalibPointers();
    }
}

void lfLens::AddCalibTCA (const lfLensCalibTCA *plctca)
{
    lfLensCalibrationSet* calibSet = GetCalibrationSetForAttributes(plctca->CalibAttr);
    calibSet->CalibTCA.emplace_back(new lfLensCalibTCA(*plctca));

    // Duplicate all Calibrations[0] components in legacy calibration structures
    if (calibSet == Calibrations[0])
    {
        UpdateLegacyCalibPointers();
    }
}

void lfLens::AddCalibVignetting (const lfLensCalibVignetting *plcv)
{
    lfLensCalibrationSet* calibSet = GetCalibrationSetForAttributes(plcv->CalibAttr);
    calibSet->CalibVignetting.push_back(new lfLensCalibVignetting(*plcv));

    // Duplicate all Calibrations[0] components in legacy calibration structures
    if (calibSet == Calibrations[0])
    {
        UpdateLegacyCalibPointers();
    }
}


void lfLens::AddCalibCrop (const lfLensCalibCrop *plcc)
{
    lfLensCalibrationSet* calibSet = GetCalibrationSetForAttributes(plcc->CalibAttr);
    calibSet->CalibCrop.push_back(new lfLensCalibCrop(*plcc));

    // Duplicate all Calibrations[0] components in legacy calibration structures
    if (calibSet == Calibrations[0])
    {
        UpdateLegacyCalibPointers();
    }
}


void lfLens::AddCalibFov (const lfLensCalibFov *plcf)
{
    lfLensCalibrationSet* calibSet = GetCalibrationSetForAttributes(plcf->CalibAttr);
    calibSet->CalibFov.push_back(new lfLensCalibFov(*plcf));

    // Duplicate all Calibrations[0] components in legacy calibration structures
    if (calibSet == Calibrations[0])
    {
        UpdateLegacyCalibPointers();
    }
}

void lfLens::RemoveCalibrations()
{
    Calibrations.clear();
}

static int __insert_spline (const void **spline, float *spline_dist, float dist, const void *val)
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

/* Coefficient interpolation

   The interpolation of model coefficients bases on spline interpolation
   (distortion/TCA) and the IDW algorithm (vignetting).  Both methods work best
   if the input data points (i.e. the values in the XML file) are
   preconditioned so that they follow more or less a linear slope.  For this
   preconditioning, we transform the axes.

   First, let's have a look at the untransformed slopes.  For distortion, the
   parameters decrease with increasing focal length, following a 1/f law.  The
   same is true for all TCA parameters besides the zeroth one (i.e. the term
   close to 1), which remains constant.  In contrast, all three vignetting
   parameters remain constant for different focal lengths, however, they do
   decrease according to 1/aperture and 1/distance.  All of this is only roughly
   true of course, however, I could really reproduce it by mass-analysing the
   existing data.

   Thus, in order to make the slopes linear, I transform the aperture and
   distance axes to reciprocal axes in __vignetting_dist ().  I keep the focal
   length axis linear, though, for all three kinds of correction.  Instead, I
   transform the parameter axis by multiplying it by the focal length of the
   respective data point for those parameters that exhibit 1/f behaviour.  Of
   course, this parameter scaling must be undone after the interpolation by
   dividing by the destination focal length.

   The ACM models are a special case because they use a coordinate system that
   scales with the focal length.  Therefore, their parameters tend to increase
   (partly heavily) by the focal length.  I undo this by dividing the
   parameters by the focal length in the same power that the respective r
   coordinate has.  For example, I divide k₁ of the distortion model by f²,
   because it says “k₁r²” in the formula.  Note that this is done *on top* of
   the multiplying by f, so that effectively, k₁ is divided by f.  Also note
   that the factor x in the Adobe formula does not mean that we have r³,
   because the left-hand side of the formula scales by f, too, and anhilates
   this factor.

   All of this only works well with polynomial-based models.  I simply don't
   know whether or how it applies to completely different types of models.

   Does it really matter?  Well, my own research revealed that only for
   vignetting, it is absolutely necessary.  Especially the parameter scaling
   for the ACM model prevents the slightly fragile IDW algorithm from failing
   badly in many cases.  The reciprocal axis for aperture also helps a lot.  I
   think that the parameter scaling for distortion and TCA for the ACM models
   is also advantageous since especially the k₃ parameter tends to explode by
   the focal length, and the splines will have a hard time to follow such
   slopes.  Well, and if you implement the facilities for axis transformation
   anyway, you can cover all other cases too, I think.


   The function __parameter_scales () below implements the parameter axis
   scaling.  If returns factors by which the parameters are multiplied, and
   divided by after the interpolation.  It takes the "type" of the correction
   (distortion/TCA/vignetting), the model, and the parameter index in this
   model.  All of these parameters should be easy to understand.

   The "values" parameter is slightly more convoluted.  It takes the focal
   lengths for which scale factors should be returned.  "number_of_values"
   contains the number of values in the "values" array.  At the same time,
   "values" contains the scale factors themselves, thus, __parameter_scales ()
   changes the "values" array in place.  This makes sense because the caller
   doesn't need the focal lengths anymore, and because in most cases, the scale
   factor is the focal length itself, so nothing needs to be changed.
 */
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

        case LF_DIST_MODEL_ACM:
            const float exponent = index < 3 ? (float)(2 * (index + 1)) : 1.0;
            for (int i=0; i < number_of_values; i++)
                values [i] /= pow (values [i], exponent);
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

        case LF_TCA_MODEL_ACM:
            const float exponent = index > 1 && index < 8 ? (float)(index / 2 * 2) : 1.0;
            for (int i=0; i < number_of_values; i++)
                values [i] /= pow (values [i], exponent);
            break;
        }
        break;

    case LF_MODIFY_VIGNETTING:
        switch (model)
        {
        case LF_VIGNETTING_MODEL_PA:
            for (int i=0; i < number_of_values; i++)
                values [i] = 1.0;
            break;

        case LF_VIGNETTING_MODEL_ACM:
            const float exponent = (float)(2 * (index + 1));
            for (int i=0; i < number_of_values; i++)
                values [i] = 1.0 / pow (values [i], exponent);
            break;
        }
    }
}

bool lfLens::InterpolateDistortion (float crop, float focal, lfLensCalibDistortion &res) const
{
    // find calibration set with closest crop factor
    lfLensCalibrationSet* calib_set = nullptr;
    float crop_ratio = 1e6f;
    for (auto c : Calibrations)
    {
        const float r = crop / c->Attributes.CropFactor;
        if (c->HasDistortion() && (r >= 0.96) && (r < crop_ratio))
        {
            crop_ratio = r;
            calib_set = c;
        }
    }
    if (calib_set == nullptr)
        return false;
    if (calib_set == Calibrations[0])
    {
        // sync legacy attributes
        Calibrations[0]->Attributes.CropFactor = CropFactor;
        Calibrations[0]->Attributes.AspectRatio = AspectRatio;
    }

    union
    {
        const lfLensCalibDistortion *spline [4];
        const void *spline_ptr [4];
    };
    float spline_dist [4] = { -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX };
    lfDistortionModel dm = LF_DIST_MODEL_NONE;

    memset ((void*)spline, 0, sizeof (spline));
    for (const lfLensCalibDistortion* c : calib_set->CalibDistortion)
    {
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
        __insert_spline ((const void **)spline_ptr, spline_dist, df, c);
    }

    if (!spline [1] || !spline [2])
    {
        if (spline [1])
            res = *spline [1];
        else if (spline [2])
            res = *spline [2];
        else
            return false;

        res.CalibAttr  = calib_set->Attributes;
        return true;
    }

    // No exact match found, interpolate the model parameters
    res.Model = dm;
    res.Focal = focal;
    res.CalibAttr  = calib_set->Attributes;

    float t = (focal - spline [1]->Focal) / (spline [2]->Focal - spline [1]->Focal);

    res.RealFocal = _lf_interpolate (
        spline [0] ? spline [0]->RealFocal : FLT_MAX,
        spline [1]->RealFocal, spline [2]->RealFocal,
        spline [3] ? spline [3]->RealFocal : FLT_MAX, t);
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

bool lfLens::InterpolateTCA (float crop, float focal, lfLensCalibTCA &res) const
{
    // find calibration set with closest crop factor
    lfLensCalibrationSet* calib_set = nullptr;
    float crop_ratio = 1e6f;
    for (auto c : Calibrations)
    {
        const float r = crop / c->Attributes.CropFactor;
        if (c->HasTCA() && (r >= 0.96) && (r < crop_ratio))
        {
            crop_ratio = r;
            calib_set = c;
        }
    }
    if (calib_set == nullptr)
        return false;
    if (calib_set == Calibrations[0])
    {
        // sync legacy attributes
        Calibrations[0]->Attributes.CropFactor = CropFactor;
        Calibrations[0]->Attributes.AspectRatio = AspectRatio;
    }

    union
    {
        const lfLensCalibTCA *spline [4];
        const void *spline_ptr [4];
    };
    float spline_dist [4] = { -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX };
    lfTCAModel tcam = LF_TCA_MODEL_NONE;

    memset ((void*)&spline, 0, sizeof (spline));
    for (const lfLensCalibTCA* c : calib_set->CalibTCA)
    {
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
            res.CalibAttr = calib_set->Attributes;
            return true;
        }
        __insert_spline ((const void **)spline_ptr, spline_dist, df, c);
    }

    if (!spline [1] || !spline [2])
    {
        if (spline [1])
            res = *spline [1];
        else if (spline [2])
            res = *spline [2];
        else
            return false;

        res.CalibAttr = calib_set->Attributes;
        return true;
    }

    // No exact match found, interpolate the model parameters
    res.Model = tcam;
    res.Focal = focal;
    res.CalibAttr = calib_set->Attributes;

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
    // approximately to range 0..1
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

bool lfLens::InterpolateVignetting (float crop,
    float focal, float aperture, float distance, lfLensCalibVignetting &res) const
{
    // find calibration set with closest crop factor
    lfLensCalibrationSet* calib_set = nullptr;
    float crop_ratio = 1e6f;
    for (auto c : Calibrations)
    {
        const float r = crop / c->Attributes.CropFactor;
        if (c->HasVignetting() && (r >= 0.96) && (r < crop_ratio))
        {
            crop_ratio = r;
            calib_set = c;
        }
    }
    if (calib_set == nullptr)
        return false;
    if (calib_set == Calibrations[0])
    {
        // sync legacy attributes
        Calibrations[0]->Attributes.CropFactor = CropFactor;
        Calibrations[0]->Attributes.AspectRatio = AspectRatio;
    }

    lfVignettingModel vm = LF_VIGNETTING_MODEL_NONE;
    res.Focal = focal;
    res.Aperture = aperture;
    res.Distance = distance;
    res.CalibAttr = calib_set->Attributes;
    for (size_t i = 0; i < ARRAY_LEN (res.Terms); i++)
        res.Terms [i] = 0;

    // Use http://en.wikipedia.org/wiki/Inverse_distance_weighting with
    // p = 3.5.
    float total_weighting = 0;
    const float power = 3.5;

    float smallest_interpolation_distance = FLT_MAX;
    for (const lfLensCalibVignetting* c : calib_set->CalibVignetting)
    {
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
            res.CalibAttr = calib_set->Attributes;
            return true;
        }

        smallest_interpolation_distance = std::min(smallest_interpolation_distance, interpolation_distance);
        float weighting = fabs (1.0 / pow (interpolation_distance, power));
        for (size_t i = 0; i < ARRAY_LEN (res.Terms); i++)
        {
            float values [1] = {c->Focal};
            __parameter_scales (values, 1, LF_MODIFY_VIGNETTING, vm, i);
            res.Terms [i] += weighting * c->Terms [i] * values [0];
        }
        total_weighting += weighting;
    }
    
    if (smallest_interpolation_distance > 1)
        return false;
    
    if (total_weighting > 0 && smallest_interpolation_distance < FLT_MAX)
    {
        for (size_t i = 0; i < ARRAY_LEN (res.Terms); i++)
        {
            float values [1] = {focal};
            __parameter_scales (values, 1, LF_MODIFY_VIGNETTING, vm, i);
            res.Terms [i] /= total_weighting * values [0];
        }
        return true;
    } else 
        return false;
}

bool lfLens::InterpolateCrop (float crop, float focal, lfLensCalibCrop &res) const
{
    // find calibration set with closest crop factor
    lfLensCalibrationSet* calib_set = nullptr;
    float crop_ratio = 1e6f;
    for (auto c : Calibrations)
    {
        const float r = crop / c->Attributes.CropFactor;
        if (c->HasCrop() && (r >= 0.96) && (r < crop_ratio))
        {
            crop_ratio = r;
            calib_set = c;
        }
    }
    if (calib_set == nullptr)
        return false;
    if (calib_set == Calibrations[0])
    {
        // sync legacy attributes
        Calibrations[0]->Attributes.CropFactor = CropFactor;
        Calibrations[0]->Attributes.AspectRatio = AspectRatio;
    }

    union
    {
        const lfLensCalibCrop *spline [4];
        const void *spline_ptr [4];
    };
    float spline_dist [4] = { -FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX };
    lfCropMode cm = LF_NO_CROP;

    memset ((void*)spline, 0, sizeof (spline));
    for (const lfLensCalibCrop* c : calib_set->CalibCrop)
    {
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
        __insert_spline ((const void **)spline_ptr, spline_dist, df, c);
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

int lfLens::AvailableModifications(float crop) const
{
    int possibleMods = 0;

    for (auto c : Calibrations)
    {
        const float r = crop / c->Attributes.CropFactor;
        if ((r >= 0.96) || (crop < 1e-6f))
        {
            if (c->HasDistortion())
                possibleMods |= LF_MODIFY_DISTORTION;

            if (c->HasTCA())
                possibleMods |= LF_MODIFY_TCA;

            if (c->HasVignetting())
                possibleMods |= LF_MODIFY_VIGNETTING;
        }
    }

    return possibleMods;
}

const lfLensCalibrationSet* const* lfLens::GetCalibrationSets() const
{
    return (lfLensCalibrationSet**) Calibrations.data();
}

const char* const* lfLens::GetMountNames() const
{
    return MountNames.data();
}

void lfLens::UpdateLegacyCalibPointers()
{
    if (!Calibrations.empty())
    {
        _lf_terminate_vec(Calibrations[0]->CalibDistortion);
        CalibDistortion = (lfLensCalibDistortion**) Calibrations[0]->CalibDistortion.data();

        _lf_terminate_vec(Calibrations[0]->CalibTCA);
        CalibTCA = (lfLensCalibTCA**) Calibrations[0]->CalibTCA.data();

        _lf_terminate_vec(Calibrations[0]->CalibVignetting);
        CalibVignetting = (lfLensCalibVignetting**) Calibrations[0]->CalibVignetting.data();

        _lf_terminate_vec(Calibrations[0]->CalibCrop);
        CalibCrop = (lfLensCalibCrop**) Calibrations[0]->CalibCrop.data();

        _lf_terminate_vec(Calibrations[0]->CalibFov);
        CalibFov = (lfLensCalibFov**) Calibrations[0]->CalibFov.data();
    }
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


//---------------------------// The C interface //---------------------------//

lfLens *lf_lens_new ()
{
    return new lfLens ();
}

lfLens *lf_lens_create ()
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

void lf_lens_add_mount (lfLens *lens, const char *val)
{
    lens->AddMount(val);
}

const char* const* lf_lens_get_mount_names (const lfLens *lens)
{
    return lens->GetMountNames();
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

cbool lf_lens_interpolate_distortion (const lfLens *lens, float crop, float focal,
    lfLensCalibDistortion *res)
{
    return lens->InterpolateDistortion (crop, focal, *res);
}

cbool lf_lens_interpolate_tca (const lfLens *lens, float crop, float focal, lfLensCalibTCA *res)
{
    return lens->InterpolateTCA (crop, focal, *res);
}

cbool lf_lens_interpolate_vignetting (const lfLens *lens, float crop, float focal, float aperture,
    float distance, lfLensCalibVignetting *res)
{
    return lens->InterpolateVignetting (crop, focal, aperture, distance, *res);
}

cbool lf_lens_interpolate_crop (const lfLens *lens, float crop, float focal,
    lfLensCalibCrop *res)
{
    return lens->InterpolateCrop (crop, focal, *res);
}


void lf_lens_add_calib_distortion (lfLens *lens, const lfLensCalibDistortion *dc)
{
    lens->AddCalibDistortion (dc);
}

void lf_lens_add_calib_tca (lfLens *lens, const lfLensCalibTCA *tcac)
{
   lens->AddCalibTCA (tcac);
}

void lf_lens_add_calib_vignetting (lfLens *lens, const lfLensCalibVignetting *vc)
{
    lens->AddCalibVignetting (vc);
}


void lf_lens_add_calib_crop (lfLens *lens, const lfLensCalibCrop *lcc)
{
    lens->AddCalibCrop (lcc);
}


void lf_lens_add_calib_fov (lfLens *lens, const lfLensCalibFov *lcf)
{
    lens->AddCalibFov (lcf);
}


void lf_lens_remove_calibrations (lfLens *lens)
{
    lens->RemoveCalibrations();
}

int lf_lens_available_modifications (lfLens *lens, float crop)
{
    return lens->AvailableModifications(crop);
}
