#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""This program generates a bitmap which simulates lens errors.  Then, Lensfun
can be used to rectify them.  This way, one can check whether Lensfun is
working properly.  The test image contains vignetting and a rectangular grid.
The grid lines exhibit distortion and TCA.  The lens projection can be
non-rectilinear.

External dependencies: Python version 3, ImageMagick, exiv2.

The program takes the lens parameters from Lensfun's database.  To the usual
search path it adds the current path (the xml files of which would override
those found in the usual locations).

As for the used camera, it takes the crop factor from the database.  The aspect
ratio needs to be provided on the command line.

It is sensible to read the help output of this program (the --help option).

The following things are particularly interesting to check:

1. Vignetting, TCA, and distortion need to be corrected fully (apart from
   rounding errors)

    Note that for seeing the perfect grid, you must transform fisheyes into
    rectilinear.

2. Vignetting has to be corrected first, then TCA, then distortion.

    This is because this is the way they are calibrated: Vignetting is
    calibrated on a totally uncorrected image.  TCA is so, too, however, TCA
    doesn't care about vignetting noticably.  Well, and then the image is
    un-distorted.

    Note that Lensfun seems to work the other way round.  But this is only
    because it uses the corrected destination image as the starting point: It
    uses the distortion formula to find the pixel on the distorted sensor
    image, then it applies the TCA correction formula to find the subpixel
    position, and finally, it applies the vignetting.

3. If he crop factor of the camera does not match the one of the calibration,
   it must work nevertheless.

    If you have a sensor smaller than the one used for calibration, this
    program testimage.py has an easy job: It just has to use the corresponsing
    inner part of the resulting picture.

    However, for Lensfun, it is more difficult because it works on the
    destination image.  But is must still work properly.

4. If the aspect ratio of the camera does not match the one of the calibration,
   it must work nevertheless.

    Classical case: Calibration with APS-C, but to-be-corrected image taken
    with a Four-Thirds sensor.

5. An insanely small calibration sensor must not affect pure geometry
   transformation.

    I stumbled upon this one accidentally.  If the cropfactor of the <lens> tag
    is very large, e.g. 10 or 100, a correction is rejected because the
    to-be-corrected sensor is larger (much larger actually).  However, one can
    use at least convert from fisheye to rectilinear, where the cropfactor of
    the calibration sensor should not matter.  Due to ``size - 1`` occurences
    in modifier.cpp, strange things happened, because the ``size`` didn't refer
    to pixel size at this point.

