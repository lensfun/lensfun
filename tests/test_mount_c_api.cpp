#include <glib.h>
#include <locale.h>
#include "lensfun.h"

#include <array>

void test_mount_c_api()
{
    lfMount* mount = lf_mount_create();
    g_assert_nonnull(mount);
    g_assert_false(lf_mount_check(mount));

    mount->Name = lf_mlstr_add(mount->Name, "en", "Nikon F");
    g_assert_true(lf_mount_check(mount));
    g_assert_null(lf_mount_get_compats(mount));

    std::array<const char*, 3> addedCompats = {"Nikon F AF", "Nikon F AI", "Nikon F AI-S"};
    for (size_t idx = 0; idx < addedCompats.size(); ++idx)
    {
        lf_mount_add_compat(mount, addedCompats[idx]);
        lf_mount_add_compat(mount, nullptr); // nullptr should not alter anything

        const char* const* compats = lf_mount_get_compats(mount);
        g_assert_nonnull(compats);

        size_t added = idx + 1;
        for (size_t i = 0; i < added; ++i)
        {
            g_assert_nonnull(compats[i]);
            g_assert_false(strcmp(compats[i], addedCompats[i]));
        }
        g_assert_null(compats[added]); // null-termminated vector
    }

    lfMount* mount2 = lf_mount_create();
    g_assert_nonnull(mount2);
    g_assert_false(lf_mount_check(mount2));

    lf_mount_copy(mount2, mount);
    auto checkCopy = [mount2, &addedCompats]()
    {
        g_assert_true(lf_mount_check(mount2));
        const char* const* compats = lf_mount_get_compats(mount2);
        for (size_t i = 0; i < addedCompats.size(); ++i)
        {
            g_assert_nonnull(compats[i]);
            g_assert_false(strcmp(compats[i], addedCompats[i]));
        }
        g_assert_null(compats[addedCompats.size()]);
    };

    checkCopy();
    // test that destroying "mount" does not break "mount2"
    lf_mount_destroy(mount);
    checkCopy();

    lf_mount_destroy(mount2);
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "");

    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/mount/c_api", test_mount_c_api);

    return g_test_run();
}
