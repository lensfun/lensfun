#include <string>
#include <limits>
#include <map>
#include <cmath>
#include <locale>

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
    (void)data;

    lfFix->db = new lfDatabase ();
    lfFix->db->LoadDirectory("data/db");

    lfFix->img_height = 1000;
    lfFix->img_width  = 1500;
}

void mod_teardown (lfFixture *lfFix, gconstpointer data)
{
    (void)data;
    lfFix->db->Destroy();
}

void test_verify_dist_poly3 (lfFixture *lfFix, gconstpointer data)
{
    (void)data;
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "pEntax 50-200 ED");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "smc Pentax-DA 50-200mm f/4-5.6 DA ED");

    lfModifier* mod = lfModifier::Create (lenses[0], 1.534f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_F32, 80.89f, 5.6f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_DISTORTION, false);

    float x[] = {0, 751, 810, 1270};
    float y[] = {0, 497, 937, 100};

    float expected_x[] = {-14.016061f, 751.0f, 810.27203f, 1275.1655f};
    float expected_y[] = {-9.3409109f, 497.0f, 938.96729f, 96.035286f};

    float coords [2];
    for (long unsigned i = 0; i < sizeof(x) / sizeof(float); i++)
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
    (void)data;
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "Canon PowerShot G12");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "Canon PowerShot G12 & compatibles (Standard)");

    g_print("%s", lenses[0]->Model);

    lfModifier* mod = lfModifier::Create (lenses[0], 4.6f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_F32, 10.89f, 5.6f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_DISTORTION, false);

    float x[] = {0, 751, 810, 1270};
    float y[] = {0, 497, 937, 100};

    float expected_x[] = {28.805828f, 751.0f, 809.50531f, 1260.1396f};
    float expected_y[] = {19.197506f, 497.0f, 933.42279f, 107.56808f};

    float coords [2];
    for (size_t i = 0; i < sizeof(x) / sizeof(float); i++)
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
    (void)data;
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "PENTAX-F 28-80mm");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "Pentax-F 28-80mm f/3.5-4.5");

    lfModifier* mod = lfModifier::Create (lenses[0], 1.534f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_F32, 30.89f, 5.6f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_DISTORTION, false);

    float x[] = {0, 751, 810, 1270};
    float y[] = {0, 497, 937, 100};

    float expected_x[] = {29.019449f, 750.99969f, 808.74231f, 1255.1388f};
    float expected_y[] = {19.339846f, 497.00046f, 927.90521f, 111.40639f};

    float coords [2];
    for (size_t i = 0; i < sizeof(x) / sizeof(float); i++)
    {
        g_assert_true(mod->ApplyGeometryDistortion (x[i], y[i], 1, 1, coords));
        //g_print("\n%.8f, %.8f\n", coords[0], coords[1]);
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, 1e-3);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, 1e-3);
    }

    mod->Destroy();
    lf_free (lenses);
}

void test_verify_vignetting_pa (lfFixture *lfFix, gconstpointer data)
{
    (void)data;
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "Olympus ED 14-42mm");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "Olympus Zuiko Digital ED 14-42mm f/3.5-5.6");

    lfModifier* mod = lfModifier::Create (lenses[0], 2.0f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_U16, 17.89f, 5.0f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_VIGNETTING, false);

    float x[] = {0, 751, 810, 1270};
    float y[] = {0, 497, 937, 100};

    lf_u16 expected[] = {22406, 22406, 24156, 28803};

    lf_u16 coords [3] = {16000, 16000, 16000};
    for (size_t i = 0; i < sizeof(x) / sizeof(float); i++)
    {
        g_assert_true(mod->ApplyColorModification(&coords[0], x[i], y[i], 1, 1, LF_CR_3(RED,GREEN,BLUE), 0));
        //g_print("\n%d, %d, %d\n", coords[0], coords[1], coords[2]);
        g_assert_cmpfloat (fabs (coords [0] - expected [i]), <=, 1e-3);
        g_assert_cmpfloat (fabs (coords [1] - expected [i]), <=, 1e-3);
        g_assert_cmpfloat (fabs (coords [2] - expected [i]), <=, 1e-3);
    }

    mod->Destroy();
    lf_free (lenses);
}

