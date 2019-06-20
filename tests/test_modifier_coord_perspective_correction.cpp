#include <string>
#include <limits>
#include <cmath>

#include "lensfun.h"
#include "../libs/lensfun/lensfunprv.h"


typedef struct
{
  void       *coordBuff;
  size_t      img_width, img_height;
  float       focal;
  lfLens     *lens;
  lfModifier *mod;
} lfFixture;

// setup a standard lens
void mod_setup (lfFixture *lfFix, gconstpointer data)
{
    lfFix->lens             = new lfLens();
    //lfFix->lens->CropFactor = 1.0f;
    //lfFix->lens->AspectRatio = 4.0f / 3.0f;
    lfFix->lens->Type       = LF_RECTILINEAR;

    lfFix->img_height = 1000;
    lfFix->img_width  = 1500;
    lfFix->focal      = 50.89f;

    lfFix->mod = new lfModifier (lfFix->lens, lfFix->focal, 1.534f, lfFix->img_width, lfFix->img_height, LF_PF_F32, false);

    lfFix->coordBuff = NULL;

    const size_t bufsize = 2 * lfFix->img_width * lfFix->img_height * sizeof (float);
    lfFix->coordBuff = g_malloc (bufsize);
}

// setup a standard image in portrait mode
void mod_setup_portrait (lfFixture *lfFix, gconstpointer data)
{
    lfFix->lens             = new lfLens();
    //lfFix->lens->CropFactor = 1.0f;
    //lfFix->lens->AspectRatio = 4.0f / 3.0f;
    lfFix->lens->Type       = LF_RECTILINEAR;

    lfFix->img_height = 1500;
    lfFix->img_width  = 1000;
    lfFix->focal      = 50.89f;

    lfFix->mod = new lfModifier (lfFix->lens, lfFix->focal, 1.534f, lfFix->img_width, lfFix->img_height, LF_PF_F32, false);

    lfFix->coordBuff = NULL;

    const size_t bufsize = 2 * lfFix->img_width * lfFix->img_height * sizeof (float);
    lfFix->coordBuff = g_malloc (bufsize);
}

void mod_teardown (lfFixture *lfFix, gconstpointer data)
{
    g_free (lfFix->coordBuff);

    delete lfFix->mod;
}

void test_mod_coord_pc_svd (lfFixture *lfFix, gconstpointer data)
{
    dvector x (5), y (5);
    x[0] = 1; y[0] = 1;
    x[1] = 2; y[1] = 2;
    x[2] = 3; y[2] = 2;
    x[3] = 2; y[3] = 0;
    x[4] = 1; y[4] = 1.5;
    matrix M (5, dvector (6));
    for (int i = 0; i < 5; i++)
    {
        M [i][0] = x [i] * x [i];
        M [i][1] = x [i] * y [i];
        M [i][2] = y [i] * y [i];
        M [i][3] = x [i];
        M [i][4] = y [i];
        M [i][5] = 1;
    }
    dvector result = svd (M);
    const float epsilon = std::numeric_limits<double>::epsilon();
    g_assert_cmpfloat (fabs (result [0] - 0.04756514941544937), <=, epsilon);
    g_assert_cmpfloat (fabs (result [1] - 0.09513029883089875), <=, epsilon);
    g_assert_cmpfloat (fabs (result [2] - 0.1902605976617977), <=, epsilon);
    g_assert_cmpfloat (fabs (result [3] + 0.4280863447390447), <=, epsilon);
    g_assert_cmpfloat (fabs (result [4] + 0.5707817929853928), <=, epsilon);
    g_assert_cmpfloat (fabs (result [5] - 0.6659120918162917), <=, epsilon);
}

