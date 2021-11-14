/*
    Auxiliary helper functions for Lensfun
    Copyright (C) 2007 by Andrew Zabolotny
*/

#ifdef _WIN32
	#undef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <locale.h>
#include <ctype.h>
#include <math.h>

static const char *_lf_get_lang ()
{
#ifndef LC_MESSAGES
   /* Windows badly sucks, like always */
#  define LC_MESSAGES LC_ALL
#endif

    static char lang [16];
    static const char *last_locale = NULL;
    const char *lc_msg = setlocale (LC_MESSAGES, NULL);

    if (!lc_msg)
        return strcpy (lang, "en");

    if (lc_msg == last_locale)
        return lang;

    const char *u = strchr (lc_msg, '_');
    if (!u || (size_t (u - lc_msg) >= sizeof (lang)))
        return strcpy (lang, "en");

    memcpy (lang, lc_msg, u - lc_msg);
    lang [u - lc_msg] = 0;

    /* On Windows we have "Russian" instead of "ru" and so on... :-( 
     * Microsoft, like always, invents his own stuff, never heard of ISO 639-1
     */
    if (u - lc_msg > 2)
    {
        /* For now, just take the first two letters of language name. */
        lang [0] = tolower (lang [0]);
        lang [1] = tolower (lang [1]);
        lang [2] = 0;
    }

    return lang;
}

LF_EXPORT void lf_free (void *data)
{
    g_free (data);
}

LF_EXPORT const char *lf_mlstr_get (const lfMLstr str)
{
    if (!str)
        return str;

    /* Get the current locale for messages */
    const char *lang = _lf_get_lang ();
    /* Default value if no language matches */
    const char *def = str;
    /* Find the corresponding string in the lot */
    const char *cur = strchr (str, 0) + 1;
    while (*cur)
    {
        const char *next = strchr (cur, 0) + 1;
        if (!strcmp (cur, lang))
            return next;
        if (!strcmp (cur, "en"))
            def = next;
        if (*(cur = next))
            cur = strchr (cur, 0) + 1;
    }

    return def;
}

LF_EXPORT lfMLstr lf_mlstr_add (lfMLstr str, const char *lang, const char *trstr)
{
    if (!trstr)
        return str;
    size_t trstr_len = strlen (trstr) + 1;

    /* Find the length of multi-language string */
    size_t str_len = 0;
    if (str)
    {
        str_len = strlen (str) + 1;
        while (str [str_len])
            str_len += 1 + strlen (str + str_len);
    }

    if (!lang)
    {
        /* Replace the default string */
        size_t def_str_len = str ? strlen (str) + 1 : 0;

        memcpy (str + trstr_len, str + def_str_len, str_len - def_str_len);

        str = (char *)g_realloc (str, str_len - def_str_len + trstr_len + 1);

        memcpy (str, trstr, trstr_len);
        str_len = str_len - def_str_len + trstr_len;
        str [str_len] = 0;

        return str;
    }

    size_t lang_len = lang ? strlen (lang) + 1 : 0;

    str = (char *)g_realloc (str, str_len + lang_len + trstr_len + 1);
    memcpy (str + str_len, lang, lang_len);
    memcpy (str + str_len + lang_len, trstr, trstr_len);
    str [str_len + lang_len + trstr_len] = 0;

    return str;
}

LF_EXPORT lfMLstr lf_mlstr_dup (const lfMLstr str)
{
    /* Find the length of multi-language string */
    size_t str_len = 0;
    if (str)
    {
        str_len = strlen (str) + 1;
        while (str [str_len])
            str_len += 2 + strlen (str + str_len + 1);
        /* Reserve space for the last - closing - zero */
        str_len++;
    }

    gchar *ret = (char *)g_malloc (str_len);
    memcpy (ret, str, str_len);
    return ret;
}

void _lf_setstr (gchar **var, const gchar *val)
{
    if (*var)
        g_free (*var);

    *var = g_strdup (val);
}

void _lf_xml_printf (GString *output, const char *format, ...)
{
    va_list args;
    va_start (args, format);
    gchar *s = g_markup_vprintf_escaped (format, args);
    va_end (args);

    g_string_append (output, s);

    g_free (s);
}

