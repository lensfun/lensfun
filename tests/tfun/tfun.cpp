/*
    Test for database search functions.
*/

#include "lensfun.h"
#include <glib.h>
#include <locale.h>

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
    g_print ("\tFocal: %g-%g\n", lens->MinFocal, lens->MaxFocal);
    g_print ("\tAperture: %g-%g\n", lens->MinAperture, lens->MaxAperture);
    g_print ("\tCenter: %g,%g\n", lens->CenterX, lens->CenterY);
    g_print ("\tCCI: %g/%g/%g\n", lens->RedCCI, lens->GreenCCI, lens->BlueCCI);
    if (lens->Mounts)
        for (int j = 0; lens->Mounts [j]; j++)
            g_print ("\tMount: %s\n", lf_db_mount_name (ldb, lens->Mounts [j]));
}

static void PrintCameras (const lfCamera **cameras, const lfDatabase *ldb)
{
    if (cameras)
        for (int i = 0; cameras [i]; i++)
        {
            g_print ("--- camera %d: ---\n", i + 1);
            PrintCamera (cameras [i], ldb);
        }
    else
        g_print ("\t- failed\n");
}

static void PrintLenses (const lfLens **lenses, const lfDatabase *ldb)
{
    if (lenses)
        for (int i = 0; lenses [i]; i++)
        {
            g_print ("--- lens %d, score %d: ---\n", i + 1, lenses [i]->Score);
            PrintLens (lenses [i], ldb);
        }
    else
        g_print ("\t- failed\n");
}

int main ()
{
    setlocale (LC_ALL, "");

    lfDatabase *ldb = lf_db_new ();
    ldb->Load ();

    g_print (">>> Looking for mount 'pEnTaX K' ...\n");
    const lfMount *mount = ldb->FindMount ("pEnTaX K");
    if (mount)
        PrintMount (mount);
    else
        g_print ("\t- failed\n");

    g_print (">>> Looking for camera 'sOnY CyBeRsHoT' ...\n");
    const lfCamera **cameras = ldb->FindCameras ("sOnY", "CyBeRsHoT");
    PrintCameras (cameras, ldb);
    lf_free (cameras);

    g_print (">>> Looking for Zenit cameras ...\n");
    cameras = ldb->FindCameras ("KMZ", NULL);
    PrintCameras (cameras, ldb);

    g_print (">>> Looking for lenses 'pEntax 50-200 ED'\n");
    const lfLens **lenses = ldb->FindLenses (NULL, "pEntax 50-200 ED");
    PrintLenses (lenses, ldb);
    lf_free (lenses);

    g_print (">>> Looking for 'Nikkor IF' lenses\n");
    lenses = ldb->FindLenses (NULL, "Nikkor IF");
    PrintLenses (lenses, ldb);

    g_print (">>> Saving results into 'tfun-results.xml' ...\n");
    const lfMount *mounts [2] = { mount, NULL };
    lfError e = ldb->Save ("tfun-results.xml", mounts, cameras, lenses);
    if (e == LF_NO_ERROR)
        g_print ("\t- success\n");
    else
        g_print ("\t- failed, error code %d\n", e);

    lf_free (cameras);
    lf_free (lenses);

    ldb->Destroy ();
    return 0;
}
