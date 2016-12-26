/* This checks for correct calculation of image center (optical axis).  For
 * quite some time, Lensfun erroneously set the optical axis in image sensor
 * coordinates instead of lens coordinates.  */

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
    lfFix->lens             = new lfLens();
    //lfFix->lens->CropFactor = 1.0f;
    //lfFix->lens->AspectRatio = 3.0f / 2.0f;
    //lfFix->lens->CenterX = 0.1f;
    //lfFix->lens->CenterY = 0.1f;
    lfFix->lens->Type       = LF_RECTILINEAR;

    lfFix->img_height = 1001;
    lfFix->img_width  = 1501;

    lfFix->mod = new lfModifier (lfFix->lens, 10.0f, lfFix->img_width, lfFix->img_height);

    lfFix->mod->Initialize (lfFix->lens, LF_PF_F32, 50.89f, 2.8f, 1000.0f, 2.0f, LF_RECTILINEAR,
                            LF_MODIFY_SCALE, true);

    lfFix->coordBuff = NULL;

    const size_t bufsize = 2 * lfFix->img_width * lfFix->img_height * sizeof (float);
    lfFix->coordBuff = g_malloc (bufsize);
}

void mod_teardown (lfFixture *lfFix, gconstpointer data)
{
    g_free (lfFix->coordBuff);

    delete lfFix->mod;
    delete lfFix->lens;
}

void test_mod_coord_scaling_only (lfFixture *lfFix, gconstpointer data)
{
    const float epsilon = std::numeric_limits<float>::epsilon();
    float expected_x[] = {-1250.0f, -1050.0f, -850.000061f, -649.999939f, -450.0f,
                          -250.000046f, -49.9999466f, 150.000015f, 349.999969f, 550.0f};
    float expected_y[] = {-1000.0f, -800.000061f, -599.999939f, -400.0f, -200.000046f,
                          -7.4505806e-06f, 200.000031f, 399.999969f, 600.0f, 800.0f};
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

  g_test_add ("/modifier/coord/centering/scaling only", lfFixture, NULL,
              mod_setup, test_mod_coord_scaling_only, mod_teardown);

  return g_test_run();
}
