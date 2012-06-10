/*
    Test for library image modificator object.
*/

#include "lensfun.h"
#include "image.h"
#include <glib.h>
#include <locale.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

/* Define this to apply stage 1 & 3 corrections in one step,
   see main comment to the lfModifier class */
#define COMBINE_13

#if defined(_MSC_VER)
#define strcasecmp _stricmp
#define snprintf _snprintf
#define strtof (float)strtod
#endif 

static struct
{
    const char *Program;
    const char *Output;
    int ModifyFlags;
    bool Inverse;
    const char *Lens;
    float Scale;
    float Crop;
    float Focal;
    float Aperture;
    float Distance;
    Image::InterpolationMethod Interpolation;
    lfLensType TargetGeom;
} opts =
{
    NULL,
    "out-%s",
    0,
    false,
    "Sigma 14mm f/2.8 EX",
    1.0,
    0,
    0,
    0,
    1.0,
    Image::I_NEAREST,
    LF_RECTILINEAR
};

static void DisplayVersion ()
{
    g_print ("lensfun version %d.%d.%d: test image modifier routines\n",
        LF_VERSION_MAJOR, LF_VERSION_MINOR, LF_VERSION_MICRO);
    g_print ("Copyright (C) 2007 Andrew Zabolotny\n\n");
    g_print ("For distribution rules and conditions of use see the file\n");
    g_print ("COPYING which is part of the distribution.\n");
}

static void DisplayUsage ()
{
    DisplayVersion ();
    g_print ("\nCommand-line options:\n\n");
    g_print ("  -d    --distortion Apply lens distortion\n");
    g_print ("  -g#   --geometry=# Convert image geometry to given (one of:\n");
    g_print ("                     rectilinear,fisheye,panoramic,equirectangular,\n");
    g_print ("                     orthographic, stereographic, equisolid, thoby)\n");
    g_print ("  -t    --tca        Apply lens chromatic aberrations\n");
    g_print ("  -v    --vignetting Apply lens vignetting\n");
    g_print ("  -c    --cci        Apply lens Color Correction Index\n");
    g_print ("  -i    --inverse    Inverse correction of the image (e.g. simulate\n");
    g_print ("                     lens distortions instead of correcting them)\n");
    g_print ("  -s#   --scale=#    Apply additional scale on the image\n");
    g_print ("  -l#   --lens=#     Use calibration data for this lens\n");
    g_print ("  -C#   --crop=#     Set camera crop factor\n");
    g_print ("  -F#   --focal=#    Set focal length at which image has been taken\n");
    g_print ("  -A#   --aperture=# Set aperture at which image has been taken\n");
    g_print ("  -D#   --distance=# Set subject distance at which image has been taken\n");
    g_print ("  -I#   --interpol=# Choose interpolation algorithm (n[earest], b[ilinear], l[anczos])\n");
    g_print ("  -o#   --output=#   Set file name for output image\n");
    g_print ("  -V    --version    Display program version and exit\n");
    g_print ("  -h    --help       Display this help text\n");
}

// atof() with sanity checks
static float _atof (const char *s)
{
    char *end = NULL;
    float r = strtof (s, &end);
    if (end && *end)
    {
        g_print ("ERROR: Invalid number `%s', parsed as %g\n", s, r);
        g_print ("Use your locale-specific number format (e.g. ',' or '.' etc)\n");
        exit (-1);
    }
    return r;
}

