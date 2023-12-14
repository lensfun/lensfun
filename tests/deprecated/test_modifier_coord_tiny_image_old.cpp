/* This checks for off-by-one errors which occur most clearly in extremely
 * small images. */

#include <string>
#include <limits>
#include <cmath>
#include <vector>
#include <locale>

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
    lfFix->lens             = new lfLens();
    //lfFix->lens->CropFactor = 1.5f;
    //lfFix->lens->AspectRatio = 4.0f / 3.0f;
    lfFix->lens->Type       = LF_RECTILINEAR;

    lfFix->img_height = 2;
    lfFix->img_width  = 3;

    lfFix->mod = new lfModifier (lfFix->lens, 1.0f, lfFix->img_width, lfFix->img_height);

    lfFix->mod->Initialize (lfFix->lens, LF_PF_F32, 50.89f, 2.8f, 1000.0f, 10.0f, LF_RECTILINEAR,
                            LF_MODIFY_SCALE, true);

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
    const float epsilon = std::numeric_limits<float>::epsilon() * 10;
    float expected_coordinates[] = {-9.0f, -4.5f, 1.0f, -4.5f, 11.0f, -4.5f,
                                    -9.0f,  5.5f, 1.0f,  5.5f, 11.0f,  5.5};
    std::vector<float> coords (2 * 3 * 2);
    g_assert_true (lfFix->mod->ApplyGeometryDistortion (0, 0, 3, 2, &coords [0]));
    for (unsigned int i = 0; i < coords.size(); i++)
        g_assert_cmpfloat (fabs (coords [i] - expected_coordinates [i]), <=, epsilon);
}


int main (int argc, char **argv)
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/modifier/coord/tiny_image/scaling only", lfFixture, NULL,
              mod_setup, test_mod_coord_scaling_only, mod_teardown);

  return g_test_run();
}
