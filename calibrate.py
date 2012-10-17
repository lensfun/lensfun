#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2012 Torsten Bronger <bronger@physik.rwth-aachen.de>
# This file is in the Public Domain.

from __future__ import unicode_literals, division, absolute_import

import subprocess, os.path, sys, multiprocessing, math, re, contextlib
import numpy, PythonMagick
from scipy.optimize.minpack import leastsq

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


raw_extension = "ARW"
create_distance_range = False


@contextlib.contextmanager
def chdir(dirname=None):
    curdir = os.getcwd()
    try:
        if dirname is not None:
            os.chdir(dirname)
        yield
    finally:
        os.chdir(curdir)


class Lens(object):

    def __init__(self, name, maker, mount, cropfactor, type_):
        self.name, self.maker, self.mount, self.cropfactor, self.type_ = name, maker, mount, cropfactor, type_
        self.calibration_lines = []

    def write(self, outfile):
        type_line = "        <type>{0}</type>\n".format(self.type_) if self.type_ else ""
        outfile.write("""
    <lens>
        <maker>{0}</maker>
        <model>{1}</model>
        <mount>{2}</mount>
        <cropfactor>{3}</cropfactor>
{4}        <calibration>
""".format(self.maker, self.name, self.mount, self.cropfactor, self.type_line))
        for line in self.calibration_lines:
            outfile.write("            {0}\n".format(line))


lens_line_pattern = re.compile(
    r"(?P<name>.+):\s*(?P<maker>.+)\s*,\s*(?P<mount>.+)\s*,\s*(?P<cropfactor>.+)(\s*,\s*(?P<type>.+))?")
distortion_line_pattern = re.compile(r"\s*distortion\((?P<focal_length>[.0-9]+)mm\)\s*=\s*"
                                     r"(?P<a>[-.0-9]+)\s*,\s*(?P<b>[-.0-9]+)\s*,\s*(?P<c>[-.0-9]+)")
lenses = {}
for line in open("lenses.txt"):
    line = line.strip()
    if not line.startswith("#"):
        match = lens_line_pattern.match(line)
        if match:
            data = match.groupdict()
            current_lens = Lens(data["name"], data["maker"], data["mount"], data["cropfactor"], data["type"])
            lenses[data["name"]] = current_lens
        else:
            match = distortion_line_pattern.match(line)
            assert match
            current_lens.calibration_lines.append(
                """<distortion model="ptlens" focal="{0}" a="{1}" b="{2}" c="{3}" />""".format(*match.groups()))


filepath_pattern = re.compile(r"(?P<lens_model>.+)--(?P<focal_length>[0-9.]+)mm--(?P<aperture>[0-9.]+)$")

def detect_exif_data(filename):
    exif_data = {}
    match = filename_pattern.match(os.path.splitext(filename)[0])
    if match:
        exif_data = (match.group("lens_model").replace("_", " "), float(match.group("focal_length")),
                     float(match.group("aperture")), distance)
    else:
        output_lines = subprocess.check_output(
            ["exiftool", "-lensmodel", "-focallength", "-aperture", filename]).splitlines()
        for line in output_lines:
            key, value = line.split(":", 1)
            key = key.strip()
            value = value.strip()
            exif_data[key] = value
        exif_data = (exif_data["Lens Model"], float(exif_data["Focal Length"].partition(".0 mm")[0]),
                     float(exif_data["Aperture"]), distance)
    return exif_data


raw_file_extensions = [".3fr", "ari", "arw", "bay", "crw", "cr2", "cap", "dcs", "dcr", "dng", "drf", "eip", "erf", "fff",
                       "iiq", "k25", "kdc", "mef", "mos", "mrw", "nef", "nrw", "obm", "orf", "pef", "ptx", "pxn", "r3d",
                       "raf", "raw", "rwl", "rw2", "rwz", "sr2", "srf", "srw", "x3f"]
def find_raw_files():
    result = []
    for file_extension in raw_file_extensions:
        result.extend(glob.glob("*." + raw_extension))
        result.extend(glob.glob("*." + raw_extension.upper()))
    return result


#
# TCA correction
#

def calculate_tca(filename):
    exif_data = detect_exif_data(filename)
    tiff_filename = os.path.splitext(filename)[0] + b".tiff"
    if not os.path.exists(tiff_filename):
        subprocess.check_call(["dcraw", "-4", "-T", filename])
    output = subprocess.check_output("tca_correct", "-o", "bv", tiff_filename).splitlines()[-1].strip()
    with open(filename + ".tca", "w") as outfile:
        outfile.write("{0}\n{1}\n{2}\n".format(exif_data["Lens Model"], exif_data["Focal Length"], output))

