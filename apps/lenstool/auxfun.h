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
        g_print ("%s", "Use your locale-specific number format (e.g. ',' or '.' etc)\n");
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

static void PrintLens (const lfLens *lens, const lfDatabase *ldb, bool verbose = false)
{
    g_print ("  %s / %s\n",
             lf_mlstr_get (lens->Maker),
             lf_mlstr_get (lens->Model));

    if (lens->MinFocal != lens->MaxFocal)
	g_print ("    |- Focal: %g-%gmm\n", lens->MinFocal, lens->MaxFocal);
    else
        g_print ("    |- Focal: %gmm\n", lens->MinFocal);

    if (lens->MaxAperture > 0.f)
	g_print ("    |- Aperture: f/%g - f/%g\n", lens->MinAperture, lens->MaxAperture);
    else
	g_print ("    |- Aperture: f/%g\n", lens->MinAperture);

    g_print ("    |- Center %g/%g\n", lens->CenterX, lens->CenterY);

    g_print ("%s", "    |- Compatible mounts: ");
    const char* const* lm = lens->GetMountNames();
    if (lm)
	for (int j = 0; lm[j]; j++)
	    g_print ("%s, ", ldb->MountName(lm[j]));
    g_print ("%s", "\n");

    g_print ("%s", "    |- Calibrations: [");

    int calibFlags = lens->AvailableModifications(-1.0f);

    if (calibFlags & LF_MODIFY_TCA)
        g_print ("%s", "tca, ");
    if (calibFlags & LF_MODIFY_VIGNETTING)
        g_print ("%s", "vign, ");
    if (calibFlags & LF_MODIFY_DISTORTION)
        g_print ("%s", "dist,");
    g_print ("%s", "]\n");

    if (verbose)
    {
        g_print ("%s", "    |- Calibration data:\n");
        const lfLensCalibrationSet* const* calibrations = lens->GetCalibrationSets();
        if (calibrations != nullptr)
        {
            for (int j = 0; calibrations[j] != NULL; j++)
            {
                g_print ("       |- Crop %g, Aspect ratio %g, [",
                         calibrations[j]->Attributes.CropFactor,
                         calibrations[j]->Attributes.AspectRatio);

                if (calibrations[j]->HasTCA())
                    g_print ("%s", "tca, ");
                if (calibrations[j]->HasVignetting())
                    g_print ("%s", "vign, ");
                if (calibrations[j]->HasDistortion())
                    g_print ("%s", "dist,");
                g_print ("%s", "]\n");
            }
        }
    }
}


#endif
