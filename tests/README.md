The tests in this folder are implemented using the GLib test framework.

    https://developer.gnome.org/glib/2.42/glib-Testing.html
 
When CMake is configured with `-DBUILD_TESTS=on` the tests are build. The 
individual tests are stand-alone executables and can also be run in a 
debugger or for example in `valgrind` for deeper analysis.
By running the tool `ctest` in the build directory the tests are executed 
and the results are summarized.

Unit test code coverage can be analysed with the `lcov` tool. 

	cmake -DCMAKE_BUILD_TYPE=Coverage -DBUILD_TESTS=ON ../ && \
	make && make coverage && sensible-browser ./coverage/index.html

Further commands:

* `ctest -VV` to show verbose output from the individual tests
* `ctest -R Modifier` to run all tests with the string `Modifier` in the name
* `ctest -E Modifier` to exclude tests with the string `Modifier` in the name