void _lf_xml_printf_mlstr (GString *output, const char *prefix,
                           const char *element, const lfMLstr val)
{
    if (!val)
        return;

    _lf_xml_printf (output, "%s<%s>%s</%s>\n", prefix, element, val, element);

    for (const char *cur = val;;)
    {
        cur = strchr (cur, 0) + 1;
        if (!*cur)
            break;
        const char *lang = cur;
        cur = strchr (cur, 0) + 1;
        _lf_xml_printf (output, "%s<%s lang=\"%s\">%s</%s>\n",
                        prefix, element, lang, cur, element);
    }
}

int _lf_ptr_array_insert_sorted (
    GPtrArray *array, void *item, GCompareFunc compare)
{
    int length = array->len;
    g_ptr_array_set_size (array, length + 1);
    void **root = array->pdata;

    int m = 0, l = 0, r = length - 1;

    // Skip trailing NULL, if any
    if (l <= r && !root [r])
        r--;

    while (l <= r)
    {
        m = (l + r) / 2;
        int cmp = compare (root [m], item);

        if (cmp == 0)
        {
            ++m;
            goto done;
        }
        else if (cmp < 0)
            l = m + 1;
        else
            r = m - 1;
    }
    if (r == m)
        m++;

done:
    memmove (root + m + 1, root + m, (length - m) * sizeof (void *));
    root [m] = item;
    return m;
}

int _lf_strcmp (const char *s1, const char *s2)
{
    if (s1 && !*s1)
        s1 = NULL;
    if (s2 && !*s2)
        s2 = NULL;

    if (!s1)
    {
        if (!s2)
            return 0;
        else
            return -1;
    }
    if (!s2)
        return +1;

    bool begin = true;
    for (;;)
    {
skip_start_spaces_s1:
        gunichar c1 = g_utf8_get_char (s1);
        s1 = g_utf8_next_char (s1);
        if (g_unichar_isspace (c1))
        {
            c1 = L' ';
            while (g_unichar_isspace (g_utf8_get_char (s1)))
                s1 = g_utf8_next_char (s1);
        }
        if (begin && c1 == L' ')
            goto skip_start_spaces_s1;
        c1 = g_unichar_tolower (c1);

skip_start_spaces_s2:
        gunichar c2 = g_utf8_get_char (s2);
        s2 = g_utf8_next_char (s2);
        if (g_unichar_isspace (c2))
        {
            c2 = L' ';
            while (g_unichar_isspace (g_utf8_get_char (s2)))
                s2 = g_utf8_next_char (s2);
        }
        if (begin && c2 == L' ')
            goto skip_start_spaces_s2;
        c2 = g_unichar_tolower (c2);

        begin = false;
        if (c1 == c2)
        {
            if (!c1)
                return 0;
            continue;
        }

        if (!c2 && c1 == L' ')
        {
            // Check if first string contains spaces up to the end
            while (g_unichar_isspace (g_utf8_get_char (s1)))
                s1 = g_utf8_next_char (s1);
            return *s1 ? +1 : 0;
        }
        if (!c1 && c2 == L' ')
        {
            // Check if second string contains spaces up to the end
            while (g_unichar_isspace (g_utf8_get_char (s2)))
                s2 = g_utf8_next_char (s2);
            return *s2 ? -1 : 0;
        }

        return c1 - c2;
    }
}

int _lf_mlstrcmp (const char *s1, const lfMLstr s2)
{
    if (!s1)
    {
        if (!s2)
            return 0;
        else
            return -1;
    }
    if (!s2)
        return +1;

    const char *s2c = s2;
    int ret = 0;
    while (*s2c)
    {
        int res = _lf_strcmp (s1, s2c);
        if (!res)
            return 0;
        if (s2c == s2)
            ret = res;

        // Skip the string
        s2c = strchr (s2c, 0) + 1;
        if (!*s2c)
            break;
        // Ignore the language descriptor
        s2c = strchr (s2c, 0) + 1;
    }

    return ret;
}

float _lf_interpolate (float y1, float y2, float y3, float y4, float t)
{
    float tg2, tg3;
    float t2 = t * t;
    float t3 = t2 * t;

    if (y1 == FLT_MAX)
        tg2 = y3 - y2;
    else
        tg2 = (y3 - y1) * 0.5f;

    if (y4 == FLT_MAX)
        tg3 = y3 - y2;
    else
        tg3 = (y4 - y2) * 0.5f;

    // Hermite polynomial
    return (2 * t3 - 3 * t2 + 1) * y2 +
        (t3 - 2 * t2 + t) * tg2 +
        (-2 * t3 + 3 * t2) * y3 +
        (t3 - t2) * tg3;
}

