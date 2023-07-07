/*
    Test for library image modificator object.
*/


#include <glib.h>
#include <locale.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "lensfun.h"
#include "image.h"
#include "auxfun.h"

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
    const char *Input;
    const char *Output;
    int ModifyFlags;
    bool Inverse;
    const char *Lens;
    const char *Camera;
    float Scale;
    float Crop;
    float Focal;
    float Aperture;
    float Distance;
    Image::InterpolationMethod Interpolation;
    lfLensType TargetGeom;
    const char *Database;
    bool Verbose;
} opts =
{
    NULL,
    NULL,
    "output.png",
    0,
    false,
    NULL,
    NULL,
    1.0f,
    0,
    0,
    0,
    1000.0f,
    Image::I_LANCZOS,
    LF_RECTILINEAR,
    NULL,
    false
};


static void DisplayVersion ()
{
    g_print ("Lenstool reference implementation for Lensfun version %d.%d.%d\n",
        LF_VERSION_MAJOR, LF_VERSION_MINOR, LF_VERSION_MICRO);
    g_print ("%s", "Copyright (C) 2007 Andrew Zabolotny\n\n");
    g_print ("%s", "For distribution rules and conditions of use see the file\n");
    g_print ("%s", "COPYING which is part of the distribution.\n");
}

static void DisplayUsage ()
{
    DisplayVersion ();
    g_print ("%s", "\nCommand-line options:\n\n");
    g_print ("%s", "  -d    --distortion Apply lens distortion\n");
    g_print ("%s", "  -g#   --geometry=# Convert image geometry to given (one of:\n");
    g_print ("%s", "                     rectilinear,fisheye,panoramic,equirectangular,\n");
    g_print ("%s", "                     orthographic, stereographic, equisolid, thoby)\n");
    g_print ("%s", "  -t    --tca        Apply lens chromatic aberrations\n");
    g_print ("%s", "  -v    --vignetting Apply lens vignetting\n");
    g_print ("%s", "  -a    --all        Apply all possible corrections (tca, vign, dist)\n");
    g_print ("%s", "  -i    --inverse    Inverse correction of the image (e.g. simulate\n");
    g_print ("%s", "                     lens distortions instead of correcting them)\n");
    g_print ("%s", "\n");
    g_print ("%s", "  -C#   --camera=#   Camera name\n");
    g_print ("%s", "  -c#   --crop=#     Set camera crop factor in case the camera is not given\n");
    g_print ("%s", "\n");
    g_print ("%s", "  -L#   --lens=#     Lens name to search for in the database\n");
    g_print ("%s", "  -F#   --focal=#    Set focal length at which image has been taken\n");
    g_print ("%s", "  -A#   --aperture=# Set aperture at which image has been taken\n");
    g_print ("%s", "  -D#   --distance=# Set subject distance at which image has been taken\n");
    g_print ("%s", "\n");
    g_print ("%s", "  -s#   --scale=#    Apply additional scale on the image\n");
    g_print ("%s", "  -I#   --interpol=# Choose interpolation algorithm (n[earest], b[ilinear], l[anczos])\n");
    g_print ("%s", "\n");
    g_print ("%s", "  -o#   --output=#   Set file name for output image\n");
    g_print ("%s", "        --database=# Only use the specified database folder or file\n");
    g_print ("%s", "        --verbose    Verbose output\n");
    g_print ("%s", "        --version    Display program version and exit\n");
    g_print ("%s", "  -h    --help       Display this help text\n");
}

