/*
    Internal library functions and definitions
    Copyright (C) 2007 by Andrew Zabolotny
*/

#ifndef __LENSFUNPRV_H__
#define __LENSFUNPRV_H__

#include <glib.h>
#include <string.h>

#define MEMBER_OFFSET(s,f)   ((unsigned int)(char *)&((s *)0)->f)
#define ARRAY_LEN(a)         (sizeof (a) / sizeof (a [0]))
#define cbool                int

// This epsilon is in image coordinate space, where 1.0 is
// half of the smallest image dimension (width or height)
// adjusted for the lens calibration data/camera crop factors.
#define NEWTON_EPS 0.00001

/** The type of a 8-bit pixel */
typedef unsigned char lf_u8;
/** The type of a 16-bit pixel */
typedef unsigned short lf_u16;
/** The type of a 32-bit pixel */
typedef unsigned int lf_u32;
/** The type of a 32-bit floating-point pixel */
typedef float lf_f32;
/** The type of a 64-bit floating-point pixel */
typedef double lf_f64;

class lfFuzzyStrCmp;

/**
 * @brief Return the absolute value of a number.
 * @param x
 *     A number
 * @return
 *     The absolute value of x
 */
template<typename T> static inline T absolute (T x)
{
    return (x < 0) ? -x : x;
}

/**
 * @brief Return the square of the argument.
 * @param x
 *     A floating-point number.
 */
template<typename T> static inline T square (T x)
{
    return x * x;
}

/**
 * @brief Clamp a double value between 0 and max, then convert to given type.
 * @param x
 *     The number to clamp.
 * @param min
 *     The minimal value.
 * @param max
 *     The maximal value. If equal to 0, no clamping by upper boundary is done.
 * @return
 *     The clamped value.
 */
template<typename T> static inline T clampd (double x, double min, double max)
{
    if (x < min)
        return static_cast<T> (min);
    else if (max != 0.0 && x > max)
        return static_cast<T> (max);
    return static_cast<T> (x);
}

/**
 * @brief Free a list of pointers.
 * @param list
 *     A NULL-terminated list of pointers
 */
extern void _lf_list_free (void **list);

/**
 * @brief Make a copy of given value into given variable using g_strdup,
 * freeing the old value if defined.
 * @param var
 *     The variable to copy value into
 * @param val
 *     The value to assign to the variable
 */
extern void _lf_setstr (gchar **var, const gchar *val);

/**
 * @brief Add a string to the end of a string list.
 * @param var
 *     A pointer to an array of strings.
 * @param val
 *     The value to be added to the array.
 */
extern void _lf_addstr (gchar ***var, const gchar *val);

/**
 * @brief Insert a item into a GPtrArray, keeping the array sorted.
 *
 * This method assumes that the array is already sorted, so
 * function uses a binary search algorithm.
 * Returns the index at which the item as inserted.
 * @param array
 *     The array of pointers to similar items.
 * @param item
 *     The item to insert into the array.
 * @param compare
 *     The function to compare two items.
 * @return
 *     The index at which the item was inserted.
 */
extern int _lf_ptr_array_insert_sorted (
    GPtrArray *array, void *item, GCompareFunc compare);

/**
 * @brief Insert a item into a GPtrArray, keeping the array sorted.  If array
 * contains a item equal to the inserted one, the new item overrides the old.
 * @param array
 *     The array of pointers to similar items.
 * @param item
 *     The item to insert into the array.
 * @param compare
 *     The function to compare two items.
 * @param dest
 *     The function to destroy old duplicate item (if found).
 * @return
 *     The index at which the item was inserted.
 */
extern int _lf_ptr_array_insert_unique (
    GPtrArray *array, void *item, GCompareFunc compare, GDestroyNotify dest);

/**
 * @brief Find a item in a sorted array.
 *
 * The function uses a binary search algorithm.
 * @param array
 *     The array of pointers to similar items.
 * @param item
 *     The item to search for.
 * @return
 *     The index where the item was found or -1 if not found.
 */
extern int _lf_ptr_array_find_sorted (
    const GPtrArray *array, void *item, GCompareFunc compare);

/**
 * @brief Add a object to a list of objects.
 *
 * Accepts an optional pointer to
 * a function that compares two values from the list, if function
 * returns true the existing object is replaced with the new one.
 * @param var
 *     A pointer to an array of objects.
 * @param val
 *     The value to be added to the array.
 * @param val_size
 *     The size of the value in bytes.
 * @param cmpf
 *     An auxiliary function which, if not NULL, should return
 *     true if two objects are similar or false if not.
 */
extern void _lf_addobj (void ***var, const void *val, size_t val_size,
    bool (*cmpf) (const void *, const void *));

