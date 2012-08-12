#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import unicode_literals, division, absolute_import

import subprocess, glob, os


images = {}

for filename in [u"DSC03230.ARW", u"DSC03231.ARW", u"DSC03232.ARW"]: #glob.glob("*.ARW"):
    basename = filename[:-4]
    output_lines = subprocess.check_output(["exiftool", "-lensmodel", "-focallength", "-aperture", filename]).splitlines()
    exif_data = {}
    for line in output_lines:
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.strip()
        exif_data[key] = value
    images[basename] = exif_data

basenames = sorted(images)
triplets = [[basenames[0]]]

for basename in basenames[1:]:
    if not triplets[-1] or images[basename] == images[triplets[-1][0]]:
        if len(triplets[-1]) < 3:
            triplets[-1].append(basename)
        else:
            raise Exception("Too many image files with the following EXIF data: ", images[basename], 
                            "\nAt least: ", triplets[-1] + [basename])
    else:
        if len(triplets[-1]) == 3:
            triplets.append([basename])
        else:
            raise Exception("Too few image files with the following EXIF data: ", images[basename],
                            "\nOnly found: ", triplets[-1])


working_directory = os.getcwd()

for triplet in triplets:
    with open("vignetting.pto", "w") as outfile:
        outfile.write(open("/home/bronger/src/vignetting/vignetting.pto").read().format(input_filenames=triplet))
    for name in ["first", "second"]:
        with open("vignetting_{0}.pto.mk".format(name), "w") as outfile:
            outfile.write(open("/home/bronger/src/vignetting/vignetting_{0}.pto.mk".format(name)).read().format(
                    input_filenames=triplet, output_filename="{triplet[0]}-{triplet[2]}".format(triplet=triplet),
                    working_directory=working_directory))
    subprocess.check_call(["make", "--makefile", "vignetting_first.pto.mk"])
    subprocess.check_call(["make", "--makefile", "vignetting_second.pto.mk"])