void test_verify_subpix_linear (lfFixture *lfFix, gconstpointer data)
{
    (void)data;
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "Olympus ED 14-42mm");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "Olympus Zuiko Digital ED 14-42mm f/3.5-5.6");

    lfModifier* mod = lfModifier::Create (lenses[0], 2.0f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_U16, 17.89f, 5.0f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_TCA, false);

    float x[] = {0, 751, 810, 1270};
    float y[] = {0, 497, 937, 100};

    float expected[][6] = {
        {-0.08681729f, -0.05789410f, 0.00002450f, -0.00001032f, -0.02400517f, -0.01601936f},
        {751.00061035f, 496.99899292f, 751.00000000f, 497.00000000f, 751.00000000f, 497.00000000f},
        {810.01995850f, 937.14440918f, 810.00000000f, 937.00000000f, 810.00042725f, 937.00305176f},
        {1270.12915039f, 99.90086365f, 1270.00000000f, 100.00000763f, 1270.00854492f, 99.99343872f}
    };

    float coords [6];
    for (size_t i = 0; i < sizeof(x) / sizeof(float); i++)
    {
        g_assert_true(mod->ApplySubpixelDistortion(x[i], y[i], 1, 1, coords));
        //g_print("{%.8f, %.8f, %.8f, %.8f, %.8f, %.8f},\n", coords[0], coords[1], coords[2], coords[3], coords[4], coords[5]);
        for (int j = 0; j < 6; j++)
            g_assert_cmpfloat (fabs (coords [j] - expected [i][j]), <=, 1e-3);
    }

    mod->Destroy();
    lf_free (lenses);
}

void test_verify_subpix_poly3 (lfFixture *lfFix, gconstpointer data)
{
    (void)data;
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "Olympus ED 14-42mm");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "Olympus Zuiko Digital ED 14-42mm f/3.5-5.6");

    lfModifier* mod = lfModifier::Create (lenses[0], 2.0f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_U16, 26.89f, 5.0f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_TCA, false);

    float x[] = {0, 751, 810, 1270};
    float y[] = {0, 497, 937, 100};

    float expected[][6] = {
        {-0.05537901f, -0.03692452f, 0.00002450f, -0.00001032f, 0.01445518f, 0.00962087f},
        {751.00061035f, 496.99902344f, 751.00000000f, 497.00000000f, 750.99981689f, 497.00030518f},
        {810.01898193f, 937.13732910f, 810.00000000f, 937.00000000f, 809.99389648f, 936.95599365f},
        {1270.11572266f, 99.91123199f, 1270.00000000f, 100.00000763f, 1269.96374512f, 100.02780914f}
    };

    for (size_t i = 0; i < sizeof(x) / sizeof(float); i++)
    {
        float coords [6];
        g_assert_true(mod->ApplySubpixelDistortion (x[i], y[i], 1, 1, coords));
        //g_print("{%.8f, %.8f, %.8f, %.8f, %.8f, %.8f},\n", coords[0], coords[1], coords[2], coords[3], coords[4], coords[5]);
        for (int j = 0; j < 6; j++)
            g_assert_cmpfloat (fabs (coords [j] - expected [i][j]), <=, 1e-3);
    }

    mod->Destroy();
    lf_free (lenses);
}

void test_verify_geom_fisheye_rectlinear (lfFixture *lfFix, gconstpointer data)
{
    (void)data;
    // select a lens from database
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "M.Zuiko Digital ED 8mm f/1.8 Fisheye");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "Olympus M.Zuiko Digital ED 8mm f/1.8 Fisheye Pro");

    lfModifier* mod = lfModifier::Create (lenses[0], 2.0f, lfFix->img_width, lfFix->img_height);

    mod->Initialize(lenses[0], LF_PF_U16, 8.0f, 5.0f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           LF_MODIFY_GEOMETRY, false);

    float x[] = {0, 751, 810, 1270};
    float y[] = {0, 497, 937, 100};

    float expected_x[] = {248.78734f, 751.0f, 802.23956f, 1151.199f};
    float expected_y[] = {165.80287f, 497.00003f, 880.88129f, 191.18344f};

    float coords [2];
    for (size_t i = 0; i < sizeof(x) / sizeof(float); i++)
    {
        g_assert_true(mod->ApplyGeometryDistortion (x[i], y[i], 1, 1, coords));
        //g_print("\n%.8f, %.8f\n", coords[0], coords[1]);
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, 1e-1);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, 1e-1);
    }

    mod->Destroy();
    lf_free (lenses);
}

int main (int argc, char **argv)
{
  setlocale (LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/modifier/coord/dist/verify_poly3", lfFixture, NULL,
              mod_setup, test_verify_dist_poly3, mod_teardown);

  g_test_add ("/modifier/coord/dist/verify_ptlens", lfFixture, NULL,
              mod_setup, test_verify_dist_ptlens, mod_teardown);

  g_test_add ("/modifier/coord/dist/verify_poly5", lfFixture, NULL,
              mod_setup, test_verify_dist_poly5, mod_teardown);

  g_test_add ("/modifier/coord/geom/verify_equisolid_linrect", lfFixture, NULL,
              mod_setup, test_verify_geom_fisheye_rectlinear, mod_teardown);

  g_test_add ("/modifier/color/vignetting/verify_pa", lfFixture, NULL,
              mod_setup, test_verify_vignetting_pa, mod_teardown);

  g_test_add ("/modifier/subpix/TCA/verify_linear", lfFixture, NULL,
              mod_setup, test_verify_subpix_linear, mod_teardown);

  g_test_add ("/modifier/subpix/TCA/verify_poly3", lfFixture, NULL,
              mod_setup, test_verify_subpix_poly3, mod_teardown);

  return g_test_run();
}
