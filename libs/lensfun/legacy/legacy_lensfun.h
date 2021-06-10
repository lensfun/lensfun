/* -*- mode:c++ -*- */
/*
    Lensfun - a library for maintaining a database of photographical lenses,
    and providing the means to correct some of the typical lens distortions.
    Copyright (C) 2007 by Andrew Zabolotny

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __LEGACY_LENSFUN_H__
#define __LEGACY_LENSFUN_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
/** Helper macro to make C/C++ work similarly */
#  define C_TYPEDEF(t,c)
#else
#  define C_TYPEDEF(t,c) typedef t c c;
#endif

/**
 * @file lensfun.h
 * This file defines the interface to the Lensfun library.
 */

/*----------------------------------------------------------------------------*/

/**
 * @defgroup Auxiliary Auxiliary definitions and functions
 * @brief These functions will help handling basic structures of the library.
 * @{
 */

/// Major library version number
#define LEGACY_LF_VERSION_MAJOR	0
/// Minor library version number
#define LEGACY_LF_VERSION_MINOR	3
/// Library micro version number
#define LEGACY_LF_VERSION_MICRO	2
/// Library bugfix number
#define LEGACY_LF_VERSION_BUGFIX	0
/// Full library version
#define LEGACY_LF_VERSION	((LEGACY_LF_VERSION_MAJOR << 24) | (LEGACY_LF_VERSION_MINOR << 16) | (LEGACY_LF_VERSION_MICRO << 8) | LEGACY_LF_VERSION_BUGFIX)

/// Latest database version supported by this release
#define LEGACY_LF_MAX_DATABASE_VERSION	1

#if defined CONF_LENSFUN_STATIC
/// This macro expands to an appropiate symbol visibility declaration
#   define LEGACY_LF_EXPORT
#else
#   ifdef CONF_SYMBOL_VISIBILITY
#       if defined PLATFORM_WINDOWS
#           define LEGACY_LF_EXPORT    __declspec(dllexport)
#       elif defined CONF_COMPILER_GCC || __clang__
#           define LEGACY_LF_EXPORT    __attribute__((visibility("default")))
#       else
#           error "I don't know how to change symbol visibility for your compiler"
#       endif
#   else
#       if defined PLATFORM_WINDOWS || defined _MSC_VER
#           define LEGACY_LF_EXPORT    __declspec(dllimport)
#       else
#           define LEGACY_LF_EXPORT
#       endif
#   endif
#endif

#ifndef CONF_LENSFUN_INTERNAL
/// For marking deprecated functions, see http://stackoverflow.com/a/21265197
#    ifdef __GNUC__
#        define DEPRECATED __attribute__((deprecated))
#    elif defined(_MSC_VER)
#        define DEPRECATED __declspec(deprecated)
#    else
#        pragma message("WARNING: You need to implement DEPRECATED for this compiler")
#        define DEPRECATED
#    endif
#else
#    define DEPRECATED
#endif

/// C-compatible bool type; don't bother to define Yet Another Boolean Type
#define legacy_cbool int

/**
 * The storage of "multi-language" strings is simple yet flexible,
 * handy and effective. The first (default) string comes first, terminated
 * by \\0 as usual, after that a language code follows, then \\0 again,
 * then the translated value and so on. The list terminates as soon as
 * a \\0 is encountered instead of next string, e.g. last string in list
 * is terminated with two null characters.
 */
typedef char *legacy_lfMLstr;

/** liblensfun error codes: negative codes are -errno, positive are here */
enum legacy_lfError
{
    /** No error occured */
    LEGACY_LF_NO_ERROR = 0,
    /** Wrong XML data format */
    LEGACY_LF_WRONG_FORMAT,
    /** No database could be loaded */
    LEGACY_LF_NO_DATABASE
};

C_TYPEDEF (enum, legacy_lfError)

/** The type of a 8-bit pixel */
typedef unsigned char legacy_lf_u8;
/** The type of a 16-bit pixel */
typedef unsigned short legacy_lf_u16;
/** The type of a 32-bit pixel */
typedef unsigned int legacy_lf_u32;
/** The type of a 32-bit floating-point pixel */
typedef float legacy_lf_f32;
/** The type of a 64-bit floating-point pixel */
typedef double legacy_lf_f64;

/**
 * The basics of memory allocation: never free objects allocated by the
 * library yourselves, instead use this function. It is a direct
 * equivalent of standard C free(), however you should not use free()
 * in the event that the library uses a separate heap.
 * @param data
 *     A pointer to memory to be freed.
 */
LEGACY_LF_EXPORT void legacy_lf_free (void *data);

/**
 * @brief Get a string corresponding to current locale from a multi-language
 * string.
 *
 * Current locale is determined from LC_MESSAGES category at the time of
 * the call, e.g. if you change LC_MESSAGES at runtime, next calls to
 * legacy_lf_mlstr_get() will return the string for the new locale.
 */
LEGACY_LF_EXPORT const char *legacy_lf_mlstr_get (const legacy_lfMLstr str);

/**
 * @brief Add a new translated string to a multi-language string.
 *
 * This uses realloc() so returned value may differ from input.
 * @param str
 *     The string to append to. Can be NULL.
 * @param lang
 *     The language for the new added string. If NULL, the default
 *     string is replaced (the first one in list, without a language
 *     designator).
 * @param trstr
 *     The translated string
 * @return
 *     The reallocated multi-language string. To free a multi-language
 *     string, use legacy_lf_free().
 */
LEGACY_LF_EXPORT legacy_lfMLstr legacy_lf_mlstr_add (legacy_lfMLstr str, const char *lang, const char *trstr);

/**
 * @brief Create a complete copy of a multi-language string.
 *
 * @param str
 *     The string to create a copy of
 * @return
 *     A new allocated multi-language string
 */
LEGACY_LF_EXPORT legacy_lfMLstr legacy_lf_mlstr_dup (const legacy_lfMLstr str);

/** @} */

/*----------------------------------------------------------------------------*/

/**
 * @defgroup Mount Structures and functions for camera mounts
 * @brief These structures and functions allow to define and examine
 * the properties of camera mounts.
 * @{
 */

/**
 * @brief This structure contains everything specific to a camera mount.
 *
 * Objects of this type are usually retrieved from the database
 * by using queries (see legacy_lfDatabase::FindMount() / legacy_lf_db_find_mount()),
 * and can be created manually in which case it is application's
 * responsability to destroy the object when it is not needed anymore.
 */
struct LEGACY_LF_EXPORT legacy_lfMount
{
    /** @brief Camera mount name.
     *
     * Mount names for fixed-lens cameras -- and only they -- must start with a
     * lower case letter.
     */
    legacy_lfMLstr Name;
    /** A list of compatible mounts */
    char **Compat;

#ifdef __cplusplus
    /**
     * @brief Initialize a new mount object. All fields are set to 0.
     */
    legacy_lfMount ();

    /**
     * Assignment operator
     */
    legacy_lfMount &operator = (const legacy_lfMount &other);

    /**
     * @brief Destroy a mount object. All allocated fields are freed.
     */
    ~legacy_lfMount ();

    /**
     * @brief Add a string to mount name.
     *
     * If lang is NULL, this replaces the default value, otherwise a new
     * language value is appended.
     * @param val
     *     The new value for the Name field.
     * @param lang
     *     The language this field is in.
     */
    void SetName (const char *val, const char *lang = NULL);

    /**
     * @brief Add a mount name to the list of compatible mounts.
     * @param val
     *     The identifier of the compatible mount.
     */
    void AddCompat (const char *val);

    /**
     * @brief Check if a mount object is valid.
     * @return
     *     true if required fields are ok.
     */
    bool Check ();
#endif
};

C_TYPEDEF (struct, legacy_lfMount)

/**
 * @brief Create a new mount object.
 * @return
 *     A new empty mount object.
 * @sa
 *     legacy_lfMount::legacy_lfMount()
 */
LEGACY_LF_EXPORT legacy_lfMount *legacy_lf_mount_new ();

/**
 * @brief Destroy a legacy_lfMount object.
 * 
 * This is equivalent to C++ "delete mount".
 * @param mount
 *     The mount object to destroy.
 * @sa
 *     legacy_lfMount::~legacy_lfMount()
 */
LEGACY_LF_EXPORT void legacy_lf_mount_destroy (legacy_lfMount *mount);

/**
 * @brief Copy the data from one legacy_lfMount structure into another.
 * @param dest
 *     The destination object
 * @param source
 *     The source object
 * @sa
 *     legacy_lfMount::operator = (const legacy_lfMount &)
 */
LEGACY_LF_EXPORT void legacy_lf_mount_copy (legacy_lfMount *dest, const legacy_lfMount *source);

/** @sa legacy_lfMount::Check */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_mount_check (legacy_lfMount *mount);

/** @} */

/*----------------------------------------------------------------------------*/

/**
 * @defgroup Camera Structures and functions for cameras
 * @brief These structures and functions allow to define and examine
 * the properties of a camera model.
 * @{
 */

/**
 * @brief Camera data.  Unknown fields are set to NULL.
 *
 * The Maker and Model fields must be filled EXACTLY as they appear in the EXIF
 * data, since this is the only means to detect camera automatically
 * (upper/lowercase is not important, though).  Some different cameras
 * (e.g. Sony Cybershot) are using same EXIF id info for different models, in
 * which case the Variant field should contain the exact model name, but, alas,
 * we cannot automatically choose between such "twin" cameras.
 */
struct LEGACY_LF_EXPORT legacy_lfCamera
{
    /** @brief Camera maker (ex: "Rollei") -- same as in EXIF */
    legacy_lfMLstr Maker;
    /** @brief Model name (ex: "Rolleiflex SL35") -- same as in EXIF */
    legacy_lfMLstr Model;
    /** @brief Camera variant. Some cameras use same EXIF id for different models */
    legacy_lfMLstr Variant;
    /** @brief Camera mount type (ex: "QBM") */
    char *Mount;
    /** @brief Camera crop factor (ex: 1.0). Must be defined. */
    float CropFactor;
    /** @brief Camera matching score, used while searching: not actually a camera parameter */
    int Score;

#ifdef __cplusplus
    /**
     * @brief Initialize a new camera object. All fields are set to 0.
     */
    legacy_lfCamera ();

    /**
     * Copy constructor.
     */
    legacy_lfCamera (const legacy_lfCamera &other);

    /**
     * @brief Destroy a camera object. All allocated fields are freed.
     */
    ~legacy_lfCamera ();

    /**
     * Assignment operator
     */
    legacy_lfCamera &operator = (const legacy_lfCamera &other);

    /**
     * @brief Add a string to camera maker.
     * 
     * If lang is NULL, this replaces the default value, otherwise a new
     * language value is appended.
     * @param val
     *     The new value for the Maker field.
     * @param lang
     *     The language this field is in.
     */
    void SetMaker (const char *val, const char *lang = NULL);

    /**
     * @brief Add a string to camera model.
     *
     * If lang is NULL, this replaces the default value, otherwise a new
     * language value is appended.
     * @param val
     *     The new value for the Model field.
     * @param lang
     *     The language this field is in.
     */
    void SetModel (const char *val, const char *lang = NULL);

    /**
     * @brief Add a string to camera variant.
     *
     * If lang is NULL, this replaces the default value, otherwise a new
     * language value is appended.
     * @param val
     *     The new value for the Variant field.
     * @param lang
     *     The language this field is in.
     */
    void SetVariant (const char *val, const char *lang = NULL);

    /**
     * @brief Set the value for camera Mount.
     * @param val
     *     The new value for Mount.
     */
    void SetMount (const char *val);

    /**
     * @brief Check if a camera object is valid.
     * @return
     *     true if the required fields are ok.
     */
    bool Check ();
#endif
};

C_TYPEDEF (struct, legacy_lfCamera)

/**
 * @brief Create a new camera object.
 * @return
 *     A new empty camera object.
 * @sa
 *     legacy_lfCamera::legacy_lfCamera
 */
LEGACY_LF_EXPORT legacy_lfCamera *legacy_lf_camera_new ();

