#include <string>
#include <limits>
#include <map>
#include <cmath>
#include "xmmintrin.h"
#include "time.h"

#include "lensfun.h"
#include "../libs/lensfun/lensfunprv.h"


typedef struct
{
  lfDatabase *db;
  size_t img_height;
  size_t img_width;
} lfFixture;

// setup a standard lens
void mod_setup (lfFixture *lfFix, gconstpointer data)
{

    lfFix->db = new lfDatabase ();
    lfFix->db->LoadDirectory("data/db");

    lfFix->img_height = 4000;
    lfFix->img_width  = 6000;
}

void mod_teardown (lfFixture *lfFix, gconstpointer data)
{
    lfFix->db->Destroy();
}

// very simple nearest neighbour interpolation to simulate some memory access
void interp_row_nearest(unsigned char** img, unsigned int row, unsigned int width, unsigned int height, float* coords)
{
    for (int c = 0; c<width; c++)
    {
        int x = coords[2*c];
        int y = coords[2*c+1];

        if (x < 0)
            x = 0;
        if (x>=width)
            x = width - 1;
        if (y < 0)
            y = 0;
        if (y>=height)
            y = height - 1;

        img[row][3*c]   = img[y][3*x];
        img[row][3*c+1] = img[y][3*x+1];
        img[row][3*c+2] = img[y][3*x+2];
    }

}

void test_perf_dist_ptlens (lfFixture *lfFix, gconstpointer data)
{
    // select a lens from database
    const lfLens** lenses = lfFix->db->FindLenses (NULL, NULL, "PENTAX-F 28-80mm");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "PENTAX-F 28-80mm f/3.5-4.5");

    lfModifier* mod = new lfModifier (lenses[0], 1.534f, lfFix->img_width, lfFix->img_height);

    mod->Initialize (lenses[0], LF_PF_F32, 30.89f, 2.8f, 1000.0f, 10.0f, LF_RECTILINEAR,
                            LF_MODIFY_DISTORTION, false);


    unsigned char** img = (unsigned char**)_mm_malloc(lfFix->img_height*sizeof(unsigned char*), 32);
    for (int r = 0; r < lfFix->img_height; r++)
        img[r] = (unsigned char*)_mm_malloc(lfFix->img_width*sizeof(unsigned char)*3, 32);

    float *res = (float*)_mm_malloc(lfFix->img_width*sizeof(float)*2, 32);
    unsigned long start_time = clock();
    for (int r = 0; r < lfFix->img_height; r++)
    {
        mod->ApplyGeometryDistortion (0, r, lfFix->img_width, 1, res);
        interp_row_nearest(img, r, lfFix->img_width, lfFix->img_height, res);
    }
    float run_time = ((float)(clock() - start_time)) / CLOCKS_PER_SEC;
    g_print("time elapsed : %.3fs,  ",run_time);

    _mm_free(res);
    _mm_free(img);

    delete mod;
    lf_free (lenses);
}

int main (int argc, char **argv)
{
  setlocale (LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/modifier/performance/dist/ptlens", lfFixture, NULL,
              mod_setup, test_perf_dist_ptlens, mod_teardown);

  return g_test_run();
}
