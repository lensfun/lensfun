#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2012 Torsten Bronger <bronger@physik.rwth-aachen.de>
# This file is in the Public Domain.

from __future__ import unicode_literals, division, absolute_import

import subprocess, os.path, sys, multiprocessing, math
import numpy, PythonMagick
from scipy.optimize.minpack import leastsq

"""Lens vignetting calibration for the Lensfun library.

Synopsis:

python ~/src/vignetting.py *.RAW

You must call this program within the directory with the RAW files.  You have
to have installed Python 2.7, ScipPy, NumPy, ImageMagick, PythonMagick, and
dcraw.  The result is a file called ``lensfun.xml`` which contains the XML
snippets ready to be copied into the Lensfun database file.

Take pictures for at least five focal length settings of zoom lenses, and for
each focal length, one triplet for four aperture settings.  Thus, 5x4 = 20
pictures for zoom lenses, and 4 pictures for primes.

Choose wisely whether you want to have D-R optimisation or vignetting
correction on or off when taking the sample pictures.

"""


images = {}
pool = multiprocessing.Pool()

for filename in sys.argv[1:]:
    pool.apply_async(subprocess.check_call, [["dcraw", "-4", "-T", filename]])
    output_lines = subprocess.check_output(["exiftool", "-lensmodel", "-focallength", "-aperture", filename]).splitlines()
    exif_data = {}
    for line in output_lines:
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.strip()
        exif_data[key] = value
    images[filename] = exif_data
pool.close()
pool.join()


database_entries = {}
working_directory = os.getcwd()

for filename in sorted(images):
    exif_data = images[filename]
    exif_data = (exif_data["Lens Model"], exif_data["Focal Length"].partition(".0 mm")[0], exif_data["Aperture"])
    output_filename = "--".join(exif_data).replace(" ", "_")

    image = PythonMagick.Image(os.path.splitext(filename)[0] + b".tiff")
    width, height = int(round(image.size().width() / 24)), int(round(image.size().height() / 24))
    image.sample(b"!{0}x{1}".format(width, height))
    width, height = image.size().width(), image.size().height()
    half_diagonal = math.hypot(width // 2, height // 2)
    radii, intensities = [], []
    for x in range(width):
        for y in range(height):
            radii.append(math.hypot(x - width // 2, y - height // 2) / half_diagonal)
            intensities.append(image.pixelColor(x, y).intensity())
    with open("{0}-all_points.dat".format(output_filename), "w") as outfile:
        for radius, intensity in zip(radii, intensities):
            outfile.write("{0} {1}\n".format(radius, intensity))
    bin_width = 10
    number_of_bins = int(half_diagonal / bin_width) + 1
    bins = [[] for i in range(number_of_bins)]
    for radius, intensity in zip(radii, intensities):
        bin_index = int(round(radius * (number_of_bins - 1)))
        bins[bin_index].append(intensity)
    radii = [i / (number_of_bins - 1) for i in range(number_of_bins)]
    intensities = [sum(bin) / len(bin) for bin in bins]
    with open("{0}-bins.dat".format(output_filename), "w") as outfile:
        for radius, intensity in zip(radii, intensities):
            outfile.write("{0} {1}\n".format(radius, intensity))
    radii, intensities = numpy.array(radii), numpy.array(intensities)

    def fit_function(radius, A, k1, k2, k3):
        return A * (1 + k1 * radius**2 + k2 * radius**4 + k3 * radius**6)

    A, k1, k2, k3 = leastsq(lambda p, x, y: y - fit_function(x, *p), [30000, -0.3, 0, 0], args=(radii, intensities))[0]
    database_entries[exif_data] = (k1, k2, k3)


outfile = open("lensfun.xml", "w")

current_lens = None
for configuration in sorted(database_entries):
    lens, focal_length, aperture = configuration
    vignetting = database_entries[configuration]
    if lens != current_lens:
        outfile.write("\n\n{0}:\n\n".format(lens))
        current_lens = lens
    outfile.write("""<vignetting model="pa" focal="{focal_length}" aperture="{aperture}" distance="45" """
                  """k1="{vignetting[0]}" k2="{vignetting[1]}" k3="{vignetting[2]}" />\n""".format(
            focal_length=focal_length, aperture=aperture, vignetting=vignetting))
