#include <string>
#include <limits>
#include <cmath>

#include "lensfun.h"
#include "../libs/lensfun/lensfunprv.h"


typedef struct
{
  void       *coordBuff;
  size_t      img_width, img_height;
  lfLens     *lens;
  lfModifier *mod;
} lfFixture;

// setup a standard lens
void mod_setup (lfFixture *lfFix, gconstpointer data)
{
    lfFix->lens             = new lfLens();
    lfFix->lens->CropFactor = 1.534f;
    lfFix->lens->AspectRatio = 1.5f;
    lfFix->lens->Type       = LF_RECTILINEAR;

    lfFix->img_height = 1000;
    lfFix->img_width  = 1500;

    lfFix->mod = lfModifier::Create (lfFix->lens, 1.534f, lfFix->img_width, lfFix->img_height);

    lfFix->mod->Initialize(lfFix->lens, LF_PF_F32, 50.89f, 2.8f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           0, false);

    lfFix->coordBuff = NULL;

    const size_t bufsize = 2 * lfFix->img_width * lfFix->img_height * sizeof (float);
    lfFix->coordBuff = g_malloc (bufsize);
}

// setup a standard image in portrait mode
void mod_setup_portrait (lfFixture *lfFix, gconstpointer data)
{
    lfFix->lens             = new lfLens();
    lfFix->lens->CropFactor = 1.534f;
    lfFix->lens->AspectRatio = 1.5f;
    lfFix->lens->Type       = LF_RECTILINEAR;

    lfFix->img_height = 1500;
    lfFix->img_width  = 1000;

    lfFix->mod = lfModifier::Create (lfFix->lens, 1.534f, lfFix->img_width, lfFix->img_height);

    lfFix->mod->Initialize(lfFix->lens, LF_PF_F32, 50.89f, 2.8f, 1000.0f, 1.0f, LF_RECTILINEAR,
                           0, false);

    lfFix->coordBuff = NULL;

    const size_t bufsize = 2 * lfFix->img_width * lfFix->img_height * sizeof (float);
    lfFix->coordBuff = g_malloc (bufsize);
}

void mod_teardown (lfFixture *lfFix, gconstpointer data)
{
    g_free (lfFix->coordBuff);

    lfFix->mod->Destroy();
    delete lfFix->lens;
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
    const float epsilon = std::numeric_limits<float>::epsilon();

    float temp_x[] = {503, 1063, 509, 1066};
    float temp_y[] = {150, 197, 860, 759};
    fvector x (temp_x, temp_x + 4);
    fvector y (temp_y, temp_y + 4);
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 0));

    float expected_x[] = {194.437012f, 283.340546f, 366.921051f, 445.642181f, 519.915833f,
                          590.108826f, 656.548401f, 719.527771f, 779.310303f, 836.133484f};
    float expected_y[] = {-88.5750961f, 45.5079803f, 171.562485f, 290.288574f, 402.307251f,
                          508.171326f, 608.374451f, 703.358887f, 793.521912f, 879.221619f};
    fvector coords (2);
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, &coords [0]));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_pc_4_points_portrait (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02277.json
    const float epsilon = std::numeric_limits<float>::epsilon();

    float temp_x[] = {145, 208, 748, 850};
    float temp_y[] = {1060, 666, 668, 1060};
    fvector x (temp_x, temp_x + 4);
    fvector y (temp_y, temp_y + 4);
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 0));

    float expected_x[] = {71.1177063f, 147.857605f, 228.261063f, 312.596893f, 401.160767f,
                          494.278839f, 592.311646f, 695.658875f, 804.764587f, 920.123962f};
    float expected_y[] = {508.4245f, 535.728271f, 564.33551f, 594.341919f, 625.852539f,
                          658.983521f, 693.863159f, 730.633789f, 769.453125f, 810.497559f};
    fvector coords (2);
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, &coords [0]));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_pc_8_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02278.json
    const float epsilon = std::numeric_limits<float>::epsilon();

    float temp_x[] = {615, 264, 1280, 813, 615, 1280, 264, 813};
    float temp_y[] = {755, 292, 622, 220, 755, 622, 292, 220};
    fvector x (temp_x, temp_x + 8);
    fvector y (temp_y, temp_y + 8);
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 0));

    float expected_x[] = {-111.943428f, 7.50743866f, 129.942276f, 255.474686f, 384.223206f,
                          516.313354f, 651.877075f, 791.052795f, 933.987122f, 1080.83423f};
    float expected_y[] = {395.057556f, 422.428436f, 450.482971f, 479.247437f, 508.748901f,
                          539.015991f, 570.078918f, 601.969543f, 634.721252f, 668.369751f};
    fvector coords (2);
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, &coords [0]));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_pc_0_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02279.json
    const float epsilon = std::numeric_limits<float>::epsilon();

    fvector empty_list;
    g_assert_false (lfFix->mod->EnablePerspectiveCorrection (empty_list, empty_list, 0));

    fvector coords (2);
    for (int i = 0; i < 10; i++)
    {
        g_assert_false (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, &coords [0]));
        g_assert_cmpfloat (fabs (coords [0]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1]), <=, epsilon);
    }
}

void test_mod_coord_pc_5_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02281.json
    const float epsilon = std::numeric_limits<float>::epsilon();

    float temp_x[] = {661, 594, 461, 426, 530};
    float temp_y[] = {501, 440, 442, 534, 562};
    fvector x (temp_x, temp_x + 5);
    fvector y (temp_y, temp_y + 5);
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 0));

    float expected_x[] = {-115.643318f, 22.0951996f, 151.909103f, 274.463257f, 390.349792f,
                          500.098633f, 604.184753f, 703.0354f, 797.03595f, 886.534668f};
    float expected_y[] = {209.866226f, 274.712097f, 335.827148f, 393.524597f, 448.082794f,
                          499.751373f, 548.754272f, 595.292114f, 639.54657f, 681.681641f};
    fvector coords (2);
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, &coords [0]));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_pc_7_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02281_with_7_points.json
    const float epsilon = std::numeric_limits<float>::epsilon();

    float temp_x[] = {661, 594, 461, 426, 530, 302, 815};
    float temp_y[] = {501, 440, 442, 534, 562, 491, 279};
    fvector x (temp_x, temp_x + 7);
    fvector y (temp_y, temp_y + 7);
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 0));

    float expected_x[] = {-138.270081f, 3.81350255f, 144.414642f, 283.556305f, 421.260925f,
                          557.550476f, 692.447327f, 825.971985f, 958.14563f, 1088.98877f};
    float expected_y[] = {522.336792f, 532.40564f, 542.369263f, 552.229492f, 561.987915f,
                          571.646057f, 581.205627f, 590.667664f, 600.034119f, 609.306335f};
    fvector coords (2);
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, &coords [0]));
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
