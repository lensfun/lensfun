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

    // Take care to replace all occurences with the respective static variables
    // when the deprecated HomeDataDir and UserUpdatesDir variables are removed.
    HomeDataDir = strdup(lfDatabase::UserLocation);
    UserUpdatesDir = strdup(lfDatabase::UserUpdatesLocation);

    Mounts = g_ptr_array_new ();
    g_ptr_array_add ((GPtrArray *)Mounts, NULL);
    Cameras = g_ptr_array_new ();
    g_ptr_array_add ((GPtrArray *)Cameras, NULL);
    Lenses = g_ptr_array_new ();
    g_ptr_array_add ((GPtrArray *)Lenses, NULL);
}

lfDatabase::~lfDatabase ()
{
    size_t i;
    for (i = 0; i < ((GPtrArray *)Mounts)->len - 1; i++)
         delete static_cast<lfMount *> (g_ptr_array_index ((GPtrArray *)Mounts, i));
    g_ptr_array_free ((GPtrArray *)Mounts, TRUE);

    for (i = 0; i < ((GPtrArray *)Cameras)->len - 1; i++)
         delete static_cast<lfCamera *> (g_ptr_array_index ((GPtrArray *)Cameras, i));
    g_ptr_array_free ((GPtrArray *)Cameras, TRUE);

    for (i = 0; i < ((GPtrArray *)Lenses)->len - 1; i++)
         delete static_cast<lfLens *> (g_ptr_array_index ((GPtrArray *)Lenses, i));
    g_ptr_array_free ((GPtrArray *)Lenses, TRUE);
}

lfDatabase *lfDatabase::Create ()
{
    return new lfDatabase ();
}

void lfDatabase::Destroy ()
{
    delete this;
}

