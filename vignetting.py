#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2012 Torsten Bronger <bronger@physik.rwth-aachen.de>
# This file is in the Public Domain.

from __future__ import unicode_literals, division, absolute_import

import subprocess, os, os.path, re, time, sys
import numpy
from scipy.optimize.minpack import leastsq

"""Synopsis:

python ~/src/vignetting.py *.RAW

You should call this program within the directory with the RAW files.  You have
to have installed Python 2.7, ScipPy, NumPy, Hugin, ptovariable, ImageMagick,
and dcraw.  The result is a file called ``lensfun.xml`` which contains the XML
snippets ready to be copied into the Lensfun database file.

Set the variable `crop_factor` in this script correctly.

The filenames of the RAW files must, if sorted alphabetically, form groups of
three images which belong to one mini-panorama.  They should heavily overlap
(the two outmost should share half of their size).  Shoot each triplet with the
very same manual exposure settings in quick sequence.  The motive should be at
least 10m away.

Take tripletts for at least five focal length settings of zoom lenses, and for
each focal length, one triplett for four aperture settings.  Thus, 5x4x3 = 60
pictures for zoom lenses, and 4x3 = 12 pictures for primes.
"""

crop_factor = 1.5

images = {}
sensor_diagonal = 43.27 / crop_factor

for filename in sys.argv[1:]:
    basename = filename[:-4]
    output_lines = subprocess.check_output(["exiftool", "-lensmodel", "-focallength", "-aperture",
                                            "-imagewidth", "-imageheight", filename]).splitlines()
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


database_entries = {}
working_directory = os.getcwd()
pto_path = os.path.join(working_directory, "vignetting.pto")

for triplet in triplets:
    exif_data = images[triplet[0]]
    width, height = int(exif_data["Image Width"]), int(exif_data["Image Height"])
    width, height = width // 4, height // 4
    processes = [(subprocess.Popen(["dcraw", "-T", filename + ".ARW"]), filename)
                 for filename in triplet if not os.path.exists(filename + ".tiff")]
    for process, filename in processes:
        assert process.wait() == 0
        subprocess.check_call(["convert", filename + ".tiff", "-scale", "{0}x{1}".format(width, height), filename + ".tiff"])
    exif_data = (exif_data["Lens Model"], exif_data["Focal Length"].partition(".0 mm")[0], exif_data["Aperture"])
    output_filename = "--".join(exif_data).replace(" ", "_")
    with open("vignetting.pto", "w") as outfile:
        outfile.write(open("/home/bronger/src/vignetting/vignetting.pto").read().format(
                input_filenames=triplet, width=width, height=height))
    subprocess.check_call(["cpfind", "--sieve1width", "20",  "--sieve1height", "20",
                           "--sieve2width", "10",  "--sieve2height", "10", "-o", pto_path, pto_path])
    subprocess.check_call(["cpclean", "-o", pto_path, pto_path])
    subprocess.check_call(["linefind", "-o", pto_path, pto_path])
    subprocess.check_call(["checkpto", pto_path])
    subprocess.check_call(["ptovariable", "--positions", "--view", "--barrel", pto_path])
    subprocess.check_call(["autooptimiser", "-n", "-l", "-s", "-o", pto_path, pto_path])
    subprocess.check_call(["ptovariable", "--vignetting", "--response", pto_path])
    current_pto = open(pto_path).read()
    number_of_vig_processes = 5
    processes = []
    for i in range(number_of_vig_processes):
        process_pto_path = "{0}.{1}.pto".format(pto_path[:-4], i)
        with open(process_pto_path, "w") as pto_file:
            pto_file.write(current_pto)
        processes.append((subprocess.Popen(["vig_optimize", "-p", "100000", "-o", process_pto_path, process_pto_path],
                                           stdout=subprocess.PIPE), process_pto_path))
        time.sleep(3)
    vignetting_data = []
    for process, process_pto_path in processes:
        assert process.wait() == 0
        result_parameters = re.search(r" Vb([-.0-9]+) Vc([-.0-9]+) Vd([-.0-9]+) ", open(process_pto_path). \
                                          readlines()[7]).groups()
        result_parameters = [float(parameter) for parameter in result_parameters]
        vignetting_data.append((result_parameters, process_pto_path))
    vignetting_data.sort()
    best_vignetting_data = vignetting_data[len(vignetting_data) // 2]
    database_entries[exif_data] = best_vignetting_data[0]
    print "Vignetting data:", database_entries[exif_data]#, [data[0][0] for data in vignetting_data]
    os.rename(best_vignetting_data[1], pto_path)
    for i, current_data in enumerate(vignetting_data):
        if i != len(vignetting_data) // 2:
            os.remove(current_data[1])
    subprocess.check_call(["pano_modify", "--canvas=70%", "--crop=AUTO", "-o", pto_path, pto_path])
    subprocess.check_call(["pto2mk", "-o", "vignetting.pto.mk", "-p", output_filename, pto_path])
    subprocess.check_call(["make", "--makefile", "vignetting.pto.mk"])
    for filename in ["vignetting.pto.mk", "vignetting.pto"] + \
            ["{0}{1}.tif".format(output_filename, suffix) for suffix in ["0000", "0001", "0002"]]:
        os.remove(filename)


def error_function(p, x, y):
    k1, k2, k3 = p
    fitted_values = 1 + k1 * x**2 + k2 * x**4 + k3 * x**6
    difference = y - fitted_values
    result = []
    for value, fitted_y in zip(difference, fitted_values):
        if fitted_y < 0.01:
            result.append(10 * value)
        if value < 0:
            result.append(value)
        else:
            result.append(3.3 * value)
    return numpy.array(result)

def get_nd_parameters(k1, k2, k3, nd, focal_length, sensor_diagonal):
    x = numpy.arange(0, 1, 0.001)
    y_vig = 1 + k1 * x**2 + k2 * x**4 + k3 * x**6
    y_filter = 10**(nd * (1 - numpy.sqrt(1 + x**2 / (2 * focal_length / sensor_diagonal)**2)))
    y_total = y_vig * y_filter
    return leastsq(error_function, [k1, k2, k3], args=(x, y_total))[0]


outfile = open("lensfun.xml", "a")

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
    # for nd, distance in [(0.9, 1.0), (1.8, 2.0), (3.0, 2.8), (3.9, 4.0), (5.7, 5.7)]:
    #     k1, k2, k3 = get_nd_parameters(vignetting[0], vignetting[1], vignetting[2], nd, float(focal_length),
    #                                    sensor_diagonal)
    #     outfile.write("""<vignetting model="pa" focal="{focal_length}" aperture="{aperture}" distance="{distance}" """
    #                   """k1="{k1}" k2="{k2}" k3="{k3}" />\n""".format(
    #             focal_length=focal_length, aperture=aperture, distance=distance, k1=k1, k2=k2, k3=k3))
