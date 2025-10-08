/*
    Lens database methods implementation
    Copyright (C) 2007 by Andrew Zabolotny
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <locale.h>
#include <glib/gstdio.h>
#include <math.h>
#include <fstream>
#include <algorithm>
#include "windows/mathconstants.h"

#ifdef PLATFORM_WINDOWS
#  include <io.h>
#else
#  include <unistd.h>
#endif

const char* const lfDatabase::UserLocation = g_build_filename (g_get_user_data_dir (),
                                CONF_PACKAGE, NULL);
const char* const lfDatabase::UserUpdatesLocation = g_build_filename (lfDatabase::UserLocation, "updates",
                                   DATABASE_SUBDIR, NULL);
const char* const lfDatabase::SystemLocation = g_build_filename (SYSTEM_DB_PATH, DATABASE_SUBDIR,
                                   NULL);
const char* const lfDatabase::SystemUpdatesLocation = g_build_filename (SYSTEM_DB_UPDATE_PATH,
                                          DATABASE_SUBDIR, NULL);

lfDatabase::lfDatabase ()
{

    // Take care to replace all occurrences with the respective static variables
    // when the deprecated HomeDataDir and UserUpdatesDir variables are removed.
    HomeDataDir = strdup(lfDatabase::UserLocation);
    UserUpdatesDir = strdup(lfDatabase::UserUpdatesLocation);
}

lfDatabase::~lfDatabase ()
{
    free(HomeDataDir);
    free(UserUpdatesDir);

    for (auto m : Mounts)
        delete m;
    for (auto c : Cameras)
        delete c;
    for (auto l : Lenses)
        delete l;
}

long long int lfDatabase::ReadTimestamp (const char *dirname)
{
    long long int timestamp = -1;
    GDir *dir = g_dir_open (dirname, 0, NULL);
    if (dir)
    {
        if (g_dir_read_name (dir))
        {
            gchar *filename = g_build_filename (dirname, "timestamp.txt", NULL);
            std::ifstream timestamp_file (filename);
            g_free (filename);
            if (!timestamp_file.fail ())
                timestamp_file >> timestamp;
            else
                timestamp = 0;
        }
        g_dir_close (dir);
    }

    return timestamp;
}

lfError lfDatabase::Load ()
{
  lfError err = LF_NO_ERROR;

    const auto timestamp_system =
        ReadTimestamp (SystemLocation);
    const auto timestamp_system_updates =
        ReadTimestamp (SystemUpdatesLocation);
    const auto timestamp_user_updates =
        ReadTimestamp (UserUpdatesDir);
    if (timestamp_system > timestamp_system_updates)
        if (timestamp_user_updates > timestamp_system)
            err = Load (UserUpdatesDir);
        else
            err = Load (SystemLocation);
    else
        if (timestamp_user_updates > timestamp_system_updates)
            err = Load (UserUpdatesDir);
        else
            err = Load (SystemUpdatesLocation);

    Load (HomeDataDir);

    return err == LF_NO_ERROR ? LF_NO_ERROR : LF_NO_DATABASE;
}

lfError lfDatabase::Load (const char *pathname)
{

  if (pathname == NULL)
    return Load();

  lfError e;

  if (g_file_test (pathname, G_FILE_TEST_IS_DIR)) {

    // if filename is a directory, try to open all XML files inside
    bool database_found = false;
    GDir *dir = g_dir_open (pathname, 0, NULL);
    if (dir)
    {
        GPatternSpec *ps = g_pattern_spec_new ("*.xml");
        if (ps)
        {
            const gchar *fn;
            while ((fn = g_dir_read_name (dir)))
            {
                size_t sl = strlen (fn);
                if (g_pattern_match (ps, sl, fn, NULL))
                {
                    gchar *ffn = g_build_filename (pathname, fn, NULL);
                    /* Ignore errors */
                    if (Load (ffn) == LF_NO_ERROR)
                        database_found = true;
                    g_free (ffn);
                }
            }
            g_pattern_spec_free (ps);
        }
        g_dir_close (dir);
    }
    e = database_found ? LF_NO_ERROR : LF_NO_DATABASE;

  } else {

    // if filename is not a folder, load the file directly
    gchar *contents;
    gsize length;
    GError *err = NULL;
    if (!g_file_get_contents (pathname, &contents, &length, &err))
        return lfError (err->code == G_FILE_ERROR_ACCES ? -EACCES : -ENOENT);

    e = Load (pathname, contents, length);

    g_free (contents);

  }

  return e;
}

lfError lfDatabase::Load (const char *data, size_t data_size)
{
    return Load("MEMORY", data, data_size);
}

//-----------------------------// XML parser //-----------------------------//

/* Private structure used by XML parse */
typedef struct
{
    lfDatabase *db;
    lfMount *mount;
    lfCamera *camera;
    lfLens *lens;
    lfLensCalibAttributes calib_attr;
    gchar *lang;
    const gchar *stack [16];
    size_t stack_depth;
    const char *errcontext;
} lfParserData;

static bool __chk_no_attrs(const gchar *element_name, const gchar **attribute_names,
                           GError **error)
{
    if (attribute_names [0])
    {
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
                     "The <%s> element cannot have any attributes!\n", element_name);
        return false;
    }
    return true;
}

