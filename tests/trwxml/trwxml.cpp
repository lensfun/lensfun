/*
    Test for XML read/write functions.
*/

#include "lensfun.h"
#include <glib.h>
#include <locale.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

static struct
{
    const char *Program;
    const char *Output;
} opts =
{
    NULL,
    "out.xml"
};

static void DisplayVersion ()
{
    g_print ("lensfun version %d.%d.%d: test XML read/write routines\n",
        LF_VERSION_MAJOR, LF_VERSION_MINOR, LF_VERSION_MICRO);
    g_print ("Copyright (C) 2007 Andrew Zabolotny\n\n");
    g_print ("For distribution rules and conditions of use see the file\n");
    g_print ("COPYING which is part of the distribution.\n");

}

static void DisplayUsage ()
{
    DisplayVersion ();
    g_print ("\nCommand-line options:\n\n");
    g_print ("  -o #  --output=#  Set file name for output XML\n");
    g_print ("  -V    --version   Display program version and exit\n");
    g_print ("  -h    --help      Display this help text\n");
}

int main (int argc, char **argv)
{
    static struct option long_options[] =
    {
        {"output", required_argument, NULL, 'o'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {0, 0, 0, 0}
    };

    setlocale (LC_ALL, "");

    opts.Program = argv [0];

    int c;
    while ((c = getopt_long (argc, argv, "o:hV", long_options, NULL)) != EOF)
    {
        switch (c)
        {
            case 'o':
                opts.Output = optarg;
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

    for (; optind < argc; optind++)
    {
        lfError e = ldb->Load (argv [optind]);
        if (e != LF_NO_ERROR)
            g_print ("Warning: Error %d reading file `%s'\n", e, argv [optind]);
    }

    ldb->Save (opts.Output);

    ldb->Destroy ();
    return 0;
}