void test_mod_coord_pc_4_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02275.json
    const float epsilon = std::numeric_limits<float>::epsilon() * 5e3;

    float x[] = {503, 1063, 509, 1066};
    float y[] = {150, 197, 860, 759};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 4, 0) & LF_MODIFY_PERSPECTIVE);

    float expected_x[] = {194.493073f, 283.405487f, 366.993439f, 445.721252f, 520.000793f,
                          590.199036f, 656.642883f, 719.625977f, 779.411743f, 836.237793f};
    float expected_y[] = {-88.6143951f, 45.4813042f, 171.546753f, 290.282318f, 402.309204f,
                          508.180389f, 608.389648f, 703.379333f, 793.546997f, 879.250549f};
    float coords [2];
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, coords));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_pc_4_points_portrait (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02277.json
    const float epsilon = std::numeric_limits<float>::epsilon() * 2e3;

    float x[] = {145, 208, 748, 850};
    float y[] = {1060, 666, 668, 1060};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 4, 0) & LF_MODIFY_PERSPECTIVE);

    float expected_x[] = {71.1361389f, 147.872391f, 228.271774f, 312.602936f, 401.16153f,
                          494.273651f, 592.299683f, 695.639282f, 804.73645f, 920.086121f};
    float expected_y[] = {508.570709f, 535.875671f, 564.483948f, 594.491272f, 626.002869f,
                          659.134766f, 694.015137f, 730.786194f, 769.605957f, 810.650635f};
    float coords [2];
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, coords));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_pc_8_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02278.json
    const float epsilon = std::numeric_limits<float>::epsilon() * 5e3;

    float x[] = {615, 264, 1280, 813, 615, 1280, 264, 813};
    float y[] = {755, 292, 622, 220, 755, 622, 292, 220};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 8, 0) & LF_MODIFY_PERSPECTIVE);

    float expected_x[] = {-111.952522f, 7.50310612f, 129.942596f, 255.479279f, 384.231903f,
                          516.325867f, 651.893066f, 791.071838f, 934.008728f, 1080.85803f};
    float expected_y[] = {395.10733f, 422.476837f, 450.529816f, 479.292572f, 508.792175f,
                          539.057251f, 570.118042f, 602.006531f, 634.755859f, 668.401611f};
    float coords [2];
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, coords));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_pc_0_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02279.json
    const float epsilon = std::numeric_limits<float>::epsilon();

    float empty_list [0];
    g_assert_false (lfFix->mod->EnablePerspectiveCorrection (empty_list, empty_list, 0, 0) & LF_MODIFY_PERSPECTIVE);

    float coords [2];
    for (int i = 0; i < 10; i++)
    {
        g_assert_false (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, coords));
    }
}

void test_mod_coord_pc_5_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02281.json
    const float epsilon = std::numeric_limits<float>::epsilon() * 5e3;

    float x[] = {661, 594, 461, 426, 530};
    float y[] = {501, 440, 442, 534, 562};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 5, 0) & LF_MODIFY_PERSPECTIVE);

    float expected_x[] = {-115.68116f, 22.0659332f, 151.887817f, 274.449463f, 390.343201f,
                          500.098785f, 604.191345f, 703.048096f, 797.054382f, 886.558716f};
    float expected_y[] = {209.86644f, 274.712341f, 335.827393f, 393.524719f, 448.082916f,
                          499.751526f, 548.754333f, 595.292175f, 639.546631f, 681.681763f};
    float coords [2];
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, coords));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_pc_7_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02281_with_7_points.json
    const float epsilon = std::numeric_limits<float>::epsilon() * 5e3;

    float x[] = {661, 594, 461, 426, 530, 302, 815};
    float y[] = {501, 440, 442, 534, 562, 491, 279};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 7, 0) & LF_MODIFY_PERSPECTIVE);

    float expected_x[] = {-138.188828f, 3.89117932f, 144.488388f, 283.626099f, 421.326416f,
                          557.611816f, 692.50415f, 826.024231f, 958.193237f, 1089.03137f};
    float expected_y[] = {522.399536f, 532.469238f, 542.433838f, 552.294983f, 562.05426f,
                          571.713257f, 581.273621f, 590.736633f, 600.103821f, 609.37677f};
    float coords [2];
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, coords));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}


int main (int argc, char **argv)
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/modifier/coord/pc/svd", lfFixture, NULL,
              mod_setup, test_mod_coord_pc_svd, mod_teardown);
  g_test_add ("/modifier/coord/pc/4 points", lfFixture, NULL,
              mod_setup, test_mod_coord_pc_4_points, mod_teardown);
  g_test_add ("/modifier/coord/pc/4 points portrait", lfFixture, NULL,
              mod_setup_portrait, test_mod_coord_pc_4_points_portrait, mod_teardown);
  g_test_add ("/modifier/coord/pc/8 points", lfFixture, NULL,
              mod_setup, test_mod_coord_pc_8_points, mod_teardown);
  g_test_add ("/modifier/coord/pc/no points", lfFixture, NULL,
              mod_setup, test_mod_coord_pc_0_points, mod_teardown);
  g_test_add ("/modifier/coord/pc/5 points", lfFixture, NULL,
              mod_setup, test_mod_coord_pc_5_points, mod_teardown);
  g_test_add ("/modifier/coord/pc/7 points", lfFixture, NULL,
              mod_setup, test_mod_coord_pc_7_points, mod_teardown);

  return g_test_run();
}