static void _xml_start_element (GMarkupParseContext *context,
                                const gchar         *element_name,
                                const gchar        **attribute_names,
                                const gchar        **attribute_values,
                                gpointer             user_data,
                                GError             **error)
{
    int i;
    lfParserData *pd = (lfParserData *)user_data;

    if (pd->stack_depth >= sizeof (pd->stack) / sizeof (pd->stack [0]))
    {
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                     "<%s>: very deeply nested element!\n", element_name);
        return;
    }

    const gchar *ctx = NULL;
    if (pd->stack_depth)
        ctx = pd->stack [pd->stack_depth - 1];
    pd->stack [pd->stack_depth++] = element_name;

    if (!strcmp (element_name, "lensdatabase"))
    {
        if (ctx)
        {
        bad_ctx:
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Inappropriate context for <%s>!\n", element_name);
            return;
        }

        int version = 0;
        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "version"))
                version = atoi (attribute_values [i]);
            else
                goto bad_attr;
        if (version < LF_MIN_DATABASE_VERSION)
        {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Database version is %d, but oldest supported is only %d!\n",
                         version, LF_MIN_DATABASE_VERSION);
            return;
        }
        if (version > LF_MAX_DATABASE_VERSION)
        {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Database version is %d, but newest supported is only %d!\n",
                         version, LF_MAX_DATABASE_VERSION);
            return;
        }
    }
    else if (!strcmp (element_name, "mount"))
    {
        if (ctx && !strcmp (ctx, "lensdatabase"))
        {
            pd->mount = new lfMount ();
            if (!__chk_no_attrs(element_name, attribute_names, error)) return;
        }
        else if (ctx &&
                 (!strcmp (ctx, "camera") ||
                  !strcmp (ctx, "lens")))
        {
            if (!__chk_no_attrs(element_name, attribute_names, error)) return;
        }
        else
            goto bad_ctx;
    }
    else if (!strcmp (element_name, "camera"))
    {
        if (!ctx || strcmp (ctx, "lensdatabase"))
            goto bad_ctx;
        pd->camera = new lfCamera ();
        if (!__chk_no_attrs(element_name, attribute_names, error)) return;
    }
    else if (!strcmp (element_name, "lens"))
    {
        if (!ctx || strcmp (ctx, "lensdatabase"))
            goto bad_ctx;
        pd->lens = new lfLens ();
        pd->lens->Type = LF_RECTILINEAR;
        pd->calib_attr.CropFactor  = 1.0;
        pd->calib_attr.AspectRatio = 1.5;
        pd->lens->CenterX = 0.0;
        pd->lens->CenterY = 0.0;
        pd->lens->CropFactor  = 1.0;
        pd->lens->AspectRatio = 1.5;
        if (!__chk_no_attrs(element_name, attribute_names, error)) return;
    }
    else if (!strcmp (element_name, "focal"))
    {
        if (!ctx || strcmp (ctx, "lens") || !pd->lens)
            goto bad_ctx;

        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "min"))
                pd->lens->MinFocal = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "max"))
                pd->lens->MaxFocal = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "value"))
                pd->lens->MinFocal = pd->lens->MaxFocal = atof (attribute_values [i]);
            else
                goto bad_attr;
    }
    else if (!strcmp (element_name, "aperture"))
    {
        if (!ctx || strcmp (ctx, "lens") || !pd->lens)
            goto bad_ctx;

        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "min"))
                pd->lens->MinAperture = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "max"))
                pd->lens->MaxAperture = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "value"))
                pd->lens->MinAperture = pd->lens->MaxAperture = atof (attribute_values [i]);
            else
                goto bad_attr;
    }
    else if (!strcmp (element_name, "center"))
    {
        if (!ctx || strcmp (ctx, "lens") || !pd->lens)
            goto bad_ctx;

        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "x"))
            {
                pd->lens->CenterX = atof (attribute_values [i]);
            }
            else if (!strcmp (attribute_names [i], "y"))
            {
                pd->lens->CenterY = atof (attribute_values [i]);
            }
            else
                goto bad_attr;
    }
    else if (!strcmp (element_name, "type"))
    {
        if (!ctx || strcmp (ctx, "lens") || !pd->lens)
            goto bad_ctx;
        if (!__chk_no_attrs(element_name, attribute_names, error)) return;
    }
    else if (!strcmp (element_name, "calibration"))
    {
        if (!ctx || strcmp (ctx, "lens"))
            goto bad_ctx;

        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "cropfactor"))
                pd->calib_attr.CropFactor = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "aspect-ratio"))
                pd->calib_attr.AspectRatio = atof (attribute_values [i]);
            else
                goto bad_attr;
    }
    else if (!strcmp (element_name, "distortion"))
    {
        if (!ctx || strcmp (ctx, "calibration"))
            goto bad_ctx;

        lfLensCalibDistortion dc;
        memset (&dc, 0, sizeof (dc));
        dc.CalibAttr = pd->calib_attr;
        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "model"))
            {
                if (!strcmp (attribute_values [i], "none"))
                    dc.Model = LF_DIST_MODEL_NONE;
                else if (!strcmp (attribute_values [i], "poly3"))
                    dc.Model = LF_DIST_MODEL_POLY3;
                else if (!strcmp (attribute_values [i], "poly5"))
                    dc.Model = LF_DIST_MODEL_POLY5;
                else if (!strcmp (attribute_values [i], "ptlens"))
                    dc.Model = LF_DIST_MODEL_PTLENS;
                else if (!strcmp (attribute_values [i], "acm"))
                    dc.Model = LF_DIST_MODEL_ACM;
                else
                {
                bad_attr:
                    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                                 "Bad attribute value `%s=%s' for element <%s>!\n",
                                 attribute_names [i], attribute_values [i], element_name);
                    return;
                }
            }
            else if (!strcmp (attribute_names [i], "focal"))
                dc.Focal = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "real-focal"))
                dc.RealFocal = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "a") ||
                     !strcmp (attribute_names [i], "k1"))
                dc.Terms [0] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "b") ||
                     !strcmp (attribute_names [i], "k2"))
                dc.Terms [1] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "c") ||
                     !strcmp (attribute_names [i], "k3"))
                dc.Terms [2] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "k4"))
                dc.Terms [3] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "k5"))
                dc.Terms [4] = atof (attribute_values [i]);
            else
            {
            unk_attr:
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             "Unknown attribute `%s' for element <%s>!\n",
                             attribute_names [i], element_name);
                return;
            }
        if (dc.RealFocal > 0)
            dc.RealFocalMeasured = true;
        else if (dc.Model == LF_DIST_MODEL_PTLENS)
            dc.RealFocal = dc.Focal * (1 - dc.Terms [0] - dc.Terms [1] - dc.Terms [2]);
        else if (dc.Model == LF_DIST_MODEL_POLY3)
            dc.RealFocal = dc.Focal * (1 - dc.Terms [0]);
        else
            dc.RealFocal = dc.Focal;

        pd->lens->AddCalibDistortion (&dc);
    }
    else if (!strcmp (element_name, "tca"))
    {
        if (!ctx || strcmp (ctx, "calibration"))
            goto bad_ctx;

        lfLensCalibTCA tcac;
        memset (&tcac, 0, sizeof (tcac));
        tcac.CalibAttr = pd->calib_attr;
        tcac.Terms [0] = tcac.Terms [1] = 1.0;
        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "model"))
            {
                if (!strcmp (attribute_values [i], "none"))
                    tcac.Model = LF_TCA_MODEL_NONE;
                else if (!strcmp (attribute_values [i], "linear"))
                    tcac.Model = LF_TCA_MODEL_LINEAR;
                else if (!strcmp (attribute_values [i], "poly3"))
                    tcac.Model = LF_TCA_MODEL_POLY3;
                else if (!strcmp (attribute_values [i], "acm"))
                    tcac.Model = LF_TCA_MODEL_ACM;
                else
                    goto bad_attr;
            }
            else if (!strcmp (attribute_names [i], "focal"))
                tcac.Focal = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "kr") ||
                     !strcmp (attribute_names [i], "vr") ||
                     !strcmp (attribute_names [i], "alpha0"))
                tcac.Terms [0] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "kb") ||
                     !strcmp (attribute_names [i], "vb") ||
                     !strcmp (attribute_names [i], "beta0"))
                tcac.Terms [1] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "cr") ||
                     !strcmp (attribute_names [i], "alpha1"))
                tcac.Terms [2] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "cb") ||
                     !strcmp (attribute_names [i], "beta1"))
                tcac.Terms [3] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "br") ||
                     !strcmp (attribute_names [i], "alpha2"))
                tcac.Terms [4] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "bb") ||
                     !strcmp (attribute_names [i], "beta2"))
                tcac.Terms [5] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "alpha3"))
                tcac.Terms [6] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "beta3"))
                tcac.Terms [7] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "alpha4"))
                tcac.Terms [8] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "beta4"))
                tcac.Terms [9] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "alpha5"))
                tcac.Terms [10] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "beta5"))
                tcac.Terms [11] = atof (attribute_values [i]);
            else
                goto unk_attr;

        pd->lens->AddCalibTCA (&tcac);
    }
    else if (!strcmp (element_name, "vignetting"))
    {
        if (!ctx || strcmp (ctx, "calibration"))
            goto bad_ctx;

        lfLensCalibVignetting vc;
        memset (&vc, 0, sizeof (vc));
        vc.CalibAttr = pd->calib_attr;
        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "model"))
            {
                if (!strcmp (attribute_values [i], "none"))
                    vc.Model = LF_VIGNETTING_MODEL_NONE;
                else if (!strcmp (attribute_values [i], "pa"))
                    vc.Model = LF_VIGNETTING_MODEL_PA;
                else if (!strcmp (attribute_values [i], "acm"))
                    vc.Model = LF_VIGNETTING_MODEL_ACM;
                else
                    goto bad_attr;
            }
            else if (!strcmp (attribute_names [i], "focal"))
                vc.Focal = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "aperture"))
                vc.Aperture = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "distance"))
                vc.Distance = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "k1") ||
                     !strcmp (attribute_names [i], "alpha1"))
                vc.Terms [0] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "k2") ||
                     !strcmp (attribute_names [i], "alpha2"))
                vc.Terms [1] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "k3") ||
                     !strcmp (attribute_names [i], "alpha3"))
                vc.Terms [2] = atof (attribute_values [i]);
            else
                goto unk_attr;

        pd->lens->AddCalibVignetting (&vc);
    }
    else if (!strcmp (element_name, "crop"))
    {
        if (!ctx || strcmp (ctx, "calibration"))
            goto bad_ctx;

        lfLensCalibCrop lcc;
        memset (&lcc, 0, sizeof (lcc));
        lcc.CalibAttr = pd->calib_attr;
        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "focal"))
                lcc.Focal = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "mode"))
            {
                if (!strcmp (attribute_values [i], "no_crop"))
                    lcc.CropMode = LF_NO_CROP;
                else if (!strcmp (attribute_values [i], "crop_rectangle"))
                    lcc.CropMode = LF_CROP_RECTANGLE;
                else if (!strcmp (attribute_values [i], "crop_circle"))
                    lcc.CropMode = LF_CROP_CIRCLE;
                else
                {
                    goto bad_attr;
                }
            }
            else if (!strcmp (attribute_names [i], "left"))
                lcc.Crop [0] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "right"))
                lcc.Crop [1] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "top"))
                lcc.Crop [2] = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "bottom"))
                lcc.Crop [3] = atof (attribute_values [i]);
            else
            {
                goto unk_attr;
            }

        pd->lens->AddCalibCrop (&lcc);
    }
    else if (!strcmp (element_name, "field_of_view"))
    {
        if (!ctx || strcmp (ctx, "calibration"))
            goto bad_ctx;

        gint line, col;
        g_markup_parse_context_get_position (context, &line, &col);
        g_warning ("[Lensfun] %s:%d:%d: <field_of_view> tag is deprecated.  Use <real-focal> attribute instead",
                   pd->errcontext, line, col);

        lfLensCalibFov lcf;
        memset (&lcf, 0, sizeof (lcf));
        lcf.CalibAttr = pd->calib_attr;
        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "focal"))
                lcf.Focal = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "fov"))
                lcf.FieldOfView = atof (attribute_values [i]);
            else
            {
                goto unk_attr;
            }

        pd->lens->AddCalibFov (&lcf);
    }
    /* Handle multi-language strings */
    else if (!strcmp (element_name, "maker") ||
             !strcmp (element_name, "model") ||
             !strcmp (element_name, "variant") ||
             !strcmp (element_name, "name"))
    {
        for (i = 0; attribute_names [i]; i++)
            if (!strcmp (attribute_names [i], "lang"))
                _lf_setstr (&pd->lang, attribute_values [i]);
            else
                goto unk_attr;
    }
    else if (!strcmp (element_name, "compat") ||
             !strcmp (element_name, "cropfactor") ||
             !strcmp (element_name, "aspect-ratio"))
    {
        if (!__chk_no_attrs(element_name, attribute_names, error)) return;
    }
    else
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                     "Unknown element <%s>!\n", element_name);
}