/**
 * @brief Destroy a legacy_lfCamera object.
 *
 * This is equivalent to C++ "delete camera".
 * @param camera
 *     The camera object to destroy.
 * @sa
 *     legacy_lfCamera::~legacy_lfCamera
 */
LEGACY_LF_EXPORT void legacy_lf_camera_destroy (legacy_lfCamera *camera);

/**
 * @brief Copy the data from one legacy_lfCamera structure into another.
 * @param dest
 *     The destination object
 * @param source
 *     The source object
 * @sa
 *     legacy_lfCamera::operator = (const legacy_lfCamera &)
 */
LEGACY_LF_EXPORT void legacy_lf_camera_copy (legacy_lfCamera *dest, const legacy_lfCamera *source);

/** @sa legacy_lfCamera::Check */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_camera_check (legacy_lfCamera *camera);

/** @} */

/*----------------------------------------------------------------------------*/

/**
 * @defgroup Lens Structures and functions for lenses
 * @brief These structures and functions allow to define and examine
 * the properties of a lens.
 * @{
 */

/**
 * @brief The Lensfun library implements several lens distortion models.
 * This enum lists them.
 *
 * Distortion usually heavily depends on the focal length, but does not depend
 * on the aperture.  In the following, \f$r_d\f$ refers to the distorted radius
 * (normalised distance to image center), and \f$r_u\f$ refers to the
 * undistorted, corrected radius.  See section \ref actualorder for further
 * information.
 *
 * For a popular explanation of lens distortion see
 * http://www.vanwalree.com/optics/distortion.html
 */
enum legacy_lfDistortionModel
{
    /** @brief Distortion parameters are unknown */
    LEGACY_LF_DIST_MODEL_NONE,
    /**
     * @brief 3rd order polynomial model, which is a subset of the PTLens
     * model.
     *
     * \f[r_d = r_u \cdot (1 - k_1 + k_1 r_u^2)\f]
     * The corresponding XML attribute is called “k1”.  It defaults to 0.
     */
    LEGACY_LF_DIST_MODEL_POLY3,
    /**
     * @brief 5th order polynomial model.
     *
     * \f[r_d = r_u \cdot (1 + k_1 r_u^2 + k_2 r_u^4)\f]
     * The corresponding XML attributes are called “k1” and “k2”.  They default
     * to 0.
     * Ref: http://www.imatest.com/docs/distortion.html
     */
    LEGACY_LF_DIST_MODEL_POLY5,
    /**
     * @brief PTLens model, which is also used by Hugin.
     *
     * \f[r_d = r_u \cdot (a r_u^3 + b r_u^2 + c r_u + 1 - a - b - c)\f]
     * The corresponding XML attributes are called “a”, “b”, and “c”.  They
     * default to 0.
     */
    LEGACY_LF_DIST_MODEL_PTLENS,
};

C_TYPEDEF (enum, legacy_lfDistortionModel)

/**
 * @brief Lens distortion calibration data.

 * Lens distortion depends only of focal length. The library will interpolate
 * the coefficients values if data for the exact focal length is not available.
 */
struct legacy_lfLensCalibDistortion
{
    /** @brief The type of distortion model used */
    enum legacy_lfDistortionModel Model;
    /** @brief Focal length in mm at which this calibration data was taken */
    float Focal;
    /** @brief Distortion coefficients, dependent on model (a,b,c or k1 or k1,k2) */
    float Terms [3];
};

C_TYPEDEF (struct, legacy_lfLensCalibDistortion)

/**
 * @brief The Lensfun library supports several models for lens lateral
 * chromatic aberrations (also called transversal chromatic
 * aberrations, TCA).
 *
 * TCAs depend on focal length, but does not depend of the aperture.  In the
 * following, \f$r_d\f$ refers to the distorted radius (normalised distance to
 * image center), and \f$r_u\f$ refers to the undistorted, corrected radius.
 * See section \ref actualorder for further information.
 *
 * For a popular explanation of chromatic aberrations see
 * http://www.vanwalree.com/optics/chromatic.html
 */
enum legacy_lfTCAModel
{
    /** @brief No TCA correction data is known */
    LEGACY_LF_TCA_MODEL_NONE,
    /**
     * @brief Linear lateral chromatic aberrations model.
     *
     * \f[\begin{aligned}
     * r_{d,R} &= r_{u,R} k_R \\
     * r_{d,B} &= r_{u,B} k_B
     * \end{aligned}\f]
     * The corresponding XML attributes are called “kr” and “kb”.  They default
     * to 1.
     * Ref: http://cipa.icomos.org/fileadmin/template/doc/TURIN/403.pdf
     */
    LEGACY_LF_TCA_MODEL_LINEAR,

    /**
     * @brief Third order polynomial.
     *
     * \f[\begin{aligned}
     * r_{d,R} &= r_{u,R} \cdot (b_R r_{u,R}^2 + c_R r_{u,R} + v_R) \\
     * r_{d,B} &= r_{u,B} \cdot (b_B r_{u,B}^2 + c_B r_{u,B} + v_B)
     * \end{aligned}\f]
     * The corresponding XML attributes are called “br”, “cr”,
     * “vr”, “bb”, “cb”, and “vb”.  vr and vb default to 1, the rest to 0.
     * Ref: http://wiki.panotools.org/Tca_correct
     */
    LEGACY_LF_TCA_MODEL_POLY3
};

C_TYPEDEF (enum, legacy_lfTCAModel)

/**
 * @brief Laterlal chromatic aberrations calibration data.
 *
 * Chromatic aberrations depend on focal length and aperture value. The library
 * will interpolate the coefficients if data for the exact focal length and
 * aperture value is not available with priority for a more exact focal length.
 */
struct legacy_lfLensCalibTCA
{
    /** @brief The lateral chromatic aberration model used */
    enum legacy_lfTCAModel Model;
    /** @brief Focal length in mm at which this calibration data was taken */
    float Focal;
    /** @brief The coefficients for TCA, dependent on model; separate for R and B */
    float Terms [6];
};

C_TYPEDEF (struct, legacy_lfLensCalibTCA)

/**
 * @brief The Lensfun library supports several models for lens vignetting
 * correction.
 *
 * We focus on optical and natural vignetting since they can be generalized for
 * all lenses of a certain type; mechanical vignetting is out of the scope of
 * this library.
 *
 * Vignetting is dependent on focal length and aperture.  There is
 * also a slight dependence on focus distance.  In the following,
 * \f$C_d\f$ refers to the corrected destination image pixel brightness, and
 * \f$C_s\f$ refers to the uncorrected source image pixel brightness.
 *
 * For a popular explanation of vignetting see
 * http://www.vanwalree.com/optics/vignetting.html
 */
enum legacy_lfVignettingModel
{
    /** @brief No vignetting correction data is known */
    LEGACY_LF_VIGNETTING_MODEL_NONE,
    /**
     * @brief Pablo D'Angelo vignetting model
     * (which is a more general variant of the \f$\cos^4\f$ law).
     *
     * \f[C_d = C_s \cdot (1 + k_1 r^2 + k_2 r^4 + k_3 r^6)\f]
     * The corresponding XML attributes are called “k1”, “k2”, and “k3”.  They
     * default to 0.
     * Ref: http://hugin.sourceforge.net/tech/
     */
    LEGACY_LF_VIGNETTING_MODEL_PA
};

C_TYPEDEF (enum, legacy_lfVignettingModel)

/**
 * @brief Lens vignetting calibration data.
 *
 * Vignetting depends on focal length, aperture and focus distance. The library
 * will interpolate the coefficients if data for the exact focal length,
 * aperture, and focus distance is not available.
 */
struct legacy_lfLensCalibVignetting
{
    /** @brief The lens vignetting model used */
    enum legacy_lfVignettingModel Model;
    /** @brief Focal length in mm at which this calibration data was taken */
    float Focal;
    /** @brief Aperture (f-number) at which this calibration data was taken */
    float Aperture;
    /** @brief Focus distance in meters */
    float Distance;
    /** @brief Lens vignetting model coefficients (depending on model) */
    float Terms [3];
};

C_TYPEDEF (struct, legacy_lfLensCalibVignetting)

/**
 *  @brief Different crop modes
 */
enum legacy_lfCropMode
{
    /** @brief no crop at all */
    LEGACY_LF_NO_CROP,
    /** @brief use a rectangular crop */
    LEGACY_LF_CROP_RECTANGLE,
    /** @brief use a circular crop, e.g. for circular fisheye images */
    LEGACY_LF_CROP_CIRCLE
};

C_TYPEDEF(enum, legacy_lfCropMode)

/**
 *  @brief Struct to save image crop, which can depend on the focal length
 */
struct legacy_lfLensCalibCrop
{
    /** @brief Focal length in mm at which this calibration data was taken */
    float Focal;
    /** @brief crop mode which should be applied to image to get rid of black borders */
    enum legacy_lfCropMode CropMode;
    /** @brief Crop coordinates, relative to image corresponding image dimension 
     *
     *  Crop goes left - 0, right - 1, top - 2, bottom - 3 
     *  Left/right refers always to long side (width in landscape mode), 
     *  top bottom to short side (height in landscape).
     *  Also negative values are allowed for cropping of fisheye images,
     *  where the crop circle can extend above the image border.
     */
    float Crop [4];
};

C_TYPEDEF (struct, legacy_lfLensCalibCrop)

/**
 *  @brief Struct to save calibrated field of view, which can depends on the focal length (DEPRECATED)
 *
 *  The Field of View (FOV) database entry is deprecated since Lensfun 
 *  version 0.3 and will be removed in future releases.
 *
 */
struct legacy_lfLensCalibFov
{
    /** Focal length in mm at which this calibration data was taken */
    float Focal;
    /** @brief Field of view for given images
     * 
     *  The Field of View (FOV) database entry is deprecated since Lensfun 
     *  version 0.3 and will be removed in future releases.
     *
     *  Especially for fisheye images the field of view calculated from the (EXIF) focal
     *  length differs slightly from the real field of view. The real field of view can be 
     *  stored in this field 
     */
    float FieldOfView;
};

C_TYPEDEF (struct, legacy_lfLensCalibFov)

/**
 *  @brief Struct to save real focal length, which can depends on the (nominal)
 *  focal length
 */
struct legacy_lfLensCalibRealFocal
{
    /** Nominal focal length in mm at which this calibration data was taken */
    float Focal;
    /** @brief Real focal length
     *
     *  When Lensfun speaks of “focal length”, the *nominal* focal length from
     *  the EXIF data or the gravure on the lens barrel is meant.  However,
     *  especially for fisheye lenses, the real focal length generally differs
     *  from that nominal focal length.  With “real focal length” I mean the
     *  focal length in the paraxial approximation, see
     *  <http://en.wikipedia.org/wiki/Paraxial_approximation>.  Note that Hugin
     *  (as of 2014) implements the calculation of the real focal length
     *  wrongly, see <http://article.gmane.org/gmane.comp.misc.ptx/34865>.
     */
    float RealFocal;
};

C_TYPEDEF (struct, legacy_lfLensCalibRealFocal)

/**
 * @brief This structure describes a single parameter for some lens model.
 */
struct legacy_lfParameter
{
    /** @brief Parameter name (something like 'k', 'k3', 'omega' etc.) */
    const char *Name;
    /** @brief Minimal value that has sense */
    float Min;
    /** @brief Maximal value that has sense */
    float Max;
    /** @brief Default value for the parameter */
    float Default;
};

C_TYPEDEF (struct, legacy_lfParameter)

/**
 * @brief Lens type.  See \ref changeofprojection for further information.
 */
