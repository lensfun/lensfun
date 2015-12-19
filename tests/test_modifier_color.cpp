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

//TODO: find better place for cTypeToLfPixelFormat()
template<typename T>
lfPixelFormat cTypeToLfPixelFormat()
{
  return LF_PF_U8;
}
template<>
lfPixelFormat cTypeToLfPixelFormat<unsigned char>()
{
  return LF_PF_U8;
}
template<>
lfPixelFormat cTypeToLfPixelFormat<unsigned short>()
{
  return LF_PF_U16;
}
template<>
lfPixelFormat cTypeToLfPixelFormat<unsigned int>()
{
  return LF_PF_U32;
}
template<>
lfPixelFormat cTypeToLfPixelFormat<float>()
{
  return LF_PF_F32;
}
template<>
lfPixelFormat cTypeToLfPixelFormat<double>()
{
  return LF_PF_F64;
}

typedef struct
{
  void       *image;
  size_t      img_width, img_height;
  lfLens     *lens;
  lfModifier *mod;
} lfFixture;

typedef struct
{
  bool    reverse;
  size_t  cpp;
  gchar  *pixDesc;
  int     comp_role;
  size_t  alignment;
} lfTestParams;

template<typename T>
void generateImage(lfFixture *lfFix, lfTestParams *p, T *pixels)
{
  T halfmax = std::numeric_limits<T>::max() / T(2);
  for(size_t i = 0; i < p->cpp * lfFix->img_width * lfFix->img_height; i++)
    pixels[i] = halfmax;
}
template<>
void generateImage<float>(lfFixture *lfFix, lfTestParams *p, float *pixels)
{
  for(size_t i = 0; i < p->cpp * lfFix->img_width * lfFix->img_height; i++)
    pixels[i] = 0.5f;
}
template<>
void generateImage<double>(lfFixture *lfFix, lfTestParams *p, double *pixels)
{
  for(size_t i = 0; i < p->cpp * lfFix->img_width * lfFix->img_height; i++)
    pixels[i] = 0.5;
}

// setup a standard lens
template<typename T>
void mod_setup(lfFixture *lfFix, gconstpointer data)
{
  lfTestParams *p = (lfTestParams *)data;

  lfFix->lens             = new lfLens();
  lfFix->lens->CropFactor = 1.0f;
  lfFix->lens->Type       = LF_RECTILINEAR;

  // Canon EOS 5D Mark III + Canon EF 24-70mm f/2.8L II USM
  lfLensCalibVignetting lensCalibVign = {LF_VIGNETTING_MODEL_PA, 24.0f, 2.8f, 1000.0f, { -0.5334f, -0.7926f, 0.5243f}};
  lfFix->lens->AddCalibVignetting(&lensCalibVign);

  lfFix->img_height = 300;
  lfFix->img_width  = 300;

  lfFix->mod = new lfModifier(lfFix->lens, 1.0f, lfFix->img_width, lfFix->img_height);

  lfFix->mod->Initialize(
    lfFix->lens, cTypeToLfPixelFormat<T>(),
    24.0f, 2.8f, 1000.0f, 1.0f, LF_RECTILINEAR,
    LF_MODIFY_VIGNETTING, p->reverse);

  lfFix->image = NULL;

  const size_t bufsize = p->cpp * lfFix->img_width * lfFix->img_height * sizeof(T);
  if(p->alignment == 0)
    lfFix->image = g_malloc(bufsize);
  else
    lfFix->image = lf_alloc_align(p->alignment, bufsize);

  generateImage<T>(lfFix, p, (T *)lfFix->image);
}

void mod_teardown(lfFixture *lfFix, gconstpointer data)
{
  lfTestParams *p = (lfTestParams *)data;

  if(p->alignment == 0)
    g_free(lfFix->image);
  else
    lf_free_align(lfFix->image);

  delete lfFix->mod;
  delete lfFix->lens;
}

template<typename T>
void test_mod_color(lfFixture *lfFix, gconstpointer data)
{
  lfTestParams *p = (lfTestParams *)data;

  for(size_t y = 0; y < lfFix->img_height; y++)
  {
    T *imgdata = (T *)lfFix->image + (size_t)p->cpp * y * lfFix->img_width;

    g_assert_true(
      lfFix->mod->ApplyColorModification(
        imgdata, 0.0, y, lfFix->img_width, 1,
        p->comp_role, p->cpp * lfFix->img_width));
  }
}

#ifdef _OPENMP
template<typename T>
void test_mod_color_parallel(lfFixture *lfFix, gconstpointer data)
{
  const lfTestParams *const p = (lfTestParams *)data;

  #pragma omp parallel for schedule(static)
  for(size_t y = 0; y < lfFix->img_height; y++)
  {
    T *imgdata = (T *)lfFix->image + (size_t)p->cpp * y * lfFix->img_width;

    g_assert_true(
      lfFix->mod->ApplyColorModification(
        imgdata, 0.0, y, lfFix->img_width, 1,
        p->comp_role, p->cpp * lfFix->img_width));
  }
}
#endif

gchar *describe(lfTestParams *p, const char *prefix, const char *f)
{
  gchar alignment[32] = "";
  g_snprintf(alignment, sizeof(alignment), "%lu-byte", p->alignment);

  return g_strdup_printf(
           "/%s/%s/%s/%s/%s",
           prefix,
           p->reverse ? "Vignetting" : "DeVignetting",
           p->pixDesc,
           f,
           p->alignment == 0 ? "unaligned" : alignment
         );
}

template<typename T>
void add_set_item(lfTestParams *p, const char *f)
{
  gchar *desc = NULL;

  desc = describe(p, "modifier/color/serialFor", f);
  g_test_add(desc, lfFixture, p, mod_setup<T>, test_mod_color<T>, mod_teardown);
  g_free(desc);
  desc = NULL;

#ifdef _OPENMP
  desc = describe(p, "modifier/color/parallelFor", f);
  g_test_add(desc, lfFixture, p, mod_setup<T>, test_mod_color_parallel<T>, mod_teardown);
  g_free(desc);
  desc = NULL;
#endif
}

void add_sets(lfTestParams *p)
{
  add_set_item<unsigned char>(p, "lf_u8");
  add_set_item<unsigned short>(p, "lf_u16");
  add_set_item<unsigned int>(p, "lf_u32");
  add_set_item<float>(p, "lf_f32");
  add_set_item<double>(p, "lf_f64");
}

void free_params(gpointer mem)
{
  lfTestParams *p = (lfTestParams *)mem;

  g_free(p->pixDesc);
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
    std::map<std::string, int> pixDesc;
    pixDesc["RGB"]  = LF_CR_3(RED, GREEN, BLUE);
    pixDesc["RGBA"] = LF_CR_4(RED, GREEN, BLUE, UNKNOWN);
    pixDesc["ARGB"] = LF_CR_4(UNKNOWN, RED, GREEN, BLUE);

    for(std::map<std::string, int>::iterator it_pixDesc = pixDesc.begin(); it_pixDesc != pixDesc.end(); ++it_pixDesc)
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
        p->cpp       = it_pixDesc->first.length();
        p->pixDesc   = g_strdup(it_pixDesc->first.c_str());
        p->comp_role = it_pixDesc->second;
        p->alignment = *it_align;

        add_sets(p);

        slist = g_slist_append(slist, p);
      }
    }
  }

  const int res = g_test_run();

  g_slist_free_full(slist, (GDestroyNotify)free_params);

  return res;
}
