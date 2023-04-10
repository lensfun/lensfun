#include <glib.h>
#include "lensfun.h"

// test multi-language string manipulation
void test_mlstr_add()
{
    // add to empty string
    lfMLstr str = NULL;
    str = lf_mlstr_add (str, NULL, "56mm F1.4 DC DN | Contemporary 018");
    g_assert_nonnull(str);
    g_assert_cmpstr(str, ==, "56mm F1.4 DC DN | Contemporary 018");

    // add with a language
    str = lf_mlstr_add (str, "en", "Sigma 56 mm F1.4 DC DN | C 018");
    g_assert_nonnull(str);
    g_assert_cmpstr(str, ==, "56mm F1.4 DC DN | Contemporary 018");
    const char* substr = str + strlen(str) + 1;
    g_assert_cmpstr(substr, ==, "en");
    substr += strlen(substr) + 1;
    g_assert_cmpstr(substr, ==, "Sigma 56 mm F1.4 DC DN | C 018");

    // replace default (bigger)
    str = lf_mlstr_add (str, NULL, "SIGMA 56mm F1.4 DC DN | Contemporary 018");
    g_assert_nonnull(str);
    g_assert_cmpstr(str, ==, "SIGMA 56mm F1.4 DC DN | Contemporary 018");
    substr = str + strlen(str) + 1;
    g_assert_cmpstr(substr, ==, "en");
    substr += strlen(substr) + 1;
    g_assert_cmpstr(substr, ==, "Sigma 56 mm F1.4 DC DN | C 018");

    // replace default (smaller)
    str = lf_mlstr_add (str, NULL, "SIGMA 56mm F1.4");
    g_assert_nonnull(str);
    g_assert_cmpstr(str, ==, "SIGMA 56mm F1.4");
    substr = str + strlen(str) + 1;
    g_assert_cmpstr(substr, ==, "en");
    substr += strlen(substr) + 1;
    g_assert_cmpstr(substr, ==, "Sigma 56 mm F1.4 DC DN | C 018");

    // replace default (same size)
    str = lf_mlstr_add (str, NULL, "SIGMA 16mm F2.8");
    g_assert_nonnull(str);
    g_assert_cmpstr(str, ==, "SIGMA 16mm F2.8");
    substr = str + strlen(str) + 1;
    g_assert_cmpstr(substr, ==, "en");
    substr += strlen(substr) + 1;
    g_assert_cmpstr(substr, ==, "Sigma 56 mm F1.4 DC DN | C 018");

    lf_free (str);
    str = NULL;

    // add to empty string with a language
    str = lf_mlstr_add (str, "en", "Sigma 56 mm F1.4 DC DN | C 018");
    g_assert_nonnull(str);
    g_assert_cmpstr(str, ==, "en");
    substr = str + strlen(str) + 1;
    g_assert_cmpstr(substr, ==, "Sigma 56 mm F1.4 DC DN | C 018");

    lf_free (str);
}

int main (int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/auxiliary/multi-language string", test_mlstr_add);

    return g_test_run();
}