enum legacy_lfLensType
{
    /** @brief Unknown lens type */
    LEGACY_LF_UNKNOWN,
    /** @brief Rectilinear lens
     *
     * Straight lines remain stright; 99% of all lenses are of this type.
     */
    LEGACY_LF_RECTILINEAR,
    /**
     * @brief Equidistant fisheye
     *
     * Ref: http://wiki.panotools.org/Fisheye_Projection
     */
    LEGACY_LF_FISHEYE,
    /**
     * @brief Panoramic (cylindrical)
     *
     * Not that there are such lenses, but useful to convert images \a to this
     * type, especially fish-eye images.
     */
    LEGACY_LF_PANORAMIC,
    /**
     * @brief Equirectangular
     *
     * Not that there are such lenses, but useful to convert images \a to this
     * type, especially fish-eye images.
     */
    LEGACY_LF_EQUIRECTANGULAR,
    /** @brief Orthographic fisheye */
    LEGACY_LF_FISHEYE_ORTHOGRAPHIC,
    /** @brief Stereographic fisheye */
    LEGACY_LF_FISHEYE_STEREOGRAPHIC,
    /** @brief Equisolid fisheye */
    LEGACY_LF_FISHEYE_EQUISOLID,
    /**
     * @brief Fisheye as measured by Thoby (for Nikkor 10.5).
     *
     * Ref: http://michel.thoby.free.fr/Blur_Panorama/Nikkor10-5mm_or_Sigma8mm/Sigma_or_Nikkor/Comparison_Short_Version_Eng.html
     */
    LEGACY_LF_FISHEYE_THOBY
};

C_TYPEDEF (enum, legacy_lfLensType)

/**
 * @brief Lens data.
 * Unknown fields are set to NULL or 0.
 *
 * To create a new lens object, use the legacy_lfLens::Create() or legacy_lf_lens_new()
 * functions. After that fill the fields for which you have data, and
 * invoke the legacy_lfLens::Check or legacy_lf_lens_check() function, which will
 * check if existing data is enough and will fill some fields using
 * information extracted from lens name.
 */
struct LEGACY_LF_EXPORT legacy_lfLens
{
    /** Lens maker (ex: "Rollei") */
    legacy_lfMLstr Maker;
    /** Lens model (ex: "Zoom-Rolleinar") */
    legacy_lfMLstr Model;
    /** Minimum focal length, mm (ex: 35). */
    float MinFocal;
    /** Maximum focal length, mm (ex: 105). Can be equal to MinFocal. */
    float MaxFocal;
    /** Smallest f-number possible (ex: 3.5). */
    float MinAperture;
    /** Biggest f-number possible (ex: 22). Can be equal to MinAperture. */
    float MaxAperture;
    /** Available mounts (NULL-terminated list) (ex: { "QBM", NULL }) */
    char **Mounts;
    /**
     * The horizontal shift of all lens distortions.
     * Note that distortion and TCA uses same geometrical lens center.
     * It is given as a relative value to avoide dependency on the
     * image and/or sensor sizes. The calibrated delta X and Y values are
     * numbers in the -0.5 .. +0.5 range. For 1 we take the maximal image
     * dimension (width or height) - this is related to the fact that the
     * lens has a circular field of projection disregarding sensor size.
     */
    float CenterX;
    /** The vertical shift of all lens distortions. (0,0) for geometric center */
    float CenterY;
    /** Crop factor at which calibration measurements were taken.  Must be defined. */
    float CropFactor;
    /** Aspect ratio of the images used for calibration measurements. */
    float AspectRatio;
    /** Lens type */
    legacy_lfLensType Type;
    /** Lens distortion calibration data, NULL-terminated (unsorted) */
    legacy_lfLensCalibDistortion **CalibDistortion;
    /** Lens TCA calibration data, NULL-terminated (unsorted) */
    legacy_lfLensCalibTCA **CalibTCA;
    /** Lens vignetting calibration data, NULL-terminated (unsorted) */
    legacy_lfLensCalibVignetting **CalibVignetting;
    /** Crop data, NULL-terminated (unsorted) */
    legacy_lfLensCalibCrop **CalibCrop;
    /** Field of view calibration data, NULL-terminated (unsorted) */
    legacy_lfLensCalibFov **CalibFov;
    /** Real focal length calibration data, NULL-terminated (unsorted) */
    legacy_lfLensCalibRealFocal **CalibRealFocal;
    /** Lens matching score, used while searching: not actually a lens parameter */
    int Score;

#ifdef __cplusplus
    /**
     * @brief Create a new lens object, initializing all fields to default values.
     */
    legacy_lfLens ();

    /**
     * Copy constructor.
     */
    legacy_lfLens (const legacy_lfLens &other);

    /**
     * @brief Destroy this and all associated objects.
     */
    ~legacy_lfLens ();

    /**
     * Assignment operator
     */
    legacy_lfLens &operator = (const legacy_lfLens &other);

    /**
     * @brief Add a string to camera maker.
     *
     * If lang is NULL, this replaces the default value, otherwise a new
     * language value is appended.
     * @param val
     *     The new value for the Maker field.
     * @param lang
     *     The language this field is in.
     */
    void SetMaker (const char *val, const char *lang = NULL);

    /**
     * @brief Add a string to camera model.
     *
     * If lang is NULL, this replaces the default value, otherwise a new
     * language value is appended.
     * @param val
     *     The new value for the Model field.
     * @param lang
     *     The language this field is in.
     */
    void SetModel (const char *val, const char *lang = NULL);

    /**
     * @brief Add a new mount type to this lens.
     *
     * This is not a multi-language string, this it's just plain replaced.
     * @param val
     *     The new value to add to the Mounts array.
     */
    void AddMount (const char *val);

    /**
     * @brief Add a new distortion calibration structure to the pool.
     *
     * The objects is copied, thus you can reuse it as soon as
     * this function returns.
     * @param dc
     *     The distortion calibration structure.
     */
    void AddCalibDistortion (const legacy_lfLensCalibDistortion *dc);

    /**
     * @brief Remove a calibration entry from the distortion calibration data.
     * @param idx
     *     The calibration data index (zero-based).
     */
    bool RemoveCalibDistortion (int idx);

    /**
     * @brief Add a new transversal chromatic aberration calibration structure
     * to the pool.
     *
     * The objects is copied, thus you can reuse it as soon as
     * this function returns.
     * @param tcac
     *     The transversal chromatic aberration calibration structure.
     */
    void AddCalibTCA (const legacy_lfLensCalibTCA *tcac);

    /**
     * @brief Remove a calibration entry from the TCA calibration data.
     * @param idx
     *     The calibration data index (zero-based).
     */
    bool RemoveCalibTCA (int idx);

    /**
     * @brief Add a new vignetting calibration structure to the pool.
     *
     * The objects is copied, thus you can reuse it as soon as
     * this function returns.
     * @param vc
     *     The vignetting calibration structure.
     */
    void AddCalibVignetting (const legacy_lfLensCalibVignetting *vc);

    /**
     * @brief Remove a calibration entry from the vignetting calibration data.
     * @param idx
     *     The calibration data index (zero-based).
     */
    bool RemoveCalibVignetting (int idx);

    /**
     * @brief Add a new lens crop structure to the pool.
     *
     * The objects is copied, thus you can reuse it as soon as
     * this function returns.
     * @param cc 
     *     The lens crop structure.
     */
    void AddCalibCrop (const legacy_lfLensCalibCrop *cc);

    /**
     * @brief Remove a lens crop entry from the lens crop structure.
     * @param idx
     *     The lens crop data index (zero-based).
     */
    bool RemoveCalibCrop (int idx);

    /**
     * @brief Add a new lens fov structure to the pool. 
     *
     * The Field of View (FOV) database entry is deprecated since Lensfun 
     * version 0.3 and will be removed in future releases.
     *
     * The objects is copied, thus you can reuse it as soon as
     * this function returns.
     * @param cf
     *     The lens fov structure.
     */
    DEPRECATED void AddCalibFov (const legacy_lfLensCalibFov *cf);

    /**
     * @brief Remove a field of view  entry from the lens fov structure.
     *
     * The Field of View (FOV) database entry is deprecated since Lensfun 
     * version 0.3 and will be removed in future releases.
     *
     * @param idx
     *     The lens information data index (zero-based).
     */
    DEPRECATED bool RemoveCalibFov (int idx);

    /**
     * @brief Add a new lens real focal length structure to the pool.
     *
     * The objects is copied, thus you can reuse it as soon as
     * this function returns.
     * @param cf
     *     The lens real focal length structure.
     */
    void AddCalibRealFocal (const legacy_lfLensCalibRealFocal *cf);

    /**
     * @brief Remove a real focal length entry from the lens real focal length
     * structure.
     * @param idx
     *     The lens information data index (zero-based).
     */
    bool RemoveCalibRealFocal (int idx);

    /**
     * @brief This method fills some fields if they are missing but
     * can be derived from other fields.
     *
     * This includes such non-obvious parameters like the range of focal
     * lengths or the range of apertures, which can be derived from lens named
     * (which is intelligently parsed) or from the list of calibrations.
     */
    void GuessParameters ();

    /**
     * @brief Check if a lens object is valid.
     * @return
     *     true if the required fields are ok.
     */
    bool Check ();

    /**
     * @brief Get the human-readable distortion model name and the descriptions
     * of the parameters required by this model.
     * @param model
     *     The model.
     * @param details
     *     If not NULL, this string will be set to a more detailed (technical)
     *     description of the model. This string may contain newlines.
     * @param params
     *     If not NULL, this variable will be set to a pointer to an array
     *     of legacy_lfParameter structures, every structure describes a model
     *     parameter. The list is NULL-terminated.
     * @return
     *     A short name of the distortion model or NULL if model is unknown.
     */
    static const char *GetDistortionModelDesc (
        legacy_lfDistortionModel model, const char **details, const legacy_lfParameter ***params);
    /**
     * @brief Get the human-readable transversal chromatic aberrations model name
     * and the descriptions of the parameters required by this model.
     * @param model
     *     The model.
     * @param details
     *     If not NULL, this string will be set to a more detailed (technical)
     *     description of the model. This string may contain newlines.
     * @param params
     *     If not NULL, this variable will be set to a pointer to an array
     *     of legacy_lfParameter structures, every structure describes a model
     *     parameter. The list is NULL-terminated.
     * @return
     *     A short name of the TCA model or NULL if model is unknown.
     */
    static const char *GetTCAModelDesc (
        legacy_lfTCAModel model, const char **details, const legacy_lfParameter ***params);

    /**
     * @brief Get the human-readable vignetting model name and the descriptions
     * of the parameters required by this model.
     * @param model
     *     The model.
     * @param details
     *     If not NULL, this string will be set to a more detailed (technical)
     *     description of the model. This string may contain newlines.
     * @param params
     *     If not NULL, this variable will be set to a pointer to an array
     *     of legacy_lfParameter structures, every structure describes a model
     *     parameter. The list is NULL-terminated.
     * @return
     *     A short name of the vignetting model or NULL if model is unknown.
     */
    static const char *GetVignettingModelDesc (
        legacy_lfVignettingModel model, const char **details, const legacy_lfParameter ***params);

    /**
     * @brief Get the human-readable crop name and the descriptions
     * of the parameters required by this model.
     * @param mode
     *     The crop mode.
     * @param details
     *     If not NULL, this string will be set to a more detailed (technical)
     *     description of the model. This string may contain newlines.
     * @param params
     *     If not NULL, this variable will be set to a pointer to an array
     *     of legacy_lfParameter structures, every structure describes a model
     *     parameter. The list is NULL-terminated.
     * @return
     *     A short name of the distortion model or NULL if model is unknown.
     */
    static const char *GetCropDesc (
        legacy_lfCropMode mode, const char **details, const legacy_lfParameter ***params);

    /**
     * @brief Get the human-readable lens type name and a short description of this
     * lens type.
     * @param type
     *     Lens type.
     * @param details
     *     If not NULL, this string will be set to a more detailed (technical)
     *     description of the lens type. This string may contain newlines.
     * @return
     *     A short name of the lens type or NULL if model is unknown.
     */
    static const char *GetLensTypeDesc (legacy_lfLensType type, const char **details);

    /**
     * @brief Interpolate lens geometry distortion data for given focal length.
     * @param focal
     *     The focal length in mm at which we need geometry distortion parameters.
     * @param res
     *     The resulting interpolated model.
     */
    bool InterpolateDistortion (float focal, legacy_lfLensCalibDistortion &res) const;

