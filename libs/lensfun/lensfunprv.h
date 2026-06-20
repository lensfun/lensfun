/*
    Internal library functions and definitions
    Copyright (C) 2007 by Andrew Zabolotny
*/

#ifndef __LENSFUNPRV_H__
#define __LENSFUNPRV_H__

#include <glib.h>
#include <string.h>
#include <vector>
#include "lensfun.h"

#define MEMBER_OFFSET(s,f)   ((unsigned int)(char *)&((s *)0)->f)
#define ARRAY_LEN(a)         (sizeof (a) / sizeof (a [0]))
#define cbool                int

// This epsilon is in image coordinate space, where 1.0 is
// half of the smallest image dimension (width or height)
// adjusted for the lens calibration data/camera crop factors.
#define NEWTON_EPS 0.00001

/**
 * @brief Make a copy of given value into given variable using g_strdup,
 * freeing the old value if defined.
 * @param var
 *     The variable to copy value into
 * @param val
 *     The value to assign to the variable
 */
LF_EXPORT void _lf_setstr (gchar **var, const gchar *val);

/**
 * @brief Appends a formatted string to a dynamically-growing string
 * using g_markup_printf_escaped() internally.
 * @param output
 *     The output array.
 * @param format
 *     The format string.
 */
LF_EXPORT void _lf_xml_printf (GString *output, const char *format, ...);

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
LF_EXPORT void _lf_xml_printf_mlstr (GString *output, const char *prefix,
                                  const char *element, const lfMLstr val);

/**
 * @brief Something like a very advanced strcmp().
 *
 * It doesn't segfault if one or both strings are NULL:
 * NULL is considered to be less than any string.
 * Actually this function does a fuzzy comparison of the strings,
 * ignoring spaces at both ends of the string, compressing multiple
 * spaces into one and ignoring character case.
 */
LF_EXPORT int _lf_strcmp (const char *s1, const char *s2);

/**
 * @brief Same as _lf_strcmp(), but compares a string with a multi-language
 * string.
 *
 * If it equals any of the translations, 0 is returned, otherwise
 * the result of strcmp() with the first (default) string is returned.
 */
LF_EXPORT int _lf_mlstrcmp (const char *s1, const lfMLstr s2);

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
LF_EXPORT gint _lf_lens_parameters_compare (const lfLens *i1, const lfLens *i2);

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
LF_EXPORT gint _lf_lens_name_compare (const lfLens *i1, const lfLens *i2);

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
LF_EXPORT float _lf_interpolate (float y1, float y2, float y3, float y4, float t);

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
LF_EXPORT guint _lf_detect_cpu_features ();

/**
 * @brief Google-in-your-pocket: a fuzzy string comparator.
 *
 * This has been designed for comparing lens and camera model names.
 * At construction the pattern is split into words and then the component
 * words from target are matched against them.
 */
class LF_EXPORT lfFuzzyStrCmp
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
     *     Returns a score in range 0-100.  If the match succeeds, this score
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
     *     succeeds, the score is the number of matched words divided by the
     *     mean word count of pattern and string, given as a percentage.  If it
     *     fails, it is 0.  It fails if no words could be matched, of if
     *     allwords was set to true and one word in pattern could not be found
     *     in match.
     */
    int Compare (const lfMLstr match);
};

template <class T>
void _lf_terminate_vec(std::vector<T> &v)
{
    int size = v.size();
    v.reserve(size+1);
    v.data()[size] = NULL;
}

// `dvector`, `matrix`, and `svg` are declared here to be able to test `svd` in
// unit tests.

typedef std::vector<double> dvector;
typedef std::vector<dvector> matrix;

LF_EXPORT dvector svd (matrix M);

#endif /* __LENSFUNPRV_H__ */