with chdir("tca"):
    pool = multiprocessing.Pool()
    for filename in find_raw_files():
        pool.apply_async(calculate_tca, [filename])
    pool.close()
    pool.join()
    for filename in find_raw_files():
        lens_name, focal_length, tca_output = [line.strip() for line in open(filename + ".tca").splitlines()]
        data = re.match(r"-r [.0]+:(?P<br>[-.0-9]+):[.0]+:(?P<vr>[-.0-9]+) -b [.0]+:(?P<bb>[-.0-9]+):[.0]+:(?P<vb>[-.0-9]+)",
                        tca_output).groupdict()
        lenses[lens_name].calibration_lines.append(
            """<tca model="poly3" focal="{0}" br="{1}" vr="{2}" bb="{3}" vb="{4}" />""".format(
                focal_length, data["br"], data["vr"], data["bb"], data["vb"]))


#
# Vignetting
#

images = {}
pool = multiprocessing.Pool()

for vignetting_directory in glob.glob("vignetting*"):
    distance = float(vignetting_directory.partition("_")[2] or "inf")
    assert distance == float("inf") or distance < 1000
    with chdir(vignetting_directory):
        for filename in find_raw_files():
            if not os.path.exists(os.path.splitext(filename)[0] + b".tiff"):
                pool.apply_async(subprocess.check_call, [["dcraw", "-4", "-T", filename]])
            exif_data = detect_exif_data(filename)
            images.setdefault(exif_data, []).append(os.path.join(vignetting_directory, filename))
pool.close()
pool.join()


vignetting_db_entries = {}
working_directory = os.getcwd()

for exif_data, filepaths in images.items():
    output_filename = "{0}--{1}--{2}--{3}".format(*exif_data).replace(" ", "_")

    radii, intensities = [], []
    for filepath in filepaths:
        image = PythonMagick.Image(os.path.splitext(filepath)[0] + b".tiff")
        width = 250
        height = int(round(image.size().height() / image.size().width() * width))
        image.sample(b"!{0}x{1}".format(width, height))
        width, height = image.size().width(), image.size().height()
        half_diagonal = math.hypot(width // 2, height // 2)
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
    bins_filename = "{0}-bins.dat".format(output_filename)
    with open(bins_filename, "w") as outfile:
        for radius, intensity in zip(radii, intensities):
            outfile.write("{0} {1}\n".format(radius, intensity))
    radii, intensities = numpy.array(radii), numpy.array(intensities)

    def fit_function(radius, A, k1, k2, k3):
        return A * (1 + k1 * radius**2 + k2 * radius**4 + k3 * radius**6)

    A, k1, k2, k3 = leastsq(lambda p, x, y: y - fit_function(x, *p), [30000, -0.3, 0, 0], args=(radii, intensities))[0]
    vignetting_db_entries[exif_data] = (k1, k2, k3)

    open("{0}.gp".format(output_filename), "w").write("""set grid
set title "{6}, {7} mm, f/{8}"
plot "{0}" with dots title "samples", "{1}" with linespoints lw 4 title "average", \
     {2} * (1 + ({3}) * x**2 + ({4}) * x**4 + ({5}) * x**6) title "fit"
pause -1""".format(all_points_filename, bins_filename, A, k1, k2, k3, *exif_data))


if create_distance_range:
    distance_configurations = {}
    for configuration in vignetting_db_entries:
        configuration, distance = configuration[:3], configuration[3]
        distance_configurations.setdefault(configuration, []).append(distance)
    for configuration, distances in distance_configurations.items():
        if len(distances) == 1 and distances[0] > 10:
            # If only one distance was measured and at more than 10m, insert it
            # twice at 10m and ∞, so that the whole range is covered.
            vignetting = vignetting_db_entries.pop(tuple(configuration + (distances[0],)))
            vignetting_db_entries[10] = vignetting_db_entries[tuple(configuration + (float("inf"),))] = vignetting


for configuration in sorted(vignetting_db_entries):
    lens, focal_length, aperture, distance = configuration
    vignetting = vignetting_db_entries[configuration]
    if distance == float("inf"):
        distance = 1000
    lenses[lens].calibration_lines.append(
        """<vignetting model="pa" focal="{focal_length}" aperture="{aperture}" distance="{distance}" """
        """k1="{vignetting[0]}" k2="{vignetting[1]}" k3="{vignetting[2]}" />""".format(
            focal_length=focal_length, aperture=aperture, vignetting=vignetting, distance=distance)))


outfile = open("lensfun.xml")
for lens in lenses.values():
    lens.write(outfile)