    /**
     * @brief Interpolate lens TCA calibration data for given focal length.
     * @param focal
     *     The focal length in mm at which we need TCA parameters.
     * @param res
     *     The resulting interpolated model.
     */
    bool InterpolateTCA (float focal, legacy_lfLensCalibTCA &res) const;

    /**
     * @brief Interpolate lens vignetting model parameters for given focal length,
     * aperture, and focus distance.
     * @param focal
     *     The focal length in mm for which we need vignetting parameters.
     * @param aperture
     *     The aperture (f-number) for which we need vignetting parameters.
     * @param distance
     *     The focus distance in meters (distance > 0) for which we need
     *     vignetting parameters.
     * @param res
     *     The resulting interpolated model.
     */
    bool InterpolateVignetting (
        float focal, float aperture, float distance, legacy_lfLensCalibVignetting &res) const;

    /**
     * @brief Interpolate lens crop data for given focal length.
     * @param focal
     *     The focal length in mm at which we need image parameters.
     * @param res
     *     The resulting interpolated information data.
     */
    bool InterpolateCrop (float focal, legacy_lfLensCalibCrop &res) const;

    /**
     * @brief Interpolate lens fov data for given focal length.
     *
     * The Field of View (FOV) database entry is deprecated since Lensfun 
     * version 0.3 and will be removed in future releases.
     *
     * @param focal
     *     The focal length in mm at which we need image parameters.
     * @param res
     *     The resulting interpolated information data.
     */
    DEPRECATED bool InterpolateFov (float focal, legacy_lfLensCalibFov &res) const;

    /**
     * @brief Interpolate lens real focal length data for given focal length.
     *
     * The Field of View (FOV) database entry is deprecated since Lensfun 
     * version 0.3 and will be removed in future releases.
     *
     * @param focal
     *     The nominal focal length in mm at which we need image parameters.
     * @param res
     *     The resulting interpolated information data.
     */
    bool InterpolateRealFocal (float focal, legacy_lfLensCalibRealFocal &res) const;
#endif
};

C_TYPEDEF (struct, legacy_lfLens)

/**
 * @brief Create a new lens object.
 * @return
 *     A new empty lens object.
 * @sa
 *     legacy_lfLens::legacy_lfLens
 */
LEGACY_LF_EXPORT legacy_lfLens *legacy_lf_lens_new ();

/**
 * @brief Destroy a legacy_lfLens object.
 *
 * This is equivalent to C++ "delete lens".
 * @param lens
 *     The lens object to destroy.
 * @sa
 *     legacy_lfLens::~legacy_lfLens
 */
LEGACY_LF_EXPORT void legacy_lf_lens_destroy (legacy_lfLens *lens);

/**
 * @brief Copy the data from one legacy_lfLens structure into another.
 * @param dest
 *     The destination object
 * @param source
 *     The source object
 * @sa
 *     legacy_lfLens::operator = (const legacy_lfCamera &)
 */
LEGACY_LF_EXPORT void legacy_lf_lens_copy (legacy_lfLens *dest, const legacy_lfLens *source);

/** @sa legacy_lfLens::Check */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_check (legacy_lfLens *lens);

/** @sa legacy_lfLens::GuessParameters */
LEGACY_LF_EXPORT void legacy_lf_lens_guess_parameters (legacy_lfLens *lens);

/** @sa legacy_lfLens::GetDistortionModelDesc */
LEGACY_LF_EXPORT const char *legacy_lf_get_distortion_model_desc (
    enum legacy_lfDistortionModel model, const char **details, const legacy_lfParameter ***params);

/** @sa legacy_lfLens::GetTCAModelDesc */
LEGACY_LF_EXPORT const char *legacy_lf_get_tca_model_desc (
    enum legacy_lfTCAModel model, const char **details, const legacy_lfParameter ***params);

/** @sa legacy_lfLens::GetVignettingModelDesc */
LEGACY_LF_EXPORT const char *legacy_lf_get_vignetting_model_desc (
    enum legacy_lfVignettingModel model, const char **details, const legacy_lfParameter ***params);

/** @sa legacy_lfLens::GetCropDesc */
LEGACY_LF_EXPORT const char *legacy_lf_get_crop_desc (
    enum legacy_lfCropMode mode, const char **details, const legacy_lfParameter ***params);

/** @sa legacy_lfLens::GetLensTypeDesc */
LEGACY_LF_EXPORT const char *legacy_lf_get_lens_type_desc (
    enum legacy_lfLensType type, const char **details);

/** @sa legacy_lfLens::InterpolateDistortion */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_interpolate_distortion (const legacy_lfLens *lens, float focal,
    legacy_lfLensCalibDistortion *res);

/** @sa legacy_lfLens::InterpolateTCA */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_interpolate_tca (const legacy_lfLens *lens, float focal, legacy_lfLensCalibTCA *res);

/** @sa legacy_lfLens::InterpolateVignetting */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_interpolate_vignetting (const legacy_lfLens *lens, float focal, float aperture,
    float distance, legacy_lfLensCalibVignetting *res);

/** @sa legacy_lfLens::InterpolateCrop */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_interpolate_crop (const legacy_lfLens *lens, float focal,
    legacy_lfLensCalibCrop *res);

/** @sa legacy_lfLens::InterpolateFov */
DEPRECATED LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_interpolate_fov (const legacy_lfLens *lens, float focal,
    legacy_lfLensCalibFov *res);

/** @sa legacy_lfLens::InterpolateRealFocal */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_interpolate_real_focal (const legacy_lfLens *lens, float focal,
    legacy_lfLensCalibRealFocal *res);

/** @sa legacy_lfLens::AddCalibDistortion */
LEGACY_LF_EXPORT void legacy_lf_lens_add_calib_distortion (legacy_lfLens *lens, const legacy_lfLensCalibDistortion *dc);

/** @sa legacy_lfLens::RemoveCalibDistortion */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_remove_calib_distortion (legacy_lfLens *lens, int idx);

/** @sa legacy_lfLens::AddCalibTCA */
LEGACY_LF_EXPORT void legacy_lf_lens_add_calib_tca (legacy_lfLens *lens, const legacy_lfLensCalibTCA *tcac);

/** @sa legacy_lfLens::RemoveCalibTCA */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_remove_calib_tca (legacy_lfLens *lens, int idx);

/** @sa legacy_lfLens::AddCalibVignetting */
LEGACY_LF_EXPORT void legacy_lf_lens_add_calib_vignetting (legacy_lfLens *lens, const legacy_lfLensCalibVignetting *vc);

/** @sa legacy_lfLens::RemoveCalibVignetting */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_remove_calib_vignetting (legacy_lfLens *lens, int idx);

/** @sa legacy_lfLens::AddCalibCrop */
LEGACY_LF_EXPORT void legacy_lf_lens_add_calib_crop (legacy_lfLens *lens, const legacy_lfLensCalibCrop *cc);

/** @sa legacy_lfLens::RemoveCalibCrop */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_remove_calib_crop (legacy_lfLens *lens, int idx);

/** @sa legacy_lfLens::AddCalibFov */
DEPRECATED LEGACY_LF_EXPORT void legacy_lf_lens_add_calib_fov (legacy_lfLens *lens, const legacy_lfLensCalibFov *cf);

/** @sa legacy_lfLens::RemoveCalibFov */
DEPRECATED LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_remove_calib_fov (legacy_lfLens *lens, int idx);

/** @sa legacy_lfLens::AddCalibRealFocal */
LEGACY_LF_EXPORT void legacy_lf_lens_add_calib_real_focal (legacy_lfLens *lens, const legacy_lfLensCalibRealFocal *cf);

/** @sa legacy_lfLens::RemoveCalibRealFocal */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_lens_remove_calib_real_focal (legacy_lfLens *lens, int idx);

/** @} */

/*----------------------------------------------------------------------------*/

/**
 * @defgroup Database Database functions
 * @brief Create, destroy and search database for objects.
 * @{
 */

/**
 * @brief Flags controlling the behavior of database searches.
 */
enum
{
    /**
     * @brief This flag selects a looser search algorithm resulting in
     * more results (still sorted by score).
     *
     * If it is not present, all results where at least one of the input
     * words is missing will be discarded.
     */
    LEGACY_LF_SEARCH_LOOSE = 1,
    /**
     * @brief This flag makes Lensfun to sort the results by focal
     * length, and remove all double lens names.
     *
     * If a lens has entries for different crop factors/aspect ratios,
     * the only best match is included into the result.  This is meant
     * to be used to create a list presented to the user to pick a lens.
     * Or, to lookup a lens for which you know maker and model exactly,
     * and to check whether the result list really contains only one
     * element.
     */
    LEGACY_LF_SEARCH_SORT_AND_UNIQUIFY = 2
};

/**
 * @brief A lens database object.
 *
 * This object contains a list of mounts, cameras and lenses through
 * which you can search. The objects are loaded from XML files
 * located in (default configuration):
 *
\verbatim
/usr/share/lensfun/
/usr/local/share/lensfun/
~/.local/share/lensfun/
\endverbatim
 *
 * Objects loaded later override objects loaded earlier.
 * Thus, if user modifies a object you must save it to its home directory
 * (see legacy_lfDatabase::HomeDataDir), where it will override any definitions
 * from the system-wide database.
 *
 * You cannot create and destroy objects of this type directly; instead
 * use the legacy_lf_db_new() / legacy_lf_db_destroy() functions or the
 * legacy_lfDatabase::Create() / legacy_lfDatabase::Destroy() methods.
 */
struct LEGACY_LF_EXPORT legacy_lfDatabase
{
    /** @brief Home lens database directory (something like "~/.local/share/lensfun") */
    char *HomeDataDir;
    /** @brief Home lens database directory for automatic updates (something
     * like "~/.local/share/lensfun/updates") */
    char *UserUpdatesDir;

#ifdef __cplusplus
    legacy_lfDatabase ();
    ~legacy_lfDatabase ();

    /**
     * @brief Create a new empty database object.
     */
    static legacy_lfDatabase *Create ();

    /**
     * @brief Destroy the database object and free all loaded data.
     */
    void Destroy ();

    /**
     * @brief Load all XML files from a directory.
     *
     * This is an internal function used by legacy_lfDatabase::Load ().  It ignores
     * all errors.
     * @param dirname
     *     The directory to be read.
     * @return
     *     True if valid lensfun XML data was succesfully loaded.
     */
    bool LoadDirectory (const char *dirname);

    /**
     * @brief Find and load the lens database.
     * 
     * See @ref dbsearch for more information.
     * @return
     *     LEGACY_LF_NO_ERROR or a error code.
     */
    legacy_lfError Load ();

    /**
     * @brief Load just a specific XML file.
     *
     * If the loaded file contains the specification of a camera/lens that's
     * already in memory, it overrides that data.
     * @param filename
     *     The name of a XML file to load. Note that Lensfun does not support
     *     the full XML specification as it uses the glib's simple XML parser,
     *     so advanced XML features are not available.
     * @return
     *     LEGACY_LF_NO_ERROR or a error code.
     */
    legacy_lfError Load (const char *filename);

    /**
     * @brief Load a set of camera/lenses from a memory array.
     *
     * This is the lowest-level loading function.
     * @param errcontext
     *     The error context to be displayed in error messages
     *     (usually this is the name of the file to which data belongs).
     * @param data
     *     The XML data.
     * @param data_size
     *     XML data size in bytes.
     * @return
     *     LEGACY_LF_NO_ERROR or a error code.
     */
    legacy_lfError Load (const char *errcontext, const char *data, size_t data_size);

    /**
     * @brief Save the whole database to a file.
     * @param filename
     *     The file name to write the XML stream into.
     * @return
     *     LEGACY_LF_NO_ERROR or a error code.
     */
    legacy_lfError Save (const char *filename) const;

    /**
     * @brief Save a set of camera and lens descriptions to a file.
     * @param filename
     *     The file name to write the XML stream into.
     * @param mounts
     *     A list of mounts to be written to the file. Can be NULL.
     * @param cameras
     *     A list of cameras to be written to the file. Can be NULL.
     * @param lenses
     *     A list of lenses to be written to the file. Can be NULL.
     * @return
     *     LEGACY_LF_NO_ERROR or a error code.
     */
    legacy_lfError Save (const char *filename,
                  const legacy_lfMount *const *mounts,
                  const legacy_lfCamera *const *cameras,
                  const legacy_lfLens *const *lenses) const;