static void _xml_end_element (GMarkupParseContext *context,
                              const gchar         *element_name,
                              gpointer             user_data,
                              GError             **error)
{
    (void)context;
    lfParserData *pd = (lfParserData *)user_data;

    g_assert (pd->stack_depth);
    pd->stack_depth--;

    if (!strcmp (element_name, "lensdatabase"))
    {
        /* nothing to do for now */
    }
    else if (!strcmp (element_name, "mount") && pd->mount)
    {
        /* Sanity check */
        if (!pd->mount->Check ())
        {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Invalid mount definition (%s)\n",
                         pd->mount ? pd->mount->Name : "???");
            return;
        }

        pd->db->AddMount(pd->mount);
        pd->mount = NULL;
    }
    else if (!strcmp (element_name, "camera"))
    {
        /* Sanity check */
        if (!pd->camera || !pd->camera->Check ())
        {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Invalid camera definition (%s/%s)\n",
                         pd->camera ? pd->camera->Maker : "???",
                         pd->camera ? pd->camera->Model : "???");
            return;
        }

        pd->db->AddCamera(pd->camera);
        pd->camera = NULL;
    }
    else if (!strcmp (element_name, "lens"))
    {
        /* Sanity check */
        if (!pd->lens || !pd->lens->Check ())
        {
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Invalid lens definition (%s/%s)\n",
                         pd->lens ? pd->lens->Maker : "???",
                         pd->lens ? pd->lens->Model : "???");
            return;
        }

        pd->db->AddLens(pd->lens);
        pd->lens = NULL;
    }
}

