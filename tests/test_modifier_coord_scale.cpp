#include <glib.h>

#include <clocale>
#include <vector>
#include <string>
#include <map>
#include <limits>

#include <cstdlib>
#include <cstdio>
#include <cmath>

#include "lensfun.h"

#if !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__DragonFly__)
#include <malloc.h>
#endif
#ifdef __APPLE__
#include <sys/malloc.h>
#endif

#include "common_code.hpp"

typedef struct
{
  void       *coordBuff;
  size_t      img_width, img_height;
  lfLens     *lens;
  lfModifier *mod;
} lfFixture;

typedef struct
{
  bool    reverse;
  float   scale;
  size_t  alignment;
} lfTestParams;

// setup a standard lens
void mod_setup(lfFixture *lfFix, gconstpointer data)
{
  lfTestParams *p = (lfTestParams *)data;

  lfFix->lens             = new lfLens();
  lfFix->lens->CropFactor = 1.0f;
  lfFix->lens->Type       = LF_RECTILINEAR;

  lfFix->img_height = 300;
  lfFix->img_width  = 300;

  lfFix->mod = new lfModifier(lfFix->lens, 1.0f, lfFix->img_width, lfFix->img_height);

  lfFix->mod->Initialize(
    lfFix->lens, LF_PF_F32,
    24.0f, 2.8f, 1000.0f, p->scale, LF_RECTILINEAR,
    LF_MODIFY_SCALE, p->reverse);

  lfFix->coordBuff = NULL;

  const size_t bufsize = 2 * lfFix->img_width * lfFix->img_height * sizeof(float);
  if(p->alignment == 0)
    lfFix->coordBuff = g_malloc(bufsize);
  else
    lfFix->coordBuff = lf_alloc_align(p->alignment, bufsize);
}

void mod_teardown(lfFixture *lfFix, gconstpointer data)
{
  lfTestParams *p = (lfTestParams *)data;

  if(p->alignment == 0)
    g_free(lfFix->coordBuff);
  else
    lf_free_align(lfFix->coordBuff);

  delete lfFix->mod;
  delete lfFix->lens;
}

void test_mod_coord_scale(lfFixture *lfFix, gconstpointer data)
{
  for(size_t y = 0; y < lfFix->img_height; y++)
  {
    float *coordData = (float *)lfFix->coordBuff + (size_t)2 * y * lfFix->img_width;

    g_assert_true(
      lfFix->mod->ApplyGeometryDistortion(0.0, y, lfFix->img_width, 1, coordData)
    );
  }
}

#ifdef _OPENMP
void test_mod_coord_scale_parallel(lfFixture *lfFix, gconstpointer data)
{
  #pragma omp parallel for schedule(static)
  for(size_t y = 0; y < lfFix->img_height; y++)
  {
    float *coordData = (float *)lfFix->coordBuff + (size_t)2 * y * lfFix->img_width;

    g_assert_true(
      lfFix->mod->ApplyGeometryDistortion(0.0, y, lfFix->img_width, 1, coordData)
    );
  }
}
#endif

gchar *describe(lfTestParams *p, const char *prefix)
{
  gchar alignment[32] = "";
  g_snprintf(alignment, sizeof(alignment), "%lu-byte", p->alignment);

  return g_strdup_printf(
           "/%s/%s/%f/%s",
           prefix,
           p->reverse ? "unScale" : "Scale",
           p->scale,
           p->alignment == 0 ? "unaligned" : alignment
         );
}

void add_set_item(lfTestParams *p)
{
  gchar *desc = NULL;

  desc = describe(p, "modifier/coord/serialFor");
  g_test_add(desc, lfFixture, p, mod_setup, test_mod_coord_scale, mod_teardown);
  g_free(desc);
  desc = NULL;

#ifdef _OPENMP
  desc = describe(p, "modifier/coord/parallelFor");
  g_test_add(desc, lfFixture, p, mod_setup, test_mod_coord_scale_parallel, mod_teardown);
  g_free(desc);
  desc = NULL;
#endif
}

int main(int argc, char **argv)
{
  setlocale(LC_ALL, "");

  g_test_init(&argc, &argv, NULL);

  GSList *slist = NULL;

  std::vector<bool> reverse;
  reverse.push_back(false);
  reverse.push_back(true);

  for(std::vector<bool>::iterator it_reverse = reverse.begin(); it_reverse != reverse.end(); ++it_reverse)
  {
    std::vector<float> scale;
    scale.push_back(0.75f);
    scale.push_back(1.25f);

    for(std::vector<float>::iterator it_scale = scale.begin(); it_scale != scale.end(); ++it_scale)
    {
      std::vector<size_t> align;
      align.push_back(0);
      align.push_back(4  * sizeof(float)); // SSE
      //align.push_back(8  * sizeof(float)); // AVX
      //align.push_back(16 * sizeof(float)); // AVX512

      for(std::vector<size_t>::iterator it_align = align.begin(); it_align != align.end(); ++it_align)
      {
        lfTestParams *p = (lfTestParams *)g_malloc(sizeof(lfTestParams));

        p->reverse   = *it_reverse;
        p->scale     = *it_scale;
        p->alignment = *it_align;

        add_set_item(p);

        slist = g_slist_append(slist, p);
      }
    }
  }

  const int res = g_test_run();

  g_slist_free_full(slist, (GDestroyNotify)g_free);

  return res;
}
