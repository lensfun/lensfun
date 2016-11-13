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

    float x[] = {503, 1063, 509, 1066};
    float y[] = {150, 197, 860, 759};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 4, 0));

    float expected_x[] = {194.595779f, 283.488678f, 367.059418f, 445.771973f, 520.037964f,
                          590.223877f, 656.656982f, 719.63031f, 779.407349f, 836.225403f};
    float expected_y[] = {-88.596962f, 45.4916496f, 171.551987f, 290.284058f, 402.308868f,
                          508.179077f, 608.388428f, 703.379028f, 793.548279f, 879.254211f};
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
    const float epsilon = std::numeric_limits<float>::epsilon();

    float x[] = {145, 208, 748, 850};
    float y[] = {1060, 666, 668, 1060};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 4, 0));

    float expected_x[] = {71.1249695f, 147.864716f, 228.267105f, 312.6008f, 401.161346f,
                          494.274689f, 592.301208f, 695.640259f, 804.735718f, 920.082397f};
    float expected_y[] = {508.610229f, 535.911316f, 564.515625f, 594.518433f, 626.025085f,
                          659.151367f, 694.025574f, 730.789795f, 769.60199f, 810.638062f};
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
    const float epsilon = std::numeric_limits<float>::epsilon();

    float x[] = {615, 264, 1280, 813, 615, 1280, 264, 813};
    float y[] = {755, 292, 622, 220, 755, 622, 292, 220};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 8, 0));

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
    g_assert_false (lfFix->mod->EnablePerspectiveCorrection (empty_list, empty_list, 0, 0));

    float coords [2];
    for (int i = 0; i < 10; i++)
    {
        g_assert_false (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, coords));
    }
}

void test_mod_coord_pc_5_points (lfFixture *lfFix, gconstpointer data)
{
    // Bases on DSC02281.json
    const float epsilon = std::numeric_limits<float>::epsilon();

    float x[] = {661, 594, 461, 426, 530};
    float y[] = {501, 440, 442, 534, 562};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 5, 0));

    float expected_x[] = {-115.650925f, 22.0856838f, 151.899597f, 274.455536f, 390.345459f,
                          500.098816f, 604.190491f, 703.047546f, 797.055237f, 886.561768f};
    float expected_y[] = {209.876465f, 274.718323f, 335.830566f, 393.52594f, 448.083191f,
                          499.751465f, 548.754517f, 595.293274f, 639.548889f, 681.68573f};
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
    const float epsilon = std::numeric_limits<float>::epsilon();

    float x[] = {661, 594, 461, 426, 530, 302, 815};
    float y[] = {501, 440, 442, 534, 562, 491, 279};
    g_assert_true (lfFix->mod->EnablePerspectiveCorrection (x, y, 7, 0));

    float expected_x[] = {-138.189011f, 3.89023399f, 144.487122f, 283.624481f, 421.325409f,
                          557.611755f, 692.505188f, 826.027039f, 958.197815f, 1089.03845f};
    float expected_y[] = {522.404114f, 532.473145f, 542.437073f, 552.297729f, 562.056335f,
                          571.714905f, 581.274719f, 590.737183f, 600.104004f, 609.376526f};
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