static void _xml_text (GMarkupParseContext *context,
                       const gchar         *text,
                       gsize                text_len,
                       gpointer             user_data,
                       GError             **error)
{
    lfParserData *pd = (lfParserData *)user_data;
    const gchar *ctx = g_markup_parse_context_get_element (context);

    while (*text && strchr (" \t\n\r", *text))
        text++;
    if (!*text)
        goto leave;

    if (!strcmp (ctx, "name"))
    {
        if (pd->mount)
            pd->mount->SetName (text, pd->lang);
        else
        {
        bad_ctx:
            g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                         "Wrong context for element <%.*s>\n",
                         int (text_len), text);
            goto leave;
        }
    }
    else if (!strcmp (ctx, "maker"))
    {
        if (pd->camera)
            pd->camera->SetMaker (text, pd->lang);
        else if (pd->lens)
            pd->lens->SetMaker (text, pd->lang);
        else
            goto bad_ctx;
    }
    else if (!strcmp (ctx, "model"))
    {
        if (pd->camera)
            pd->camera->SetModel (text, pd->lang);
        else if (pd->lens)
            pd->lens->SetModel (text, pd->lang);
        else
            goto bad_ctx;
    }
    else if (!strcmp (ctx, "variant"))
    {
        if (pd->camera)
            pd->camera->SetVariant (text, pd->lang);
        else
            goto bad_ctx;
    }
    else if (!strcmp (ctx, "mount"))
    {
        if (pd->camera)
            pd->camera->SetMount (text);
        else if (pd->lens)
            pd->lens->AddMount (text);
        else
            goto bad_ctx;
    }
    else if (!strcmp (ctx, "compat"))
    {
        if (pd->mount)
            pd->mount->AddCompat (text);
        else
            goto bad_ctx;
    }
    else if (!strcmp (ctx, "cropfactor"))
    {
        if (pd->camera)
            pd->camera->CropFactor = atof (text);
        else if (pd->lens)
        {
            pd->calib_attr.CropFactor = atof (text);
            pd->lens->CropFactor = pd->calib_attr.CropFactor;
        }
        else
            goto bad_ctx;
    }
    else if (!strcmp (ctx, "aspect-ratio"))
    {
        if (pd->lens)
        {
            const char *colon = strpbrk (text, ":");
            if (colon)
                pd->calib_attr.AspectRatio = atof (text) / atof (colon + 1);
            else
                pd->calib_attr.AspectRatio = atof (text);
            pd->lens->AspectRatio = pd->calib_attr.AspectRatio;
        }
        else
            goto bad_ctx;
    }
    else if (!strcmp (ctx, "type"))
    {
        if (pd->lens)
        {
            if (!_lf_strcmp (text, "rectilinear"))
                pd->lens->Type = LF_RECTILINEAR;
            else if (!_lf_strcmp (text, "fisheye"))
                pd->lens->Type = LF_FISHEYE;
            else if (!_lf_strcmp (text, "panoramic"))
                pd->lens->Type = LF_PANORAMIC;
            else if (!_lf_strcmp (text, "equirectangular"))
                pd->lens->Type = LF_EQUIRECTANGULAR;
            else if (!_lf_strcmp (text, "orthographic"))
                pd->lens->Type = LF_FISHEYE_ORTHOGRAPHIC;
            else if (!_lf_strcmp (text, "stereographic"))
                pd->lens->Type = LF_FISHEYE_STEREOGRAPHIC;
            else if (!_lf_strcmp (text, "equisolid"))
                pd->lens->Type = LF_FISHEYE_EQUISOLID;
            else if (!_lf_strcmp (text, "fisheye_thoby"))
                pd->lens->Type = LF_FISHEYE_THOBY;
            else
            {
                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             "Invalid lens type `%s' (%s/%s)\n", text,
                             pd->camera ? pd->camera->Maker : "???",
                             pd->camera ? pd->camera->Model : "???");
                return;
            }
        }
        else
            goto bad_ctx;
    }

leave:
    lf_free (pd->lang);
    pd->lang = NULL;
}

lfError lfDatabase::Load (const char *errcontext, const char *data, size_t data_size)
{
    static GMarkupParser gmp =
    {
        _xml_start_element,
        _xml_end_element,
        _xml_text,
        NULL,
        NULL
    };

    /* Temporarily drop numeric format to "C" */
#if defined(PLATFORM_WINDOWS)
    _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    setlocale (LC_NUMERIC, "C");
#else
    auto loc = uselocale((locale_t) 0); // get current local
    auto nloc = newlocale(LC_NUMERIC_MASK, "C", (locale_t) 0);
    uselocale(nloc);
#endif

    lfParserData pd;
    memset (&pd, 0, sizeof (pd));
    pd.db = this;
    pd.errcontext = errcontext;

    GMarkupParseContext *mpc = g_markup_parse_context_new (
        &gmp, (GMarkupParseFlags)0, &pd, NULL);

    GError *err = NULL;
    lfError e = g_markup_parse_context_parse (mpc, data, data_size, &err) ?
        LF_NO_ERROR : LF_WRONG_FORMAT;

    /* Display the parsing error as a warning */
    if (e != LF_NO_ERROR)
    {
        gint line, col;
        g_markup_parse_context_get_position (mpc, &line, &col);
        g_warning ("[Lensfun] %s:%d:%d: %s", errcontext, line, col, err->message);
    }

    g_markup_parse_context_free (mpc);

    /* Restore numeric format */
#if defined(PLATFORM_WINDOWS)
    _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
#else
    uselocale(loc);
    freelocale(nloc);
#endif

    return e;
}