static Image *ApplyModifier (int modflags, bool reverse, Image *img,
                             const lfModifier *mod)
{
    // Create a new image where we will copy the modified image
    Image *newimg = new Image ();
    // Output image always equals input image size, although
    // this is not a requirement of the library, it's just a
    // limitation of the testbed.
    newimg->Resize (img->width, img->height);

#ifdef COMBINE_13
    int lwidth = img->width * 2 * 3;
#else
    int lwidth = img->width * 2;
    if (modflags & LF_MODIFY_TCA)
        lwidth *= 3;
#endif
    float *pos = new float [lwidth];

    int step_start = reverse ? 2 : 0;
    int step_delta = reverse ? -1 : +1;
    int step_finish = reverse ? -1 : 3;

    for (int step = step_start; step != step_finish; step += step_delta)
    {
        RGBpixel *dst = newimg->image;
        char *imgdata = (char *)img->image;
        bool ok = true;

        img->InitInterpolation (opts.Interpolation);

        for (unsigned y = 0; ok && y < img->height; y++)
            switch (step)
            {
#ifdef COMBINE_13
                case 0:
                    ok = false;
                    break;

                case 2:
                    /* TCA and geometry correction */
                    ok = mod->ApplySubpixelGeometryDistortion (0.0, y, img->width, 1, pos);
#else
                case 0:
                    /* TCA correction */
                    ok = mod->ApplySubpixelDistortion (0.0, y, img->width, 1, pos);
#endif
                    if (ok)
                    {
                        float *src = pos;
                        for (unsigned x = 0; x < img->width; x++)
                        {
                            dst->red   = img->GetR (src [0], src [1]);
                            dst->green = img->GetG (src [2], src [3]);
                            dst->blue  = img->GetB (src [4], src [5]);
                            src += 2 * 3;
                            dst++;
                        }
                    }
                    break;

                case 1:
                    /* Colour correction: vignetting and CCI */
                    ok = mod->ApplyColorModification (imgdata, 0.0, y, img->width, 1,
                        LF_CR_4 (RED, GREEN, BLUE, UNKNOWN), 0);
                    imgdata += img->width * 4;
                    break;

#ifndef COMBINE_13
                case 2:
                    /* Distortion and geometry correction, scaling */
                    ok = mod->ApplyGeometryDistortion (0.0, y, newimg->width, 1, pos);
                    if (ok)
                    {
                        float *src = pos;
                        for (unsigned x = 0; x < img->width; x++)
                        {
                            img->Get (*dst, src [0], src [1]);
                            src += 2;
                            dst++;
                        }
                    }
                    break;
#endif
            }
        // After TCA and distortion steps switch img and newimg.
        // This is crucial since newimg is now the input image
        // to the next stage.
        if (ok && (step == 0 || step == 2))
        {
            Image *tmp = newimg;
            newimg = img;
            img = tmp;
        }
    }

    delete [] pos;
    delete newimg;
    return img;
}

static bool smartstreq (const char *str, const char *pattern)
{
    const char *src = str;
    while (*src)
    {
        char cs = toupper (*src++);
        char cp = toupper (*pattern++);
        if (!cs)
            return (src != str);
        if (!cp)
            return false;
        if (cs != cp)
            return false;
    }
    return true;
}

