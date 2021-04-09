#!/usr/bin/env python3

"""This tool extracts the distortion coefficients from the database and makes
statistics with them.  In particular, it makes plots and measures how well the
data can be interpolated linerarly, in different coordinate systems.

This scrips only looks at zoom lenses with at least 3 focal lengths.
Otherwise, linear interpolation would be unnecessary or trivial.
"""

import glob, sys, os, inspect, subprocess, math, argparse
from xml.etree import ElementTree
from pathlib import Path


parser = argparse.ArgumentParser(description="Statistical evaluation of Lensfun’s database.")
parser.add_argument("--inverse", action="store_true",
                    help="Use the *inverse* focal length for the x axis of interpolation and plotting.")
parser.add_argument("--in-focal-length", action="store_true",
                    help="Use the focal length as the unith length for the pixel coordinate system.  This makes a "
                    "transformation or the coefficients necessary, thus changing the y values of interpolation and plotting.")
parser.add_argument("--root-dir", type=str, help="Root directory of the database.  Defaults to ../../data/db.")
args = parser.parse_args()


def divide(x, y):
    try:
        return x / y
    except ZeroDivisionError:
        return float("NaN")


def collect_distortion_data(db_files):
    """Returns the slopes of a, b, c versus (inverse) focal length of all zoom
    lenses with at least three focal lengths with calibration data.  The
    returned tuples have five items with the values focal length, inverse focal
    length, a, b, and c.
    """
    lines = []
    for filepath in db_files:
        for lens in ElementTree.parse(filepath).getroot().findall("lens"):
            calibration = lens.find("calibration")
            points = []
            min_f = max_f = min_inv_f = max_inv_f = min_a = max_a = min_b = max_b = min_c = max_c = None
            for distortion in calibration and calibration.findall("distortion") or ():
                model = distortion.attrib["model"]
                focal = float(distortion.attrib["focal"])
                inv_focal = 1 / focal
                if model == "ptlens":
                    a, b, c = distortion.get("a", "0"), distortion.get("b", "0"), distortion.get("c", "0")
                elif model == "poly3":
                    a, b, c = "0", distortion.attrib.get("k1", 0), "0"
                if args.in_focal_length:
                    a, b, c = float(a) * focal**3, float(b) * focal**2, float(c) * focal
                else:
                    a, b, c = float(a), float(b), float(c)
                points.append((focal, inv_focal, a, b, c))
                if min_f is None or focal < min_f:
                    min_f = focal
                if min_inv_f is None or inv_focal < min_inv_f:
                    min_inv_f = inv_focal
                if min_a is None or a < min_a:
                    min_a = a
                if min_b is None or b < min_b:
                    min_b = b
                if min_c is None or c < min_c:
                    min_c = c
                if max_inv_f is None or inv_focal > max_inv_f:
                    max_inv_f = inv_focal
                if max_f is None or focal > max_f:
                    max_f = focal
                if max_a is None or a > max_a:
                    max_a = a
                if max_b is None or b > max_b:
                    max_b = b
                if max_c is None or c > max_c:
                    max_c = c
            if 3 <= len(points):
                line = []
                for focal, inv_focal, a, b, c in points:
                    line.append((divide(focal - min_f, max_f - min_f),
                                 divide(inv_focal - min_inv_f, max_inv_f - min_inv_f),
                                 divide(a - min_a, max_a - min_a),
                                 divide(b - min_b, max_b - min_b),
                                 divide(c - min_c, max_c - min_c)))
                lines.append(line)
    return lines


def create_distortion_plots(data):
    outfile = open("distortion.dat", "w")
    for line in data:
        for point in line:
            print(*point, file=outfile)
        print(file=outfile)


def calculate_interpolation_error(data):
    """Returns the linear interpolation errors for a, b, and c.  I take the linear
    interpolation between two neighbours of a point and calculate the
    difference to the value at point.  This is a measure of the difficulty to
    interpolate linearly.  The results are averaged error squares.
    """
    x_axis_index = 1 if args.inverse else 0
    errors = {}
    for line in data:
        for i in range(1, len(line) - 1):
            x_l, x_0, x_r = line[i - 1][x_axis_index], line[i][x_axis_index], line[i + 1][x_axis_index]
            for coefficient_index in range(len(line[0]) - 2):
                y_l, y_0, y_r = line[i - 1][coefficient_index + 2], line[i][coefficient_index + 2], \
                    line[i + 1][coefficient_index + 2]
                mean = y_l + (x_0 - x_l) / (x_r - x_l) * (y_r - y_l)
                Δ = y_0 - mean
                if not math.isnan(Δ):
                    errors.setdefault(coefficient_index, []).append(Δ**2)
    return [sum(errors[index]) / len(errors[index]) for index in sorted(errors)]


if args.root_dir:
    root_dir = args.root_dir
else:
    root_dir = Path(os.path.dirname(__file__))/"../../data/db"

db_files = glob.glob(str(root_dir/"*.xml"))

distortion_data = collect_distortion_data(db_files)
create_distortion_plots(distortion_data)
print(calculate_interpolation_error(distortion_data))

open("plot.gp", "w").write("""plot "distortion.dat" using {}:5 with lines
pause -1
""".format(2 if args.inverse else 1))
