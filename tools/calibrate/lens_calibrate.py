#!/usr/bin/python3

#######################################################################
#
# A script to calibrate camera lenes for lensfun
#
# Copyright (c) 2012-2016 Torsten Bronger <bronger@physik.rwth-aachen.de>
# Copyright (c)      2018 Andreas Schneider <asn@cryptomilk.org>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#######################################################################
#
# Requires: python3-exiv2
# Requires: python3-numpy
# Requires: python3-spicy
#
# Requires: darktable or dcraw
# Requires: hugin-tools (tca_correct)
# Requires: ImageMagick (convert)
#

import os
import argparse
import configparser
import codecs
import re
import math
import numpy
import struct
import subprocess
from subprocess import DEVNULL
from scipy.optimize.minpack import leastsq

from pyexiv2.metadata import ImageMetadata
from pyexiv2.exif import ExifTag

DARKTABLE_DISTORTION_SIDECAR = '''<?xml version="1.0" encoding="UTF-8"?>
<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="XMP Core 4.4.0-Exiv2">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about=""
    xmlns:xmp="http://ns.adobe.com/xap/1.0/"
    xmlns:xmpMM="http://ns.adobe.com/xap/1.0/mm/"
    xmlns:darktable="http://darktable.sf.net/"
   xmp:Rating="1"
   xmpMM:DerivedFrom="7M3_DAYLIGHT.ARW"
   darktable:xmp_version="2"
   darktable:raw_params="0"
   darktable:auto_presets_applied="1"
   darktable:history_end="3">
   <darktable:mask_id>
    <rdf:Seq/>
   </darktable:mask_id>
   <darktable:mask_type>
    <rdf:Seq/>
   </darktable:mask_type>
   <darktable:mask_name>
    <rdf:Seq/>
   </darktable:mask_name>
   <darktable:mask_version>
    <rdf:Seq/>
   </darktable:mask_version>
   <darktable:mask>
    <rdf:Seq/>
   </darktable:mask>
   <darktable:mask_nb>
    <rdf:Seq/>
   </darktable:mask_nb>
   <darktable:mask_src>
    <rdf:Seq/>
   </darktable:mask_src>
   <darktable:history>
    <rdf:Seq>
     <rdf:li
      darktable:operation="sharpen"
      darktable:enabled="1"
      darktable:modversion="1"
      darktable:params="000000400000003f0000003f"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="7"
      darktable:blendop_params="gz12eJxjYGBgkGAAgRNODESDBnsIHll8ANNSGQM="/>
     <rdf:li
      darktable:operation="flip"
      darktable:enabled="1"
      darktable:modversion="2"
      darktable:params="ffffffff"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="7"
      darktable:blendop_params="gz12eJxjYGBgkGAAgRNODESDBnsIHll8ANNSGQM="/>
     <rdf:li
      darktable:operation="basecurve"
      darktable:enabled="1"
      darktable:modversion="5"
      darktable:params="gz09eJxjYIAAruuLrbmuK1vPmilpN2vmTLuzZ87YGRsb2zMwONgbGxcD6QYoHgVDCbAhsZkwZCFxCgBDtg6p"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="7"
      darktable:blendop_params="gz12eJxjYGBgkGAAgRNODESDBnsIHll8ANNSGQM="/>
    </rdf:Seq>
   </darktable:history>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>
'''

