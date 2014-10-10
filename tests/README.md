The tests in this folder are implemented using the GLib test framework.

    https://developer.gnome.org/glib/2.42/glib-Testing.html
 
When CMake is configured with `-DBUILD_TESTS=on` the tests are build. The 
individual tests are stand-alone exceutables and can also be run in a 
debugger or for example in `valgrind` for deeper analysis.
By running the tool `ctest` in the build directory the tests are executed 
and the results are summarized.