long int lfDatabase::ReadTimestamp (const char *dirname)
{
    long int timestamp = -1;
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

bool lfDatabase::LoadDirectory (const gchar *dirname)
{
    return Load(dirname) == LF_NO_ERROR;
}

lfError lfDatabase::Load ()
{
  lfError err = LF_NO_ERROR;

    bool database_found = false;

    const int timestamp_system =
        ReadTimestamp (SystemLocation);
    const int timestamp_system_updates =
        ReadTimestamp (SystemUpdatesLocation);
    const int timestamp_user_updates =
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

//-----------------------------// XML parser //-----------------------------//

/* Private structure used by XML parse */
typedef struct
{
    lfDatabase *db;
    lfMount *mount;
    lfCamera *camera;
    lfLens *lens;
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
                         "Inappropiate context for <%s>!\n", element_name);
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
        // Set default values for lens database entries.  This way, they are
        // still 0 (aka "unknown") for dummy lfLens instances created ad hoc
        // for search matching.
        pd->lens->Type = LF_RECTILINEAR;
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
                pd->lens->CenterX = atof (attribute_values [i]);
            else if (!strcmp (attribute_names [i], "y"))
                pd->lens->CenterY = atof (attribute_values [i]);
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
        if (!__chk_no_attrs(element_name, attribute_names, error)) return;
    }
    else if (!strcmp (element_name, "distortion"))
    {
        if (!ctx || strcmp (ctx, "calibration"))
            goto bad_ctx;

        lfLensCalibDistortion dc;
        memset (&dc, 0, sizeof (dc));
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
            pd->lens->CropFactor = atof (text);
        else
            goto bad_ctx;
    }
    else if (!strcmp (ctx, "aspect-ratio"))
    {
        if (pd->lens)
        {
            const char *colon = strpbrk (text, ":");
            if (colon)
                pd->lens->AspectRatio = atof (text) / atof (colon + 1);
            else
                pd->lens->AspectRatio = atof (text);
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
    char *old_numeric = setlocale (LC_NUMERIC, NULL);
    old_numeric = strdup(old_numeric);
    setlocale(LC_NUMERIC,"C");

    /* eek! GPtrArray does not have a method to insert a pointer
     into middle of the array... We have to remove the trailing
     NULL and re-append it after loading ... */
    g_ptr_array_remove_index_fast ((GPtrArray *)Mounts, ((GPtrArray *)Mounts)->len - 1);
    g_ptr_array_remove_index_fast ((GPtrArray *)Cameras, ((GPtrArray *)Cameras)->len - 1);
    g_ptr_array_remove_index_fast ((GPtrArray *)Lenses, ((GPtrArray *)Lenses)->len - 1);

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

    /* Re-add the trailing NULL */
    g_ptr_array_add ((GPtrArray *)Mounts, NULL);
    g_ptr_array_add ((GPtrArray *)Cameras, NULL);
    g_ptr_array_add ((GPtrArray *)Lenses, NULL);

    /* Restore numeric format */
    setlocale (LC_NUMERIC, old_numeric);
    free(old_numeric);

    return e;
}

lfError lfDatabase::Save (const char *filename) const
{
    return Save (filename,
                 (lfMount **)((GPtrArray *)Mounts)->pdata,
                 (lfCamera **)((GPtrArray *)Cameras)->pdata,
                 (lfLens **)((GPtrArray *)Lenses)->pdata);
}

lfError lfDatabase::Save (const char *filename,
                          const lfMount *const *mounts,
                          const lfCamera *const *cameras,
                          const lfLens *const *lenses) const
{
    char *output = Save (mounts, cameras, lenses);
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

char *lfDatabase::Save (const lfMount *const *mounts,
                        const lfCamera *const *cameras,
                        const lfLens *const *lenses)
{
    /* Temporarily drop numeric format to "C" */
    char *old_numeric = setlocale (LC_NUMERIC, NULL);
    old_numeric = strdup(old_numeric);
    setlocale(LC_NUMERIC,"C");

    int i, j;
    GString *output = g_string_sized_new (1024);

    g_string_append (output, "<!DOCTYPE lensdatabase SYSTEM \"lensfun-database.dtd\">\n");
    g_string_append (output, "<lensdatabase>\n\n");

    if (mounts)
        for (i = 0; mounts [i]; i++)
        {
            g_string_append (output, "\t<mount>\n");
            _lf_xml_printf_mlstr (output, "\t\t", "name",
                                        mounts [i]->Name);
            if (mounts [i]->Compat)
                for (j = 0; mounts [i]->Compat [j]; j++)
                    _lf_xml_printf (output, "\t\t<compat>%s</compat>\n",
                                    mounts [i]->Compat [j]);
            g_string_append (output, "\t</mount>\n\n");
        }

    if (cameras)
        for (i = 0; cameras [i]; i++)
        {
            g_string_append (output, "\t<camera>\n");

            _lf_xml_printf_mlstr (output, "\t\t", "maker", cameras [i]->Maker);
            _lf_xml_printf_mlstr (output, "\t\t", "model", cameras [i]->Model);
            _lf_xml_printf_mlstr (output, "\t\t", "variant", cameras [i]->Variant);
            _lf_xml_printf (output, "\t\t<mount>%s</mount>\n",
                            cameras [i]->Mount);
            _lf_xml_printf (output, "\t\t<cropfactor>%g</cropfactor>\n",
                            cameras [i]->CropFactor);

            g_string_append (output, "\t</camera>\n\n");
        }

    if (lenses)
        for (i = 0; lenses [i]; i++)
        {
            g_string_append (output, "\t<lens>\n");

            _lf_xml_printf_mlstr (output, "\t\t", "maker", lenses [i]->Maker);
            _lf_xml_printf_mlstr (output, "\t\t", "model", lenses [i]->Model);
            if (lenses [i]->MinFocal)
            {
                if (lenses [i]->MinFocal == lenses [i]->MaxFocal)
                    _lf_xml_printf (output, "\t\t<focal value=\"%g\" />\n",
                                    lenses [i]->MinFocal);
                else
                    _lf_xml_printf (output, "\t\t<focal min=\"%g\" max=\"%g\" />\n",
                                    lenses [i]->MinFocal, lenses [i]->MaxFocal);
            }
            if (lenses [i]->MinAperture)
            {
                if (lenses [i]->MinAperture == lenses [i]->MaxAperture)
                    _lf_xml_printf (output, "\t\t<aperture value=\"%g\" />\n",
                                    lenses [i]->MinAperture);
                else
                    _lf_xml_printf (output, "\t\t<aperture min=\"%g\" max=\"%g\" />\n",
                                    lenses [i]->MinAperture, lenses [i]->MaxAperture);
            }

            if (lenses [i]->Mounts)
                for (j = 0; lenses [i]->Mounts [j]; j++)
                    _lf_xml_printf (output, "\t\t<mount>%s</mount>\n",
                                    lenses [i]->Mounts [j]);

            if (lenses [i]->Type != LF_RECTILINEAR)
                _lf_xml_printf (output, "\t\t<type>%s</type>\n",
                                lenses [i]->Type == LF_FISHEYE ? "fisheye" :
                                lenses [i]->Type == LF_PANORAMIC ? "panoramic" :
                                lenses [i]->Type == LF_EQUIRECTANGULAR ? "equirectangular" :
                                lenses [i]->Type == LF_FISHEYE_ORTHOGRAPHIC ? "orthographic" :
                                lenses [i]->Type == LF_FISHEYE_STEREOGRAPHIC ? "stereographic" :
                                lenses [i]->Type == LF_FISHEYE_EQUISOLID ? "equisolid" :
                                lenses [i]->Type == LF_FISHEYE_THOBY ? "fisheye_thoby" :
                                "rectilinear");

            if (lenses [i]->CenterX || lenses [i]->CenterY)
                _lf_xml_printf (output, "\t\t<center x=\"%g\" y=\"%g\" />\n",
                                lenses [i]->CenterX, lenses [i]->CenterY);

            _lf_xml_printf (output, "\t\t<cropfactor>%g</cropfactor>\n",
                            lenses [i]->CropFactor);
            _lf_xml_printf (output, "\t\t<aspect-ratio>%g</aspect-ratio>\n",
                            lenses [i]->AspectRatio);

            if (lenses [i]->CalibDistortion || lenses [i]->CalibTCA ||
                lenses [i]->CalibVignetting || lenses [i]->CalibCrop ||
                lenses [i]->CalibFov)
                g_string_append (output, "\t\t<calibration>\n");

            if (lenses [i]->CalibDistortion)
            {
                for (j = 0; lenses [i]->CalibDistortion [j]; j++)
                {
                    lfLensCalibDistortion *cd = lenses [i]->CalibDistortion [j];

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
            }

            if (lenses [i]->CalibTCA)
            {
                for (j = 0; lenses [i]->CalibTCA [j]; j++)
                {
                    lfLensCalibTCA *ctca = lenses [i]->CalibTCA [j];
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
            }

            if (lenses [i]->CalibVignetting)
            {
                for (j = 0; lenses [i]->CalibVignetting [j]; j++)
                {
                    lfLensCalibVignetting *cv = lenses [i]->CalibVignetting [j];
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
            }

            if (lenses [i]->CalibCrop)
            {
                for (j = 0; lenses [i]->CalibCrop [j]; j++)
                {
                    lfLensCalibCrop *lcc = lenses [i]->CalibCrop [j];

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
            }

            if (lenses [i]->CalibFov)
            {
                for (j = 0; lenses [i]->CalibFov [j]; j++)
                {
                    lfLensCalibFov *lcf = lenses [i]->CalibFov [j];

                    if (lcf->FieldOfView>0)
                    {
                        _lf_xml_printf (output, "\t\t\t<field_of_view focal=\"%g\" fov=\"%g\" />\n",
                            lcf->Focal, lcf->FieldOfView);
                    };
                }
            }

            if (lenses [i]->CalibDistortion || lenses [i]->CalibTCA ||
                lenses [i]->CalibVignetting || lenses [i]->CalibCrop ||
                lenses [i]->CalibFov)
                g_string_append (output, "\t\t</calibration>\n");

            g_string_append (output, "\t</lens>\n\n");
        }

    g_string_append (output, "</lensdatabase>\n");

    /* Restore numeric format */
    setlocale (LC_NUMERIC, old_numeric);
    free(old_numeric);

    return g_string_free (output, FALSE);
}

static gint __find_camera_compare (gconstpointer a, gconstpointer b)
{
    lfCamera *i1 = (lfCamera *)a;
    lfCamera *i2 = (lfCamera *)b;

    if (i1->Maker && i2->Maker)
    {
        int cmp = _lf_strcmp (i1->Maker, i2->Maker);
        if (cmp != 0)
            return cmp;
    }

    if (i1->Model && i2->Model)
        return _lf_strcmp (i1->Model, i2->Model);

    return 0;
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
    int idx = _lf_ptr_array_find_sorted ((GPtrArray *)Cameras, &tc, __find_camera_compare);
    if (idx < 0)
        return NULL;

    guint idx1 = idx;
    while (idx1 > 0 &&
           __find_camera_compare (g_ptr_array_index ((GPtrArray *)Cameras, idx1 - 1), &tc) == 0)
        idx1--;

    guint idx2 = idx;
    while (++idx2 < ((GPtrArray *)Cameras)->len - 1 &&
           __find_camera_compare (g_ptr_array_index ((GPtrArray *)Cameras, idx2), &tc) == 0)
        ;

    const lfCamera **ret = g_new (const lfCamera *, idx2 - idx1 + 1);
    for (guint i = idx1; i < idx2; i++)
        ret [i - idx1] = (lfCamera *)g_ptr_array_index ((GPtrArray *)Cameras, i);
    ret [idx2 - idx1] = NULL;
    return ret;
}

static gint _lf_compare_camera_score (gconstpointer a, gconstpointer b)
{
    lfCamera *i1 = (lfCamera *)a;
    lfCamera *i2 = (lfCamera *)b;

    return i2->Score - i1->Score;
}

const lfCamera **lfDatabase::FindCamerasExt (const char *maker, const char *model,
                                             int sflags) const
{
    if (maker && !*maker)
        maker = NULL;
    if (model && !*model)
        model = NULL;

    GPtrArray *ret = g_ptr_array_new ();

    lfFuzzyStrCmp fcmaker (maker, (sflags & LF_SEARCH_LOOSE) == 0);
    lfFuzzyStrCmp fcmodel (model, (sflags & LF_SEARCH_LOOSE) == 0);

    for (size_t i = 0; i < ((GPtrArray *)Cameras)->len - 1; i++)
    {
        lfCamera *dbcam = static_cast<lfCamera *> (g_ptr_array_index ((GPtrArray *)Cameras, i));
        int score1 = 0, score2 = 0;
        if ((!maker || (score1 = fcmaker.Compare (dbcam->Maker))) &&
            (!model || (score2 = fcmodel.Compare (dbcam->Model))))
        {
            dbcam->Score = score1 + score2;
            _lf_ptr_array_insert_sorted (ret, dbcam, _lf_compare_camera_score);
        }
    }

    // Add a NULL to mark termination of the array
    if (ret->len)
        g_ptr_array_add (ret, NULL);

    // Free the GPtrArray but not the actual list.
    return (const lfCamera **) (g_ptr_array_free (ret, FALSE));
}

const lfCamera *const *lfDatabase::GetCameras () const
{
    return (lfCamera **)((GPtrArray *)Cameras)->pdata;
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
    if (camera)
        lens.AddMount (camera->Mount);
    // Guess lens parameters from lens model name
    lens.GuessParameters ();
    lens.CropFactor = camera ? camera->CropFactor : 0.0;
    return FindLenses (&lens, sflags);
}

static gint _lf_compare_lens_score (gconstpointer a, gconstpointer b)
{
    lfLens *i1 = (lfLens *)a;
    lfLens *i2 = (lfLens *)b;

    return i2->Score - i1->Score;
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

static void _lf_add_compat_mounts (
    const lfDatabase *This, const lfLens *lens, GPtrArray *mounts, char *mount)
{
    const lfMount *m = This->FindMount (mount);
    if (m && m->Compat)
        for (int i = 0; m->Compat [i]; i++)
        {
            mount = m->Compat [i];

            int idx = _lf_ptr_array_find_sorted (mounts, mount, (GCompareFunc)_lf_strcmp);
            if (idx >= 0)
                continue; // mount already in the list

            // Check if the mount is not already in the main list
            bool already = false;
            for (int j = 0; lens->Mounts [j]; j++)
                if (!_lf_strcmp (mount, lens->Mounts [j]))
                {
                    already = true;
                    break;
                }
            if (!already)
                _lf_ptr_array_insert_sorted (mounts, mount, (GCompareFunc)_lf_strcmp);
        }
}

const lfLens **lfDatabase::FindLenses (const lfLens *lens, int sflags) const
{
    GPtrArray *ret = g_ptr_array_new ();
    GPtrArray *mounts = g_ptr_array_new ();

    lfFuzzyStrCmp fc (lens->Model, (sflags & LF_SEARCH_LOOSE) == 0);

    // Create a list of compatible mounts
    if (lens->Mounts)
        for (int i = 0; lens->Mounts [i]; i++)
            _lf_add_compat_mounts (this, lens, mounts, lens->Mounts [i]);
    g_ptr_array_add (mounts, NULL);

    int score;
    const bool sort_and_uniquify = (sflags & LF_SEARCH_SORT_AND_UNIQUIFY) != 0;
    for (size_t i = 0; i < ((GPtrArray *)Lenses)->len - 1; i++)
    {
        lfLens *dblens = static_cast<lfLens *> (g_ptr_array_index ((GPtrArray *)Lenses, i));
        if ((score = _lf_lens_compare_score (
            lens, dblens, &fc, (const char **)mounts->pdata)) > 0)
        {
            dblens->Score = score;
            if (sort_and_uniquify) {
                bool already = false;
                for (size_t i = 0; i < ret->len; i++)
                {
                    const lfLens *previous_lens = static_cast<lfLens *> (g_ptr_array_index (ret, i));
                    if (!_lf_lens_name_compare (previous_lens, dblens))
                    {
                        if (dblens->Score > previous_lens->Score)
                            ret->pdata[i] = dblens;
                        already = true;
                        break;
                    }
                }
                if (!already)
                    _lf_ptr_array_insert_sorted (ret, dblens, _lf_compare_lens_details);
            }
            else
                _lf_ptr_array_insert_sorted (ret, dblens, _lf_compare_lens_score);
        }
    }

    // Add a NULL to mark termination of the array
    if (ret->len)
        g_ptr_array_add (ret, NULL);

    g_ptr_array_free (mounts, TRUE);

    // Free the GPtrArray but not the actual list.
    return (const lfLens **) (g_ptr_array_free (ret, FALSE));
}

const lfLens *const *lfDatabase::GetLenses () const
{
    return (lfLens **)((GPtrArray *)Lenses)->pdata;
}

const lfMount *lfDatabase::FindMount (const char *mount) const
{
    lfMount tm;
    tm.SetName (mount);
    int idx = _lf_ptr_array_find_sorted ((GPtrArray *)Mounts, &tm, _lf_mount_compare);
    if (idx < 0)
        return NULL;

    return (const lfMount *)g_ptr_array_index ((GPtrArray *)Mounts, idx);
}

const char *lfDatabase::MountName (const char *mount) const
{
    const lfMount *m = FindMount (mount);
    if (!m)
        return mount;
    return lf_mlstr_get (m->Name);
}

const lfMount * const *lfDatabase::GetMounts () const
{
    return (lfMount **)((GPtrArray *)Mounts)->pdata;
}

void lfDatabase::AddMount (lfMount *mount)
{
    _lf_ptr_array_insert_unique (
        (GPtrArray *)Mounts, mount, _lf_mount_compare, (GDestroyNotify)lf_mount_destroy);
}

void lfDatabase::AddCamera (lfCamera *camera)
{
    _lf_ptr_array_insert_unique (
        (GPtrArray *)Cameras, camera, _lf_camera_compare, (GDestroyNotify)lf_camera_destroy);
}

void lfDatabase::AddLens (lfLens *lens)
{
    _lf_ptr_array_insert_unique (
        (GPtrArray *)Lenses, lens, _lf_lens_compare, (GDestroyNotify)lf_lens_destroy);
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

void lf_db_destroy (lfDatabase *db)
{
    db->Destroy ();
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

lfError lf_db_load_data (lfDatabase *db, const char *errcontext,
                         const char *data, size_t data_size)
{
    return db->Load (errcontext, data, data_size);
}

lfError lf_db_save_all (const lfDatabase *db, const char *filename)
{
    return db->Save (filename);
}

lfError lf_db_save_file (const lfDatabase *db, const char *filename,
                         const lfMount *const *mounts,
                         const lfCamera *const *cameras,
                         const lfLens *const *lenses)
{
    return db->Save (filename, mounts, cameras, lenses);
}

char *lf_db_save (const lfMount *const *mounts,
                  const lfCamera *const *cameras,
                  const lfLens *const *lenses)
{
    return lfDatabase::Save (mounts, cameras, lenses);
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

const lfCamera *const *lf_db_get_cameras (const lfDatabase *db)
{
    return db->GetCameras ();
}

const lfLens **lf_db_find_lenses_hd (const lfDatabase *db, const lfCamera *camera,
                                     const char *maker, const char *lens, int sflags)
{
    return db->FindLenses (camera, maker, lens, sflags);
}

const lfLens **lf_db_find_lenses (const lfDatabase *db, const lfLens *lens, int sflags)
{
    return db->FindLenses (lens, sflags);
}

const lfLens *const *lf_db_get_lenses (const lfDatabase *db)
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

const lfMount * const *lf_db_get_mounts (const lfDatabase *db)
{
    return db->GetMounts ();
}
