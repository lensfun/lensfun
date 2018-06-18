#include <glib.h>
#include <locale.h>
#include "lensfun.h"

void test_lens_guess_parameter()
{
    lfLens* lens;

    lens = new lfLens();
    lens->SetMaker("Sigma");
    lens->SetModel("Sigma 100-300mm f/4 APO EX DG HSM");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 100.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 300.0f);
    g_assert_cmpfloat(lens->MinAperture, ==, 4.0f);
    delete lens;

    lens = new lfLens();
    lens->SetMaker("Tamron");
    lens->SetModel("Tamron SP AF 90mm f/2.8 Di Macro 1:1");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 90.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 90.0f);
    g_assert_cmpfloat(lens->MinAperture, ==, 2.8f);
    delete lens;

    lens = new lfLens();
    lens->SetMaker("Sigma");
    lens->SetModel("D-Xenon 1:4-5.6 50-200mm AL");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 50.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 200.0f);
    g_assert_cmpfloat(lens->MinAperture, ==, 4.0f);
    delete lens;

    lens = new lfLens();
    lens->SetMaker("Nikon");
    lens->SetModel("1 Nikkor AW 11-27.5mm f/3.5-5.6");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 11.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 27.5f);
    g_assert_cmpfloat(lens->MinAperture, ==, 3.5f);
    delete lens;


    lens = new lfLens();
    lens->SetMaker("Fujifilm");
    lens->SetModel("XF 35mm f/1.4 R");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 35.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 35.0f);
    g_assert_cmpfloat(lens->MinAperture, ==, 1.4f);
    delete lens;

    lens = new lfLens();
    lens->SetMaker("KMZ");
    lens->SetModel("MC MTO 11CA 10/1000");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 1000.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 1000.0f);
    g_assert_cmpfloat(lens->MinAperture, ==, 10.0f);
    delete lens;

    lens = new lfLens();
    lens->SetMaker("Fotasy");
    lens->SetModel("Fotasy M3517 35mm f/1.7");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 35.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 35.0f);
    g_assert_cmpfloat(lens->MinAperture, ==, 1.7f);
    delete lens;

    lens = new lfLens();
    lens->SetMaker("Vivitar");
    lens->SetModel("Vivitar Series One 70-210mm 1:3.5 SN 22...");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 70.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 210.0f);
    g_assert_cmpfloat(lens->MinAperture, ==, 3.5f);
    delete lens;

    // check if extenders are ignored
    lens = new lfLens();
    lens->SetMaker("Canon");
    lens->SetModel("Canon EF 300mm f/2.8L IS II USM + EF 1.4x ext. III");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 0.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 0.0f);
    g_assert_cmpfloat(lens->MinAperture, ==, 0.0f);
    delete lens;

    // ignore nonsense input
    lens = new lfLens();
    lens->SetMaker("Pentax");
    lens->SetModel("Foo 8912 DX F90 AL");
    lens->GuessParameters();
    g_assert_cmpfloat(lens->MinFocal, ==, 0.0f);
    g_assert_cmpfloat(lens->MaxFocal, ==, 0.0f);
    g_assert_cmpfloat(lens->MinAperture, ==, 0.0f);
    delete lens;

}


int main (int argc, char **argv)
{

    setlocale (LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/lens/parameter guessing", test_lens_guess_parameter);

    return g_test_run();
}