lfError lfDatabase::Save (const char *filename) const
{
    char *output = nullptr;
    size_t len = 0;
    Save(output, len);

    if (!output)
        return lfError (-ENOMEM);

    int fh = g_open (filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    if (fh < 0)
    {
        g_free (output);
        return lfError (-errno);
    }

    int ol = strlen (output);
    ol = (write (fh, output, ol) == ol);
    close (fh);

    g_free (output);

    return ol ? LF_NO_ERROR : lfError (-ENOSPC);
}

lfError lfDatabase::Save (char*& xml, size_t& data_size) const
{
    /* Temporarily drop numeric format to "C" */
#if defined(PLATFORM_WINDOWS)
    _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    setlocale (LC_NUMERIC, "C");
#else
    auto loc = uselocale((locale_t) 0); // get current local
    auto nloc = newlocale(LC_NUMERIC_MASK, "C", (locale_t) 0);
    uselocale(nloc);
#endif

    GString *output = g_string_sized_new (1024);

    g_string_append (output, "<!DOCTYPE lensdatabase SYSTEM \"lensfun-database.dtd\">\n");
    g_string_append (output, "<lensdatabase>\n\n");

    for (auto* m: Mounts)
    {
        g_string_append (output, "\t<mount>\n");
        _lf_xml_printf_mlstr (output, "\t\t", "name",
                                    m->Name);
        if (m->Compat)
            for (int j = 0; m->Compat [j]; j++)
                _lf_xml_printf (output, "\t\t<compat>%s</compat>\n",
                                m->Compat [j]);
        g_string_append (output, "\t</mount>\n\n");
    }

    for (auto* c: Cameras)
    {
        g_string_append (output, "\t<camera>\n");

        _lf_xml_printf_mlstr (output, "\t\t", "maker", c->Maker);
        _lf_xml_printf_mlstr (output, "\t\t", "model", c->Model);
        _lf_xml_printf_mlstr (output, "\t\t", "variant", c->Variant);
        _lf_xml_printf (output, "\t\t<mount>%s</mount>\n",
                        c->Mount);
        _lf_xml_printf (output, "\t\t<cropfactor>%g</cropfactor>\n",
                        c->CropFactor);

        g_string_append (output, "\t</camera>\n\n");
    }


    for (auto* l: Lenses)
    {
        g_string_append (output, "\t<lens>\n");

        _lf_xml_printf_mlstr (output, "\t\t", "maker", l->Maker);
        _lf_xml_printf_mlstr (output, "\t\t", "model", l->Model);
        if (l->MinFocal)
        {
            if (l->MinFocal == l->MaxFocal)
                _lf_xml_printf (output, "\t\t<focal value=\"%g\" />\n",
                                l->MinFocal);
            else
                _lf_xml_printf (output, "\t\t<focal min=\"%g\" max=\"%g\" />\n",
                                l->MinFocal, l->MaxFocal);
        }
        if (l->MinAperture)
        {
            if (l->MinAperture == l->MaxAperture)
                _lf_xml_printf (output, "\t\t<aperture value=\"%g\" />\n",
                                l->MinAperture);
            else
                _lf_xml_printf (output, "\t\t<aperture min=\"%g\" max=\"%g\" />\n",
                                l->MinAperture, l->MaxAperture);
        }

        if (l->Mounts)
            for (int j = 0; l->Mounts [j]; j++)
                _lf_xml_printf (output, "\t\t<mount>%s</mount>\n",
                                l->Mounts [j]);

        if (l->Type != LF_RECTILINEAR)
            _lf_xml_printf (output, "\t\t<type>%s</type>\n",
                            l->Type == LF_FISHEYE ? "fisheye" :
                            l->Type == LF_PANORAMIC ? "panoramic" :
                            l->Type == LF_EQUIRECTANGULAR ? "equirectangular" :
                            l->Type == LF_FISHEYE_ORTHOGRAPHIC ? "orthographic" :
                            l->Type == LF_FISHEYE_STEREOGRAPHIC ? "stereographic" :
                            l->Type == LF_FISHEYE_EQUISOLID ? "equisolid" :
                            l->Type == LF_FISHEYE_THOBY ? "fisheye_thoby" :
                            "rectilinear");

        // save legacy attributes
        if (l->CenterX || l->CenterY)
            _lf_xml_printf (output, "\t\t<center x=\"%g\" y=\"%g\" />\n",
                        l->CenterX, l->CenterY);
        if (l->CropFactor > 0.0)
            _lf_xml_printf (output, "\t\t<cropfactor>%g</cropfactor>\n",
                        l->CropFactor);
        if (l->AspectRatio != 1.5)
            _lf_xml_printf (output, "\t\t<aspect-ratio>%g</aspect-ratio>\n",
                        l->AspectRatio);

        for (auto *calib : l->Calibrations)
        {
            if (calib->Empty())
                continue;

            g_string_append (output, "\t\t<calibration ");
            if (calib->Attributes.CropFactor > 0.0)
                _lf_xml_printf (output, "cropfactor=\"%g\" ", calib->Attributes.CropFactor);
            if (calib->Attributes.AspectRatio != 1.5)
                _lf_xml_printf (output, "aspect-ratio=\"%g\" ", calib->Attributes.AspectRatio);
            g_string_append (output, ">\n");

            for (const lfLensCalibDistortion* cd : calib->CalibDistortion)
            {
                _lf_xml_printf (output, "\t\t\t<distortion focal=\"%g\" ",
                                cd->Focal);
                if (cd->RealFocal > 0)
                _lf_xml_printf (output, "real-focal=\"%g\" ", cd->RealFocal);
                switch (cd->Model)
                {
                    case LF_DIST_MODEL_POLY3:
                        _lf_xml_printf (
                            output, "model=\"poly3\" k1=\"%g\" />\n",
                            cd->Terms [0]);
                        break;

                    case LF_DIST_MODEL_POLY5:
                        _lf_xml_printf (
                            output, "model=\"poly5\" k1=\"%g\" k2=\"%g\" />\n",
                            cd->Terms [0], cd->Terms [1]);
                        break;

                    case LF_DIST_MODEL_PTLENS:
                        _lf_xml_printf (
                            output, "model=\"ptlens\" a=\"%g\" b=\"%g\" c=\"%g\" />\n",
                            cd->Terms [0], cd->Terms [1], cd->Terms [2]);
                        break;

                    case LF_DIST_MODEL_ACM:
                        _lf_xml_printf (
                            output, "model=\"acm\" k1=\"%g\" k2=\"%g\" k3=\"%g\" k4=\"%g\" k5=\"%g\" />\n",
                            cd->Terms [0], cd->Terms [1], cd->Terms [2], cd->Terms [3], cd->Terms [4]);
                        break;

                    default:
                        _lf_xml_printf (output, "model=\"none\" />\n");
                        break;
                }
            }

            for (const lfLensCalibTCA* ctca : calib->CalibTCA)
            {
                _lf_xml_printf (output, "\t\t\t<tca focal=\"%g\" ", ctca->Focal);
                switch (ctca->Model)
                {
                    case LF_TCA_MODEL_LINEAR:
                        _lf_xml_printf (output, "model=\"linear\" kr=\"%g\" kb=\"%g\" />\n",
                                        ctca->Terms [0], ctca->Terms [1]);
                        break;

                    case LF_TCA_MODEL_POLY3:
                        _lf_xml_printf (output, "model=\"poly3\" vr=\"%g\" vb=\"%g\" "
                                        "cr=\"%g\" cb=\"%g\" br=\"%g\" bb=\"%g\" />\n",
                                        ctca->Terms [0], ctca->Terms [1], ctca->Terms [2],
                                        ctca->Terms [3], ctca->Terms [4], ctca->Terms [5]);
                        break;

                    case LF_TCA_MODEL_ACM:
                        _lf_xml_printf (output, "model=\"acm\" alpha0=\"%g\" beta0=\"%g\" "
                                        "alpha1=\"%g\" beta1=\"%g\" alpha2=\"%g\" "
                                        "beta2=\"%g\" alpha3=\"%g\" beta3=\"%g\" "
                                        "alpha4=\"%g\" beta4=\"%g\" alpha5=\"%g\" "
                                        "beta5=\"%g\" />\n",
                                        ctca->Terms [0], ctca->Terms [1], ctca->Terms [2],
                                        ctca->Terms [3], ctca->Terms [4], ctca->Terms [5],
                                        ctca->Terms [6], ctca->Terms [7], ctca->Terms [8],
                                        ctca->Terms [9], ctca->Terms [10], ctca->Terms [11]);
                        break;

                    default:
                        _lf_xml_printf (output, "model=\"none\" />\n");
                        break;
                }
            }

            for (const lfLensCalibVignetting* cv : calib->CalibVignetting)
            {
                _lf_xml_printf (output, "\t\t\t<vignetting focal=\"%g\" aperture=\"%g\" distance=\"%g\" ",
                                cv->Focal, cv->Aperture, cv->Distance);
                switch (cv->Model)
                {
                    case LF_VIGNETTING_MODEL_PA:
                        _lf_xml_printf (output, "model=\"pa\" k1=\"%g\" k2=\"%g\" k3=\"%g\" />\n",
                                        cv->Terms [0], cv->Terms [1], cv->Terms [2]);
                        break;

                    case LF_VIGNETTING_MODEL_ACM:
                        _lf_xml_printf (output, "model=\"acm\" alpha1=\"%g\" alpha2=\"%g\" alpha3=\"%g\" />\n",
                                        cv->Terms [0], cv->Terms [1], cv->Terms [2]);
                        break;

                    default:
                        _lf_xml_printf (output, "model=\"none\" />\n");
                        break;
                }
            }

            for (const lfLensCalibCrop* lcc: calib->CalibCrop)
            {
                _lf_xml_printf (output, "\t\t\t<crop focal=\"%g\" ",
                                lcc->Focal);
                switch (lcc->CropMode)
                {
                    case LF_CROP_RECTANGLE:
                        _lf_xml_printf (
                            output, "mode=\"crop_rectangle\" left=\"%g\" right=\"%g\" top=\"%g\" bottom=\"%g\" />\n",
                            lcc->Crop [0], lcc->Crop [1], lcc->Crop [2], lcc->Crop [3]);
                        break;

                    case LF_CROP_CIRCLE:
                        _lf_xml_printf (
                            output, "mode=\"crop_circle\" left=\"%g\" right=\"%g\" top=\"%g\" bottom=\"%g\" />\n",
                            lcc->Crop [0], lcc->Crop [1], lcc->Crop [2], lcc->Crop [3]);
                        break;

                    case LF_NO_CROP:
                    default:
                        _lf_xml_printf (output, "mode=\"no_crop\" />\n");
                        break;
                }
            }

            for (const lfLensCalibFov* lcf: calib->CalibFov)
            {
                if (lcf->FieldOfView > 0)
                {
                    _lf_xml_printf (output, "\t\t\t<field_of_view focal=\"%g\" fov=\"%g\" />\n",
                        lcf->Focal, lcf->FieldOfView);
                };
            }

            g_string_append (output, "\t\t</calibration>\n");
        }
        g_string_append (output, "\t</lens>\n\n");
    }

    g_string_append (output, "</lensdatabase>\n");

    /* Restore numeric format */
#if defined(PLATFORM_WINDOWS)
    _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
#else
    uselocale(loc);
    freelocale(nloc);
#endif

    data_size = output->len;    
    xml = g_string_free (output, FALSE);

    return LF_NO_ERROR;
}

int __find_camera_compare (lfCamera *a, lfCamera *b)
{    
    if (a->Maker && b->Maker)
    {
        int cmp = _lf_strcmp (a->Maker, b->Maker);
        if (cmp != 0)
            return cmp;
    }

    if (a->Model && b->Model)
        return _lf_strcmp (a->Model, b->Model);

    return 0;
}

bool _lf_sort_camera_compare (lfCamera* a, lfCamera* b)
{
    return __find_camera_compare(a, b) < 0;
}

const lfCamera **lfDatabase::FindCameras (const char *maker, const char *model) const
{
    if (maker && !*maker)
        maker = NULL;
    if (model && !*model)
        model = NULL;

    lfCamera tc;
    tc.SetMaker (maker);
    tc.SetModel (model);

    std::vector<lfCamera*> search_result;
    for (auto c: Cameras)
    {
        if (!__find_camera_compare(c, &tc))
            search_result.push_back(c);
    }
    std::sort(search_result.begin(), search_result.end(), _lf_sort_camera_compare);


    if (search_result.size() > 0)
    {
        const lfCamera **ret = g_new (const lfCamera *, search_result.size() + 1);
        memcpy(ret, search_result.data(), search_result.size() * sizeof(lfCamera*));

        // Add a NULL to mark termination of the array
        ret[search_result.size()] = NULL;

        return ret;
    }
    else
        return NULL;
}

bool _lf_compare_camera_score (lfCamera *a, lfCamera *b)
{

    return a->Score > b->Score;
}

const lfCamera **lfDatabase::FindCamerasExt (const char *maker, const char *model,
                                             int sflags) const
{
    if (maker && !*maker)
        maker = NULL;
    if (model && !*model)
        model = NULL;

    lfFuzzyStrCmp fcmaker (maker, (sflags & LF_SEARCH_LOOSE) == 0);
    lfFuzzyStrCmp fcmodel (model, (sflags & LF_SEARCH_LOOSE) == 0);

    std::vector<lfCamera*> search_result;
    for (auto dbcam: Cameras)
    {
        int score1 = 0, score2 = 0;
        if ((!maker || (score1 = fcmaker.Compare (dbcam->Maker))) &&
            (!model || (score2 = fcmodel.Compare (dbcam->Model))))
        {
            dbcam->Score = score1 + score2;
            search_result.push_back(dbcam);
        }
    }

    std::sort(search_result.begin(), search_result.end(), _lf_compare_camera_score);

    if (search_result.size() > 0)
    {
        const lfCamera **ret = g_new (const lfCamera *, search_result.size() + 1);
        memcpy(ret, search_result.data(), search_result.size() * sizeof(lfCamera*));

        // Add a NULL to mark termination of the array
        ret[search_result.size()] = NULL;

        return ret;
    }
    else
        return NULL;
}

const lfCamera *const *lfDatabase::GetCameras ()
{
    size_t size = Cameras.size();
    Cameras.reserve(size + 1);
    Cameras.data()[size] = NULL;
    return Cameras.data();
}

static int _lf_compare_num (float a, float b)
{
    if (!a || !b)
        return 0; // neutral

    float r = a / b;
    if (r <= 0.99 || r >= 1.01)
        return -1; // strong no
    return +1; // strong yes
}

int lfDatabase::MatchScore (const lfLens *pattern, const lfLens *match, const lfCamera *camera,
                            void *fuzzycmp, const char* const* compat_mounts) const
{
    int score = 0;

    // Compare numeric fields first since that's easy.

    if (camera != NULL && !match->Calibrations.empty()) {

        int crop_score = 0;

        for (auto& mc : match->Calibrations)
        {
            if (camera->CropFactor > 0.01 && camera->CropFactor < mc->Attributes.CropFactor * 0.96)
                continue;

            if (camera->CropFactor >= mc->Attributes.CropFactor * 1.41)
                crop_score = std::max(2, crop_score);
            else if (camera->CropFactor >= mc->Attributes.CropFactor * 1.31)
                crop_score = std::max(4, crop_score);
            else if (camera->CropFactor >= mc->Attributes.CropFactor * 1.21)
                crop_score = std::max(6, crop_score);
            else if (camera->CropFactor >= mc->Attributes.CropFactor * 1.11)
                crop_score = std::max(8, crop_score);
            else if (camera->CropFactor >= mc->Attributes.CropFactor * 1.01)
                crop_score = std::max(10, crop_score);
            else if (camera->CropFactor >= mc->Attributes.CropFactor)
                crop_score = std::max(5, crop_score);
            else if (camera->CropFactor >= mc->Attributes.CropFactor * 0.96)
                crop_score = std::max(3, crop_score);
        }

        if (crop_score == 0)
            return 0;
        else
            score += crop_score;
    }
    switch (_lf_compare_num (pattern->MinFocal, match->MinFocal))
    {
        case -1:
            return 0;

        case +1:
            score += 10;
            break;
    }

    switch (_lf_compare_num (pattern->MaxFocal, match->MaxFocal))
    {
        case -1:
            return 0;

        case +1:
            score += 10;
            break;
    }

    switch (_lf_compare_num (pattern->MinAperture, match->MinAperture))
    {
        case -1:
            return 0;

        case +1:
            score += 10;
            break;
    }

    switch (_lf_compare_num (pattern->MaxAperture, match->MaxAperture))
    {
        case -1:
            return 0;

        case +1:
            score += 10;
            break;
    }

    if (compat_mounts && !compat_mounts [0])
        compat_mounts = NULL;

    // Check the lens mount, if specified
    if (match->Mounts && (camera || compat_mounts))
    {
        bool matching_mount_found = false;

        if (camera && camera->Mount)
            for (int j = 0; match->Mounts [j]; j++)
                if (!_lf_strcmp (camera->Mount, match->Mounts [j]))
                {
                    matching_mount_found = true;
                    score += 10;
                    goto exit_mount_search;
                }

        if (compat_mounts)
            for (int i = 0; compat_mounts [i]; i++)
                for (int j = 0; match->Mounts [j]; j++)
                    if (!_lf_strcmp (compat_mounts [i], match->Mounts [j]))
                    {
                        matching_mount_found = true;
                        score += 9;
                        goto exit_mount_search;
                    }

    exit_mount_search:
        if (!matching_mount_found)
            return 0;
    }

    // If maker is specified, check it using our patented _lf_strcmp(tm) technology
    if (pattern->Maker && match->Maker)
    {
        if (_lf_mlstrcmp (pattern->Maker, match->Maker) != 0)
            return 0; // Bah! different maker.
        else
            score += 10; // Good doggy, here's a cookie
    }

    // And now the most complex part - compare models
    if (pattern->Model && match->Model)
    {

        int _score = static_cast<lfFuzzyStrCmp*>(fuzzycmp)->Compare (match->Model);
        if (!_score)
            return 0; // Model does not match
        _score = (_score * 4) / 10;
        if (!_score)
            _score = 1;
        score += _score;
    }

    return score;
}

static bool _lf_compare_lens_score (gconstpointer a, gconstpointer b)
{
    lfLens *i1 = (lfLens *)a;
    lfLens *i2 = (lfLens *)b;

    return i2->Score < i1->Score;
}

static gint _lf_compare_lens_details (gconstpointer a, gconstpointer b)
{
    // Actually, we not only sort by focal length, but by MinFocal, MaxFocal,
    // MinAperature, Maker, and Model -- in this order of priorities.
    lfLens *i1 = (lfLens *)a;
    lfLens *i2 = (lfLens *)b;

    int cmp = _lf_lens_parameters_compare (i1, i2);
    if (cmp != 0)
        return cmp;

    return _lf_lens_name_compare (i1, i2);
}

bool _lf_sort_lens_details (lfLens* a, lfLens* b)
{
    return _lf_compare_lens_details(a, b) < 0;
}


const lfLens **lfDatabase::FindLenses (const lfCamera *camera,
                                       const char *maker, const char *model,
                                       int sflags) const
{
    if (maker && !*maker)
        maker = NULL;
    if (model && !*model)
        model = NULL;

    lfLens lens;
    lens.SetMaker (maker);
    lens.SetModel (model);

    // Guess lens parameters from lens model name
    lens.GuessParameters ();

    std::vector<lfLens*>  search_res;

    lfFuzzyStrCmp fc (lens.Model, (sflags & LF_SEARCH_LOOSE) == 0);

    // get a list of compatible mounts
    const char* const* compat_mounts = NULL;
    if ((camera != NULL) && (camera->Mount))
    {
        const lfMount* m = FindMount(camera->Mount);
        if (m)
            compat_mounts = m->GetCompats();
    }

    int score;
    const bool sort_and_uniquify = (sflags & LF_SEARCH_SORT_AND_UNIQUIFY) != 0;

    for (auto dblens: Lenses)
    {
        if ((score = MatchScore (&lens, dblens, camera, &fc, compat_mounts)) > 0)
        {
            dblens->Score = score;
            if (sort_and_uniquify)
            {
                bool already = false;
                for (auto &s: search_res)
                {
                    if (!_lf_lens_name_compare (s, dblens))
                    {
                        if (dblens->Score > s->Score)
                            s = dblens;
                        already = true;
                        break;
                    }
                }
                if (!already)
                    search_res.push_back(dblens);
            }
            else
                search_res.push_back(dblens);
        }
    }

    if (sort_and_uniquify)
        std::sort(search_res.begin(), search_res.end(), _lf_sort_lens_details);
    else
        std::sort(search_res.begin(), search_res.end(), _lf_compare_lens_score);

    if (search_res.size() > 0)
    {
        const lfLens **ret = g_new (const lfLens *, search_res.size() + 1);
        memcpy(ret, search_res.data(), search_res.size() * sizeof(lfLens*));

        // Add a NULL to mark termination of the array
        ret[search_res.size()] = NULL;
        return ret;
    }
    else
        return NULL;
}


const lfLens *const *lfDatabase::GetLenses ()
{
    size_t size = Lenses.size();
    Lenses.reserve(size + 1);
    Lenses.data()[size] = NULL;
    return Lenses.data();
}

const lfMount *lfDatabase::FindMount (const char *mount) const
{
    lfMount tm;
    tm.SetName (mount);

    for (const auto m: Mounts)
    {
        if (*m == tm)
            return m;
    }

    return NULL;
}

const char *lfDatabase::MountName (const char *mount) const
{
    const lfMount *m = FindMount (mount);
    if (!m)
        return mount;
    return lf_mlstr_get (m->Name);
}

const lfMount * const *lfDatabase::GetMounts ()
{
    size_t size = Mounts.size();
    Mounts.reserve(size + 1);
    Mounts.data()[size] = NULL;
    return Mounts.data();
}

void lfDatabase::AddMount (lfMount *mount)
{
    Mounts.push_back(mount);
}

void lfDatabase::AddCamera (lfCamera *camera)
{
    Cameras.push_back(camera);
}

void lfDatabase::AddLens (lfLens *lens)
{
    Lenses.push_back(lens);
}

//---------------------------// The C interface //---------------------------//

const char* const lf_db_system_location = lfDatabase::SystemLocation;
const char* const lf_db_system_updates_location = lfDatabase::SystemUpdatesLocation;
const char* const lf_db_user_location = lfDatabase::UserLocation;
const char* const lf_db_user_updates_location = lfDatabase::UserUpdatesLocation;

lfDatabase *lf_db_new ()
{
    return new lfDatabase ();
}

lfDatabase *lf_db_create ()
{
    return new lfDatabase ();
}


void lf_db_destroy (lfDatabase *db)
{
    delete db;
}

long int lf_db_read_timestamp (const char *dirname)
{
    return lfDatabase::ReadTimestamp(dirname);
}

lfError lf_db_load (lfDatabase *db)
{
    return db->Load ();
}

lfError lf_db_load_file (lfDatabase *db, const char *filename)
{
    return db->Load (filename);
}

cbool lf_db_load_directory (lfDatabase *db, const char *dirname)
{
    return db->Load (dirname);
}

lfError lf_db_load_path (lfDatabase *db, const char *pathname)
{
    return db->Load (pathname);
}

lfError lf_db_load_data (lfDatabase *db, const char *, const char *data, size_t data_size)
{
    return db->Load (data, data_size);
}

lfError lf_db_load_str (lfDatabase *db, const char *xml, size_t data_size)
{
    return db->Load (xml, data_size);
}

lfError lf_db_save_all (const lfDatabase *db, const char *filename)
{
    return db->Save (filename);
}

lfError lf_db_save_str (const lfDatabase *db, char **xml, size_t* data_size)
{
    return db->Save (*xml, *data_size);
}

const lfCamera **lf_db_find_cameras (const lfDatabase *db,
                                     const char *maker, const char *model)
{
    return db->FindCameras (maker, model);
}

const lfCamera **lf_db_find_cameras_ext (
    const lfDatabase *db, const char *maker, const char *model, int sflags)
{
    return db->FindCamerasExt (maker, model, sflags);
}

const lfCamera *const *lf_db_get_cameras (lfDatabase *db)
{
    return db->GetCameras ();
}

const lfLens **lf_db_find_lenses_hd (const lfDatabase *db, const lfCamera *camera,
                                     const char *maker, const char *lens, int sflags)
{
    return db->FindLenses (camera, maker, lens, sflags);
}

const lfLens **lf_db_find_lenses (const lfDatabase *db, const lfCamera *camera,
                                    const char *maker, const char *lens, int sflags)
{
    return db->FindLenses (camera, maker, lens, sflags);
}


const lfLens *const *lf_db_get_lenses (lfDatabase *db)
{
    return db->GetLenses ();
}

const lfMount *lf_db_find_mount (const lfDatabase *db, const char *mount)
{
    return db->FindMount (mount);
}

const char *lf_db_mount_name (const lfDatabase *db, const char *mount)
{
    return db->MountName (mount);
}

const lfMount * const *lf_db_get_mounts (lfDatabase *db)
{
    return db->GetMounts ();
}
