---
title: Development
linktitle: Development
layout: default
weight: 7
---

The source code is available from a git repository hosted at Sourceforge. You can check out the current head revision by

        >$ git clone http://git.code.sf.net/p/lensfun/code lensfun-code

The build system is based on CMake. Enter the Lensfun folder and create the CMake build directory:

        >$ cd lensfun-code/
        >$ mkdir cmake_build
        >$ cd cmake_build/
        >$ cmake ../

Beneath many other files this should have created a Makefile that can now be used to compile and install as usual with

        >$ make
        >$ make install

More information about the build system and available options can be found in the README file in the root folder.
