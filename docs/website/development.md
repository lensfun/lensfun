---
title: Development
linktitle: Development
layout: default
weight: 6
---

The source code is available from a subversion repository hosted at Berlios. You can checkout the current head revision by

    svn checkout svn://svn.berlios.de/lensfun/trunk lensfun

The build system is based on CMake. There is also a proprietary python based system which is not developed anymore and thus it's not recommended to use it. Enter the lensfun folder and create the CMake build directory.

    cd lensfun/
    mkdir cmake_build
    cmake ../

Beneath many other files this should have created a Makefile that can now be used to compile and install by

    make
    make install


