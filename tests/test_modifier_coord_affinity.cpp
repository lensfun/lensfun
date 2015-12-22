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
    lfFix->lens->CropFactor = 1.0f;
    lfFix->lens->AspectRatio = 4.0f / 3.0f;
    lfFix->lens->Type       = LF_RECTILINEAR;

    lfFix->img_height = 1001;
    lfFix->img_width  = 1501;

    lfFix->mod = lfModifier::Create (lfFix->lens, 1.5f, lfFix->img_width, lfFix->img_height);

    lfFix->mod->Initialize(lfFix->lens, LF_PF_F32, 50.0f, 2.8f, 1000.0f, 1.0f, LF_RECTILINEAR,
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

    lfFix->mod->Initialize(lfFix->lens, LF_PF_F32, 50.0f, 2.8f, 1000.0f, 1.0f, LF_RECTILINEAR,
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

void test_mod_coord_affinity_rotation (lfFixture *lfFix, gconstpointer data)
{
    const float epsilon = std::numeric_limits<float>::epsilon();

    g_assert_true (lfFix->mod->EnableScaleRotateShift (1.0, M_PI_2, 0, 0));

    float expected_x[] = {250.000061f, 350.000031f, 450.0f, 550.0f, 650.0f,
                          750.0f, 850.0f, 950.0f, 1050.0f, 1150.0f};
    float expected_y[] = {1250.0f, 1150.0f, 1050.0f, 950.0f, 850.0f,
                          750.0f, 650.0f, 550.0f, 450.0f, 349.999969f};
    std::vector<float> coords (2);
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, &coords [0]));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

void test_mod_coord_affinity_shift (lfFixture *lfFix, gconstpointer data)
{
    const float epsilon = std::numeric_limits<float>::epsilon();

    g_assert_true (lfFix->mod->EnableScaleRotateShift (1.0, 0.0, 0.1, 0.1));

    /* The very first coordinate is hypot(1000, 1500) / 2 / 10. */
    float expected_x[] = {90.1387711f, 190.138763f, 290.138794f, 390.138794f, 490.138763f,
                          590.138794f, 690.138794f, 790.138794f, 890.138794f, 990.138794f};
    float expected_y[] = {90.1387863f, 190.138779f, 290.138763f, 390.138794f, 490.138794f,
                          590.138794f, 690.138794f, 790.138794f, 890.138794f, 990.138794f};
    std::vector<float> coords (2);
    for (int i = 0; i < 10; i++)
    {
        g_assert_true (lfFix->mod->ApplyGeometryDistortion (100.0f * i, 100.0f * i, 1, 1, &coords [0]));
        g_assert_cmpfloat (fabs (coords [0] - expected_x [i]), <=, epsilon);
        g_assert_cmpfloat (fabs (coords [1] - expected_y [i]), <=, epsilon);
    }
}

/* Here, I mainly check that shift is executed *before* rotation (in the
 * direction uncorrected–corrected). */
void test_mod_coord_affinity_rotation_shift (lfFixture *lfFix, gconstpointer data)
{
    const float epsilon = std::numeric_limits<float>::epsilon();

    g_assert_true (lfFix->mod->EnableScaleRotateShift (1.0, M_PI_2, 0.1, 0.1));

    float expected_x[] = {159.861282f, 259.861267f, 359.861206f, 459.861237f, 559.861206f,
                          659.861206f, 759.861206f, 859.861206f, 959.861206f, 1059.86121f};
    float expected_y[] = {1159.86133f, 1059.86121f, 959.861206f, 859.861267f, 759.861206f,
                          659.861206f, 559.861206f, 459.861206f, 359.861206f, 259.861206f};
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

  g_test_add ("/modifier/coord/affinity/rotation", lfFixture, NULL,
              mod_setup, test_mod_coord_affinity_rotation, mod_teardown);
  g_test_add ("/modifier/coord/affinity/shift", lfFixture, NULL,
              mod_setup, test_mod_coord_affinity_shift, mod_teardown);
  g_test_add ("/modifier/coord/affinity/rotation+shift", lfFixture, NULL,
              mod_setup, test_mod_coord_affinity_rotation_shift, mod_teardown);

  return g_test_run();
}
