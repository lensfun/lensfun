#include <glib.h>
#include <locale.h>
#include "lensfun.h"

typedef struct {
    lfDatabase* db;
} lfFixture;


void db_setup(lfFixture *lfFix, gconstpointer data)
{
    lfFix->db = new lfDatabase ();
    lfFix->db->LoadDirectory("data/db");
}


void db_teardown(lfFixture *lfFix, gconstpointer data)
{
    delete lfFix->db;
}


// test different lens search strings
void test_DB_lens_search(lfFixture* lfFix, gconstpointer data)
{
    const lfLens **lenses = NULL;

    lenses = lfFix->db->FindLenses (NULL, NULL, "pEntax 50-200 ED");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "smc Pentax-DA 50-200mm f/4-5.6 DA ED");
    lf_free (lenses);

    lenses = lfFix->db->FindLenses (NULL, NULL, "smc Pentax-DA 50-200mm f/4-5.6 DA ED");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "smc Pentax-DA 50-200mm f/4-5.6 DA ED");
    lf_free (lenses);

    lenses = lfFix->db->FindLenses (NULL, NULL, "PENTAX fa 28mm 2.8");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "smc Pentax-FA 28mm f/2.8 AL");
    lf_free (lenses);

    /*lenses = lfFix->db->FindLenses (NULL, NULL, "Fotasy M3517 35mm f/1.7");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "Fotasy M3517 35mm f/1.7");
    lf_free (lenses);

    lenses = lfFix->db->FindLenses (NULL, NULL, "Minolta MD 35mm 1/2.8");
    g_assert_nonnull(lenses);
    g_assert_cmpstr(lenses[0]->Model, ==, "Minolta MD 35mm 1/2.8");
    lf_free (lenses);*/
}

// test different camera search strings
void test_DB_cam_search(lfFixture* lfFix, gconstpointer data)
{
    const lfCamera **cameras = NULL;

    cameras = lfFix->db->FindCamerasExt("pentax", "K100D");
    g_assert_nonnull(cameras);
    g_assert_cmpstr(cameras[0]->Model, ==, "Pentax K100D");
    lf_free (cameras);

    cameras = lfFix->db->FindCamerasExt(NULL, "K 100 D");
    g_assert_nonnull(cameras);
    g_assert_cmpstr(cameras[0]->Model, ==, "Pentax K100D");
    lf_free (cameras);

    cameras = lfFix->db->FindCamerasExt(NULL, "PentAX K100 D");
    g_assert_nonnull(cameras);
    g_assert_cmpstr(cameras[0]->Model, ==, "Pentax K100D");
    lf_free (cameras);

}

int main (int argc, char **argv)
{

    setlocale (LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add("/database/lens search", lfFixture, NULL, db_setup, test_DB_lens_search, db_teardown);
    g_test_add("/database/camera search", lfFixture, NULL, db_setup, test_DB_cam_search, db_teardown);

    return g_test_run();
}
