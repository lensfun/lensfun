/*
    Windows-specific stuff
    Copyright (C) 2008 by Andrew Zabolotny
*/

#include "config.h"
#include <windows.h>
#include <glib/gstdio.h>

#ifndef CONF_DATADIR

char *_lf_get_database_dir ()
{
    static gchar *dir = NULL;

    if (dir)
    {
        g_free (dir);
        dir = NULL;
    }

    HMODULE h = GetModuleHandle ("lensfun.dll");
    if (!h)
        return g_strdup ("");

    char buff [FILENAME_MAX];
    if (GetModuleFileName (h, buff, sizeof (buff)) <= 0)
        return g_strdup ("");

    char *eos = strchr (buff, 0);
    while (eos > buff && eos [-1] != '/' && eos [-1] != ':' && eos [-1] != '\\')
        eos--;
    *eos = 0;

    dir = g_build_filename (buff, CONF_PACKAGE, NULL);

    return dir;
}

#endif
