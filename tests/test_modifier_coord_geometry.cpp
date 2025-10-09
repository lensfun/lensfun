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
  bool        reverse;
  gchar      *sourceType;
  lfLensType  sourceLensType;
  gchar      *targetType;
  lfLensType  targetLensType;
  size_t      alignment;
} lfTestParams;

// setup a standard lens
void mod_setup(lfFixture *lfFix, gconstpointer data)
{
  (void)data;
  lfTestParams *p = (lfTestParams *)data;

  lfFix->lens             = new lfLens();
  lfFix->lens->Type       = p->sourceLensType;

  lfFix->img_height = 300;
  lfFix->img_width  = 300;

  lfFix->mod = new lfModifier(lfFix->lens, 24.0f, 1.0f, lfFix->img_width, lfFix->img_height, LF_PF_F32, p->reverse);

  lfFix->mod->EnableProjectionTransform(p->targetLensType);

  lfFix->coordBuff = NULL;

  const size_t bufsize = 2 * lfFix->img_width * lfFix->img_height * sizeof(float);
  if(p->alignment == 0)
    lfFix->coordBuff = g_malloc(bufsize);
  else
    lfFix->coordBuff = lf_alloc_align(p->alignment, bufsize);
}

void mod_teardown(lfFixture *lfFix, gconstpointer data)
{
  (void)data;
  lfTestParams *p = (lfTestParams *)data;

  if(p->alignment == 0)
    g_free(lfFix->coordBuff);
  else
    lf_free_align(lfFix->coordBuff);

  delete lfFix->mod;
  delete lfFix->lens;
}

void test_mod_coord_geometry(lfFixture *lfFix, gconstpointer data)
{
  (void)data;
  for(size_t y = 0; y < lfFix->img_height; y++)
  {
    float *coordData = (float *)lfFix->coordBuff + (size_t)2 * y * lfFix->img_width;

    g_assert_true(
      lfFix->mod->ApplyGeometryDistortion(0.0, y, lfFix->img_width, 1, coordData)
    );
  }
}

#ifdef _OPENMP
void test_mod_coord_geometry_parallel(lfFixture *lfFix, gconstpointer data)
{
  (void)data;
  #pragma omp parallel for schedule(static)
  for(int y = 0; y < static_cast<int>(lfFix->img_height); y++)
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
           "/%s/%s/%s/%s/%s",
           prefix,
           p->reverse ? "unGeom" : "Geom",
           p->sourceType,
           p->targetType,
           p->alignment == 0 ? "unaligned" : alignment
         );
}

void add_set_item(lfTestParams *p)
{
  gchar *desc = NULL;

  desc = describe(p, "modifier/coord/serialFor");
  g_test_add(desc, lfFixture, p, mod_setup, test_mod_coord_geometry, mod_teardown);
  g_free(desc);
  desc = NULL;

#ifdef _OPENMP
  desc = describe(p, "modifier/coord/parallelFor");
  g_test_add(desc, lfFixture, p, mod_setup, test_mod_coord_geometry_parallel, mod_teardown);
  g_free(desc);
  desc = NULL;
#endif
}

void free_params(gpointer mem)
{
  lfTestParams *p = (lfTestParams *)mem;

  g_free(p->sourceType);
  g_free(p->targetType);
  g_free(mem);
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
    std::map<std::string, lfLensType> lensType;
    lensType["LF_RECTILINEAR"]           = LF_RECTILINEAR;
    lensType["LF_FISHEYE"]               = LF_FISHEYE;
    lensType["LF_PANORAMIC"]             = LF_PANORAMIC;
    lensType["LF_EQUIRECTANGULAR"]       = LF_EQUIRECTANGULAR;
    lensType["LF_FISHEYE_ORTHOGRAPHIC"]  = LF_FISHEYE_ORTHOGRAPHIC;
    lensType["LF_FISHEYE_STEREOGRAPHIC"] = LF_FISHEYE_STEREOGRAPHIC;
    lensType["LF_FISHEYE_EQUISOLID"]     = LF_FISHEYE_EQUISOLID;
    lensType["LF_FISHEYE_THOBY"]         = LF_FISHEYE_THOBY;

    for(std::map<std::string, lfLensType>::iterator it_sourceType = lensType.begin(); it_sourceType != lensType.end(); ++it_sourceType)
    {
      for(std::map<std::string, lfLensType>::iterator it_targetType = lensType.begin(); it_targetType != lensType.end(); ++it_targetType)
      {
        if(it_sourceType->second == it_targetType->second)
          continue;

        std::vector<size_t> align;
        align.push_back(0);
        align.push_back(4  * sizeof(float)); // SSE
        //align.push_back(8  * sizeof(float)); // AVX
        //align.push_back(16 * sizeof(float)); // AVX512

        for(std::vector<size_t>::iterator it_align = align.begin(); it_align != align.end(); ++it_align)
        {
          lfTestParams *p = (lfTestParams *)g_malloc(sizeof(lfTestParams));

          p->reverse        = *it_reverse;
          p->sourceType     = g_strdup(it_sourceType->first.c_str());
          p->sourceLensType = it_sourceType->second;
          p->targetType     = g_strdup(it_targetType->first.c_str());
          p->targetLensType = it_targetType->second;
          p->alignment      = *it_align;

          add_set_item(p);

          slist = g_slist_append(slist, p);
        }
      }
    }
  }

  const int res = g_test_run();

  g_slist_free_full(slist, (GDestroyNotify)free_params);

  return res;
}