DARKTABLE_TCA_SIDECAR = '''<?xml version="1.0" encoding="UTF-8"?>
<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="XMP Core 4.4.0-Exiv2">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about=""
    xmlns:xmp="http://ns.adobe.com/xap/1.0/"
    xmlns:xmpMM="http://ns.adobe.com/xap/1.0/mm/"
    xmlns:darktable="http://darktable.sf.net/"
   xmp:Rating="1"
   xmpMM:DerivedFrom="7M3_DAYLIGHT.ARW"
   darktable:xmp_version="2"
   darktable:raw_params="0"
   darktable:auto_presets_applied="1"
   darktable:history_end="3">
   <darktable:mask_id>
    <rdf:Seq/>
   </darktable:mask_id>
   <darktable:mask_type>
    <rdf:Seq/>
   </darktable:mask_type>
   <darktable:mask_name>
    <rdf:Seq/>
   </darktable:mask_name>
   <darktable:mask_version>
    <rdf:Seq/>
   </darktable:mask_version>
   <darktable:mask>
    <rdf:Seq/>
   </darktable:mask>
   <darktable:mask_nb>
    <rdf:Seq/>
   </darktable:mask_nb>
   <darktable:mask_src>
    <rdf:Seq/>
   </darktable:mask_src>
   <darktable:history>
    <rdf:Seq>
     <rdf:li
      darktable:operation="flip"
      darktable:enabled="1"
      darktable:modversion="2"
      darktable:params="ffffffff"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="7"
      darktable:blendop_params="gz12eJxjYGBgkGAAgRNODESDBnsIHll8ANNSGQM="/>
     <rdf:li
      darktable:operation="basecurve"
      darktable:enabled="0"
      darktable:modversion="5"
      darktable:params="gz09eJxjYICAL3eYbKcsErU1fXPdVmRLpl1B+T07pyon+6WC0fb9R6rtGRgaoHgUDCXAhsRmwpCFxCkAdoEQ3Q=="
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="7"
      darktable:blendop_params="gz12eJxjYGBgkGAAgRNODESDBnsIHll8ANNSGQM="/>
     <rdf:li
      darktable:operation="sharpen"
      darktable:enabled="0"
      darktable:modversion="1"
      darktable:params="000000400000003f0000003f"
      darktable:multi_name=""
      darktable:multi_priority="0"
      darktable:blendop_version="7"
      darktable:blendop_params="gz12eJxjYGBgkGAAgRNODESDBnsIHll8ANNSGQM="/>
    </rdf:Seq>
   </darktable:history>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>
'''

def is_raw_file(filename):
    raw_file_extensions = [
            ".3FR", ".ARI", ".ARW", ".BAY", ".CRW", ".CR2", ".CAP", ".DCS",
            ".DCR", ".DNG", ".DRF", ".EIP", ".ERF", ".FFF", ".IIQ", ".K25",
            ".KDC", ".MEF", ".MOS", ".MRW", ".NEF", ".NRW", ".OBM", ".ORF",
            ".PEF", ".PTX", ".PXN", ".R3D", ".RAF", ".RAW", ".RWL", ".RW2",
            ".RWZ", ".SR2", ".SRF", ".SRW", ".X3F", ".JPG", ".JPEG", ".TIF",
            ".TIFF",
        ]
    file_ext = os.path.splitext(filename)[1]

    return file_ext.upper() in raw_file_extensions

def has_exif_tag(data, tag):
    return tag in data

def image_read_exif(filename):
    data = ImageMetadata(filename)

    # This reads the metadata and closes the file
    data.read()

    lens_model = None
    tag = 'Exif.Photo.LensModel'
    if has_exif_tag(data, tag):
        lens_model = data[tag].value
    else:
        tag = 'Exif.NikonLd3.LensIDNumber'
        if has_exif_tag(data, tag):
            lens_model = data[tag].human_value

        tag = 'Exif.Panasonic.LensType'
        if has_exif_tag(data, tag):
            lens_model = data[tag].value

        tag = 'Exif.Sony1.LensID'
        if has_exif_tag(data, tag):
            lens_model = data[tag].human_value

        tag = 'Exif.Minolta.LensID'
        if has_exif_tag(data, tag):
            lens_model = data[tag].human_value

    if lens_model is None:
       lens_model = 'Standard'

    tag = 'Exif.Photo.FocalLength'
    if has_exif_tag(data, tag):
        focal_length = float(data[tag].value)
    else:
        print("%s doesn't have Exif.Photo.FocalLength set. " +
              "Please fix it manually.")

    tag = 'Exif.Photo.FNumber'
    if has_exif_tag(data, tag):
        aperture = float(data[tag].value)
    else:
        print("%s doesn't have Exif.Photo.FNumber set. " +
              "Please fix it manually.")

    return { "lens_model" : lens_model, "focal_length" : focal_length, "aperture" : aperture }

