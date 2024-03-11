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
  bool            reverse;
  gchar          *tcaModel;
  lfLensCalibTCA  calib;
  size_t          alignment;
} lfTestParams;

// setup a standard lens
void mod_setup(lfFixture *lfFix, gconstpointer data)
{
  (void)data;
  lfTestParams *p = (lfTestParams *)data;

  lfFix->lens             = new lfLens();
  lfFix->lens->Type       = LF_RECTILINEAR;

  lfFix->lens->AddCalibTCA(&p->calib);

  lfFix->img_height = 300;
  lfFix->img_width  = 300;

  lfFix->mod = new lfModifier(lfFix->lens, 24.0f, 1.0f, lfFix->img_width, lfFix->img_height, LF_PF_F32, p->reverse);

  lfFix->mod->EnableTCACorrection();

  lfFix->coordBuff = NULL;

  const size_t bufsize = 2 * 3 * lfFix->img_width * lfFix->img_height * sizeof(float);
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

void test_mod_subpix(lfFixture *lfFix, gconstpointer data)
{
  (void)data;
  for(size_t y = 0; y < lfFix->img_height; y++)
  {
    float *coordData = (float *)lfFix->coordBuff + (size_t)2 * 3 * y * lfFix->img_width;

    g_assert_true(
      lfFix->mod->ApplySubpixelDistortion(0.0, y, lfFix->img_width, 1, coordData)
    );
  }
}

#ifdef _OPENMP
void test_mod_subpix_parallel(lfFixture *lfFix, gconstpointer data)
{
  (void)data;
  #pragma omp parallel for schedule(static)
  for(int y = 0; y < static_cast<int>(lfFix->img_height); y++)
  {
    float *coordData = (float *)lfFix->coordBuff + (size_t)2 * 3 * y * lfFix->img_width;

    g_assert_true(
      lfFix->mod->ApplySubpixelDistortion(0.0, y, lfFix->img_width, 1, coordData)
    );
  }
}
#endif

gchar *describe(lfTestParams *p, const char *prefix)
{
  gchar alignment[32] = "";
  g_snprintf(alignment, sizeof(alignment), "%lu-byte", p->alignment);

  return g_strdup_printf(
           "/%s/%s/%s/%s",
           prefix,
           p->reverse ? "UnTCA" : "TCA",
           p->tcaModel,
           p->alignment == 0 ? "unaligned" : alignment
         );
}

void add_set_item(lfTestParams *p)
{
  gchar *desc = NULL;

  desc = describe(p, "modifier/subpix/serialFor");
  g_test_add(desc, lfFixture, p, mod_setup, test_mod_subpix, mod_teardown);
  g_free(desc);
  desc = NULL;

#ifdef _OPENMP
  desc = describe(p, "modifier/subpix/parallelFor");
  g_test_add(desc, lfFixture, p, mod_setup, test_mod_subpix_parallel, mod_teardown);
  g_free(desc);
  desc = NULL;
#endif
}

void free_params(gpointer mem)
{
  lfTestParams *p = (lfTestParams *)mem;

  g_free(p->tcaModel);
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
    lfLensCalibAttributes cs = {1.0f, 1.5f};

    std::map<std::string, lfLensCalibTCA> tcaCalib;
    // ??? + Nikon AF-S DX Nikkor 35mm f/1.8G
    tcaCalib["LF_TCA_MODEL_LINEAR"] = lfLensCalibTCA
    {
      LF_TCA_MODEL_LINEAR, 35.0f, {1.0003f, 1.0003f}, cs
    };
    // Canon EOS 5D Mark III + Canon EF 24-70mm f/2.8L II USM
    tcaCalib["LF_TCA_MODEL_POLY3"] = lfLensCalibTCA
    {
      LF_TCA_MODEL_POLY3,  24.0f, {1.0002104f, 1.0000529f, 0.0f, 0.0f, -0.0000220f, -0.0000000f}, cs
    };

    for(std::map<std::string, lfLensCalibTCA>::iterator it_tcaCalib = tcaCalib.begin(); it_tcaCalib != tcaCalib.end(); ++it_tcaCalib)
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
        p->tcaModel  = g_strdup(it_tcaCalib->first.c_str());
        p->calib     = it_tcaCalib->second;
        p->alignment = *it_align;

        add_set_item(p);

        slist = g_slist_append(slist, p);
      }
    }
  }

  const int res = g_test_run();

  g_slist_free_full(slist, (GDestroyNotify)free_params);

  return res;
}