    /**
     * @brief Save a set of camera and lens descriptions into a memory array.
     * @param mounts
     *     A list of mounts to be written to the file. Can be NULL.
     * @param cameras
     *     A list of cameras to be written to the file. Can be NULL.
     * @param lenses
     *     A list of lenses to be written to the file. Can be NULL.
     * @return
     *     A pointer to an allocated string with the output.
     *     Free it with legacy_lf_free().
     */
    static char *Save (const legacy_lfMount *const *mounts,
                       const legacy_lfCamera *const *cameras,
                       const legacy_lfLens *const *lenses);

    /**
     * @brief Find a set of cameras that fit given criteria.
     *
     * The maker and model must be given (if possible) exactly as they are
     * spelled in database, except that the library will compare
     * case-insensitively and will compress spaces. This means that the
     * database must contain camera maker/lens *exactly* how it is given
     * in EXIF data, but you may add human-friendly translations of them
     * using the multi-language string feature (including a translation
     * to "en" to avoid displaying EXIF tags in user interface - they are
     * often upper-case which looks ugly).
     * @param maker
     *     Camera maker (either from EXIF tags or from some other source).
     *     The string is expected to be pure ASCII, since EXIF data does
     *     not allow 8-bit data to be used.
     * @param model
     *     Camera model (either from EXIF tags or from some other source).
     *     The string is expected to be pure ASCII, since EXIF data does
     *     not allow 8-bit data to be used.
     * @return
     *     A NULL-terminated list of cameras matching the search criteria
     *     or NULL if none. Release return value with legacy_lf_free() (only the list
     *     of pointers, not the camera objects!).
     */
    const legacy_lfCamera **FindCameras (const char *maker, const char *model) const;

    /**
     * @brief Searches all translations of camera maker and model.
     *
     * Thus, you may search for a user-entered camera even in a language
     * different from English.  This function is somewhat similar to
     * FindCameras(), but uses a different search algorithm.
     *
     * This is a lot slower than FindCameras().
     * @param maker
     *     Camera maker. This can be any UTF-8 string.
     * @param model
     *     Camera model. This can be any UTF-8 string.
     * @param sflags
     *     Additional flags influencing the search algorithm.
     *     This is a combination of LEGACY_LF_SEARCH_XXX flags.
     * @return
     *     A NULL-terminated list of cameras matching the search criteria
     *     or NULL if none. Release return value with legacy_lf_free() (only the list
     *     of pointers, not the camera objects!).
     */
    const legacy_lfCamera **FindCamerasExt (const char *maker, const char *model,
                                     int sflags = 0) const;

    /**
     * @brief Retrieve a full list of cameras.
     * @return
     *     An NULL-terminated list containing all cameras loaded until now.
     *     The list is valid only until the lens database is modified.
     *     The returned pointer does not have to be freed.
     */
    const legacy_lfCamera *const *GetCameras () const;

    /**
     * @brief Parse a human-friendly lens description (ex: "smc PENTAX-F 35-105mm F4-5.6"
     * or "SIGMA AF 28-300 F3.5-5.6 DL IF") and return a list of legacy_lfLens'es which
     * are matching this description.
     *
     * Multiple lenses may be returned if multiple lenses match (perhaps due to
     * non-unique lens description provided, e.g.  "Pentax SMC").
     *
     * The matching algorithm works as follows: first the user description
     * is tried to be interpreted according to several well-known lens naming
     * schemes, so additional data like focal and aperture ranges are extracted
     * if they are present. After that word matching is done; a lens matches
     * the description ONLY IF it contains all the words found in the description
     * (including buzzwords e.g. IF IZ AL LD DI USM SDM etc). Order of the words
     * does not matter. An additional check is done on the focal/aperture ranges,
     * they must exactly match if they are specified.
     * @param camera
     *     The camera (can be NULL if camera is unknown, or just certain
     *     fields in structure can be NULL). The algorithm will look for
     *     a lens with crop factor not larger than of given camera, since
     *     the mathematical models of the lens can be incorrect for sensor
     *     sizes larger than the one used for calibration. Also camera
     *     mount is taken into account, the lenses with incompatible
     *     mounts will be filtered out.
     * @param maker
     *     Lens maker or NULL if not known.
     * @param model
     *     A human description of the lens model(-s).
     * @param sflags
     *     Additional flags influencing the search algorithm.
     *     This is a combination of LEGACY_LF_SEARCH_XXX flags.
     * @return
     *     A list of lenses parsed from user description or NULL.
     *     Release memory with legacy_lf_free(). The list is ordered in the
     *     most-likely to least-likely order, e.g. the first returned
     *     value is the most likely match.
     */
    const legacy_lfLens **FindLenses (const legacy_lfCamera *camera, const char *maker,
                               const char *model, int sflags = 0) const;

    /**
     * @brief Find a set of lenses that fit certain criteria.
     * @param lens
     *     The approximative lense. Uncertain fields may be NULL.
     *     The "CropFactor" field defines the minimal value for crop factor;
     *     no lenses with crop factor larger than that will be returned.
     *     The Mounts field will be scanned for allowed mounts, if NULL
     *     any mounts are considered compatible.
     * @param sflags
     *     Additional flags influencing the search algorithm.
     *     This is a combination of LEGACY_LF_SEARCH_XXX flags.
     * @return
     *     A NULL-terminated list of lenses matching the search criteria
     *     or NULL if none. Release memory with legacy_lf_free(). The list is ordered
     *     in the most-likely to least-likely order, e.g. the first returned
     *     value is the most likely match.
     */
    const legacy_lfLens **FindLenses (const legacy_lfLens *lens, int sflags = 0) const;

    /**
     * @brief Retrieve a full list of lenses.
     * @return
     *     An NULL-terminated list containing all lenses loaded until now.
     *     The list is valid only until the lens database is modified.
     *     The returned pointer does not have to be freed.
     */
    const legacy_lfLens *const *GetLenses () const;

    /**
     * @brief Return the legacy_lfMount structure given the (basic) mount name.
     * @param mount
     *     The basic mount name.
     * @return
     *     A pointer to legacy_lfMount structure or NULL.
     */
    const legacy_lfMount *FindMount (const char *mount) const;

    /**
     * @brief Get the name of a mount in current locale.
     * @param mount
     *     The basic mount name.
     * @return
     *     The name of the mount in current locale (UTF-8 string).
     */
    const char *MountName (const char *mount) const;

    /**
     * @brief Retrieve a full list of mounts.
     * @return
     *     An array containing all mounts loaded until now.
     *     The list is valid only until the mount database is modified.
     *     The returned pointer does not have to be freed.
     */
    const legacy_lfMount *const *GetMounts () const;

    /**
     * @brief Add a mount to the database.
     * @param mount
     *     the mount to add
     */
    void AddMount (legacy_lfMount *mount);

    /**
     * @brief Add a camera to the database.
     * @param camera
     *     the camera to add
     */
    void AddCamera (legacy_lfCamera *camera);

    /**
     * @brief Add a lens to the database.
     * @param lens
     *     the lens to add
     */
    void AddLens (legacy_lfLens *lens);

private:
#endif
    void *Mounts;
    void *Cameras;
    void *Lenses;
};

C_TYPEDEF (struct, legacy_lfDatabase)

/**
 * @brief Create a new empty database object.
 *
 * Usually the application will want to do this at startup,
 * after which it would be a good idea to call legacy_lf_db_load().
 * @return
 *     A new empty database object.
 * @sa
 *     legacy_lfDatabase::legacy_lfDatabase
 */
LEGACY_LF_EXPORT legacy_lfDatabase *legacy_lf_db_new (void);

/**
 * @brief Destroy the database object.
 *
 * This is the only way to correctly destroy the database object.
 * @param db
 *     The database to destroy.
 * @sa
 *     legacy_lfDatabase::~legacy_lfDatabase
 */
LEGACY_LF_EXPORT void legacy_lf_db_destroy (legacy_lfDatabase *db);

/** @sa legacy_lfDatabase::Load() */
LEGACY_LF_EXPORT legacy_lfError legacy_lf_db_load (legacy_lfDatabase *db);

/** @sa legacy_lfDatabase::Load(const char *) */
LEGACY_LF_EXPORT legacy_lfError legacy_lf_db_load_file (legacy_lfDatabase *db, const char *filename);

/** @sa legacy_lfDatabase::Load(const char *, const char *, size_t) */
LEGACY_LF_EXPORT legacy_lfError legacy_lf_db_load_data (legacy_lfDatabase *db, const char *errcontext,
                                   const char *data, size_t data_size);

/** @sa legacy_lfDatabase::Save(const char *) */
LEGACY_LF_EXPORT legacy_lfError legacy_lf_db_save_all (const legacy_lfDatabase *db, const char *filename);

/** @sa legacy_lfDatabase::Save(const char *, const legacy_lfMount *const *, const legacy_lfCamera *const *, const legacy_lfLens *const *) */
LEGACY_LF_EXPORT legacy_lfError legacy_lf_db_save_file (const legacy_lfDatabase *db, const char *filename,
                                   const legacy_lfMount *const *mounts,
                                   const legacy_lfCamera *const *cameras,
                                   const legacy_lfLens *const *lenses);

/** @sa legacy_lfDatabase::Save(const legacy_lfMount *const *, const legacy_lfCamera *const *, const legacy_lfLens *const *) */
LEGACY_LF_EXPORT char *legacy_lf_db_save (const legacy_lfMount *const *mounts,
                            const legacy_lfCamera *const *cameras,
                            const legacy_lfLens *const *lenses);

/** @sa legacy_lfDatabase::FindCameras */
LEGACY_LF_EXPORT const legacy_lfCamera **legacy_lf_db_find_cameras (
    const legacy_lfDatabase *db, const char *maker, const char *model);

/** @sa legacy_lfDatabase::FindCamerasExt */
LEGACY_LF_EXPORT const legacy_lfCamera **legacy_lf_db_find_cameras_ext (
    const legacy_lfDatabase *db, const char *maker, const char *model, int sflags);

/** @sa legacy_lfDatabase::GetCameras */
LEGACY_LF_EXPORT const legacy_lfCamera *const *legacy_lf_db_get_cameras (const legacy_lfDatabase *db);

/** @sa legacy_lfDatabase::FindLenses(const legacy_lfCamera *, const char *, const char *) */
LEGACY_LF_EXPORT const legacy_lfLens **legacy_lf_db_find_lenses_hd (
    const legacy_lfDatabase *db, const legacy_lfCamera *camera, const char *maker,
    const char *lens, int sflags);

/** @sa legacy_lfDatabase::FindLenses(const legacy_lfCamera *, const legacy_lfLens *) */
LEGACY_LF_EXPORT const legacy_lfLens **legacy_lf_db_find_lenses (
    const legacy_lfDatabase *db, const legacy_lfLens *lens, int sflags);

/** @sa legacy_lfDatabase::GetLenses */
LEGACY_LF_EXPORT const legacy_lfLens *const *legacy_lf_db_get_lenses (const legacy_lfDatabase *db);

/** @sa legacy_lfDatabase::FindMount */
LEGACY_LF_EXPORT const legacy_lfMount *legacy_lf_db_find_mount (const legacy_lfDatabase *db, const char *mount);

/** @sa legacy_lfDatabase::MountName */
LEGACY_LF_EXPORT const char *legacy_lf_db_mount_name (const legacy_lfDatabase *db, const char *mount);

/** @sa legacy_lfDatabase::GetMounts */
LEGACY_LF_EXPORT const legacy_lfMount *const *legacy_lf_db_get_mounts (const legacy_lfDatabase *db);

/** @} */

/*----------------------------------------------------------------------------*/

/**
 * @defgroup Correction Image correction
 * @brief This group of functions will allow you to apply a image transform
 * that will correct some lens defects.
 * @{
 */

