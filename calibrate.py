#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2012 Torsten Bronger <bronger@physik.rwth-aachen.de>
# This file is in the Public Domain.

from __future__ import unicode_literals, division, absolute_import

missing_packages = set()

import subprocess, os.path, sys, multiprocessing, math, re, contextlib, glob
try:
    import numpy
except ImportError:
    missing_packages.add("python-numpy")
try:
    import PythonMagick
except ImportError:
    missing_packages.add("python-pythonmagick")
try:
    from scipy.optimize.minpack import leastsq
except ImportError:
    missing_packages.add("python-scipy")
def test_program(program, package_name):
    try:
        subprocess.call([program], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except OSError:
        missing_packages.add(package_name)
test_program("dcraw", "dcraw")
test_program("tca_correct", "hugin-tools")
test_program("exiftool", "libimage-exiftool-perl")
if missing_packages:
    print("The following packages are missing (Ubuntu packages, names may differ on other systems):\n    {0}\nAbort.".
          format("  ".join(missing_packages)))
    sys.exit()


@contextlib.contextmanager
def chdir(dirname=None):
    curdir = os.getcwd()
    try:
        if dirname is not None:
            os.chdir(dirname)
        yield
    finally:
        os.chdir(curdir)


raw_file_extensions = ["3fr", "ari", "arw", "bay", "crw", "cr2", "cap", "dcs", "dcr", "dng", "drf", "eip", "erf", "fff",
                       "iiq", "k25", "kdc", "mef", "mos", "mrw", "nef", "nrw", "obm", "orf", "pef", "ptx", "pxn", "r3d",
                       "raf", "raw", "rwl", "rw2", "rwz", "sr2", "srf", "srw", "x3f"]
def find_raw_files():
    result = []
    for file_extension in raw_file_extensions:
        result.extend(glob.glob("*." + file_extension))
        result.extend(glob.glob("*." + file_extension.upper()))
    return result


filepath_pattern = re.compile(r"(?P<lens_model>.+)--(?P<focal_length>[0-9.]+)mm--(?P<aperture>[0-9.]+)$")

def detect_exif_data(filename):
    exif_data = {}
    match = filepath_pattern.match(os.path.splitext(filename)[0])
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
        try:
            exif_data = (exif_data["Lens Model"], float(exif_data["Focal Length"].partition(".0 mm")[0]),
                         float(exif_data["Aperture"]))
        except KeyError:
            print("""Some EXIF data is missing in your RAW files.  You have to
rename them according to the scheme "Lens_name--16mm--1.4.RAW
(Use your RAW file extension of course.)  Abort.""")
            sys.exit()
    return exif_data


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
""".format(self.maker, self.name, self.mount, self.cropfactor))
        if self.type_:
            outfile.write("        <type>{0}</type>\n".format(self.type_))
        if self.calibration_lines:
            outfile.write("        <calibration>\n")
            for line in self.calibration_lines:
                outfile.write("            {0}\n".format(line))
            outfile.write("        </calibration>\n")
        outfile.write("    </lens>\n")


lens_line_pattern = re.compile(
    r"(?P<name>.+):\s*(?P<maker>.+)\s*,\s*(?P<mount>.+)\s*,\s*(?P<cropfactor>.+)(\s*,\s*(?P<type>.+))?")
distortion_line_pattern = re.compile(r"\s*distortion\((?P<focal_length>[.0-9]+)mm\)\s*=\s*"
                                     r"(?P<a>[-.0-9]+)\s*,\s*(?P<b>[-.0-9]+)\s*,\s*(?P<c>[-.0-9]+)")
lenses = {}
try:
    for linenumber, original_line in enumerate(open("lenses.txt")):
        linenumber += 1
        line = original_line.strip()
        if not line.startswith("#"):
            match = lens_line_pattern.match(line)
            if match:
                data = match.groupdict()
                current_lens = Lens(data["name"], data["maker"], data["mount"], data["cropfactor"], data["type"])
                lenses[data["name"]] = current_lens
            else:
                match = distortion_line_pattern.match(line)
                if not match:
                    print("Invalid line {0} in lenses.txt:\n{1}Abort.".format(linenumber, original_line))
                    sys.exit()
                current_lens.calibration_lines.append(
                    """<distortion model="ptlens" focal="{0}" a="{1}" b="{2}" c="{3}" />""".format(*match.groups()))
except IOError:
    focal_lengths = {}
    def browse_directory(directory):
        if os.path.exists(directory):
            with chdir(directory):
                for filename in find_raw_files():
                    exif_data = detect_exif_data(filename)
                    focal_lengths.setdefault(exif_data[0], set()).add(exif_data[1])
    browse_directory("distortion")
    browse_directory("tca")
    for directory in glob.glob("vignetting*"):
        browse_directory(directory)
    with open("lenses.txt", "w") as outfile:
        if focal_lengths:
            outfile.write("""# For suggestions for <maker> and <mount> see <http://goo.gl/BSARX>.
# Omit <type> for rectilinear lenses.
""")
            for lens_name, lenghts in focal_lengths.items():
                outfile.write("\n{0}: <maker>, <mount>, <cropfator>, <type>\n".format(lens_name))
                for length in sorted(lengths):
                    outfile.write("distortion({0}mm) = , , \n".format(length))
        else:
            outfile.write("""# No RAW images found.  Please have a look at
# http://wilson.homeunix.com/lens_calibration_tutorial/
""")
    print("I wrote a template lenses.txt.  Please fill this file with proper information.  Abort.")
    sys.exit()


#
# TCA correction
#

def calculate_tca(filename):
    exif_data = detect_exif_data(filename)
    tiff_filename = os.path.splitext(filename)[0] + b".tiff"
    if not os.path.exists(tiff_filename):
        subprocess.check_call(["dcraw", "-4", "-T", "-o", "0", "-M", filename])
    output = subprocess.check_output(["tca_correct", "-o", "bv", tiff_filename]).splitlines()[-1].strip()
    with open(filename + ".tca", "w") as outfile:
        outfile.write("{0}\n{1}\n{2}\n".format(exif_data[0], exif_data[1], output))

if os.path.exists("tca"):
    with chdir("tca"):
        pool = multiprocessing.Pool()
        for filename in find_raw_files():
            pool.apply_async(calculate_tca, [filename])
        pool.close()
        pool.join()
        for filename in find_raw_files():
            filename = filename + ".tca"
            lens_name, focal_length, tca_output = [line.strip() for line in open(filename).readlines()]
            data = re.match(
                r"-r [.0]+:(?P<br>[-.0-9]+):[.0]+:(?P<vr>[-.0-9]+) -b [.0]+:(?P<bb>[-.0-9]+):[.0]+:(?P<vb>[-.0-9]+)",
                tca_output).groupdict()
            try:
                lenses[lens_name].calibration_lines.append(
                    """<tca model="poly3" focal="{0}" br="{1}" vr="{2}" bb="{3}" vb="{4}" />""".format(
                        focal_length, data["br"], data["vr"], data["bb"], data["vb"]))
            except KeyError:
                print("""Lens "{0}" not found in lenses.txt.  Abort.""".format(lens_name))
                sys.exit()
            os.remove(filename)


#
# Vignetting
#

images = {}
pool = multiprocessing.Pool()
distances = set()

for vignetting_directory in glob.glob("vignetting*"):
    distance = float(vignetting_directory.partition("_")[2] or "inf")
    assert distance == float("inf") or distance < 1000
    distances.add(distance)
    with chdir(vignetting_directory):
        for filename in find_raw_files():
            if not os.path.exists(os.path.splitext(filename)[0] + b".tiff"):
                pool.apply_async(subprocess.check_call, [["dcraw", "-4", "-T", "-M", "-o", "0", filename]])
            exif_data = detect_exif_data(filename) + (distance,)
            images.setdefault(exif_data, []).append(os.path.join(vignetting_directory, filename))
pool.close()
pool.join()


vignetting_db_entries = {}
working_directory = os.getcwd()

for exif_data, filepaths in images.items():
    output_filename = "{0}--{1}--{2}--{3}".format(*exif_data).replace(" ", "_")

    radii, intensities = [], []
    for filepath in filepaths:
        image = PythonMagick.Image(str(os.path.splitext(filepath)[0] + ".tiff"))
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
set title "{6}, {7} mm, f/{8}, {9} m"
plot "{0}" with dots title "samples", "{1}" with linespoints lw 4 title "average", \
     {2} * (1 + ({3}) * x**2 + ({4}) * x**4 + ({5}) * x**6) title "fit"
pause -1""".format(all_points_filename, bins_filename, A, k1, k2, k3, *exif_data))