/**
 * @brief Remove an object from a list of objects, freeing memory which was
 * allocated by _lf_addobj().
 * @param var
 *     A pointer to an array of objects.
 * @param idx
 *     The index of the object to remove (zero-based).
 * @return
 *     false if idx is out of range.
 */
extern bool _lf_delobj (void ***var, int idx);

/**
 * @brief Appends a formatted string to a dynamically-growing string
 * using g_markup_printf_escaped() internally.
 * @param output
 *     The output array.
 * @param format
 *     The format string.
 */
extern void _lf_xml_printf (GString *output, const char *format, ...);

/**
 * @brief Output a multi-language value to output string.
 *
 * Outputs a number of lines which looks like:
 *
 * \Verbatim
 * ${prefix}<${element}>${val}</${element}>
 * ${prefix}<${element} lang="xxx">${val[xxx]}</${element}>
 * ...
 * \EndVerbatim
 * @param output
 *     The output array.
 * @param prefix
 *     The prefix at the start of every line of output.
 * @param element
 *     The name of the element to output.
 * @param val
 *     The value of the multi-language string.
 */
extern void _lf_xml_printf_mlstr (GString *output, const char *prefix,
                                  const char *element, const lfMLstr val);

/**
 * @brief Get the XML id of the given distortion model
 * @param model
 *     The model.
 */
extern const char *_lf_get_distortion_model_id (lfDistortionModel model);

/**
 * @brief Something like a very advanced strcmp().
 *
 * It doesn't segfault if one or both strings are NULL:
 * NULL is considered to be less than any string.
 * Actually this function does a fuzzy comparison of the strings,
 * ignoring spaces at both ends of the string, compressing multiple
 * spaces into one and ignoring character case.
 */
extern int _lf_strcmp (const char *s1, const char *s2);

/**
 * @brief Same as _lf_strcmp(), but compares a string with a multi-language
 * string.
 *
 * If it equals any of the translations, 0 is returned, otherwise
 * the result of strcmp() with the first (default) string is returned.
 */
extern int _lf_mlstrcmp (const char *s1, const lfMLstr s2);

/**
 * @brief Comparison function for mount sorting and finding.
 *
 * Since this function is used when reading the database, it effectively
 * enforces the primary key for mounts, which is their Name.
 * @param a
 *     A pointer to first lfMount object.
 * @param b
 *     A pointer to second lfMount object.
 * @return
 *     Positive if a > b, negative if a < b, zero if they are equal.
 */
extern gint _lf_mount_compare (gconstpointer a, gconstpointer b);

/**
 * @brief Comparison function for camera sorting and finding.
 *
 * Since this function is used when reading the database, it effectively
 * enforces the primary key for cameras, which is the combination of the
 * attributes Maker, Model, and Variant.
 * @param a
 *     A pointer to first lfCamera object.
 * @param b
 *     A pointer to second lfCamera object.
 * @return
 *     Positive if a > b, negative if a < b, zero if they are equal.
 */
extern gint _lf_camera_compare (gconstpointer a, gconstpointer b);

/**
 * @brief Comparison helper function for lens sorting and finding.
 *
 * This function compares the numerical parameters of the lenses: MinFocal,
 * MaxFocal, and MinAperture, in this order.  Since it is not meant to be used
 * as a sorting key function directly, it doesn't take generic pointers as
 * parameters.  Instead, it is supposed to be used by such sorting key
 * functions like _lf_lens_compare.
 * @param i1
 *     A pointer to first lfLens object.
 * @param i2
 *     A pointer to second lfLens object.
 * @return
 *     Positive if i1 > i2, negative if i1 < i2, zero if they are equal.
 */
extern gint _lf_lens_parameters_compare (const lfLens *i1, const lfLens *i2);

/**
 * @brief Comparison helper function for lens sorting and finding.
 *
 * This function compares the names of the lenses: Maker and Model, in this
 * order.  Since it is not meant to be used as a sorting key function directly,
 * it doesn't take generic pointers as parameters.  Instead, it is supposed to
 * be used by such sorting key functions like _lf_lens_compare.
 * @param i1
 *     A pointer to first lfLens object.
 * @param i2
 *     A pointer to second lfLens object.
 * @return
 *     Positive if i1 > i2, negative if i1 < i2, zero if they are equal.
 */
extern gint _lf_lens_name_compare (const lfLens *i1, const lfLens *i2);

/**
 * @brief Comparison function for lens sorting and finding.
 *
 * Since this function is used when reading the database, it effectively
 * enforces the primary key for lenses, which is the combination of the
 * attributes Maker, Model, and CropFactor.
 * @param a
 *     A pointer to first lfLens object.
 * @param b
 *     A pointer to second lfLens object.
 * @return
 *     Positive if a > b, negative if a < b, zero if they are equal.
 */