/** @brief A list of bitmask flags used for ordering various image corrections */
enum
{
    /** Correct (or apply) lens transversal chromatic aberrations */
    LEGACY_LF_MODIFY_TCA        = 0x00000001,
    /** Correct (or apply) lens vignetting */
    LEGACY_LF_MODIFY_VIGNETTING = 0x00000002,
    /* Value 0x00000004 is deprecated. */
    /** Correct (or apply) lens distortion */
    LEGACY_LF_MODIFY_DISTORTION = 0x00000008,
    /** Convert image geometry */
    LEGACY_LF_MODIFY_GEOMETRY   = 0x00000010,
    /** Additional resize of image */
    LEGACY_LF_MODIFY_SCALE      = 0x00000020,
    /** Apply all possible corrections */
    LEGACY_LF_MODIFY_ALL        = ~0
};

/** @brief A list of pixel formats supported by internal colour callbacks */
enum legacy_lfPixelFormat
{
    /** Unsigned 8-bit R,G,B */
    LEGACY_LF_PF_U8,
    /** Unsigned 16-bit R,G,B */
    LEGACY_LF_PF_U16,
    /** Unsigned 32-bit R,G,B */
    LEGACY_LF_PF_U32,
    /** 32-bit floating-point R,G,B */
    LEGACY_LF_PF_F32,
    /** 64-bit floating-point R,G,B */
    LEGACY_LF_PF_F64
};

C_TYPEDEF (enum, legacy_lfPixelFormat)

/** @brief These constants define the role of every pixel component, four bits
 * each.  "pixel" refers here to a set of values which share the same (x, y)
 * coordinates. */
enum legacy_lfComponentRole
{
    /**
     * This marks the end of the role list. It doesn't have to be specified
     * explicitly, since LEGACY_LF_CR_X macros always pad the value with zeros
     */
    LEGACY_LF_CR_END = 0,
    /**
     * This value tells that what follows applies to next pixel.
     * This can be used to define Bayer images, e.g. use
     * LEGACY_LF_CR_3(LEGACY_LF_CR_RED, LEGACY_LF_CR_NEXT, LEGACY_LF_CR_GREEN) for even rows and
     * LEGACY_LF_CR_3(LEGACY_LF_CR_GREEN, LEGACY_LF_CR_NEXT, LEGACY_LF_CR_BLUE) for odd rows.
     */
    LEGACY_LF_CR_NEXT,
    /** This component has an unknown/doesn't matter role */
    LEGACY_LF_CR_UNKNOWN,
    /** This is the pixel intensity (grayscale) */
    LEGACY_LF_CR_INTENSITY,
    /** This is the Red pixel component */
    LEGACY_LF_CR_RED,
    /** This is the Green pixel component */
    LEGACY_LF_CR_GREEN,
    /** This is the Blue pixel component */
    LEGACY_LF_CR_BLUE
};

C_TYPEDEF (enum, legacy_lfComponentRole)