//------------------------// Fuzzy string matching //------------------------//

lfFuzzyStrCmp::lfFuzzyStrCmp (const char *pattern, bool allwords)
{
    pattern_words = g_ptr_array_new ();
    match_words = g_ptr_array_new ();
    Split (pattern, pattern_words);
    match_all_words = allwords;
}

lfFuzzyStrCmp::~lfFuzzyStrCmp ()
{
    Free (pattern_words);
    g_ptr_array_free (pattern_words, TRUE);
    g_ptr_array_free (match_words, TRUE);
}

void lfFuzzyStrCmp::Free (GPtrArray *dest)
{
    for (size_t i = 0; i < dest->len; i++)
        g_free (g_ptr_array_index (dest, i));
    g_ptr_array_set_size (dest, 0);
}

void lfFuzzyStrCmp::Split (const char *str, GPtrArray *dest)
{
    if (!str)
        return;

    while (*str)
    {
        // cast to unsigned as utf-8 may lead to negative otherwise (undefined behavior)
        while (*str && isspace ((unsigned char)*str))
            str++;
        if (!*str)
            break;

        const char *word = str++;
        int strip_suffix = 0;

        // Split into words based on character class
        if (isdigit ((unsigned char)*word))
        {
            while (*str && (isdigit ((unsigned char)*str) || *str == '.'))
                str++;
            if (str - word >= 2 && *(str - 2) == '.' && *(str - 1) == '0')
                strip_suffix = 2;
        }
        else if (ispunct ((unsigned char)*word))
            while (*str && ispunct ((unsigned char)*str))
                str++;
        else
            while (*str && !isspace ((unsigned char)*str) && !isdigit ((unsigned char)*str) && !ispunct ((unsigned char)*str))
                str++;

        // Skip solitary symbols, including a single letter "f", except for "+"
        // and "*", which sometimes occur in lens model names as important
        // characters
        if (str - word == 1 && (ispunct ((unsigned char)*word) || tolower ((unsigned char)*word) == 'f')
            && *word != '*' && *word != '+')
            continue;

        gchar *item = g_utf8_casefold (word, str - word - strip_suffix);
        _lf_ptr_array_insert_sorted (dest, item, (GCompareFunc)strcmp);
    }
}

int lfFuzzyStrCmp::Compare (const char *match)
{
    Split (match, match_words);
    if (!match_words->len || !pattern_words->len)
        return 0;

    size_t mi = 0;
    int score = 0;

    for (size_t pi = 0; pi < pattern_words->len; pi++)
    {
        const char *pattern_str = (char *)g_ptr_array_index (pattern_words, pi);
        int old_mi = mi;

        for (; mi < match_words->len; mi++)
        {
            // Since we casefolded our strings at init time, we can
            // use now a regular strcmp, which is way faster...
            int r = strcmp (pattern_str, (char *)g_ptr_array_index (match_words, mi));

            if (r == 0)
            {
                score++;
                break;
            }

            if (r < 0)
            {
                // Since our arrays are sorted, if pattern word becomes
                // "smaller" than next word from the match, this means
                // there's no chance anymore to find it.
                if (match_all_words)
                {
                    Free (match_words);
                    return 0;
                }
                else
                {
                    mi = old_mi - 1;
                    break;
                }
            }
        }

        if (match_all_words)
        {
            if (mi >= match_words->len)
            {
                // Found a word not present in match
                Free (match_words);
                return 0;
            }

            mi++;
        }
        else
        {
            if (mi >= match_words->len)
                // Found a word not present in match
                mi = old_mi;
            else
                mi++;
        }
    }

    score = (score * 200) / (pattern_words->len + match_words->len);

    Free (match_words);

    return score;
}

int lfFuzzyStrCmp::Compare (const lfMLstr match)
{
    if (!match)
        return 0;

    const char *mc = match;
    int ret = 0;
    while (*mc)
    {
        int res = Compare (mc);
        if (res > ret)
        {
            ret = res;
            if (ret >= 100)
                break;
        }

        // Skip the string
        mc = strchr (mc, 0) + 1;
        if (!*mc)
            break;
        // Ignore the language descriptor
        mc = strchr (mc, 0) + 1;
    }

    return ret;
}