extern gint _lf_lens_compare (gconstpointer a, gconstpointer b);

/**
 * @brief Get an interpolated value.
 *
 * Currently this uses a kind of Catmull-Rom splines with linear
 * interpolation at the ends, allowing for non-evenly spaced values
 * and handling the extreme cases correctly (the one at the start
 * of spline and at the end of spline). The region of interest is
 * from y2 to y3, selected by values of t from 0.0 to 1.0.
 * @param y1
 *     The Y coordinate of the first spline point.
 *     If equal to FLT_MAX, the first point is considered non-existent.
 * @param y2
 *     The Y coordinate of the second spline point.
 * @param y3
 *     The Y coordinate of the third spline point.
 * @param y4
 *     The Y coordinate of the fourth spline point.
 *     If equal to FLT_MAX, the fourth point is considered non-existent.
 * @param t
 *     Value from 0.0 to 1.0 selects a point on spline on the interval
 *     between points 2 and 3.
 */
extern float _lf_interpolate (float y1, float y2, float y3, float y4, float t);

/**
 * @brief Compare a lens with a pattern and return a matching score.
 *
 * The comparison is quasi-intelligent: the order of words in a name
 * does not matter; the more words from match are present in the pattern,
 * the higher is score. Numeric parameters have to coincide or not be specified
 * at all, otherwise the score drops to zero (well, a 1% tolerance is allowed
 * for rounding errors etc).
 * @param pattern
 *     A pattern to compare against. Unsure fields should be set to NULL.
 *     It is generally a good idea to call GuessParameters() first since
 *     that may give additional info for quicker comparison.
 * @param match
 *     The object to match against.
 * @param fuzzycmp
 *     A fuzzy comparator initialized with pattern->Model
 * @param compat_mounts
 *     An additional list of compatible mounts, can be NULL.
 *     This does not include the mounts from pattern->Mounts.
 * @return
 *     A numeric score in the range 0 to 100, where 100 means that
 *     every field matches and 0 means that at least one field is
 *     fundamentally different.
 */
extern int _lf_lens_compare_score (const lfLens *pattern, const lfLens *match,
                                   lfFuzzyStrCmp *fuzzycmp, const char **compat_mounts);

enum
{
    LF_CPU_FLAG_MMX             = 0x00000001,
    LF_CPU_FLAG_SSE             = 0x00000002,
    LF_CPU_FLAG_CMOV            = 0x00000004,
    LF_CPU_FLAG_3DNOW           = 0x00000008,
    LF_CPU_FLAG_3DNOW_EXT       = 0x00000010,
    LF_CPU_FLAG_AMD_ISSE        = 0x00000020,
    LF_CPU_FLAG_SSE2            = 0x00000040,
    LF_CPU_FLAG_SSE3            = 0x00000080,
    LF_CPU_FLAG_SSSE3           = 0x00000100,
    LF_CPU_FLAG_SSE4_1          = 0x00000200,
    LF_CPU_FLAG_SSE4_2          = 0x00000400
};

/**
 * @brief Detect supported CPU features (used for runtime selection of accelerated
 * functions for specific architecture extensions).
 */
extern guint _lf_detect_cpu_features ();

/**
 * @brief Google-in-your-pocket: a fuzzy string comparator.
 *
 * This has been designed for comparing lens and camera model names.
 * At construction the pattern is split into words and then the component
 * words from target are matched against them.
 */
class lfFuzzyStrCmp
{
    GPtrArray *pattern_words;
    GPtrArray *match_words;
    bool match_all_words;

    void Split (const char *str, GPtrArray *dest);
    void Free (GPtrArray *dest);

public:
    /**
     * @param pattern
     *     The pattern which will be compared against a number of strings.
     *     This is typically what was found in the EXIF data.
     * @param allwords
     *     If true, all words of the pattern must be present in the
     *     target string. If not, a looser result will be accepted,
     *     although this will be reflected in the match score.
     */
    lfFuzzyStrCmp (const char *pattern, bool allwords);
    ~lfFuzzyStrCmp ();

    /**
     * @brief Fuzzy compare the pattern with a string.
     * @param match
     *     The string to match against.  This is typically taken from the
     *     Lensfun database.
     * @return
     *     Returns a score in range 0-100.  If the match succedes, this score
     *     is the number of matched words divided by the mean word count of
     *     pattern and string, given as a percentage.  If it fails, it is 0.
     *     It fails if no words could be matched, of if allwords was set to
     *     true and one word in pattern could not be found in match.
     */
    int Compare (const char *match);

