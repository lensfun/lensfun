#ifndef __LENSTOOL_AUXFUN_H__
#define __LENSTOOL_AUXFUN_H__

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

static void PrintCamera (const lfCamera *camera, const lfDatabase *ldb)
{
    g_print ("  %s / %s %s%s%s\n",
             lf_mlstr_get (camera->Maker),
             lf_mlstr_get (camera->Model),
             camera->Variant ? "(" : "",
             camera->Variant ? lf_mlstr_get (camera->Variant) : "",
             camera->Variant ? ")" : "");
    g_print ("    |- Mount: %s\n", lf_db_mount_name (ldb, camera->Mount));
    g_print ("    |- Crop factor: %g\n", camera->CropFactor);
}

static void PrintLens (const lfLens *lens, const lfDatabase *ldb)
{
    g_print ("  %s / %s\n",
             lf_mlstr_get (lens->Maker),
             lf_mlstr_get (lens->Model));
    g_print ("    |- Crop factor: %g\n", lens->CropFactor);
    g_print ("    |- Aspect ratio: %g\n", lens->AspectRatio);

    if (lens->MinFocal != lens->MaxFocal)
        g_print ("    |- Focal: %g-%gmm\n", lens->MinFocal, lens->MaxFocal);
    else
        g_print ("    |- Focal: %gmm\n", lens->MinFocal);

    g_print ("    |- Min-Aperture: f/%g\n", lens->MinAperture);
    g_print ("    |- Center: %g,%g\n", lens->CenterX, lens->CenterY);
    g_print ("    |- Compatible mounts: ");
    if (lens->Mounts)
        for (int j = 0; lens->Mounts [j]; j++)
            g_print ("%s, ", lf_db_mount_name (ldb, lens->Mounts [j]));    
    g_print ("\n");

    g_print ("    |- Calibration data: ");
    if (lens->CalibTCA)
        g_print ("tca, ");
    if (lens->CalibVignetting)
        g_print ("vign, ");
    if (lens->CalibDistortion)
        g_print ("dist,");
    g_print ("\n");

}


#endif