"""

import array, subprocess, math, os, argparse, sys
from math import sin, tan, atan, floor, ceil, sqrt
from math import pi as π
from xml.etree import ElementTree
import lensfun


parser = argparse.ArgumentParser(description="Generate test images for Lensfun.")
parser.add_argument("lens_model_name", metavar="lens", help="Lens model name.  Must match an entry in Lensfun's database exactly.")
parser.add_argument("camera_model_name", metavar="camera",
                    help="Camera model name.  Must match an entry in Lensfun's database exactly.")
parser.add_argument("focal_length", type=float,
                    help="Focal length in mm.  Must match an entry in Lensfun's database exactly, no interpolation is done.")
parser.add_argument("aperture", type=float,
                    help="Aperture in f-stops.  Must match an entry in Lensfun's database exactly, no interpolation is done.")
parser.add_argument("distance", type=float,
                    help="Distance in metres.  Must match an entry in Lensfun's database exactly, no interpolation is done.")
parser.add_argument("--width", type=int, default=600, help="The resulting bitmap's long edge width in pixels.  Default: 600")
parser.add_argument("--aspect-ratio", type=float, default=1.5, help="The resulting bitmap's aspect ratio, >= 1.  Default: 1.5")
parser.add_argument("--portrait", action="store_true",
                    help="Whether the resulting bitmap should be in portrait orientation.  Default: landscape")
parser.add_argument("--outfile", default="testimage.tiff", help="Path to the output file.  Default: testimage.tiff")
parser.add_argument("--db-path", help="Path to the database.  If not given, look in the same places as Lensfun.")
parser.add_argument("--no-vignetting", dest="vignetting", action="store_false",
                    help="Supresses simulation of vignetting.  *Much* faster.")
args = parser.parse_args()
lens_model_name, camera_model_name, focal_length, aperture, distance = \
                                args.lens_model_name, args.camera_model_name, args.focal_length, args.aperture, args.distance
focal_length, aperture, distance = float(focal_length), float(aperture), float(distance)
width, aspect_ratio, portrait = args.width, args.aspect_ratio, args.portrait


def get_database_elements():
    lens_element = camera_element = None
    distortion_element = tca_element = vignetting_element = real_focal_length_element = fov_element = None
    files_found = False
    def crawl_directory(dirpath):
        nonlocal lens_element, camera_element, distortion_element, tca_element, vignetting_element, real_focal_length_element, \
            fov_element, files_found
        for root, __, filenames in os.walk(dirpath):
            for filename in filenames:
                if filename.endswith(".xml"):
                    files_found = True
                    tree = ElementTree.parse(os.path.join(root, filename)).getroot()
                    for element in tree:
                        if camera_element is None and \
                           element.tag == "camera" and element.find("model").text == camera_model_name:
                            camera_element = element
                        elif element.tag == "lens" and element.find("model").text == lens_model_name:
                            if lens_element is not None:
                                print("This program cannot handle lens model names that occur multiple times in the\n"
                                      "read XML files.  Please use another lens.")
                                sys.exit(1)
                            lens_element = element
                            if element.find("calibration") is not None:
                                for calibration_element in element.find("calibration"):
                                    if calibration_element.tag == "distortion" and \
                                       float(calibration_element.attrib["focal"]) == focal_length:
                                        distortion_element = calibration_element
                                    elif calibration_element.tag == "tca" and \
                                       float(calibration_element.attrib["focal"]) == focal_length:
                                        tca_element = calibration_element
                                    elif calibration_element.tag == "vignetting" and \
                                       float(calibration_element.attrib["focal"]) == focal_length and \
                                       float(calibration_element.attrib["aperture"]) == aperture and \
                                       float(calibration_element.attrib["distance"]) == distance:
                                        vignetting_element = calibration_element
                                    elif calibration_element.tag == "real-focal-length" and \
                                       float(calibration_element.attrib["focal"]) == focal_length:
                                        real_focal_length_element = calibration_element
                                    elif calibration_element.tag == "field_of_view" and \
                                       float(calibration_element.attrib["focal"]) == focal_length:
                                        fov_element = calibration_element
    paths_search_list = [args.db_path] if args.db_path else lensfun.get_database_directories()
    for path in paths_search_list:
        crawl_directory(path)
    if not files_found:
        print("No XML files found.")
        sys.exit(1)
    if lens_element is None:
        print("Lens model name not found.")
        sys.exit(1)
    if camera_element is None:
        print("Camera model name not found.")
        sys.exit(1)
    return lens_element, camera_element, distortion_element, tca_element, vignetting_element, real_focal_length_element, fov_element

lens_element, camera_element, distortion_element, tca_element, vignetting_element, real_focal_length_element, fov_element = \
        get_database_elements()


camera_cropfactor = float(camera_element.find("cropfactor").text)
lens_cropfactor = float(lens_element.find("cropfactor").text)
R_cf = lens_cropfactor / camera_cropfactor
if lens_element.find("center") is not None:
    center_x = float(lens_element.find("center").get("x", "0"))
    center_y = float(lens_element.find("center").get("y", "0"))
else:
    center_x = center_y = 0

def get_lens_aspect_ratio(lens_element):
    try:
        aspect_ratio = lens_element.find("aspect-ratio").text
    except AttributeError:
        return 1.5
    if ":" in aspect_ratio:
        numerator, denominator = aspect_ratio.split(":")
        return int(numerator) / int(denominator)
    return float(aspect_ratio)
lens_aspect_ratio = get_lens_aspect_ratio(lens_element)


def get_float_attribute(element, attribute_name, default=0):
    try:
        return float(element.attrib[attribute_name])
    except KeyError:
        return default

try:
    lens_type = lens_element.find("type").text
except AttributeError:
    lens_type = "rectilinear"

def get_hugin_correction():
    """Get the correction factor for the focal length.  This is necessary because
    the Hugin distortion models PTLens and Poly3 menipulate the focal length.
    """
    if distortion_element is not None:
        model = distortion_element.attrib["model"]
        if model == "ptlens":
            a = get_float_attribute(distortion_element, "a")
            b = get_float_attribute(distortion_element, "b")
            c = get_float_attribute(distortion_element, "c")
            return 1 - a - b - c
        elif model == "poly3":
            k1 = get_float_attribute(distortion_element, "k1")
            return 1 - k1
    return 1
hugin_correction = get_hugin_correction()

def get_real_focal_length():
    if real_focal_length_element is not None:
        return float(real_focal_length_element.attrib["real-focal"])
    elif fov_element is not None:
        fov = float(fov_element.attrib["fov"]) * π / 180
        half_width_in_millimeters = \
                sqrt(36**2 + 24**2) / 2 / sqrt(lens_aspect_ratio**2 + 1) * lens_aspect_ratio / lens_cropfactor
        if lens_type == "stereographic":
            result = half_width_in_millimeters / (2 * tan(fov / 4))
        elif lens_type in ["fisheye", "panoramic", "equirectangular"]:
            result = half_width_in_millimeters / (fov / 2)
        elif lens_type == "orthographic":
            result = half_width_in_millimeters / sin(fov / 2)
        elif lens_type == "equisolid":
            result = half_width_in_millimeters / (2 * sin(fov / 4))
        elif lens_type == "fisheye_thoby":
            result = half_width_in_millimeters / (1.47 * sin(0.713 * fov / 2))
        else:
            assert lens_type == "rectilinear"
            result = half_width_in_millimeters / tan(fov / 2)
        return result * hugin_correction
    else:
        # Interpret the nominal focal length Hugin-compatibly; it's guesswork
        # and a fallback anyway.
        return focal_length * hugin_correction

def get_projection_function():
    projection = None
    if lens_type == "stereographic":
        def projection(ϑ):
            return 2 * tan(ϑ / 2)
    elif lens_type == "fisheye":
        def projection(ϑ):
            return ϑ
    elif lens_type == "panoramic":
        assert False, "Panoramic lenses are not yet supported."
    elif lens_type == "equirectangular":
        assert False, "Equirectangular lenses are not yet supported."
    elif lens_type == "orthographic":
        def projection(ϑ):
            return sin(ϑ)
    elif lens_type == "equisolid":
        def projection(ϑ):
            return 2 * sin(ϑ / 2)
    elif lens_type == "fisheye_thoby":
        def projection(ϑ):
            return 1.47 * sin(0.713 * ϑ)
    return projection
projection = get_projection_function()
    
def get_distortion_function():
    def distortion(r):
        return r
    if distortion_element is not None:
        model = distortion_element.attrib["model"]
        if model == "ptlens":
            a = get_float_attribute(distortion_element, "a")
            b = get_float_attribute(distortion_element, "b")
            c = get_float_attribute(distortion_element, "c")
            def distortion(r):
                return a * r**4 + b * r**3 + c * r**2 + (1 - a - b - c) * r
        elif model == "poly3":
            k1 = get_float_attribute(distortion_element, "k1")
            def distortion(r):
                return k1 * r**3 + (1 - k1) * r
        elif model == "poly5":
            k1 = get_float_attribute(distortion_element, "k1")
            k2 = get_float_attribute(distortion_element, "k2")
            def distortion(r):
                return r * (1 + k1 * r**2 + k2 * r**4)
    return distortion
distortion = get_distortion_function()

def get_tca_functions():
    def tca_red(r):
        return r
    def tca_blue(r):
        return r
    if tca_element is not None:
        model = tca_element.attrib["model"]
        if model == "linear":
            kr = get_float_attribute(tca_element, "kr", 1)
            def tca_red(r):
                return r * kr
            kb = get_float_attribute(tca_element, "kb", 1)
            def tca_blue(r):
                return r * kb
        elif model == "poly3":
            br = get_float_attribute(tca_element, "br")
            cr = get_float_attribute(tca_element, "cr")
            vr = get_float_attribute(tca_element, "vr", 1)
            def tca_red(r):
                return r**3 * br + r**2 * cr + r * vr
            bb = get_float_attribute(tca_element, "bb")
            cb = get_float_attribute(tca_element, "cb")
            vb = get_float_attribute(tca_element, "vb", 1)
            def tca_blue(r):
                return r**3 * bb + r**2 * cb + r * vb
    return tca_red, tca_blue
tca_red, tca_blue = get_tca_functions()

def get_vignetting_function():
    def vignetting(r):
        return 1
    if vignetting_element is not None:
        model = vignetting_element.attrib["model"]
        if model == "pa":
            k1 = get_float_attribute(vignetting_element, "k1")
            k2 = get_float_attribute(vignetting_element, "k2")
            k3 = get_float_attribute(vignetting_element, "k3")
            def vignetting(r):
                return 1 + k1 * r**2 + k2 * r**4 + k3 * r**6
    return vignetting
vignetting = get_vignetting_function()


class Image:

    def __init__(self, width, height):
        """width and height in pixels."""
        self.pixels = array.array("H", width * height * 3 * [16383])
        self.width = width
        self.height = height
        self.half_height = height / 2
        self.half_width = width / 2
        self.half_diagonal = sqrt(width**2 + height**2) / 2
        self.pixel_scaling = (self.height - 1) / 2
        
        # This factor transforms from the vignetting cooredinate system of the
        # to-be-corrected sensor to its distortion coordinate system.
        self.aspect_ratio_correction = sqrt(aspect_ratio**2 + 1)
        
        # This factor transforms from the distortion coordinate system of the
        # to-be-corrected sensor to the distortion coordinate system of the
        # calibration sensor.
        self.ar_plus_cf_correction = (1 / self.aspect_ratio_correction) * R_cf * sqrt(lens_aspect_ratio**2 + 1)

    def add_to_pixel(self, x, y, weighting, red, green, blue):
        """Adds the given brightness (from 0 to 1) to the pixel at the position x, y.
        `weighting` is multiplied with `red`, `green`, and `blue` before it is
        added, so it is some sort of opacity.  Coordinates must be integers.
        Coordinates outside the picture are ignored.
        """
        if 0 < x < self.width and 0 < y < self.height:
            offset = 3 * (y * width + x)
            weighting *= 65535
            self.pixels[offset] = max(min(self.pixels[offset] + int(red * weighting), 65535), 0)
            self.pixels[offset + 1] = max(min(self.pixels[offset + 1] + int(green * weighting), 65535), 0)
            self.pixels[offset + 2] = max(min(self.pixels[offset + 2] + int(blue * weighting), 65535), 0)

    def add_to_position(self, x, y, red, green, blue):
        """Adds the given brightness (from 0 to 1) to the pixel at the position x, y.
        x = y = 0 is the image centre.  The special bit here is that x and y
        are floats in the relative Hugin distortion coordinate system, and
        bilinear interpolation is performed.
        """
        x = (x + aspect_ratio + center_x) * self.pixel_scaling
        y = (y + 1 + center_y) * self.pixel_scaling
        floor_x = int(floor(x))
        floor_y = int(floor(y))
        ceil_x = int(ceil(x)) if x != floor_x else int(x) + 1
        ceil_y = int(ceil(y)) if y != floor_y else int(y) + 1
        self.add_to_pixel(floor_x, floor_y, (ceil_x - x) * (ceil_y - y), red, green, blue)
        self.add_to_pixel(floor_x, ceil_y, (ceil_x - x) * (y - floor_y), red, green, blue)
        self.add_to_pixel(ceil_x, floor_y, (x - floor_x) * (ceil_y - y), red, green, blue)
        self.add_to_pixel(ceil_x, ceil_y, (x - floor_x) * (y - floor_y), red, green, blue)

    def r_vignetting(self, x, y):
        """Returns the r coordinate in the calibration coordinate system to the pixel
        coordinates (x, y) for vignetting, i.e. r = 1 is half-diagonal.
        """
        x = x / self.pixel_scaling - aspect_ratio - center_x
        y = y / self.pixel_scaling - 1 - center_y
        return sqrt(x**2 + y**2) / self.aspect_ratio_correction * R_cf

    def set_vignetting(self, function):
        for y in range(self.height):
            for x in range(self.width):
                offset = 3 * (y * self.width + x)
                for index in range(3):
                    self.pixels[offset + index] = \
                            max(min(int(self.pixels[offset + index] * function(self.r_vignetting(x, y))), 65535), 0)

    def rotate_by_90_degrees(self):
        """This changes the orientation to portrait.  Use this method shortly before
        writing the image, because further operations (creating grid etc) are
        undefined.
        """
        new_pixels = array.array("H", self.width * self.height * 3 * [0])
        for y in range(self.height):
            for x in range(self.width):
                offset_source = 3 * (y * self.width + x)
                offset_destination = 3 * ((self.width - x - 1) * self.height + y)
                new_pixels[offset_destination] = self.pixels[offset_source]
                new_pixels[offset_destination + 1] = self.pixels[offset_source + 1]
                new_pixels[offset_destination + 2] = self.pixels[offset_source + 2]
        self.pixels = new_pixels
        self.width, self.height = self.height, self.width

    def write(self, filepath):
        """Writes the image to a file.  If it is a TIFF or a JPEG, EXIF information is
        written, too.  ImageMagick and exiv2 are needed for anything beyond PPM
        output.
        """
        self.pixels.byteswap()
        ppm_data = "P6\n{} {}\n65535\n".format(self.width, self.height).encode("ascii") + self.pixels.tostring()
        self.pixels.byteswap()
        if filepath.endswith(".ppm"):
            open(filepath, "wb").write(ppm_data)
        else:
            subprocess.Popen(
                ["convert", "ppm:-", "-set", "colorspace", "RGB", filepath], stdin=subprocess.PIPE).communicate(ppm_data)
        if os.path.splitext(filepath)[1].lower() in [".tiff", ".tif", ".jpeg", "jpg"]:
            subprocess.call(["exiv2",
                             "-Mset Exif.Photo.FocalLength {}/10".format(int(focal_length * 10)),
                             "-Mset Exif.Photo.FNumber {}/10".format(int(aperture * 10)),
                             "-Mset Exif.Photo.SubjectDistance {}/10".format(int(distance * 10)),
                             "-Mset Exif.Photo.LensModel {}".format(lens_model_name),
                             "-Mset Exif.Image.Model {}".format(camera_model_name),
                             "-Mset Exif.Image.Make {}".format(camera_element.find("maker").text),
                             filepath])

    def create_grid(self, distortion, projection, tca_red, tca_blue):
        full_frame_diagonal = sqrt(36**2 + 24**2)
        diagonal_by_focal_length = full_frame_diagonal / 2 / camera_cropfactor / (get_real_focal_length() / hugin_correction)
        def apply_lens_projection(x, y):
            r_vignetting = sqrt(x**2 + y**2) / self.aspect_ratio_correction
            ϑ = atan(r_vignetting * diagonal_by_focal_length)
            scaling = (projection(ϑ) / diagonal_by_focal_length) / r_vignetting
            return x * scaling, y * scaling
        line_brightness = 0.4
        def set_pixel(x, y):
            """y goes from -1 to 1, x domain depends on aspect ratio."""
            if x == y == 0:
                self.add_to_position(0, 0, 1, 1, 1)
            else:
                # 1. Lens projection (fisheye etc)
                if projection:
                    x, y = apply_lens_projection(x, y)

                # 2. Distortion on top of that
                r = sqrt(x**2 + y**2) * self.ar_plus_cf_correction
                r_distorted = distortion(r)
                distortion_scaling = r_distorted / r
                x *= distortion_scaling
                y *= distortion_scaling
                self.add_to_position(x, y, 0, line_brightness, 0)

                # 3. TCA of red and blue channels on top of that
                tca_scaling = tca_red(r_distorted) / r_distorted
                self.add_to_position(x * tca_scaling, y * tca_scaling, line_brightness, 0, 0)
                tca_scaling = tca_blue(r_distorted) / r_distorted
                self.add_to_position(x * tca_scaling, y * tca_scaling, 0, 0, line_brightness)
        number_of_lines = 30
        for i in range(number_of_lines + 1):
            points_per_line = self.width
            y = i * (2 / number_of_lines) - 1
            for j in range(points_per_line):
                x = j * (2 * lens_aspect_ratio / points_per_line) - lens_aspect_ratio
                set_pixel(x, y)
            points_per_line = self.height
            x = i * (2 * lens_aspect_ratio / number_of_lines) - lens_aspect_ratio
            for j in range(points_per_line + 1):
                y = j * (2 / points_per_line) - 1
                set_pixel(x, y)


image = Image(width, int(round(width / aspect_ratio)))
image.create_grid(distortion, projection, tca_red, tca_blue)
if args.vignetting:
    image.set_vignetting(vignetting)
if portrait:
    image.rotate_by_90_degrees()
image.write(args.outfile)