# convert raw file to 16bit tiff
def convert_raw_for_distortion(input_file, output_file=None):
    if output_file is None:
        output_file = ("%s.tif" % os.path.splitext(input_file)[0])
    sidecar_file = (os.path.join(os.path.dirname(output_file), "distortion.xmp"))

    if not os.path.isfile(sidecar_file):
        with open(sidecar_file, 'w') as f:
            f.write(DARKTABLE_DISTORTION_SIDECAR)

    if not os.path.exists(output_file):
        print("Converting %s to %s ..." % (input_file, output_file), end='')
        cmd = [
                "darktable-cli",
                input_file,
                sidecar_file,
                output_file,
                "--core",
                "--conf", "plugins/lighttable/export/iccprofile=image",
                "--conf", "plugins/lighttable/export/style=none",
                "--conf", "plugins/imageio/format/tiff/bpp=16",
                "--conf", "plugins/imageio/format/tiff/compress=5"
            ]
        try:
            subprocess.check_call(cmd, stdout=DEVNULL, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError:
            raise
        except OSError:
            print("Could not find darktable-cli")
            return None

        print(" DONE")

    return output_file

def convert_raw_for_tca_or_vignetting(input_file, output_file=None):
    if output_file is None:
        output_file = ("%s.ppm" % os.path.splitext(input_file)[0])
    sidecar_file = (os.path.join(os.path.dirname(output_file), "darktable.xmp"))

    if not os.path.isfile(sidecar_file):
        with open(sidecar_file, 'w') as f:
            f.write(DARKTABLE_TCA_SIDECAR)

    if not os.path.exists(output_file):
        cmd = [
                "darktable-cli",
                input_file,
                sidecar_file,
                output_file,
                "--core",
                "--conf", "plugins/lighttable/export/iccprofile=image",
                "--conf", "plugins/lighttable/export/style=none",
            ]
        try:
            subprocess.check_call(cmd, stdout=DEVNULL, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError:
            raise
        except OSError:
            print("Could not find darktable-cli")

    return output_file

def convert_ppm_for_vignetting(input_file):
    output_file = ("%s.pgm" % os.path.splitext(input_file)[0])

    if not os.path.exists(output_file):
        # FIXME Is the smaller size used for supplying averaged
        # pixel values for the plotting?
        cmd = [ "convert",
                input_file,
                '-set',
                'colorspace',
                'RGB',
                '-resize',
                '400',
                output_file ]
        try:
            subprocess.check_call(cmd, stdout=DEVNULL, stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError:
            raise
        except OSError:
            print("Could not find convert")

    return output_file

def create_lenses_config(lenses_exif_group):
    config = configparser.ConfigParser()
    for lenses in lenses_exif_group:
        config[lenses] = {
                'maker' : '[unknown]',
                'mount' : '[unknown]',
                'cropfactor' : '1.0',
                'aspect_ratio' : '3:2',
                'type' : 'normal'
                }
        for exif_data in lenses_exif_group[lenses]:
            distortion = ("distortion(%dmm)" % exif_data['focal_length'])
            config[lenses][distortion] = '0.0, 0.0, 0.0'
    with open('lenses.conf', 'w') as configfile:
        config.write(configfile)

    print("A template has been created for distortion corrections as lenses.conf.")
    print("Please fill this file with proper information. The most important")
    print("values are:")
    print("")
    print("maker:        is the manufacturer or the lens, e.g. 'FE 16-35mm F2.8 GM'")
    print("mount:        is the name of the mount system, e.g. 'Sony E'")
    print("cropfactor:   is the crop factor of the camera as a float, e.g. '1.0' for")
    print("              full frame")
    print("aspect_ratio: is the aspect_ratio, e.g. '3:2'")
    print("type:         is the type of the lens, e.g. 'normal' for rectilinear")
    print("              lenses. Other possible values are: stereographic, equisolid,")
    print("              stereographic, panoramic or fisheye.")
    print("")
    print("You can find details for distortion calculations here:")
    print("")
    print("https://hackmd.io/s/SkOIRlr5z#")

    return

def parse_lenses_config(filename):
    config = configparser.ConfigParser()
    config.read(filename)

    lenses = {}

    for section in config.sections():
        lenses[section] = {}
        lenses[section]['distortion'] = {}
        lenses[section]['tca'] = {}
        lenses[section]['vignetting'] = {}

        for key in config[section]:
            if key.startswith('distortion'):
                focal_length = key[11:len(key) - 3]
                lenses[section]['distortion'][focal_length] = config[section][key]
            else:
                lenses[section][key] = config[section][key]

    return lenses

def tca_correct(input_file, original_file, exif_data, complex_tca=False):
    output_file = ("%s.tca" % os.path.splitext(input_file)[0])

    if not os.path.exists(output_file):
        print("Running TCA corrections for %s ..." % (input_file), end='')
        tca_complexity = 'v'
        if complex_tca:
            tca_complexity = 'bv'
        cmd = [ "tca_correct", "-o", tca_complexity, input_file ]
        try:
            output = subprocess.check_output(cmd, stderr=DEVNULL)
        except subprocess.CalledProcessError:
            raise
        except OSError:
            print("Could not find darktable-cli")
            return None

        tca_data = re.match(r"-r [.0]+:(?P<br>[-.0-9]+):[.0]+:(?P<vr>[-.0-9]+) -b [.0]+:(?P<bb>[-.0-9]+):[.0]+:(?P<vb>[-.0-9]+)",
                            output.decode('ascii')).groupdict()

        tca_config = configparser.ConfigParser()
        tca_config[exif_data['lens_model']] = {
                'focal_length' : exif_data['focal_length'],
                'complex_tca' : complex_tca,
                'tca' : output.decode('ascii'),
                'br' : tca_data['br'],
                'vr' : tca_data['vr'],
                'bb' : tca_data['bb'],
                'vb' : tca_data['vb'],
                }
        with open(output_file, "w") as tcafile:
            tca_config.write(tcafile)

        if complex_tca:
            gp_file = ("%s.gp" % output_file)
            with open(gp_file, "w") as f:
                f.write('set title "%s" noenhanced\n' % original_file)
                f.write('plot [0:1.8] %s * x**2 + %s title "red", %s * x**2 + %s title "blue"\n' %
                        (tca_data['br'], tca_data["vr"], tca_data["bb"], tca_data["vb"]))
                f.write('pause -1')

        print(" DONE")

def fit_function(radius, A, k1, k2, k3):
    return A * (1 + k1 * radius**2 + k2 * radius**4 + k3 * radius**6)

def calculate_vignetting(input_file, sidecar_file=None, exif_data=None):
    basename = os.path.splitext(input_file)[0]
    all_points_filename = ("%s.all_points.dat" % basename)
    bins_filename = ("%s.bins.dat" % basename)
    gp_filename = ("%s.gp" % basename)
    vig_filename = ("%s.vig" % basename)

    # TODO not supported yet
    distance = float('inf')

    if os.path.exists(vig_filename):
        return

    print("Generating vignetting data for %s ... " % input_file, end='')

    content = ''
    with open(input_file, 'rb') as f:
        content = f.read()

    width, height = None, None
    header_size = 0
    for i, line in enumerate(content.splitlines(True)):
        header_size += len(line)
        if i == 0:
            assert line == b"P5\n", "Wrong image format (must be NetPGM binary)"
        else:
            line = line.partition(b"#")[0].strip()
            if line:
                if not width:
                    width, height = line.split()
                    width, height = int(width), int(height)
                else:
                    assert line == b"65535", "Wrong grayscale depth: {} (must be 65535)".format(int(line))
                    break

    half_diagonal = math.hypot(width // 2, height // 2)
    image_data = struct.unpack("!{0}s{1}H".format(header_size, width * height), content)[1:]

    radii, intensities = [], []

    maximal_radius = 1.0
    #
    # The file content should look like this:
    #
    # [vignetting]
    # maximal_radius = 0.8
    #
    if sidecar_file:
        sidecar_config = configparser.ConfigParser()
        sidecar_config.read(sidecar_filename)
        maximal_radius = float(sidecar_config['vignetting']['maximal_radius'])
        if maximal_radius > 1.0 or maximal_radius < 0.0:
            maximal_radius = 1.0

    for i, intensity in enumerate(image_data):
        y, x = divmod(i, width)
        radius = math.hypot(x - width // 2, y - height // 2) / half_diagonal
        if radius <= maximal_radius:
            radii.append(radius)
            intensities.append(intensity)

    with open(all_points_filename, 'w') as f:
        for radius, intensity in zip(radii, intensities):
            f.write("%f %d\n" % (radius, intensity))

    number_of_bins = 16
    bins = [[] for i in range(number_of_bins)]
    for radius, intensity in zip(radii, intensities):
        # The zeroth and the last bin are only half bins which means that their
        # means are skewed.  But this is okay: For the zeroth, the curve is
        # supposed to be horizontal anyway, and for the last, it underestimates
        # the vignetting at the rim which is a good thing (too much of
        # correction is bad).
        bin_index = int(round(radius / maximal_radius * (number_of_bins - 1)))
        bins[bin_index].append(intensity)
    radii = [i / (number_of_bins - 1) * maximal_radius for i in range(number_of_bins)]
    intensities = [numpy.median(bin) for bin in bins]

    with open(bins_filename, 'w') as f:
        for radius, intensity in zip(radii, intensities):
            f.write("%f %d\n" % (radius, intensity))

    radii, intensities = numpy.array(radii), numpy.array(intensities)

    A, k1, k2, k3 = leastsq(lambda p, x, y: y - fit_function(x, *p), [30000, -0.3, 0, 0], args=(radii, intensities))[0]

    vig_config = configparser.ConfigParser()
    vig_config[exif_data['lens_model']] = {
                'focal_length' : exif_data['focal_length'],
                'aperture' : exif_data['aperture'],
                'distance' : distance,
                'A' : A,
                'k1' : k1,
                'k2' : k2,
                'k3' : k3,
                }
    with open(vig_filename, "w") as vigfile:
        vig_config.write(vigfile)

    if distance == float("inf"):
        distance = "∞"
    with codecs.open(gp_filename, "w", encoding="utf-8") as c:
        c.write('set grid\n')
        c.write('set title "%s, %f mm, f/%0.1f, %s m" noenhanced\n' %
                (exif_data['lens_model'], exif_data['focal_length'],
                 exif_data['aperture'], distance))
        c.write('plot "%s" with dots title "samples", ' %
                all_points_filename)
        c.write('"%s" with linespoints lw 4 title "average", ' %
                bins_filename)
        c.write('%f * (1 + (%f) * x**2 + (%f) * x**4 + (%f) * x**6) title "fit"\n' %
                (A, k1, k2, k3))
        c.write('pause -1')

    print(" DONE")

def init():
    # Create directory structure
    dirlist = ['distortion', 'tca', 'vignetting']

    for d in dirlist:
        if os.path.isfile(d):
            print("ERROR: '%s' is a file, can't create directory!" % d)
            return
        elif not os.path.isdir(d):
            os.mkdir(d)

    print("The following directory structure has been created in the "
          "local directory\n\n"
          "1. distortion - Put RAW file created for distortion in here\n"
          "2. tca        - Put chromatic abbrevation RAW files in here\n"
          "3. vignetting - Put RAW files to calculate vignetting in here\n")

def run_distortion():
    lenses_config_exists = os.path.isfile('lenses.conf')
    lenses_exif_group = {}

    print('Running distortion corrections ...')

    if not os.path.isdir("distortion/exported"):
        os.mkdir("distortion/exported")

    for path, directories, files in os.walk('distortion'):
        for filename in files:
            if path != "distortion":
                continue
            if not is_raw_file(filename):
                continue

            input_file = os.path.join(path, filename)
            output_file = os.path.join(path, "exported", ("%s.tif" % os.path.splitext(filename)[0]))

            exif_data = image_read_exif(input_file)
            if exif_data is not None:
                if exif_data['lens_model'] not in lenses_exif_group:
                    lenses_exif_group[exif_data['lens_model']] = []
                lenses_exif_group[exif_data['lens_model']].append(exif_data)

                # Add focal length to file name for easier identification
                if exif_data['focal_length'] > 1.0:
                    output_file = os.path.join(path, "exported", ("%s_%dmm.tif" % (os.path.splitext(filename)[0], exif_data['focal_length'])))

            # Convert RAW files to TIF for hugin
            output_file = convert_raw_for_distortion(input_file, output_file)

    if not lenses_config_exists:
        sorted_lenses_exif_group = {}
        for lenses in sorted(lenses_exif_group):
            # TODO: Remove duplicates?
            sorted_lenses_exif_group[lenses] = sorted(lenses_exif_group[lenses], key=lambda exif : exif['focal_length'])

        create_lenses_config(sorted_lenses_exif_group)

def run_tca(complex_tca):
    if not os.path.isdir("tca/exported"):
        os.mkdir("tca/exported")

    for path, directories, files in os.walk('tca'):
        for filename in files:
            if path != "tca":
                continue

            # Convert RAW files to tiff for tca_correct
            input_file = os.path.join(path, filename)

            exif_data = image_read_exif(input_file)

            output_file = os.path.join(path, "exported", ("%s.ppm" % os.path.splitext(filename)[0]))
            output_file = convert_raw_for_tca_or_vignetting(input_file, output_file)

            tca_correct(output_file, input_file, exif_data, complex_tca)

def run_vignetting():
    if not os.path.isdir("vignetting/exported"):
        os.mkdir("vignetting/exported")

    for path, directories, files in os.walk('vignetting'):
        for filename in files:
            if path != "vignetting":
                continue

            # Convert RAW files to tiff for tca_correct
            input_file = os.path.join(path, filename)
            sidecar_file = ("%s.rad" % os.path.splitext(filename)[0])
            if not os.path.exists(sidecar_file):
                sidecar_file = None

            output_file = os.path.join(path, "exported", ("%s.ppm" % os.path.splitext(filename)[0]))

            exif_data = image_read_exif(input_file)

            output_file = os.path.join(path, "exported", ("%s.ppm" % os.path.splitext(filename)[0]))
            output_file = convert_raw_for_tca_or_vignetting(input_file, output_file)

            pgm_file = convert_ppm_for_vignetting(output_file)

            calculate_vignetting(pgm_file, sidecar_file, exif_data)

def run_generate_xml():
    print("Generating lensfun.xml")

    lenses_config_exists = os.path.isfile('lenses.conf')

    if not lenses_config_exists:
        print("lenses.conf doesn't exist, run distortion first")
        return

    # We need maker, model, mount, crop_factor etc.
    lenses = parse_lenses_config('lenses.conf')

    # Scan tca files and add to lenses
    for path, directories, files in os.walk('tca/exported'):
        for filename in files:
            if os.path.splitext(filename)[1] != '.tca':
                continue

            config = configparser.ConfigParser()
            config.read(os.path.join(path, filename))

            for lens_model in config.sections():
                focal_length = config[lens_model]['focal_length']

                lenses[lens_model]['tca'][focal_length] = {}

                for key in config[lens_model]:
                    if key != 'focal_length':
                        lenses[lens_model]['tca'][focal_length][key] = config[lens_model][key]

    # Scan vig files and add to lenses
    for path, directories, files in os.walk('vignetting/exported'):
        for filename in files:
            if os.path.splitext(filename)[1] != '.vig':
                continue

            config = configparser.ConfigParser()
            config.read(os.path.join(path, filename))

            for lens_model in config.sections():
                focal_length = config[lens_model]['focal_length']

                lenses[lens_model]['vignetting'][focal_length] = {}

                for key in config[lens_model]:
                    if key != 'focal_length':
                        lenses[lens_model]['vignetting'][focal_length][key] = config[lens_model][key]

    # write lenses to xml
    with open('lensfun.xml', 'w') as f:
        f.write('<lensdatabase>\n')
        for lens_model in lenses:
            f.write('    <lens>\n')
            f.write('        <maker>%s</maker>\n' % lenses[lens_model]['maker'])
            f.write('        <model>%s</model>\n' % lens_model)
            f.write('        <mount>%s</mount>\n' % lenses[lens_model]['mount'])
            f.write('        <cropfactor>%s</cropfactor>\n' % lenses[lens_model]['cropfactor'])
            if lenses[lens_model]['type'] != 'normal':
                f.write('        <type>%s</type>\n' % lenses[lens_model]['type'])

            # Add calibration data
            f.write('        <calibration>\n')

            # Add distortion entries
            for focal_length in lenses[lens_model]['distortion']:
                data = list(map(str.strip, lenses[lens_model]['distortion'][focal_length].split(',')))
                if data[1] is None:
                    f.write('           '
                            '<distortion model="poly3" focal="%s" k1="%s" />\n' %
                            (focal_length, data[0]))
                else:
                    f.write('           '
                            '<distortion model="ptlens" focal="%s" a="%s" b="%s" c="%s" />\n' %
                            (focal_length, data[0], data[1], data[2]))

            # Add tca entries
            for focal_length in lenses[lens_model]['tca']:
                data = lenses[lens_model]['tca'][focal_length]
                if data['complex_tca'] == 'True':
                    f.write('           '
                            '<tca model="poly3" focal="%s" br="%s" vr="%s" bb="%s" vb="%s" />\n' %
                            (focal_length, data['br'], data['vr'], data['bb'], data['vb']))
                else:
                    f.write('           '
                            '<tca model="poly3" focal="%s" vr="%s" vb="%s" />\n' %
                            (focal_length, data['vr'], data['vb']))

            # Add vignetting entries
            for focal_length in lenses[lens_model]['vignetting']:
                data = lenses[lens_model]['vignetting'][focal_length]
                for distance in [ '10', '1000' ]:
                    f.write('           '
                            '<vignetting model="pa" focal="%s" aperture="%s" distance="%s" '
                            'k1="%s" k2="%s" k3="%s" />\n' %
                            (focal_length, data['aperture'], distance,
                             data['k1'], data['k2'], data['k3']))

            f.write('        </calibration>\n')
            f.write('    </lens>\n')
        f.write('</lensdatabase>\n')

class CustomDescriptionFormatter(argparse.ArgumentDefaultsHelpFormatter, argparse.RawDescriptionHelpFormatter):
    pass

def main():

    description = '''
This is an overview about the calibration steps.\n
\n
To setup the required directory structure simply run:

    lens_calibrate.py init

The next step is to copy the RAW files you created to the corresponding
directories.

Once you have done that run:

    lens_calibrate.py distortion

This will create tiff file you can use to figure out the the lens distortion
values (a), (b) and (c) using hugin. It will also create a lenses.conf where
you need to fill out missing values.

If you don't want to do distortion corrections you need to create the
lenses.conf file manually. It needs to look like this:

    [MODEL NAME]
    maker =
    mount =
    cropfactor =
    aspect_ratio =
    type =

The section name needs to be the lens model name you can figure out with:

    exiv2 -g LensModel -pt <raw file>

The required options are:

maker:        is the manufacturer or the lens, e.g. 'FE 16-35mm F2.8 GM'
mount:        is the name of the mount system, e.g. 'Sony E'
cropfactor:   is the crop factor of the camera as a float, e.g. '1.0' for full
              frame
aspect_ratio: is the aspect ratio of your camera, normally it is '3:2'
type:         is the type of the lens, e.g. 'normal' for rectilinear lenses.
              Other possible values are: stereographic, equisolid, stereographic,
              panoramic or fisheye.

If you want TCA corrections just run:

    lens_calibrate.py tca

If you want vignetting corrections run:

    lens_calibrate.py vignetting

Once you have created data for all corrections you can generate an xml file
which can be consumed by lensfun. Just call:

    lens_calibrate.py generate_xml

To use the data in your favourite software you just have to copy the generated
lensfun.xml file to:

    ~/.local/share/lensfun/

Create a bug report or pull request to add the lens to the project at:

https://github.com/lensfun/lensfun/

-----------------------------

'''

    parser = argparse.ArgumentParser(description=description,
                                     formatter_class=CustomDescriptionFormatter)

    parser.add_argument('--complex-tca',
                        action='store_true',
                        help='Turns on non-linear polynomials for TCA')
    #parser.add_argument('-r, --rawconverter', choices=['darktable', 'dcraw'])

    parser.add_argument('action',
                        choices=[
                            'init',
                            'distortion',
                            'tca',
                            'vignetting',
                            'generate_xml'],
                        help='This runs one of the actions for lens calibration')

    args = parser.parse_args()

    if args.action == 'init':
        init()
    elif args.action == 'distortion':
        run_distortion()
    elif args.action == 'tca':
        run_tca(args.complex_tca)
    elif args.action == 'vignetting':
        run_vignetting()
    elif args.action == 'generate_xml':
        run_generate_xml()

if __name__ == "__main__":
    main()