    /**
     * @brief Compares the pattern with a multi-language string.
     *
     * This function returns the largest score as compared against
     * every of the translated strings.
     * @param match
     *     The multi-language string to match against.  This is typically taken
     *     from the Lensfun database.
     * @return
     *     Returns the maximal score in range 0-100.  For every component of
     *     the multi-language string, a score is computed: If the match
     *     succedes, the score is the number of matched words divided by the
     *     mean word count of pattern and string, given as a percentage.  If it
     *     fails, it is 0.  It fails if no words could be matched, of if
     *     allwords was set to true and one word in pattern could not be found
     *     in match.
     */
    int Compare (const lfMLstr match);
};

/**
 * @brief The real Database class.
 *
 * It contains some additional data fields
 * which must not be seen from the application at all (even
 * private members don't qualify because we will have to include
 * glib.h from lens.h and that's something we want to hide).
 */
struct lfExtDatabase : public lfDatabase
{
    GPtrArray *Mounts;
    GPtrArray *Cameras;
    GPtrArray *Lenses;

    lfExtDatabase ()
    {
        HomeDataDir = g_build_filename (g_get_user_data_dir (),
                                        CONF_PACKAGE, NULL);
        Mounts = g_ptr_array_new ();
        g_ptr_array_add (Mounts, NULL);
        Cameras = g_ptr_array_new ();
        g_ptr_array_add (Cameras, NULL);
        Lenses = g_ptr_array_new ();
        g_ptr_array_add (Lenses, NULL);
    }

    ~lfExtDatabase ()
    {
        size_t i;
        for (i = 0; i < Mounts->len - 1; i++)
             delete static_cast<lfMount *> (g_ptr_array_index (Mounts, i));
        g_ptr_array_free (Mounts, TRUE);

        for (i = 0; i < Cameras->len - 1; i++)
             delete static_cast<lfCamera *> (g_ptr_array_index (Cameras, i));
        g_ptr_array_free (Cameras, TRUE);

        for (i = 0; i < Lenses->len - 1; i++)
             delete static_cast<lfLens *> (g_ptr_array_index (Lenses, i));
        g_ptr_array_free (Lenses, TRUE);

        g_free (HomeDataDir);
    }
};

/// Common ancestor for lfCoordCallbackData and lfColorCallbackData
struct lfCallbackData
{
    int priority;
    void *data;
    size_t data_size;
};

/// Subpixel distortion callback
struct lfSubpixelCallbackData : public lfCallbackData
{
    lfSubpixelCoordFunc callback;
};

/// A single pixel coordinate modifier callback.
struct lfCoordCallbackData : public lfCallbackData
{
    lfModifyCoordFunc callback;
};

/// A single pixel color modifier callback.
struct lfColorCallbackData : public lfCallbackData
{
    lfModifyColorFunc callback;
};

/**
 * @brief This is the extended lfModifier class, with implementation details
 * hidden from the public header file.
 */
struct lfExtModifier : public lfModifier
{
    lfExtModifier (const lfLens *lens, float crop, int width, int height);
    ~lfExtModifier ();

    void AddCallback (GPtrArray *arr, lfCallbackData *d,
                      int priority, void *data, size_t data_size);

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
    GPtrArray *SubpixelCallbacks;
    /// A list of pixel color modifier callbacks.
    GPtrArray *ColorCallbacks;
    /// A list of pixel coordinate modifier callbacks.
    GPtrArray *CoordCallbacks;

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
    static void ModifyCoord_Geom_Rect_FishEye (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Rect_Panoramic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Rect_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_FishEye_Rect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_FishEye_Panoramic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_FishEye_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Panoramic_Rect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Panoramic_FishEye (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Panoramic_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Rect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_FishEye (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Panoramic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Orthographic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Orthographic_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Stereographic (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Stereographic_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Equisolid (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Equisolid_ERect (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_ERect_Thoby (void *data, float *iocoord, int count);
    static void ModifyCoord_Geom_Thoby_ERect (void *data, float *iocoord, int count);
#ifdef VECTORIZATION_SSE2
    static void ModifyColor_DeVignetting_PA_SSE2 (
      void *data, float _x, float _y, lf_u16 *pixels, int comp_role, int count);
    static void ModifyColor_DeVignetting_PA_SSE2_aligned (
      void *data, float _x, float _y, lf_u16 *pixels, int comp_role, int count);
    static void ModifyColor_DeVignetting_PA_Select (
      void *data, float x, float y, lf_u16 *pixels, int comp_role, int count);
#endif

    template<typename T> static void ModifyColor_Vignetting_PA (
        void *data, float x, float y, T *rgb, int comp_role, int count);
    template<typename T> static void ModifyColor_DeVignetting_PA (
        void *data, float x, float y, T *rgb, int comp_role, int count);

    static void ModifyCoord_Scale (void *data, float *iocoord, int count);
};

#endif /* __LENSFUNPRV_H__ */