static bool ParseParameters(int argc, char **argv)
{
    static struct option long_options[] = {
        {"output", required_argument, NULL, 'o'},
        {"distortion", no_argument, NULL, 'd'},
        {"geometry", optional_argument, NULL, 'g'},
        {"tca", no_argument, NULL, 't'},
        {"vignetting", no_argument, NULL, 'v'},
        {"all", no_argument, NULL, 'a'},
        {"inverse", no_argument, NULL, 'i'},
        {"scale", required_argument, NULL, 'S'},
        {"lens", required_argument, NULL, 'L'},
        {"camera", required_argument, NULL, 'C'},
        {"crop", required_argument, NULL, 'c'},
        {"focal", required_argument, NULL, 'F'},
        {"aperture", required_argument, NULL, 'A'},
        {"distance", required_argument, NULL, 'D'},
        {"interpol", required_argument, NULL, 'I'},
        {"help", no_argument, NULL, 'h'},
        {"database", required_argument, NULL, 3},
        {"version", no_argument, NULL, 4},
        {"verbose", no_argument, NULL, 5},
        {0, 0, 0, 0}
    };

    opts.Program = argv [0];

    int c;
    while ((c = getopt_long (argc, argv, "o:dg::tvaiS:L:C:c:F:A:D:I:h", long_options, NULL)) != EOF) {
        switch (c) {
            case 'o':
                opts.Output = optarg;
                break;
            case 'd':
                opts.ModifyFlags |= LF_MODIFY_DISTORTION;
                break;
            case 'g':
                opts.ModifyFlags |= LF_MODIFY_GEOMETRY;
                if (optarg) {
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
                    else {
                        DisplayUsage();
                        g_print ("%s", "\nTarget lens geometry must be one of 'rectilinear', 'fisheye', 'panoramic', 'equirectangular'\n'orthographic', 'stereographic', 'equisolid', 'thoby'\n");
                        return false;
                    }
                }
                break;
            case 't':
                opts.ModifyFlags |= LF_MODIFY_TCA;
                break;
            case 'v':
                opts.ModifyFlags |= LF_MODIFY_VIGNETTING;
                break;
            case 'a':
                opts.ModifyFlags |= LF_MODIFY_VIGNETTING;
                opts.ModifyFlags |= LF_MODIFY_TCA;
                opts.ModifyFlags |= LF_MODIFY_DISTORTION;
                break;
            case 'i':
                opts.Inverse = true;
                break;
            case 'S':
                opts.ModifyFlags |= LF_MODIFY_SCALE;
                opts.Scale = _atof (optarg);
                break;
            case'L':
                opts.Lens = optarg;
                break;
            case'C':
                opts.Camera = optarg;
                break;
            case 'c':
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
                else {
                    DisplayUsage();
                    g_print ("\nUnknown interpolation method `%s'\n", optarg);
                    return false;
                }
                break;
            case 'h':
                DisplayUsage ();
                return false;
            case 3:
              opts.Database = optarg;
              break;
            case 4:
                DisplayVersion ();
                return false;
            case 5:
                opts.Verbose = true;
                break;
            default:
                return false;
        }
    }

    if (optind <= argc)
        opts.Input = argv [optind];

    if (!opts.Lens && !opts.Camera) {
        DisplayUsage();
        g_print ("%s", "\nAt least a lens or camera name is required to perform a database lookup!\n");
        return false;
    }

    if (!opts.Lens && opts.Input) {
        DisplayUsage();
        g_print ("%s", "\nNo lens information (-L) supplied to process specified input image!\n");
        return false;
    }

    return true;
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
                    /* Colour correction: vignetting */
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



int main (int argc, char **argv)
{
    setlocale (LC_ALL, "");

    if (!ParseParameters(argc, argv))
        return -1;

    // load database
    lfDatabase *ldb = new lfDatabase ();

    if (opts.Verbose) {
        if (opts.Database == NULL) {
          g_print ("%s", "\rConfigured database search locations\n");
          g_print ("\r  - System-wide database: %s\n", ldb->SystemLocation);
          g_print ("\r  - System-wide database updates: %s\n", ldb->SystemUpdatesLocation);
          g_print ("\r  - Current user database: %s\n", ldb->UserLocation);
          g_print ("\r  - Current user database updates: %s\n\n", ldb->UserUpdatesLocation);
        } else {
          g_print ("\rLooking for lens database in %s\n\n", opts.Database);
        }
    }

    if (ldb->Load (opts.Database) != LF_NO_ERROR) {
        delete ldb;
        g_print ("%s", "\rERROR: Database could not be loaded\n");
        return -1;
    } else g_print ("%s", "\rDatabase loaded successfully.\n\n");


    // try to find camera in the database
    const lfCamera *cam = NULL;
    if (opts.Camera) {
        const lfCamera ** cameras = ldb->FindCamerasExt(NULL, opts.Camera);
        if (cameras)
            cam = cameras[0];
        else
            g_print ("Cannot find a camera matching `%s' in database\n", opts.Camera);
        lf_free (cameras);
     }

    // try to find a matching lens in the database
    const lfLens *lens = NULL;
    if (opts.Lens) {
        const lfLens **lenses = ldb->FindLenses (cam, NULL, opts.Lens);
        if (lenses)
            lens = lenses [0];
        else
            g_print ("Cannot find a lens matching `%s' in database\n", opts.Lens);
        lf_free (lenses);
    }

    // print camera and lens information if in verbose mode or if no input file is specified
    if (opts.Verbose || !opts.Input) {
        if (cam && lens) {
            g_print("%s", "Matching lens and camera combination found in the database:\n");
            PrintCamera(cam, ldb);
            PrintLens(lens, ldb, opts.Verbose);
        } else if (!cam && lens) {
            g_print("%s", "Matching lens found in the database:\n");
            PrintLens(lens, ldb, opts.Verbose);
        } else if (!lens && cam) {
            g_print("%s", "Matching camera found in the database:\n");
            PrintCamera(cam, ldb);
        }
    } else {
        if (cam && lens) {
            g_print("= Selecting %s / %s\n", cam->Model, lens->Model);
        } else if (!cam && lens) {
            g_print("= Selecting %s\n", lens->Model);
        }
    }

    // nothing to process, so lets quit here
    if (!opts.Input) {
        delete ldb;
        return 0;
    }

    // assume standard values if parameters are not specified
    if (cam)
        opts.Crop = cam->CropFactor;
    if (!opts.Focal)
        opts.Focal = lens->MinFocal;
    if (!opts.Aperture)
        opts.Aperture = lens->MinAperture;

    if (opts.Verbose) {
        g_print("\nProcessing parameters:\n"
                "    |- Image crop factor: %g\n"
                "    |- Focal length: %gmm\n"
                "    |- Aperture: f/%g\n"
                "    |- Distance: %gm\n\n",
                opts.Crop, opts.Focal, opts.Aperture, opts.Distance);
    } else {
        g_print("= Processing parameters: Crop %g, Focal %gmm, Aperture f/%g, Distance: %gm\n",
                opts.Crop, opts.Focal, opts.Aperture, opts.Distance);
    }

    Image *img = new Image ();
    g_print ("~ Loading `%s' ... ", opts.Input);
    if (!img->Open (opts.Input)) {
        g_print ("\rERROR: failed to open file `%s'\n", opts.Input);
        delete img;
        delete ldb;
        return -1;
    }
    if (!img->LoadPNG ()) {
        g_print ("\rERROR: failed to parse PNG data from file `%s'\n", opts.Input);
        delete img;
        delete ldb;
        return -1;
    }
    g_print ("done.\n~ Image size [%ux%u].\n", img->width, img->height);

    lfModifier *mod = new lfModifier (lens, opts.Focal, opts.Crop, img->width, img->height, LF_PF_U8, opts.Inverse);
    if (!mod) {
        g_print ("%s", "\rWarning: failed to create modifier\n");
        delete img;
        delete ldb;
        return -1;
    }    

    // Enable desired modifications
    if (opts.ModifyFlags & LF_MODIFY_TCA)
        mod->EnableTCACorrection();

    if (opts.ModifyFlags & LF_MODIFY_VIGNETTING)
        mod->EnableVignettingCorrection(opts.Aperture, opts.Distance);

    if (opts.ModifyFlags & LF_MODIFY_DISTORTION)
        mod->EnableDistortionCorrection();

    if (opts.ModifyFlags & LF_MODIFY_GEOMETRY)
        mod->EnableProjectionTransform(opts.TargetGeom);

    if (opts.Scale != 1.0)
        mod->EnableScaling(opts.Scale);

    // Check if modifications could have been enabled
    g_print("%s", "~ Selected modifications: ");
    const int modflags = mod->GetModFlags();
    if (modflags & LF_MODIFY_TCA)
        g_print ("%s", "[tca]");

    if (modflags & LF_MODIFY_VIGNETTING)        
        g_print ("%s", "[vign]");

    if (modflags & LF_MODIFY_DISTORTION)
        g_print ("%s", "[dist]");

    if (modflags & LF_MODIFY_GEOMETRY)
        g_print ("%s", "[geom]");

    if (opts.Scale != 1.0)
        g_print ("%s", "[scale]");

    if (modflags==0)
        g_print ("%s", "[NOTHING]");
    g_print ("%s", "\n");

    g_print("%s", "~ Run processing chain... ");

    clock_t st;
    clock_t xt = clock ();
    while (xt == (st = clock ()))
        ;

    img = ApplyModifier (modflags, opts.Inverse, img, mod);

    clock_t et = clock ();
    g_print ("done (%.3g secs)\n", double (et - st) / CLOCKS_PER_SEC);

    delete mod;

    g_print ("~ Save output as `%s'...", opts.Output);
    bool ok = img->SavePNG (opts.Output);

    delete img;
    delete ldb;

    if (ok) {
        g_print ("%s", " done\n");
        return 0;
    } else {
        g_print ("%s", " FAILED\n");
        return -1;
    }

}