int main (int argc, char **argv)
{
    static struct option long_options[] =
    {
        {"output", required_argument, NULL, 'o'},
        {"distortion", no_argument, NULL, 'd'},
        {"geometry", optional_argument, NULL, 'g'},
        {"tca", no_argument, NULL, 't'},
        {"vignetting", no_argument, NULL, 'v'},
        {"cci", no_argument, NULL, 'c'},
        {"inverse", no_argument, NULL, 'i'},
        {"scale", required_argument, NULL, 's'},
        {"autoscale", no_argument, NULL, 'S'},
        {"lens", required_argument, NULL, 'l'},
        {"crop", required_argument, NULL, 'C'},
        {"focal", required_argument, NULL, 'F'},
        {"aperture", required_argument, NULL, 'A'},
        {"distance", required_argument, NULL, 'D'},
        {"interpol", required_argument, NULL, 'I'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {0, 0, 0, 0}
    };

    setlocale (LC_ALL, "");

    opts.Program = argv [0];

    int c;
    while ((c = getopt_long (argc, argv, "o:dg::tvcis:Sl:C:F:A:D:I:hV", long_options, NULL)) != EOF)
    {
        switch (c)
        {
            case 'o':
                opts.Output = optarg;
                break;
            case 'd':
                opts.ModifyFlags |= LF_MODIFY_DISTORTION;
                break;
            case 'g':
                opts.ModifyFlags |= LF_MODIFY_GEOMETRY;
                if (optarg)
                {
                    if (!strcasecmp (optarg, "rectilinear"))
                        opts.TargetGeom = LF_RECTILINEAR;
                    else if (!strcasecmp (optarg, "fisheye"))
                        opts.TargetGeom = LF_FISHEYE;
                    else if (!strcasecmp (optarg, "panoramic"))
                        opts.TargetGeom = LF_PANORAMIC;
                    else if (!strcasecmp (optarg, "equirectangular"))
                        opts.TargetGeom = LF_EQUIRECTANGULAR;
                    else if (!strcasecmp (optarg, "orthographic"))
                        opts.TargetGeom = LF_FISHEYE_ORTHOGRAPHIC;
                    else if (!strcasecmp (optarg, "stereographic"))
                        opts.TargetGeom = LF_FISHEYE_STEREOGRAPHIC;
                    else if (!strcasecmp (optarg, "equisolid"))
                        opts.TargetGeom = LF_FISHEYE_EQUISOLID;
                    else if (!strcasecmp (optarg, "thoby"))
                        opts.TargetGeom = LF_FISHEYE_THOBY;
                    else
                    {
                        g_print ("Target lens geometry must be one of 'rectilinear', 'fisheye', 'panoramic', 'equirectangular'\n'orthographic', 'stereographic', 'equisolid', 'thoby'\n");
                        return -1;
                    }
                }
                break;
            case 't':
                opts.ModifyFlags |= LF_MODIFY_TCA;
                break;
            case 'v':
                opts.ModifyFlags |= LF_MODIFY_VIGNETTING;
                break;
            case 'c':
                opts.ModifyFlags |= LF_MODIFY_CCI;
                break;
            case 'i':
                opts.Inverse = true;
                break;
            case 's':
                opts.ModifyFlags |= LF_MODIFY_SCALE;
                opts.Scale = _atof (optarg);
                break;
            case 'S':
                opts.ModifyFlags |= LF_MODIFY_SCALE;
                opts.Scale = 0.0;
                break;
            case'l':
                opts.Lens = optarg;
                break;
            case 'C':
                opts.Crop = _atof (optarg);
                break;
            case 'F':
                opts.Focal = _atof (optarg);
                break;
            case 'A':
                opts.Aperture = _atof (optarg);
                break;
            case 'D':
                opts.Distance = _atof (optarg);
                break;
            case 'I':
                if (smartstreq (optarg, "nearest"))
                    opts.Interpolation = Image::I_NEAREST;
                else if (smartstreq (optarg, "bilinear"))
                    opts.Interpolation = Image::I_BILINEAR;
                else if (smartstreq (optarg, "lanczos"))
                    opts.Interpolation = Image::I_LANCZOS;
                else
                {
                    g_print ("Unknown interpolation method `%s'\n", optarg);
                    return -1;
                }
                break;
            case 'h':
                DisplayUsage ();
                return 0;
            case 'V':
                DisplayVersion ();
                return 0;
            default:
                return -1;
        }
    }

    if (optind >= argc)
    {
        g_print ("No files given on command line\n\n");
        DisplayUsage ();
        return -1;
    }

    lfDatabase *ldb = lf_db_new ();
    ldb->Load ();

    const lfLens **lenses = ldb->FindLenses (NULL, NULL, opts.Lens);
    if (!lenses)
    {
        g_print ("Cannot find the lens `%s' in database\n", opts.Lens);
        return -1;
    }

    const lfLens *lens = lenses [0];
    lf_free (lenses);

    if (!opts.Crop)
        opts.Crop = lens->CropFactor;
    if (!opts.Focal)
        opts.Focal = lens->MinFocal;
    if (!opts.Aperture)
        opts.Aperture = lens->MinAperture;

    g_print ("%sCorrecting image as taken by lens `%s, %s'\n"
             "    at crop factor %g, focal length %g, aperture %g, distance %g\n",
             opts.Inverse ? "Inverse " : "", lens->Maker, lens->Model,
             opts.Crop, opts.Focal, opts.Aperture, opts.Distance);

    for (; optind < argc; optind++)
    {
        Image *img = new Image ();
        g_print ("Loading `%s' ...", argv [optind]);
        if (!img->Open (argv [optind]))
        {
            g_print ("\rWarning: failed to open file `%s'\n", argv [optind]);
            delete img;
            continue;
        }

        if (!img->LoadPNG ())
        {
            g_print ("\rWarning: failed to parse PNG data from file `%s'\n", argv [optind]);
            delete img;
            continue;
        }

        g_print ("\b\b\b[%ux%u], processing", img->width, img->height);
        lfModifier *mod = lfModifier::Create (lens, opts.Crop, img->width, img->height);
        int modflags = mod->Initialize (
            lens, LF_PF_U8, opts.Focal,
            opts.Aperture, opts.Distance, opts.Scale, opts.TargetGeom,
            opts.ModifyFlags, opts.Inverse);

        if (!mod)
        {
            g_print ("\rWarning: failed to create modifier\n");
            delete img;
            continue;
        }

        if (modflags & LF_MODIFY_TCA)
            g_print ("[tca]");
        if (modflags & LF_MODIFY_VIGNETTING)
            g_print ("[vign]");
        if (modflags & LF_MODIFY_CCI)
            g_print ("[cci]");
        if (modflags & LF_MODIFY_DISTORTION)
            g_print ("[dist]");
        if (modflags & LF_MODIFY_GEOMETRY)
            g_print ("[geom]");
        if (opts.Scale != 1.0)
            g_print ("[scale]");
        g_print (" ...");

        clock_t st;
        clock_t xt = clock ();
        while (xt == (st = clock ()))
            ;

        img = ApplyModifier (modflags, opts.Inverse, img, mod);

        clock_t et = clock ();
        g_print ("\b\b\b(%.2g secs)", double (et - st) / CLOCKS_PER_SEC);

        mod->Destroy ();

        char out_fname [200];
        snprintf (out_fname, sizeof (out_fname), opts.Output, argv [optind]);
        g_print (", saving `%s'...", out_fname);
        bool ok = img->SavePNG (out_fname);
        delete img;

        g_print ("\b\b\b\b, %s\n", ok ? "done" : "FAILED");
    }

    ldb->Destroy ();
    return 0;
}