/** @brief This macro defines a pixel format consisting of one component */
#define LEGACY_LF_CR_1(a)              (LEGACY_LF_CR_ ## a)
/** @brief This macro defines a pixel format consisting of two components */
#define LEGACY_LF_CR_2(a,b)            ((LEGACY_LF_CR_ ## a) | ((LEGACY_LF_CR_ ## b) << 4))
/** @brief This macro defines a pixel format consisting of three components */
#define LEGACY_LF_CR_3(a,b,c)          ((LEGACY_LF_CR_ ## a) | ((LEGACY_LF_CR_ ## b) << 4) | \
                                 ((LEGACY_LF_CR_ ## c) << 8))
/** @brief This macro defines a pixel format consisting of four components */
#define LEGACY_LF_CR_4(a,b,c,d)        ((LEGACY_LF_CR_ ## a) | ((LEGACY_LF_CR_ ## b) << 4) | \
                                 ((LEGACY_LF_CR_ ## c) << 8) | ((LEGACY_LF_CR_ ## d) << 12))
/** @brief This macro defines a pixel format consisting of five components */
#define LEGACY_LF_CR_5(a,b,c,d,e)      ((LEGACY_LF_CR_ ## a) | ((LEGACY_LF_CR_ ## b) << 4) | \
                                 ((LEGACY_LF_CR_ ## c) << 8) | ((LEGACY_LF_CR_ ## d) << 12) | \
                                 ((LEGACY_LF_CR_ ## e) << 16))
/** @brief This macro defines a pixel format consisting of six components */
#define LEGACY_LF_CR_6(a,b,c,d,e,f)    ((LEGACY_LF_CR_ ## a) | ((LEGACY_LF_CR_ ## b) << 4) | \
                                 ((LEGACY_LF_CR_ ## c) << 8) | ((LEGACY_LF_CR_ ## d) << 12) | \
                                 ((LEGACY_LF_CR_ ## e) << 16) | ((LEGACY_LF_CR_ ## f) << 20))
/** @brief This macro defines a pixel format consisting of seven components */
#define LEGACY_LF_CR_7(a,b,c,d,e,f,g)   ((LEGACY_LF_CR_ ## a) | ((LEGACY_LF_CR_ ## b) << 4) | \
                                 ((LEGACY_LF_CR_ ## c) << 8) | ((LEGACY_LF_CR_ ## d) << 12) | \
                                 ((LEGACY_LF_CR_ ## e) << 16) | ((LEGACY_LF_CR_ ## f) << 20) | \
                                 ((LEGACY_LF_CR_ ## g) << 24))
/** @brief This macro defines a pixel format consisting of eight components */
#define LEGACY_LF_CR_8(a,b,c,d,e,f,g,h) ((LEGACY_LF_CR_ ## a) | ((LEGACY_LF_CR_ ## b) << 4) | \
                                 ((LEGACY_LF_CR_ ## c) << 8) | ((LEGACY_LF_CR_ ## d) << 12) | \
                                 ((LEGACY_LF_CR_ ## e) << 16) | ((LEGACY_LF_CR_ ## f) << 20) | \
                                 ((LEGACY_LF_CR_ ## g) << 24) | ((LEGACY_LF_CR_ ## h) << 28))

/**
 * @brief A callback function which modifies the separate coordinates for all color
 * components for every pixel in a strip.
 *
 * This kind of callbacks are used in the second stage of image modification.
 * @param data
 *     A opaque pointer to some data.
 * @param iocoord
 *     A pointer to an array of count*3 pixel coordinates (X,Y).
 *     The first coordinate pair is for the R component, second for G
 *     and third for B of same pixel. There are count*2*3 floats total
 *     in this array.
 * @param count
 *     Number of coordinate groups to handle.
 */
typedef void (*legacy_lfSubpixelCoordFunc) (void *data, float *iocoord, int count);

/**
 * @brief A callback function which modifies the colors of a strip of pixels
 *
 * This kind of callbacks are used in the first stage of image modification.
 * @param data
 *     A opaque pointer to some data.
 * @param x
 *     The X coordinate of the beginning of the strip. For next pixels
 *     the X coordinate increments sequentialy.
 * @param y
 *     The Y coordinate of the pixel strip. This is a constant across
 *     all pixels of the strip.
 * @param pixels
 *     A pointer to pixel data. It is the responsability of the function
 *     inserting the callback into the chain to provide a callback operating
 *     with the correct pixel format.
 * @param comp_role
 *     The role of every pixel component. This is a bitfield, made by one
 *     of the LEGACY_LF_CR_X macros which defines the roles of every pixel field.
 *     For example, LEGACY_LF_CR_4(RED,GREEN,BLUE,UNKNOWN) will define a RGBA
 *     (or RGBX) pixel format, and the UNKNOWN field will not be modified.
 * @param count
 *     Number of pixels to process.
 */
typedef void (*legacy_lfModifyColorFunc) (void *data, float x, float y,
                                   void *pixels, int comp_role, int count);

/**
 * @brief A callback function which modifies the coordinates of a strip of pixels.
 *
 * This kind of callbacks are used in third stage of image modification.
 * @param data
 *     A opaque pointer to some data.
 * @param iocoord
 *     A pointer to an array @a count pixel coordinates (X,Y).
 *     The function must replace the coordinates with their new values.
 * @param count
 *     Number of coordinate groups to handle.
 */
typedef void (*legacy_lfModifyCoordFunc) (void *data, float *iocoord, int count);

// @cond
    
/// Common ancestor for legacy_lfCoordCallbackData and legacy_lfColorCallbackData
struct legacy_lfCallbackData
{
    int priority;
    void *data;
    size_t data_size;
};

// A test point in the autoscale algorithm
typedef struct { float angle, dist; } legacy_lfPoint;

// @endcond

/**
 * @brief A modifier object contains optimized data required to rectify a
 * image.
 *
 * You can either create an empty modifier object and then enable the
 * required modification functions individually, or you can take
 * a lens object, which contains a set of correction models, and
 * create a modifier object from it.
 *
 * Normally, you will create an instance with legacy_lfModifier::Create and initilise
 * immediately it with legacy_lfModifier::Initialize, passing a valid lens object in
 * both cases.  Users of the plain C interface will use legacy_lf_modifier_new() and
 * legacy_lf_modifier_initialize() instead.
 *
 * Every image modification has a corresponding inverse function,
 * e.g. the library allows both to correct lens distortions and to
 * simulate lens characteristics.
 *
 * The normal order of applying a lens correction on a image is:
 * <ol>
 * <li>Fix lens vignetting
 * <li>Fix chromatic aberrations (TCA)
 * <li>Fix lens distortion
 * <li>Fix lens geometry
 * <li>Scale the image
 * </ol>
 *
 * This is the theoretical order.  But in reality, when using this library, the
 * order is reversed (see \ref corrections for an explanation):
 *
 * <ol>
 * <li>Color<ol>
 * <li>Fix lens vignetting
 * </ol>
 * <li>Coordinates<ol>
 * <li>Scale the image
 * <li>Fix lens geometry
 * <li>Fix lens distortion
 * </ol>
 * <li>Subpixel coordinates<ol>
 * <li>Fix chromatic aberrations (TCA)
 * </ol>
 * </ol>
 *
 * This process is divided into three stages.
 *
 * In the first stage, the colors of the image pixels are fixed
 * (vignetting). You pass a pointer to your pixel data, and it will be modified
 * in place.
 *
 * Then, the distortions introduced by the lens are removed (distortion and
 * geometry), and an additional scaling factor is applied if required.  This
 * operation requires building new image in a new allocated buffer: you cannot
 * modify the image in place, or bad things will happen.
 *
 * And finally, in the subpixel distortion stage, application corrects
 * transversal chromatic aberrations. For every target pixel coordinate the
 * modifier will return you three new coordinates: the first will tell you the
 * coordinates of the red component, second of the green, and third of the blue
 * component. This again requires copying the image, but you can use the same
 * buffers as in stage two, just in reverse order.
 *
 * Of course, you can skip some stages of the process, e.g. if you, for
 * example, don't want to change a fisheye image to a rectilinear you
 * can omit that step.
 *
 * Obviously, when simulating lens distortions, the modification stages
 * must be applied in reverse order. While the library takes care to reverse
 * the steps which are grouped into a single stage, the application must
 * apply the stages themselves in reverse order.
 *
 * HOWEVER. Doing it in three stages is not memory efficient, and is prone to
 * error accumulation because you have to interpolate pixels twice - once
 * during stage 2 and once during stage 3. To avoid this, it is sensful to do
 * stages 2 & 3 in one step.  In this case the output R,G,B coordinates from
 * stage 2, which treats the colour channels equally, are fed directly into
 * stage 3, which will correct the R,G,B coordinates further.
 * ApplySubpixelGeometryDistortion() does this in a convenient fashion.
 */
#ifdef __cplusplus
}
#endif

struct LEGACY_LF_EXPORT legacy_lfModifier
{
#ifdef __cplusplus
    /**
     * @brief Create a empty image modifier object.
     *
     * Before using the returned object you must add the required
     * modifier callbacks (see methods AddXXXCallback below).
     *
     * You must provide the original image width/height even if you
     * plan to correct just a part of the image.
     *
     * @param lens
     *     For all modifications, the crop factor, aspect ratio, and
     *     center shift of this lens will be used.
     * @param crop
     *     The crop factor for current camera. The distortion models will
     *     take this into account if lens models were measured on a camera
     *     with a different crop factor.
     * @param width
     *     The width of the image you want to correct.
     * @param height
     *     The height of the image you want to correct.
     */
    legacy_lfModifier (const legacy_lfLens *lens, float crop, int width, int height);
    ~legacy_lfModifier ();

    /**
     * @brief Create a empty image modifier object.
     *
     * Before using the returned object you must add the required
     * modifier callbacks (see methods AddXXXCallback below).
     *
     * You must provide the original image width/height even if you
     * plan to correct just a part of the image.
     *
     * @param lens
     *     For all modifications, the crop factor, aspect ratio, and
     *     center shift of this lens will be used.
     * @param crop
     *     The crop factor for current camera. The distortion models will
     *     take this into account if lens models were measured on a camera
     *     with a different crop factor.
     * @param width
     *     The width of the image you want to correct.
     * @param height
     *     The height of the image you want to correct.
     * @return
     *     A new empty image modifier object.
     */
    static legacy_lfModifier *Create (const legacy_lfLens *lens, float crop, int width, int height);

    /**
     * @brief Initialize the process of correcting aberrations in a image.
     *
     * The modifier object will be set up to rectify all aberrations
     * found in the lens description. Make sure the focal length,
     * aperture, and focus distance are correct in order to ensure
     * proper rectification.
     *
     * Aperture and focus distance are only used for vignetting
     * correction. Because the dependence of vignetting on focus
     * distance is very small, and it is usually not available in EXIF
     * data, you may give an approximative value here.  If you really
     * do not know, as a last resort, use "1000" as a default value.
     * @param lens
     *     The lens which aberrations you want to correct in a image.
     *     It should be the same lens object as the one passed to
     *     legacy_lfModifier::Create.
     * @param format
     *     Pixel format of your image (bits per pixel component)
     * @param focal
     *     The focal length in mm at which the image was taken.
     * @param aperture
     *     The aperture (f-number) at which the image was taken.
     * @param distance
     *     The approximative focus distance in meters (distance > 0).
     * @param scale
     *     An additional scale factor to be applied onto the image
     *     (1.0 - no scaling; 0.0 - automatic scaling).
     * @param targeom
     *     Target geometry. If LEGACY_LF_MODIFY_GEOMETRY is set in @a flags and
     *     @a targeom is different from lens->Type, a geometry conversion
     *     will be applied on the image.
     * @param flags
     *     A set of flags (se LEGACY_LF_MODIFY_XXX) telling which distortions
     *     you want corrected. A value of LEGACY_LF_MODIFY_ALL orders correction
     *     of everything possible (will enable all correction models
     *     present in lens description).
     * @param reverse
     *     If this parameter is true, a reverse transform will be prepared.
     *     That is, you take a undistorted image at input and convert it so
     *     that it will look as if it would be a shot made with @a lens.
     * @return
     *     A set of LEGACY_LF_MODIFY_XXX flags in effect. This is the @a flags argument
     *     with dropped bits for operations which are actually no-ops.
     */
    int Initialize (
        const legacy_lfLens *lens, legacy_lfPixelFormat format, float focal, float aperture,
        float distance, float scale, legacy_lfLensType targeom, int flags, bool reverse);

    /**
     * @brief Destroy the modifier object.
     *
     * This is the only correct way to destroy a legacy_lfModifier object.
     */
    void Destroy ();

    /**
     * @brief Add a user-defined callback to the coordinate correction chain.
     * @param callback
     *     The callback to be called for every strip of pixels.
     * @param priority
     *     Callback priority (0-999). Callbacks are always called in
     *     increasing priority order.
     * @param data
     *     A pointer to additional user data. A copy of this data
     *     is made internally, so client application may do whatever
     *     it needs with this data after this call.
     * @param data_size
     *     User data size in bytes. If data size is zero, the data is not
     *     copied and instead a verbatim copy of the 'data' parameter
     *     is passed to the callback.
     */
    void AddCoordCallback (legacy_lfModifyCoordFunc callback, int priority,
                           void *data, size_t data_size);

    /**
     * @brief Add a user-defined callback to the subpixel coordinate
     * rectification chain.
     * @param callback
     *     The callback to be called for every strip of pixels.
     * @param priority
     *     Callback priority (0-999). Callbacks are always called in
     *     increasing priority order.
     * @param data
     *     A pointer to additional user data. A copy of this data
     *     is made internally, so client application may do whatever
     *     it needs with this data after this call.
     * @param data_size
     *     User data size in bytes. If data size is zero, the data is not
     *     copied and instead a verbatim copy of the 'data' parameter
     *     is passed to the callback.
     */
    void AddSubpixelCallback (legacy_lfSubpixelCoordFunc callback, int priority,
                              void *data, size_t data_size);

    /**
     * @brief Add a user-defined callback to the color modification chain.
     * @param callback
     *     The callback to be called for every strip of pixels.
     * @param priority
     *     Callback priority (0-999). Callbacks are always called in
     *     increasing priority order.
     * @param data
     *     A pointer to additional user data. A copy of this data
     *     is made internally, so client application may do whatever
     *     it needs with this data after this call.
     * @param data_size
     *     User data size in bytes. If data size is zero, the data is not
     *     copied and instead a verbatim copy of the 'data' parameter
     *     is passed to the callback.
     */
    void AddColorCallback (legacy_lfModifyColorFunc callback, int priority,
                           void *data, size_t data_size);

    /**
     * @brief Add the stock TCA correcting callback into the chain.
     *
     * The TCA correction callback always has a fixed priority of 500.
     * The behaviour is undefined if you'll add more than one TCA
     * correction callback to a modifier.
     * @param model
     *     Lens TCA model data.
     * @param reverse
     *     If true, the reverse model will be applied, e.g. simulate
     *     a lens' TCA on a clean image.
     * @return
     *     True if TCA model is valid and the callback was added to chain.
     */
    bool AddSubpixelCallbackTCA (legacy_lfLensCalibTCA &model, bool reverse = false);

    /**
     * @brief Add the stock lens vignetting correcting callback into the chain.
     * The vignetting correction callback always has a fixed priority of
     * 250 when rectifying a image or 750 when doing reverse transform.
     * @param model
     *     Lens vignetting model data.
     * @param format
     *     Pixel format of your image (bits per pixel component)
     * @param reverse
     *     If true, the reverse model will be applied, e.g. simulate
     *     a lens' vignetting on a clean image.
     * @return
     *     True if vignetting model is valid and the callback was added to chain.
     */
    bool AddColorCallbackVignetting (legacy_lfLensCalibVignetting &model, legacy_lfPixelFormat format,
                                     bool reverse = false);

    /**
     * @brief Add the stock lens distortion correcting callback into the chain.
     *
     * The distortion correction callback always has a fixed priority of 750
     * when rectifying a image and 250 on reverse transform.
     * @param model
     *     Lens distortion model data.
     * @param reverse
     *     If true, the reverse model will be applied, e.g. simulate
     *     a lens' distortion on a clean image.
     * @return
     *     True if distortion model is valid and the callback was added to chain.
     */
    bool AddCoordCallbackDistortion (legacy_lfLensCalibDistortion &model, bool reverse = false);

    /**
     * @brief Add the stock lens geometry rectification callback into the
     * chain.
     *
     * The geometry correction callback always has a fixed priority of 500.
     * @param from
     *     The lens model for source image.
     * @param to
     *     The lens model for target image.
     * @param focal
     *     Lens focal length in mm.
     * @return
     *     True if a library has a callback for given from->to conversion.
     */
    bool AddCoordCallbackGeometry (legacy_lfLensType from, legacy_lfLensType to, float focal);

    /**
     * @brief Add the stock image scaling callback into the chain.
     *
     * The scaling callback always has a fixed priority of 100.
     * Note that scaling should be always the first operation to perform
     * no matter if we're doing a forward or reverse transform.
     * @param scale
     *     Image scale. If equal to 0.0, the image is automatically scaled
     *     so that there won't be any unfilled gaps in the resulting image.
     *     Note that all coordinate distortion callbacks must be already
     *     added to the stack for this to work correctly!
     * @param reverse
     *     If true, the reverse model will be applied.
     * @return
     *     True if the callback was added to chain.
     */
    bool AddCoordCallbackScale (float scale, bool reverse = false);

    /**
     * @brief Compute the automatic scale factor for the image.
     *
     * This expects that all coordinate distortion callbacks are already
     * set up and working. The function will try at its best to find a
     * scale value that will ensure that no pixels with coordinates out
     * of range will get into the resulting image. But since this is
     * an approximative method, the returned scale sometimes is a little
     * less than the optimal scale (e.g. you can still get some black
     * corners with some high-distortion cases).
     * @param reverse
     *     If true, the reverse scaling factor is computed.
     */
    float GetAutoScale (bool reverse);

    /**
     * @brief Image correction step 1: fix image colors.
     *
     * This currently is only vignetting transform.
     * @param pixels
     *     This points to image pixels. The actual pixel format depends on both
     *     pixel_format and comp_role arguments.
     *     The results are stored in place in the same format.
     *     Warning: this array should be aligned at least on a 16-byte boundary.
     * @param x
     *     The X coordinate of the corner of the block.
     * @param y
     *     The Y coordinate of the corner of the block.
     * @param width
     *     The width of the image block in pixels.
     * @param height
     *     The height of the image block in pixels.
     * @param comp_role
     *     The role of every pixel component. This is a bitfield, made by one
     *     of the LEGACY_LF_CR_X macros which defines the roles of every pixel field.
     *     For example, LEGACY_LF_CR_4(RED,GREEN,BLUE,UNKNOWN) will define a RGBA
     *     (or RGBX) pixel format, and the UNKNOWN field will not be modified.
     * @param row_stride
     *     The size of a image row in bytes. This can be actually different
     *     from width * pixel_width as some image formats use funny things
     *     like alignments, filler bytes etc.
     * @return
     *     true if return buffer has been altered, false if nothing to do
     */
    bool ApplyColorModification (void *pixels, float x, float y, int width, int height,
                                 int comp_role, int row_stride) const;

    /**
     * @brief Image correction step 2: apply the transforms on a block of pixel
     * coordinates.
     *
     * The undistorted coordinates are computed for every pixel in a
     * rectangular block: \f$(x_u, y_u), (x_u+1, y_u), \ldots, (x_u +
     * \mathrm{width} - 1, y_u), (x_u, y_u + 1), \ldots, (x_u + \mathrm{width}
     * - 1, y_u + \mathrm{height} - 1)\f$.
     *
     * The corrected coordinates are put into the output buffer
     * sequentially, X and Y values.
     *
     * This routine has been designed to be safe to use in parallel from
     * several threads.
     * @param xu
     *     The undistorted X coordinate of the start of the block of pixels.
     * @param yu
     *     The undistorted Y coordinate of the start of the block of pixels.
     * @param width
     *     The width of the block in pixels.
     * @param height
     *     The height of the block in pixels.
     * @param res
     *     A pointer to an output array which receives the respective X and Y
     *     distorted coordinates for every pixel of the block. The size of
     *     this array must be at least width*height*2 elements.
     *     Warning: this array should be aligned at least on a 16-byte boundary.
     * @return
     *     true if return buffer has been filled, false if nothing to do
     */
    bool ApplyGeometryDistortion (float xu, float yu, int width, int height,
                                  float *res) const;

    /**
     * @brief Image correction step 3: apply subpixel distortions.
     *
     * The undistorted R,G,B coordinates are computed for every pixel in a
     * square block: \f$(x_u, y_u), (x_u+1, y_u), \ldots, (x_u +
     * \mathrm{width} - 1, y_u), (x_u, y_u + 1), \ldots, (x_u + \mathrm{width}
     * - 1, y_u + \mathrm{height} - 1)\f$.
     *
     * Returns the corrected coordinates separately for R/G/B channels
     * The resulting coordinates are put into the output buffer
     * sequentially, X and Y values.
     *
     * This routine has been designed to be safe to use in parallel from
     * several threads.
     * @param xu
     *     The undistorted X coordinate of the start of the block of pixels.
     * @param yu
     *     The undistorted Y coordinate of the start of the block of pixels.
     * @param width
     *     The width of the block in pixels.
     * @param height
     *     The height of the block in pixels.
     * @param res
     *     A pointer to an output array which receives the respective X and Y
     *     distorted coordinates of the red, green and blue channels for
     *     every pixel of the block. The size of this array must be
     *     at least width*height*2*3 elements.
     *     Warning: this array should be aligned at least on a 16-byte boundary.
     * @return
     *     true if return buffer has been filled, false if nothing to do
     */
    bool ApplySubpixelDistortion (float xu, float yu, int width, int height,
                                  float *res) const;

    /**
     * @brief Apply stage 2 & 3 in one step.
     *
     * See the main comment to the legacy_lfModifier class.  The undistorted (R, G, B)
     * coordinates are computed for every pixel in a square block: \f$(x_u,
     * y_u), (x_u+1, y_u), \ldots, (x_u + \mathrm{width} - 1, y_u), (x_u, y_u +
     * 1), \ldots, (x_u + \mathrm{width} - 1, y_u + \mathrm{height} - 1)\f$.
     *
     * Returns the corrected coordinates separately for R/G/B channels
     * The resulting coordinates are put into the output buffer
     * sequentially, X and Y values.
     *
     * This routine has been designed to be safe to use in parallel from
     * several threads.
     * @param xu
     *     The undistorted X coordinate of the start of the block of pixels.
     * @param yu
     *     The undistorted Y coordinate of the start of the block of pixels.
     * @param width
     *     The width of the block in pixels.
     * @param height
     *     The height of the block in pixels.
     * @param res
     *     A pointer to an output array which receives the respective X and Y
     *     distorted coordinates for every pixel of the block. The size of
     *     this array must be at least width*height*2*3 elements.
     *     Warning: this array should be aligned at least on a 16-byte boundary.
     * @return
     *     true if return buffer has been filled, false if nothing to do
     */
    bool ApplySubpixelGeometryDistortion (float xu, float yu, int width, int height,
                                          float *res) const;

private:
    /**
     * @brief Determine the real focal length.
     *
     * Returns the real focal length (in contrast to the nominal focal length
     * given in the "focal" attribute in the XML files).  This is the textbook
     * focal length, derived from the magnification in paraxial approximation.
     * It is needed for accurate geometry transformations, e.g. from fisheye to
     * rectilinear.  If there is neither real focal length nor FOV data
     * available, the nominal focal length is returned as a fallback.
     *
     * In practice, its effect is mostly negligible.  When converting to
     * rectilinear, it merely results in a magnification (which probably is
     * reverted by autoscaling).  When converting to a fisheye projection
     * besides stereographic, the degree of distortion is not detectable by the
     * human eye.  Moreover, most non-fisheyes have quite accurate nominal
     * focal lengths printed on the lens.  Thus, the only use case left is the
     * conversion from non-stereographic fisheye to stereographic.  This maps
     * perfect circled to perfect circles, so it is noticeable if the nominal
     * focal length is rather off.
     *
     * @param lens
     *     The lens for which the focal length should be returned.
     * @param focal
     *     The nominal focal length for which the real focal length should be
     *     returned.
     * @return
     *     the real focal length, or, if no real focal length data and no FOV
     *     data is included into the calibration, the given nominal focal
     *     length
     */
    float GetRealFocalLength (const legacy_lfLens *lens, float focal);

    void AddCallback (void *arr, legacy_lfCallbackData *d,
                      int priority, void *data, size_t data_size);

    /**
     * @brief Calculate distance between point and image edge.
     *
     * This is an internal function used for autoscaling.  It returns the
     * distance between the given point and the edge of the image.  The
     * coordinate system used is the normalized system.  Points inside the
     * image frame yield negative values, points outside positive values, and
     * on the frame zero.
     * @param coord
     *     The x, y coordinates of the points, as a 2-component array.
     * @return
     *     The distance of the point from the edge.
     */
    double AutoscaleResidualDistance (float *coord) const;
    /**
     * @brief Calculate distance of the corrected edge point from the centre.
     *
     * This is an internal function used for autoscaling.  It returns the
     * distance of the point on the edge of the corrected image which lies in
     * the direction of the given coordinates.  "Distance" means "distance from
     * origin".  This way, the necessary autoscaling value for this direction
     * can be calculated by the calling routine.
     * @param point
     *     The polar coordinates of the point for which the distance of the
     *     corrected counterpart should be calculated.
     * @return
     *     The distance of the corrected image edge from the origin.
     */
    float GetTransformedDistance (legacy_lfPoint point) const;

    static void ModifyCoord_UnTCA_Linear (void *data, float *iocoord, int count);
    static void ModifyCoord_TCA_Linear (void *data, float *iocoord, int count);
    static void ModifyCoord_UnTCA_Poly3 (void *data, float *iocoord, int count);
    static void ModifyCoord_TCA_Poly3 (void *data, float *iocoord, int count);

    static void ModifyCoord_UnDist_Poly3 (void *data, float *iocoord, int count);
    static void ModifyCoord_Dist_Poly3 (void *data, float *iocoord, int count);
#ifdef VECTORIZATION_SSE
    static void ModifyCoord_Dist_Poly3_SSE (void *data, float *iocoord, int count);
#endif
    static void ModifyCoord_UnDist_Poly5 (void *data, float *iocoord, int count);
    static void ModifyCoord_Dist_Poly5 (void *data, float *iocoord, int count);
    static void ModifyCoord_UnDist_PTLens (void *data, float *iocoord, int count);
    static void ModifyCoord_Dist_PTLens (void *data, float *iocoord, int count);
#ifdef VECTORIZATION_SSE
    static void ModifyCoord_UnDist_PTLens_SSE (void *data, float *iocoord, int count);
    static void ModifyCoord_Dist_PTLens_SSE (void *data, float *iocoord, int count);
#endif
    static void ModifyCoord_Geom_FishEye_Rect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Panoramic_Rect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Rect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Rect_FishEye (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Panoramic_FishEye (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_FishEye (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Rect_Panoramic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_FishEye_Panoramic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Panoramic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Rect_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_FishEye_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Panoramic_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Orthographic_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Orthographic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Stereographic_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Stereographic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Equisolid_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Equisolid (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Thoby_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Thoby (void *data, float *iocoord, int count);
#ifdef VECTORIZATION_SSE
    static void ModifyColor_DeVignetting_PA_SSE (
      void *data, float _x, float _y, legacy_lf_f32 *pixels, int comp_role, int count);
#endif
#ifdef VECTORIZATION_SSE2
    static void ModifyColor_DeVignetting_PA_SSE2 (
      void *data, float _x, float _y, legacy_lf_u16 *pixels, int comp_role, int count);
#endif

    template<typename T> static void ModifyColor_Vignetting_PA (
        void *data, float x, float y, T *rgb, int comp_role, int count);
    template<typename T> static void ModifyColor_DeVignetting_PA (
        void *data, float x, float y, T *rgb, int comp_role, int count);

    static void ModifyCoord_Scale (void *data, float *iocoord, int count);
#endif
    /// Image width and height
    int Width, Height;
    /// The center of distortions in normalized coordinates
    double CenterX, CenterY;
    /// The coefficients for conversion to and from normalized coords
    double NormScale, NormUnScale;
    /// Factor to transform from normalized into absolute coords (mm).  Needed
    /// for geometry transformation.
    double NormalizedInMillimeters;
    /// Used for conversion from distortion to vignetting coordinate system of
    /// the calibration sensor
    double AspectRatioCorrection;

    /// A list of subpixel coordinate modifier callbacks.
    void *SubpixelCallbacks;
    /// A list of pixel color modifier callbacks.
    void *ColorCallbacks;
    /// A list of pixel coordinate modifier callbacks.
    void *CoordCallbacks;

    /// Maximal x and y value in normalized coordinates for the original image
    double MaxX, MaxY;
};

#ifdef __cplusplus
extern "C" {
#endif

C_TYPEDEF (struct, legacy_lfModifier)

/** @sa legacy_lfModifier::Create */
LEGACY_LF_EXPORT legacy_lfModifier *legacy_lf_modifier_new (
    const legacy_lfLens *lens, float crop, int width, int height);

/** @sa legacy_lfModifier::Destroy */
LEGACY_LF_EXPORT void legacy_lf_modifier_destroy (legacy_lfModifier *modifier);

/** @sa legacy_lfModifier::Initialize */
LEGACY_LF_EXPORT int legacy_lf_modifier_initialize (
    legacy_lfModifier *modifier, const legacy_lfLens *lens, legacy_lfPixelFormat format,
    float focal, float aperture, float distance, float scale,
    legacy_lfLensType targeom, int flags, legacy_cbool reverse);

/** @sa legacy_lfModifier::AddCoordCallback */
LEGACY_LF_EXPORT void legacy_lf_modifier_add_coord_callback (
    legacy_lfModifier *modifier, legacy_lfModifyCoordFunc callback, int priority,
    void *data, size_t data_size);

/** @sa legacy_lfModifier::AddSubpixelCallback */
LEGACY_LF_EXPORT void legacy_lf_modifier_add_subpixel_callback (
    legacy_lfModifier *modifier, legacy_lfSubpixelCoordFunc callback, int priority,
    void *data, size_t data_size);

/** @sa legacy_lfModifier::AddColorCallback */
LEGACY_LF_EXPORT void legacy_lf_modifier_add_color_callback (
    legacy_lfModifier *modifier, legacy_lfModifyColorFunc callback, int priority,
    void *data, size_t data_size);

/** @sa legacy_lfModifier::AddSubpixelCallbackTCA */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_modifier_add_subpixel_callback_TCA (
    legacy_lfModifier *modifier, legacy_lfLensCalibTCA *model, legacy_cbool reverse);

/** @sa legacy_lfModifier::AddColorCallbackVignetting */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_modifier_add_color_callback_vignetting (
    legacy_lfModifier *modifier, legacy_lfLensCalibVignetting *model,
    legacy_lfPixelFormat format, legacy_cbool reverse);

/** @sa legacy_lfModifier::AddCoordCallbackDistortion */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_modifier_add_coord_callback_distortion (
    legacy_lfModifier *modifier, legacy_lfLensCalibDistortion *model, legacy_cbool reverse);

/** @sa legacy_lfModifier::AddCoordCallbackGeometry */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_modifier_add_coord_callback_geometry (
    legacy_lfModifier *modifier, legacy_lfLensType from, legacy_lfLensType to, float focal);

/** @sa legacy_lfModifier::AddCoordCallbackScale */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_modifier_add_coord_callback_scale (
    legacy_lfModifier *modifier, float scale, legacy_cbool reverse);

/** @sa legacy_lfModifier::GetAutoScale */
LEGACY_LF_EXPORT float legacy_lf_modifier_get_auto_scale (
    legacy_lfModifier *modifier, legacy_cbool reverse);

/** @sa legacy_lfModifier::ApplySubpixelDistortion */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_modifier_apply_subpixel_distortion (
    legacy_lfModifier *modifier, float xu, float yu, int width, int height, float *res);

/** @sa legacy_lfModifier::ApplyColorModification */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_modifier_apply_color_modification (
    legacy_lfModifier *modifier, void *pixels, float x, float y, int width, int height,
    int comp_role, int row_stride);

/** @sa legacy_lfModifier::ApplyGeometryDistortion */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_modifier_apply_geometry_distortion (
    legacy_lfModifier *modifier, float xu, float yu, int width, int height, float *res);

/** @sa legacy_lfModifier::ApplySubpixelGeometryDistortion */
LEGACY_LF_EXPORT legacy_cbool legacy_lf_modifier_apply_subpixel_geometry_distortion (
    legacy_lfModifier *modifier, float xu, float yu, int width, int height, float *res);

/** @} */

#undef legacy_cbool

#ifdef __cplusplus
}
#endif

#endif /* __LENSFUN_H__ */
