#include <glib.h>
#include <locale.h>
#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include "lensfun.h"

typedef struct {
    size_t      img_width, img_height;
    lfLens*     lens;
    lfModifier* mod;
} lfFixture;

// setup a standard lens
void mod_setup(lfFixture *lfFix, gconstpointer data)
{
    lfFix->lens              = new lfLens();
    lfFix->lens->CropFactor  = 1.0f;
    lfFix->lens->AspectRatio = 1.0f;
    lfFix->lens->Type        = LF_RECTILINEAR;

    lfLensCalibDistortion lensCalibDist = {LF_DIST_MODEL_POLY3, 12.0f, {0.1,0.5,0.5}};
    lfFix->lens->AddCalibDistortion(&lensCalibDist);

    // width and height have to be odd, so we have a non fractional center position
    lfFix->img_height = 301;
    lfFix->img_width  = 301;
}


void mod_teardown(lfFixture *lfFix, gconstpointer data)
{
    lfFix->mod->Destroy();
    delete lfFix->lens;
}

// test to verifiy that projection center is image center
void test_mod_projection_center(lfFixture* lfFix, gconstpointer data)
{
    float in[2]  = {0, 0};
    float res[2] = {0, 0};

    // check mapping of center to center
    lfLensType geom_types [] = {LF_RECTILINEAR, LF_PANORAMIC, LF_EQUIRECTANGULAR , LF_FISHEYE, LF_FISHEYE_EQUISOLID, LF_FISHEYE_ORTHOGRAPHIC, LF_FISHEYE_THOBY, LF_UNKNOWN};
    const char* geom_names [] = {"rectilinear", "panoramic", "equirect", "fisheye", "fisheye-equisolid", "fisheye-orthographic", "fisheye-thoby", NULL};
    int  j  = 0;
    while (geom_types[j]!=LF_UNKNOWN) {

        lfFix->lens->Type = geom_types[j];

        int i = 0;
        while (geom_types[i]!=LF_UNKNOWN) {

            if(g_test_verbose())
                g_print("  ~ Conversion from %s -> %s \n", geom_names[j], geom_names[i]);

            lfFix->mod = lfModifier::Create (lfFix->lens, 1.0f, lfFix->img_width, lfFix->img_height);
            lfFix->mod->Initialize (
                lfFix->lens, LF_PF_U8, 12.0f,
                6.7f, 2.0f, 1.0f, geom_types[i],
                LF_MODIFY_GEOMETRY, false);

            // check if center is not influenced
            in[0] = (lfFix->img_width-1)/2;
            in[1] = (lfFix->img_height-1)/2;
            if (lfFix->mod->ApplyGeometryDistortion(in[0],in[1],1,1,res)) {
                g_assert_cmpfloat(in[0],==,res[0]);
                g_assert_cmpfloat(in[1],==,res[1]);
            }
            i++;
        }
        j++;
    }
}

// check if output becomes NaN when processing geometry conversion
void test_mod_projection_borders(lfFixture* lfFix, gconstpointer data)
{
    float in[2]  = {lfFix->img_width, lfFix->img_height};
    float in2[2] = {(lfFix->img_width-1)/2, (lfFix->img_height-1)/2};
    float res[2] = {0, 0};

    lfLensType geom_types [] = {LF_RECTILINEAR, LF_PANORAMIC, LF_EQUIRECTANGULAR, LF_FISHEYE_STEREOGRAPHIC, LF_FISHEYE, LF_FISHEYE_EQUISOLID, LF_FISHEYE_ORTHOGRAPHIC, LF_FISHEYE_THOBY, LF_UNKNOWN};
    const char* geom_names [] = {"rectilinear", "panoramic", "equirect", "fisheye-sterographic", "fisheye", "fisheye-equisolid", "fisheye-orthographic", "fisheye-thoby", NULL};
    int  j  = 0;
    while (geom_types[j]!=LF_UNKNOWN) {

        lfFix->lens->Type = geom_types[j];

        int  i  = 0;
        while (geom_types[i]!=LF_UNKNOWN) {

            if(g_test_verbose())
                g_print("  ~ Conversion from %s -> %s \n", geom_names[j], geom_names[i]);

            lfFix->mod = lfModifier::Create (lfFix->lens, 1.0f, lfFix->img_width, lfFix->img_height);
            lfFix->mod->Initialize (
                lfFix->lens, LF_PF_U8, 12.0f,
                6.7f, 2.0f, 1.0f, geom_types[i],
                LF_MODIFY_GEOMETRY, false);

            if (lfFix->mod->ApplyGeometryDistortion(0,0,1,1,res)) {
                g_assert_false(isnan(res[0]));
                g_assert_false(isnan(res[1]));
            }

            if (lfFix->mod->ApplyGeometryDistortion(in[0],in[1],1,1,res)) {
                g_assert_false(isnan(res[0]));
                g_assert_false(isnan(res[1]));
            }

            if (lfFix->mod->ApplyGeometryDistortion(in2[0],in2[1],1,1,res)) {
                g_assert_false(isnan(res[0]));
                g_assert_false(isnan(res[1]));
            }

            i++;
        }
        j++;
    }
}


int main (int argc, char **argv)
{

    setlocale (LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add("/modifier/projection center", lfFixture, NULL, mod_setup, test_mod_projection_center, mod_teardown);
    g_test_add("/modifier/projection borders", lfFixture, NULL, mod_setup, test_mod_projection_borders, mod_teardown);

    return g_test_run();
}
