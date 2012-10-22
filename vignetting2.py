#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2012 Torsten Bronger <bronger@physik.rwth-aachen.de>
# This file is in the Public Domain.

from __future__ import unicode_literals, division, absolute_import

import subprocess, os.path, sys, multiprocessing, math
import numpy, PythonMagick
from scipy.optimize.minpack import leastsq
from scipy.interpolate import interp1d

"""Lens vignetting calibration for the Lensfun library
(http://lensfun.berlios.de/).

Synopsis:

python vignetting2.py *.RAW

You must call this program within the directory with the RAW files.  You have
to have installed Python 2.7, ScipPy, NumPy, ImageMagick, PythonMagick,
exiftool, and dcraw.  The result is a file called ``lensfun.xml`` which
contains the XML snippets ready to be copied into the Lensfun database file.

Take pictures for at least five focal length settings of zoom lenses, and for
each focal length, take four aperture settings.  Thus, 5x4 = 20 pictures for
zoom lenses, and 4 pictures for primes.  At least.

Use e.g. translucent milk glass as a diffusor, lay it on the camera lens (shoot
upwards), and illuminate your ceiling uniformly.  Or any equivalent setup.
Never put a light source close to the diffusor!  The diffusor should not take
away more light than 3 stops (factor 8 in exposure time), and not less than 1.7
stops (factor 3.2 in exposure time).  But don't take a picture of a white wall
without a diffusor in front of the lens.  Probably it won't be uniformly
illuminated.

Make sure that the camera doesn't see any fine-grained details in the diffusor.
I took paper instead of milk glass -- it works but you see its texture with
higher f numbers.

Focus plane doesn't matter for my lenses, but your milage may vary.  Anyway,
this script exports a fixed focal (dummy) distance of 45m.

You can check the fits with the Gnuplot files.  For example,

    gnuplot E_16mm_F2.8--16.0--22.0.gp

shows you the vignetting calibration for the "E 16mm" lens, 16mm, f/22.

Choose wisely whether you want to have e.g. D-R optimisation or vignetting
correction on or off when taking the sample pictures.  (Believe it or not, for
the NEX-7, it's sensful to use camera-corrected images.)  If it is unusual,
document what you did in the Lensfun file that you sumbit to the Lensfun
project.

"""


images = {}
pool = multiprocessing.Pool()

for filename in sys.argv[1:]:
    if not os.path.exists(os.path.splitext(filename)[0] + b".tiff"):
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
all_decays = []

for filename in sorted(images):
    exif_data = images[filename]
    exif_data = (exif_data["Lens Model"], float(exif_data["Focal Length"].partition(".0 mm")[0]),
                 float(exif_data["Aperture"]))
    output_filename = "{0}--{1}--{2}".format(*exif_data).replace(" ", "_")

    image = PythonMagick.Image(os.path.splitext(filename)[0] + b".tiff")
    width = 250
    height = int(round(image.size().height() / image.size().width() * width))
    image.sample(b"!{0}x{1}".format(width, height))
    width, height = image.size().width(), image.size().height()
    half_diagonal = math.hypot(width // 2, height // 2)
    radii, intensities = [], []
    for x in range(width):
        for y in range(height):
            radii.append(math.hypot(x - width // 2, y - height // 2) / half_diagonal)
            intensities.append(image.pixelColor(x, y).intensity())
    all_points_filename = "{0}-all_points.dat".format(output_filename)
    with open(all_points_filename, "w") as outfile:
        for radius, intensity in zip(radii, intensities):
            outfile.write("{0} {1}\n".format(radius, intensity))
    bin_width = 10
    number_of_bins = int(half_diagonal / bin_width) + 1
    bins = [[] for i in range(number_of_bins)]
    for radius, intensity in zip(radii, intensities):
        # The zeroth and the last bin are only half bins which means that their
        # means are skewed.  But this is okay: For the zeroth, the curve is
        # supposed to be horizontal anyway, and for the last, it underestimates
        # the vignetting at the rim which is a good thing (too much of
        # correction is bad).
        bin_index = int(round(radius * (number_of_bins - 1)))
        bins[bin_index].append(intensity)
    radii = [i / (number_of_bins - 1) for i in range(number_of_bins)]
    intensities = [sum(bin) / len(bin) for bin in bins]
    vignetting_curve = interp1d(radii, intensities, "quadratic")
    all_decays.append(vignetting_curve(0.1) / vignetting_curve(0.9))
    bins_filename = "{0}-bins.dat".format(output_filename)
    with open(bins_filename, "w") as outfile:
        for radius, intensity in zip(radii, intensities):
            outfile.write("{0} {1}\n".format(radius, intensity))
    radii, intensities = numpy.array(radii), numpy.array(intensities)

    def fit_function(radius, A, k1, k2, k3):
        return A * (1 + k1 * radius**2 + k2 * radius**4 + k3 * radius**6)

    A, k1, k2, k3 = leastsq(lambda p, x, y: y - fit_function(x, *p), [30000, -0.3, 0, 0], args=(radii, intensities))[0]
    database_entries[exif_data] = (k1, k2, k3)

    open("{0}.gp".format(output_filename), "w").write("""set grid
set title "{6}, {7} mm, f/{8}"
plot "{0}" with dots title "samples", "{1}" with linespoints lw 4 title "average", \
     {2} * (1 + ({3}) * x**2 + ({4}) * x**4 + ({5}) * x**6) title "fit"
pause -1""".format(all_points_filename, bins_filename, A, k1, k2, k3, *exif_data))

mu, sigma = numpy.mean(all_decays), numpy.std(all_decays)
print "relative fall-off: {0:.3} +/- {1:.2}%".format(mu, sigma / mu * 100), all_decays

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
