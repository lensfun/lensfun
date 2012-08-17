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

You must call this program within the directory with the RAW files.  You have
to have installed Python 2.7, ScipPy, NumPy, Hugin, ptovariable, ImageMagick,
and dcraw.  The result is a file called ``lensfun.xml`` which contains the XML
snippets ready to be copied into the Lensfun database file.

Set the variable `crop_factor` in this script correctly.

The filenames of the RAW files must, if sorted alphabetically, form groups of
three images which belong to one mini-panorama.  They should heavily overlap
(the two outmost should share half of their size).  Shoot each triplet with the
very same manual exposure settings in quick sequence.  The motive should be at
least 10m away.

Take triplets for at least five focal length settings of zoom lenses, and for
each focal length, one triplet for four aperture settings.  Thus, 5x4x3 = 60
pictures for zoom lenses, and 4x3 = 12 pictures for primes.
"""

crop_factor = 1.5


pto_file_contents = """# hugin project file
#hugin_ptoversion 2
p f2 w3000 h1500 v360  E10.2928 R0 n"TIFF_m c:LZW r:CROP"
m g1 i0 f0 m2 p0.00784314

# image lines
#-hugin  cropFactor=1.5
i w{width} h{height} f0 v73.7060715691069 Ra0 Rb0 Rc0 Rd0 Re0 Eev10.2927816680626 Er1 Eb1 r0 p0 y0 TrX0 TrY0 TrZ0 j0 a0 b0 c0 d0 e0 g0 t0 Va1 Vb0 Vc0 Vd0 Vx0 Vy0  Vm5 n"{input_filenames[0]}.tiff"
#-hugin  cropFactor=1.5
i w{width} h{height} f0 v=0 Ra=0 Rb=0 Rc=0 Rd=0 Re=0 Eev10.2927816680626 Er1 Eb1 r0 p0 y0 TrX0 TrY0 TrZ0 j0 a=0 b=0 c=0 d=0 e=0 g=0 t=0 Va=0 Vb=0 Vc=0 Vd=0 Vx=0 Vy=0  Vm5 n"{input_filenames[1]}.tiff"
#-hugin  cropFactor=1.5
i w{width} h{height} f0 v=0 Ra=0 Rb=0 Rc=0 Rd=0 Re=0 Eev10.2927816680626 Er1 Eb1 r0 p0 y0 TrX0 TrY0 TrZ0 j0 a=0 b=0 c=0 d=0 e=0 g=0 t=0 Va=0 Vb=0 Vc=0 Vd=0 Vx=0 Vy=0  Vm5 n"{input_filenames[2]}.tiff"


# specify variables that should be optimized
v r1
v p1
v y1
v r2
v p2
v y2
v


# control points

#hugin_optimizeReferenceImage 0
#hugin_blender enblend
#hugin_remapper nona
#hugin_enblendOptions 
#hugin_enfuseOptions 
#hugin_hdrmergeOptions -m avg -c
#hugin_outputLDRBlended true
#hugin_outputLDRLayers false
#hugin_outputLDRExposureRemapped false
#hugin_outputLDRExposureLayers false
#hugin_outputLDRExposureBlended false
#hugin_outputLDRExposureLayersFused false
#hugin_outputHDRBlended false
#hugin_outputHDRLayers false
#hugin_outputHDRStacks false
#hugin_outputLayersCompression LZW
#hugin_outputImageType tif
#hugin_outputImageTypeCompression LZW
#hugin_outputJPEGQuality 90
#hugin_outputImageTypeHDR exr
#hugin_outputImageTypeHDRCompression LZW
"""

images = {}
sensor_diagonal = 43.27 / crop_factor

for filename in sys.argv[1:]:
    basename = filename[:-4]
    output_lines = subprocess.check_output(["exiftool", "-lensmodel", "-focallength", "-aperture", filename]).splitlines()
    exif_data = {}
    for line in output_lines:
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.strip()
        exif_data[key] = value
    images[basename] = exif_data

def get_image_size(filename):
    output_lines = subprocess.check_output(["exiftool", "-imagewidth", "-imageheight", filename]).splitlines()
    exif_data = {}
    for line in output_lines:
        key, value = line.split(":", 1)
        key = key.strip()
        value = value.strip()
        if key == "Image Width":
            width = int(value)
        elif key == "Image Height":
            height = int(value)
    return width, height

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
    processes = [(subprocess.Popen(["dcraw", "-T", filename + ".ARW"]), filename)
                 for filename in triplet if not os.path.exists(filename + ".tiff")]
    for process, filename in processes:
        assert process.wait() == 0
        width, height = get_image_size(filename + ".tiff")
        width, height = width // 4, height // 4
        subprocess.check_call(["convert", filename + ".tiff", "-scale", "{0}x{1}".format(width, height), filename + ".tiff"])
    try:
        width, height
    except NameError:
        width, height = get_image_size(triplet[0] + ".tiff")
    exif_data = images[triplet[0]]
    exif_data = (exif_data["Lens Model"], exif_data["Focal Length"].partition(".0 mm")[0], exif_data["Aperture"])
    output_filename = "--".join(exif_data).replace(" ", "_")
    with open(pto_path, "w") as outfile:
        outfile.write(pto_file_contents.format(input_filenames=triplet, width=width, height=height))
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
    # for nd, distance in [(0.9, 1.0), (1.8, 2.0), (3.0, 2.8), (3.9, 4.0), (5.7, 5.7)]:
    #     k1, k2, k3 = get_nd_parameters(vignetting[0], vignetting[1], vignetting[2], nd, float(focal_length),
    #                                    sensor_diagonal)
    #     outfile.write("""<vignetting model="pa" focal="{focal_length}" aperture="{aperture}" distance="{distance}" """
    #                   """k1="{k1}" k2="{k2}" k3="{k3}" />\n""".format(
    #             focal_length=focal_length, aperture=aperture, distance=distance, k1=k1, k2=k2, k3=k3))
