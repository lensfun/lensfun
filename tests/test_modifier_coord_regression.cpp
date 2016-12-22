#include <string>
#include <limits>
#include <map>
#include <cmath>

#include "lensfun.h"
#include "../libs/lensfun/lensfunprv.h"


typedef struct
{
  lfDatabase *db;
  size_t img_height;
  size_t img_width;
} lfFixture;

// setup a standard lens
void mod_setup (lfFixture *lfFix, gconstpointer data)
{

    lfFix->db = new lfDatabase ();
    lfFix->db->LoadDirectory("data/db");

    lfFix->img_height = 1000;
    lfFix->img_width  = 1500;
}

void mod_teardown (lfFixture *lfFix, gconstpointer data)
{
    lfFix->db->Destroy();
}

void test_verify_dist_poly3 (lfFixture *lfFix, gconstpointer data)
{
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "pEntax 50-200 ED");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "smc Pentax-DA 50-200mm f/4-5.6 DA ED");

    lfModifier* mod = lfModifier::Create (lenses[0], 1.534f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_F32, 80.89f, 5.6f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_DISTORTION, false);

    float x[] = {0, 661, 594, 461, 426, 530, 302, 815};
    float y[] = {0, 501, 440, 442, 534, 562, 491, 279};

    float expected_x[] = {-14.03764153, 660.98400879, 593.90045166, 460.42358398, 425.20950317, 529.73608398,
                          299.93029785, 815.08001709};
    float expected_y[] = {-9.35532570, 501.00027466, 439.96191406, 441.88513184, 534.08428955, 562.07513428,
                          490.96069336, 278.73065186};
    float coords [2];
    for (int i = 0; i < 8; i++)
    {
        g_assert_true(mod->ApplyGeometryDistortion (x[i], y[i], 1, 1, coords));
        //g_print("\n%.8f, %.8f\n", coords[0], coords[1]);
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, 1e-3);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, 1e-3);
    }

    mod->Destroy();
    lf_free (lenses);
}

void test_verify_dist_poly5 (lfFixture *lfFix, gconstpointer data)
{
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "Canon PowerShot G12");
    g_assert_cmpstr(lenses[0]->Model, ==, "Canon PowerShot G12 & compatibles (Standard)");

    g_print(lenses[0]->Model);

    lfModifier* mod = lfModifier::Create (lenses[0], 4.6f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_F32, 10.89f, 5.6f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_DISTORTION, false);

    float x[] = {0, 661, 594, 461, 426, 530, 302, 815};
    float y[] = {0, 501, 440, 442, 534, 562, 491, 279};

    float expected_x[] = {28.85699272, 661.02795410, 594.17456055, 462.02410889, 427.41033936, 530.46545410,
                          305.76791382, 814.85888672};
    float expected_y[] = {19.23155594, 500.99951172, 440.06680298, 442.20410156, 533.84960938, 561.86743164,
                          491.07156372, 279.47506714};
    float coords [2];
    for (int i = 0; i < 8; i++)
    {
        g_assert_true(mod->ApplyGeometryDistortion (x[i], y[i], 1, 1, coords));
        //g_print("\n%.8f, %.8f\n", coords[0], coords[1]);
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, 1e-3);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, 1e-3);
    }

    mod->Destroy();
    lf_free (lenses);
}

void test_verify_dist_ptlens (lfFixture *lfFix, gconstpointer data)
{
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "PENTAX-F 28-80mm");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "PENTAX-F 28-80mm f/3.5-4.5");

    lfModifier* mod = lfModifier::Create (lenses[0], 1.534f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_F32, 30.89f, 5.6f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_DISTORTION, false);

    float x[] = {0, 661, 594, 461, 426, 530, 302, 815};
    float y[] = {0, 501, 440, 442, 534, 562, 491, 279};

    float expected_x[] = {29.04440117, 661.46417236, 595.44708252, 465.34692383, 431.28689575,
                          532.68041992, 311.40554810, 814.19482422};
    float expected_y[] = {19.35648155, 500.99212646, 440.55371094, 442.86636353, 533.43615723,
                          561.23681641, 491.17864990, 281.71047974};
    float coords [2];
    for (int i = 0; i < 8; i++)
    {
        g_assert_true(mod->ApplyGeometryDistortion (x[i], y[i], 1, 1, coords));
        //g_print("\n%.8f, %.8f\n", coords[0], coords[1]);
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, 1e-3);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, 1e-3);
    }

    mod->Destroy();
    lf_free (lenses);
}

int main (int argc, char **argv)
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/modifier/coord/dist/verify_poly3", lfFixture, NULL,
              mod_setup, test_verify_dist_poly3, mod_teardown);

  g_test_add ("/modifier/coord/dist/verify_poly5", lfFixture, NULL,
              mod_setup, test_verify_dist_poly5, mod_teardown);

  g_test_add ("/modifier/coord/dist/verify_ptlens", lfFixture, NULL,
              mod_setup, test_verify_dist_ptlens, mod_teardown);

  return g_test_run();
}
