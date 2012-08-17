#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import unicode_literals, division, absolute_import

import subprocess, glob, os, os.path, re


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


database_entries = {}
working_directory = os.getcwd()
pto_path = os.path.join(working_directory, "vignetting.pto")

for triplet in triplets:
    exif_data = images[triplet[0]]
    exif_data = (exif_data["Lens Model"], exif_data["Focal Length"].partition(".0 mm")[0], exif_data["Aperture"])
    output_filename = "--".join(exif_data).replace(" ", "_")
    with open("vignetting.pto", "w") as outfile:
        outfile.write(open("/home/bronger/src/vignetting/vignetting.pto").read().format(input_filenames=triplet))
    # FixMe: The makefile should be replaced with a pto2mk call.
    with open("vignetting_second.pto.mk", "w") as outfile:
        outfile.write(open("/home/bronger/src/vignetting/vignetting_second.pto.mk").read().format(
                input_filenames=triplet, output_filename=output_filename, working_directory=working_directory))
    subprocess.check_call(["cpfind", "--sieve1width", "20",  "--sieve1height", "20",
                           "--sieve2width", "10",  "--sieve2height", "10", "-o", pto_path, pto_path])
    subprocess.check_call(["cpclean", "-o", pto_path, pto_path])
    subprocess.check_call(["linefind", "-o", pto_path, pto_path])
    subprocess.check_call(["checkpto", pto_path])
    subprocess.check_call(["ptovariable", "--positions", "--view", "--barrel", pto_path])
    subprocess.check_call(["autooptimiser", "-n", "-l", "-s", "-o", pto_path, pto_path])
    subprocess.check_call(["ptovariable", "--vignetting", "--response", pto_path])
    current_pto = open(pto_path).read()
    processes = []
    for i in range(5):
        process_pto_path = "{0}.{1}.pto".format(pto_path[:-4], i)
        with open(process_pto_path, "w") as pto_file:
            pto_file.write(current_pto)
        processes.append((subprocess.Popen(["vig_optimize", "-p", "100000", "-o", process_pto_path, process_pto_path],
                                           stdout=subprocess.PIPE), process_pto_path))
    vignetting_data = []
    for process, process_pto_path in processes:
        assert process.wait() == 0
        result_parameters = re.search(r" Vb([-.0-9]+) Vc([-.0-9]+) Vd([-.0-9]+) ", open(process_pto_path). \
                                          readlines()[7]).groups()
        vignetting_data.append((float(result_parameters[0]), result_parameters, process_pto_path))
    best_vignetting_data = vignetting_data[len(vignetting_data) // 2 + 1]
    database_entries[exif_data] = best_vignetting_data[1]
    print "Vignetting data:", database_entries[exif_data], [data[1] for data in vignetting_data]
    os.rename(best_vignetting_data[2], pto_path)
    for i, current_data in vignetting_data:
        if i != len(vignetting_data) // 2 + 1:
            os.remove(current_data[2])
    subprocess.check_call(["pano_modify", "--canvas=70%", "--crop=AUTO", "-o", pto_path, pto_path])
    subprocess.check_call(["make", "--makefile", "vignetting_second.pto.mk"])
    database_entries[exif_data] = re.search(r" Vb([-.0-9]+) Vc([-.0-9]+) Vd([-.0-9]+) ", open("vignetting.pto"). \
                                                readlines()[7]).groups()
    for filename in ["vignetting_second.pto.mk", "vignetting.pto"] + \
            ["{0}{1}.tif".format(output_filename, suffix) for suffix in ["0000", "0001", "0002"]]:
        os.remove(filename)


outfile = open("lensfun.xml", "a")

current_lens = None
for configuration in sorted(database_entries):
    lens, focal_length, aperture = configuration
    if lens != current_lens:
        outfile.write("\n\n{0}:\n\n".format(lens))
        current_lens = lens
    outfile.write("""<vignetting model="pa" focal="{focal_length}" aperture="{aperture}" distance="45" """
                  """k1="{vignetting[0]}" k2="{vignetting[1]}" k3="{vignetting[2]}" />\n""".format(
            focal_length=focal_length, aperture=aperture, vignetting=database_entries[configuration]))
