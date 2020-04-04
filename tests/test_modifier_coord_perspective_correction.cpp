#include <string>
#include <limits>
#include <cmath>
#include <locale>

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
    const double epsilon = std::numeric_limits<double>::epsilon() * 5;
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

    float expected_x[] = {194.93747f, 283.76633f, 367.27982f, 445.94156f, 520.16211f,
                          590.3075f, 656.70416f, 719.64496f, 779.39276f, 836.18463f};
    float expected_y[] = {-88.539207f, 45.526031f, 171.56934f, 290.28986f, 402.30765f,
                          508.1748f, 608.38434f, 703.37805f, 793.55273f, 879.26605f};
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

    float expected_x[] = {71.087723f, 147.83899f, 228.25151f, 312.59366f, 401.16068f,
                          494.27817f, 592.30609f, 695.64337f, 804.73334f, 920.07019f};
    float expected_y[] = {508.74167f, 536.03046f, 564.62103f, 594.60876f, 626.09875f,
                          659.20654f, 694.06024f, 730.80176f, 769.58856f, 810.5965f};
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
    float* empty_list = nullptr;
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

    float expected_x[] = {-115.54961f, 22.151754f, 151.93915f, 274.47577f, 390.35281f,
                          500.09869f, 604.18756f, 703.04572f, 797.05792f, 886.5719f};
    float expected_y[] = {209.91034f, 274.73886f, 335.84152f, 393.53049f, 448.0842f,
                          499.75153f, 548.75549f, 595.297f, 639.55695f, 681.69928f};
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

    float expected_x[] = {-138.18913f, 3.8870707f, 144.48228f, 283.6199f, 421.32199f,
                          557.61121f, 692.50861f, 826.03589f, 958.21338f, 1089.0619f};
    float expected_y[] = {522.41956f, 532.48621f, 542.44806f, 552.30658f, 562.06348f,
                          571.72015f, 581.27826f, 590.73932f, 600.10474f, 609.37598f};
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
