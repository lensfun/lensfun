/*
    Test for database search functions.
*/

#include "lensfun.h"
#include <glib.h>
#include <getopt.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

static void DisplayVersion ()
{
    g_print ("Lensfun version %d.%d.%d: test database search routines\n",
        LF_VERSION_MAJOR, LF_VERSION_MINOR, LF_VERSION_MICRO);
    g_print ("Copyright (C) 2007 Andrew Zabolotny\n\n");
    g_print ("For distribution rules and conditions of use see the file\n");
    g_print ("COPYING which is part of the distribution.\n");
}

static void DisplayUsage ()
{
    DisplayVersion ();
    g_print ("\nIf no options are given, some standard tests will be run.\n");    
    g_print ("Command-line options:\n\n");
    g_print ("  -L#   --lens=#     Use calibration data for this lens\n");
    g_print ("  -m#   --max=#      Limit the amount results\n");
    g_print ("  -f    --fuzzy      More fuzzy search algorithm\n");
    g_print ("  -V    --version    Display program version and exit\n");
    g_print ("  -h    --help       Display this help text\n");
}

static void PrintMount (const lfMount *mount)
{
    g_print ("Mount: %s\n", lf_mlstr_get (mount->Name));
    if (mount->Compat)
        for (int j = 0; mount->Compat [j]; j++)
            g_print ("\tCompat: %s\n", mount->Compat [j]);
}

static void PrintCamera (const lfCamera *camera, const lfDatabase *ldb)
{
    g_print ("Camera: %s / %s %s%s%s\n",
             lf_mlstr_get (camera->Maker),
             lf_mlstr_get (camera->Model),
             camera->Variant ? "(" : "",
             camera->Variant ? lf_mlstr_get (camera->Variant) : "",
             camera->Variant ? ")" : "");
    g_print ("\tMount: %s\n", lf_db_mount_name (ldb, camera->Mount));
    g_print ("\tCrop factor: %g\n", camera->CropFactor);
}

static void PrintLens (const lfLens *lens, const lfDatabase *ldb)
{
    g_print ("Lens: %s / %s\n",
             lf_mlstr_get (lens->Maker),
             lf_mlstr_get (lens->Model));
    g_print ("\tCrop factor: %g\n", lens->CropFactor);
    g_print ("\tAspect ratio: %g\n", lens->AspectRatio);
    g_print ("\tFocal: %g-%g\n", lens->MinFocal, lens->MaxFocal);
    g_print ("\tAperture: %g-%g\n", lens->MinAperture, lens->MaxAperture);
    g_print ("\tCenter: %g,%g\n", lens->CenterX, lens->CenterY);
    if (lens->Mounts)
        for (int j = 0; lens->Mounts [j]; j++)
            g_print ("\tMount: %s\n", lf_db_mount_name (ldb, lens->Mounts [j]));
}

static void PrintCameras (const lfCamera **cameras, const lfDatabase *ldb, int maxEntries=-1)
{
    if (cameras)
        for (int i = 0; cameras [i]; i++)
        {
            g_print ("--- camera %d: ---\n", i + 1);
            PrintCamera (cameras [i], ldb);
            if ((maxEntries > 0) && (i>=maxEntries-1))
                break;
        }
    else
        g_print ("\t- failed\n");
}

static void PrintLenses (const lfLens **lenses, const lfDatabase *ldb, int maxEntries=-1)
{
    if (lenses)
        for (int i = 0; lenses [i]; i++)
        {
            g_print ("--- lens %d, score %d: ---\n", i + 1, lenses [i]->Score);
            PrintLens (lenses [i], ldb);
            if ((maxEntries > 0) && (i>=maxEntries-1))
                break;
        }
    else
        g_print ("\t- failed\n");
}


int main (int argc, char **argv)
{

    static struct option long_options[] =
    {
        {"lens", required_argument, NULL, 'L'},
        {"max", required_argument, NULL, 'm'},
        {"fuzzy", no_argument, NULL, 'f'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {0, 0, 0, 0}
    };

    static struct
    {
        int         maxResults;
        bool        fuzzySearch;
        const char  *Lens;
    } opts =
    {
        -1,
        0,
        NULL,
    };
    
    setlocale (LC_ALL, "");

    int c;
    while ((c = getopt_long (argc, argv, "L:m:fhV", long_options, NULL)) != EOF)
    {
        switch (c)
        {
            case'L':
                opts.Lens = optarg;
                break;    
            case 'm':
                opts.maxResults = atoi(optarg);
                break;
            case 'f':
                opts.fuzzySearch = LF_SEARCH_LOOSE;
                break;
            case 'h':
                DisplayUsage();
                return 0;
            case 'V':
                DisplayVersion ();
                return 0;
            default:
                return -1;
        }
    }


    lfDatabase *ldb = lf_db_new ();
    ldb->Load ();

    if (opts.Lens == NULL)
    {
        g_print ("No lens name is given\n");
        DisplayUsage();
        ldb->Destroy ();
        return -1;
    }
    g_print (">>> Looking for lens '%s' %d\n", opts.Lens, opts.fuzzySearch);
    const lfLens **lenses = ldb->FindLenses (NULL, NULL, opts.Lens, opts.fuzzySearch);
    PrintLenses (lenses, ldb, opts.maxResults);
    lf_free (lenses);

    ldb->Destroy ();
    return 0;
}
