/* This checks for correct calculation of image center (optical axis).  For
 * quite some time, Lensfun erroneously set the optical axis in image sensor
 * coordinates instead of lens coordinates.  */

#include <locale.h>

#include <string>
#include <limits>
#include <cmath>
#include <vector>

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
    (void)data;
    lfFix->lens             = new lfLens ();
    lfFix->lens->CenterX    = 0.1;
    lfFix->lens->CenterY    = 0.1;

    lfLensCalibAttributes cs = {1.0, 1.5};
    lfLensCalibDistortion calib_data = {
        LF_DIST_MODEL_POLY3, 50.0f, 50.0f, false, {-0.1}, cs
    };
    lfFix->lens->AddCalibDistortion (&calib_data);
    lfFix->lens->Type = LF_RECTILINEAR;

    lfFix->img_height = 1001;
    lfFix->img_width  = 1501;

    lfFix->mod = new lfModifier (lfFix->lens, 50.0f, 10.0f, lfFix->img_width, lfFix->img_height, LF_PF_F32, true);

    lfFix->coordBuff = NULL;

    const size_t bufsize = 2 * lfFix->img_width * lfFix->img_height * sizeof (float);
    lfFix->coordBuff = g_malloc (bufsize);
}

void mod_teardown (lfFixture *lfFix, gconstpointer data)
{
    (void)data;
    g_free (lfFix->coordBuff);
    delete lfFix->mod;
    delete lfFix->lens;
}

void test_mod_coord_scaling_only (lfFixture *lfFix, gconstpointer data)
{
    (void)data;

    lfFix->mod->EnableScaling(2.0f);

    const float epsilon = 1e-3f;
    float expected_x[] = {-800.0f, -600.0f, -400.0f, -200.0f, 0.0f,
                          200.0f, 400.0f, 600.0f, 800.0f, 1000.0f};
    float expected_y[] = {-550.0f, -350.0f, -150.0f, 50.0f, 250.0f,
                          450.0f, 650.0f, 850.0f, 1050.0f, 1250.0f};
    std::vector<float> coords (2);
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, &coords [0]));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_distortion (lfFixture *lfFix, gconstpointer data)
{
    (void)data;

    lfFix->mod->EnableDistortionCorrection();

    const float epsilon = 1e-3f;
    float expected_x[] = {-2.2818394f, 98.536278f, 199.12755f, 299.52982f, 400.0f,
                          500.0f, 600.0f, 700.0f, 800.0f, 900.0f};
    float expected_y[] = {-1.5687886f, 99.059074f, 199.49104f, 299.76492f, 400.0f,
                          500.0f, 600.0f, 700.0f, 800.0f, 900.0f};
    std::vector<float> coords (2);
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

  g_test_add ("/modifier/coord/centering/scaling", lfFixture, NULL,
              mod_setup, test_mod_coord_scaling_only, mod_teardown);

  g_test_add ("/modifier/coord/centering/distortion", lfFixture, NULL,
              mod_setup, test_mod_coord_distortion, mod_teardown);

  return g_test_run();
}