if len(distances) == 1 and list(distances)[0] > 10:
    # If only one distance was measured and at more than 10m, insert it twice
    # at 10m and ∞, so that the whole range is covered.
    new_vignetting_db_entries = {}
    for configuration, vignetting in vignetting_db_entries.items():
        new_vignetting_db_entries[tuple(configuration[:3] + (10,))] = \
            new_vignetting_db_entries[tuple(configuration[:3] + (float("inf"),))] = vignetting
    vignetting_db_entries = new_vignetting_db_entries


for configuration in sorted(vignetting_db_entries):
    lens, focal_length, aperture, distance = configuration
    vignetting = vignetting_db_entries[configuration]
    if distance == float("inf"):
        distance = 1000
    try:
        lenses[lens].calibration_lines.append(
            """<vignetting model="pa" focal="{focal_length}" aperture="{aperture}" distance="{distance}" """
            """k1="{vignetting[0]}" k2="{vignetting[1]}" k3="{vignetting[2]}" />""".format(
                focal_length=focal_length, aperture=aperture, vignetting=vignetting, distance=distance))
    except KeyError:
        print("""Lens "{0}" not found in lenses.txt.  Abort.""".format(lens_name))
        sys.exit()


outfile = open("lensfun.xml", "w")
for lens in lenses.values():
    lens.write(outfile)
