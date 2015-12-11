#include <glib.h>

#include <clocale>
#include <vector>
#include <string>
#include <map>
#include <limits>

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <iostream>

#include "lensfun.h"
#include "../libs/lensfun/lensfunprv.h"


typedef struct
{
} lfFixture;

void mod_setup(lfFixture *lfFix, gconstpointer data)
{
}

void mod_teardown(lfFixture *lfFix, gconstpointer data)
{
}

void test_mod_coord_perspective_correction(lfFixture *lfFix, gconstpointer data)
{
    fvector x(5), y(5);
    x[0] = 1; y[0] = 1;
    x[1] = 2; y[1] = 2;
    x[2] = 3; y[2] = 2;
    x[3] = 2; y[3] = 0;
    x[4] = 1; y[4] = 1.5;
    matrix M(5, fvector(6));
    for (int i = 0; i < 5; i++)
    {
        M [i][0] = x [i] * x [i];
        M [i][1] = x [i] * y [i];
        M [i][2] = y [i] * y [i];
        M [i][3] = x [i];
        M [i][4] = y [i];
        M [i][5] = 1;
    }
    fvector result = svd (M);
    const float epsilon = std::numeric_limits<float>::epsilon() * 20;
    g_assert_cmpfloat (fabs (result [0] - 0.0475653), <=, epsilon);
    g_assert_cmpfloat (fabs (result [1] - 0.0951303), <=, epsilon);
    g_assert_cmpfloat (fabs (result [2] - 0.190261), <=, epsilon);
    g_assert_cmpfloat (fabs (result [3] + 0.428087), <=, epsilon);
    g_assert_cmpfloat (fabs (result [4] + 0.570781), <=, epsilon);
    g_assert_cmpfloat (fabs (result [5] - 0.665912), <=, epsilon);
}


int main(int argc, char **argv)
{
  setlocale(LC_ALL, "");

  g_test_init(&argc, &argv, NULL);

  g_test_add("/modifier/coord/perspective_control", lfFixture, NULL,
             mod_setup, test_mod_coord_perspective_correction, mod_teardown);

  return g_test_run();
}
