#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""This program checks the accuracy of Lensfun's autoscaling algorithm.
Lensfun calculates the autoscaling by testing all four corner points and the
four edge centres.  For each of these 8 points, it calculates the scaling
necessary, and then takes the maximal value.

It calculates the scaling necessary by determining the distance of the point of
the corrected image edge which lies in the same direction as the corner point.
(Because these two points should lie in top of each other after the
autoscaling.)  Then, it divides the distance of the corner point by the
distance of the corrected edge point.  “Distance” means “distance from origin”.

The choice of those 8 points is the critical compromise here.  In most cases,
the maximal scaling is indeed necessary for one of these points.  For example,
a corrected pincushion distortion pulls the corners most closely to the centre,
so they need the maximal scaling.  On the other hand, a corrected barrel
distorion leaves the edge centres most closely to the origin, so their position
is critical.  In case of a nasty moustache distortion, the maximal scaling may
be inbetween, and in these cases, Lensfun's approach fails.

This program shows for which lenses it fails, and how badly.  For
non-rectilinear lenses, the projection is changed to rectilinear first.
Currenly (as of April 2015), there is nothing to worry about.  Only three
lenses fail, and all of them exhibit only very slim black areas after the
correction.  If one leaves the original projection, two fisheyes join the
pack.

The simple reason why the current approach works well is that the used
distortion polynomials usually have monotonic behaviour in the important
region, so the maximal values are at the borders of the region.  This is even
more true for projection changes.

For problematic lenses, this program prints the number of sampling points on
the longer edges necessary to get a decent autoscaling (< 1‰).  Lensfun itself
only uses 2.  If the problem of black areas increases, one can increase the
number of edge sampling points to the maximal value this program prints.

The only safe approach is to test *all* edge pixels.  Really.  This is most
wasteful in almost all cases, so Lensfun doesn't do it (for now).  On the other
hand, its complexity is O(size).  All other operations of Lensfun are O(size²).
"""

import math, glob, multiprocessing, argparse, os.path
from xml.etree import ElementTree
from scipy.optimize import brentq


parser = argparse.ArgumentParser(description="Check autoscaling algorithm.")
parser.add_argument("database_path", metavar="database_path", help="The full path to the Lensfun database.")
parser.add_argument("--original-geometry", dest="original_geometry", action="store_true",
                    help="Use original geometry of fisheyes rather than transforming to rectilinear.")
args = parser.parse_args()


class Calibration:

    def __init__(self, f_per_half_height, a=None, b=None, c=None, k1=None, type_="rectilinear", aspect_ratio=1.5):
        self.f_per_half_height, self.a, self.b, self.c, self.k1, self.type_, self.aspect_ratio = \
                f_per_half_height, a, b, c, k1, type_, aspect_ratio

    def de_ptlens(self, r_):
        try:
            return brentq(lambda r: r * (self.a * r**3 + self.b * r**2 + self.c * r + 1 - self.a - self.b - self.c) - r_, 0, 2.1)
        except ValueError:
            return float("inf")

    def de_poly3(self, r_):
        try:
            return brentq(lambda r: r * (self.k1 * r**2 + 1 - self.k1) - r_, 0, 2)
        except ValueError:
            return float("inf")

    def de_fisheye(self, r):
        angle = r / self.f_per_half_height
        if angle < math.pi / 2:
            return self.f_per_half_height * math.tan(angle)
        else:
            return float("inf")

    def de_equisolidangle(self, r):
        try:
            return self.f_per_half_height * math.tan(2 * math.asin(r / self.f_per_half_height / 2))
        except ValueError:
            return float("inf")

    def de_stereographic(self, r):
        angle = 2 * math.atan(r / self.f_per_half_height / 2)
        if angle < math.pi / 2:
            return self.f_per_half_height * math.tan(angle)
        else:
            return float("inf")

    def get_scaling(self, x, y):
        r_ = math.hypot(x, y)
        r = self.de_poly3(r_) if self.a is None else self.de_ptlens(r_)
        if self.type_ == "rectilinear" or args.original_geometry:
            pass
        elif self.type_ == "fisheye":
            r = self.de_fisheye(r)
        elif self.type_ == "equisolid":
            r = self.de_equisolidangle(r)
        elif self.type_ == "stereographic":
            r = self.de_stereographic(r)
        return r_ / r

    def get_autoscale(self, samples):
        scalings = []
        for i in range(samples):
            x = self.aspect_ratio * (-1 + i / samples * 2)
            scalings.append(self.get_scaling(x, +1))
        samples = int(math.ceil(samples / self.aspect_ratio / 2)) * 2
        for i in range(samples):
            y = -1 + i / samples * 2
            scalings.append(self.get_scaling(+ self.aspect_ratio, y))
        return max(scalings) or 1

    def get_perfect_sample_number(self):
        perfect_autoscale = self.get_autoscale(100)
        samples = 2
        scalings = []
        while True:
            scalings.append(self.get_autoscale(samples))
            if all(abs(scaling - perfect_autoscale) / perfect_autoscale < 1e-3 for scaling in scalings[-3:]):
                return samples
            samples += 2


def process_calibration(distortion, lens, crop_factor, type_, aspect_ratio):
    f_per_half_height = float(distortion.attrib.get("focal")) / (12 / crop_factor)
    model = distortion.attrib.get("model")
    if model == "ptlens":
        calibration = Calibration(f_per_half_height,
                                  a=float(distortion.attrib.get("a", 0)),
                                  b=float(distortion.attrib.get("b", 0)),
                                  c=float(distortion.attrib.get("c", 0)), type_=type_, aspect_ratio=aspect_ratio)
    elif model == "poly3":
        calibration = Calibration(f_per_half_height, k1=float(distortion.attrib.get("k1", 0)),
                                  type_=type_, aspect_ratio=aspect_ratio)
    else:
        return None, None, None
    return lens.find("model").text, distortion.attrib.get("focal"), calibration.get_perfect_sample_number()

results = set()
pool = multiprocessing.Pool()
for filename in glob.glob(os.path.join(args.database_path, "*.xml")):
    tree = ElementTree.parse(filename)
    for lens in tree.findall("lens"):
        type_element = lens.find("type")
        type_ = "rectilinear" if type_element is None else type_element.text.strip()
        crop_factor = float(lens.find("cropfactor").text)
        aspect_ratio_element = lens.find("aspect-ratio")
        if aspect_ratio_element is None:
            aspect_ratio = 1.5
        else:
            numerator, __, denominator = aspect_ratio_element.text.partition(":")
            if denominator:
                aspect_ratio = float(numerator) / float(denominator)
            else:
                aspect_ratio = float(numerator)
        calibration_element = lens.find("calibration")
        if calibration_element is not None:
            for distortion in calibration_element.findall("distortion"):
                results.add(pool.apply_async(process_calibration, (distortion, lens, crop_factor, type_, aspect_ratio)))
pool.close()
pool.join()

problematic_lenses = {}
for result in results:
    model, focal_length, perfect_sample_number = result.get()
    if perfect_sample_number and perfect_sample_number > 2:
        problematic_lenses.setdefault(perfect_sample_number, set()).add((model, focal_length))
if problematic_lenses:
    print("Number of equidistant samples on the long edge:\n")
    for perfect_sample_number, lenses in problematic_lenses.items():
        print(",\n".join("{} at {}mm".format(*lens) for lens in lenses), ": ", perfect_sample_number)
