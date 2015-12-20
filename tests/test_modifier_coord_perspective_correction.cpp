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
    lfFix->lens->CropFactor = 1.0f;
    lfFix->lens->AspectRatio = 4.0f / 3.0f;
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

    float expected_x[] = {194.43692f, 283.340424f, 366.920746f, 445.641846f, 519.915894f,
                          590.108826f, 656.548462f, 719.527649f, 779.310303f, 836.133362f};
    float expected_y[] = {-88.5750732f, 45.5079613f, 171.562485f, 290.288574f, 402.307251f,
                          508.171326f, 608.374451f, 703.358887f, 793.521851f, 879.221619f};
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

    float expected_x[] = {-111.943428f, 7.50738907f, 129.942307f, 255.474655f, 384.223328f,
                          516.313416f, 651.877136f, 791.052856f, 933.987305f, 1080.83435f};
    float expected_y[] = {395.057678f, 422.428528f, 450.483154f, 479.247589f, 508.749023f,
                          539.01593f, 570.078918f, 601.969543f, 634.721375f, 668.369812f};
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

    float expected_x[] = {-115.643349f, 22.0951462f, 151.909119f, 274.463135f, 390.349792f,
                          500.098633f, 604.184753f, 703.0354f, 797.03595f, 886.534729f};
    float expected_y[] = {209.866318f, 274.712097f, 335.827271f, 393.524567f, 448.082886f,
                          499.751404f, 548.75415f, 595.292114f, 639.546631f, 681.681702f};
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

    float expected_x[] = {-138.270096f, 3.81354356f, 144.414719f, 283.556183f, 421.260986f,
                          557.550598f, 692.447144f, 825.971863f, 958.145752f, 1088.98865f};
    float expected_y[] = {522.336792f, 532.405762f, 542.369385f, 552.229553f, 561.987976f,
                          571.646179f, 581.205627f, 590.667847f, 600.034363f, 609.306396f};
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
