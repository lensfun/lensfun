#include <locale.h>

#include "lensfun.h"
#include "../libs/lensfun/lensfunprv.h"


typedef struct
{
    lfFuzzyStrCmp *fuzzycmp;
} lfFixture;

void mod_setup (lfFixture *lfFix, gconstpointer data)
{
}

void mod_teardown (lfFixture *lfFix, gconstpointer data)
{
}

void test_dot_zero_missing_in_raw (lfFixture *lfFix, gconstpointer data)
{
    // Name in RAW file
    lfFix->fuzzycmp = new lfFuzzyStrCmp ("Nikkor 18mm f/4 DX", true);
    // Name in database file
    int score = lfFix->fuzzycmp->Compare ("Nikkor 18mm f/4.0 DX");
    g_assert_cmpint (score, >, 0);
    delete lfFix->fuzzycmp;
}

void test_dot_zero_missing_in_db (lfFixture *lfFix, gconstpointer data)
{
    // Name in RAW file
    lfFix->fuzzycmp = new lfFuzzyStrCmp ("Nikkor 18mm f/4.0 DX", true);
    // Name in database file
    int score = lfFix->fuzzycmp->Compare ("Nikkor 18mm f/4 DX");
    g_assert_cmpint (score, >, 0);
    delete lfFix->fuzzycmp;
}


int main (int argc, char **argv)
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/lfFuzzyStrCmp/\".0\" missing in RAW", lfFixture, NULL,
              mod_setup, test_dot_zero_missing_in_raw, mod_teardown);
  g_test_add ("/lfFuzzyStrCmp/\".0\" missing in DB", lfFixture, NULL,
              mod_setup, test_dot_zero_missing_in_db, mod_teardown);

  return g_test_run();
}
