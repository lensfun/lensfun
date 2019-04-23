#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import glob, sys, os, inspect, subprocess, math
from xml.etree import ElementTree
from pathlib import Path


def divide(x, y):
    try:
        return x / y
    except ZeroDivisionError:
        return float("NaN")


def collect_distortion_data(db_files):
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
                a, b, c = float(a) * focal**3, float(b) * focal**2, float(c) * focal
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
            if len(points) > 4:
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


def calculate_interpolation_error(data, exponents, r_max):
    error = 0
    for line in data:
        for i in range(1, len(line) - 1):
            x_l, x_0, x_r = line[i - 1][0], line[i][0], line[i + 1][0]
            for coefficient_index in range(len(line[0]) - 2):
                y_l, y_0, y_r = line[i - 1][coefficient_index + 1], line[i][coefficient_index + 1], \
                    line[i + 1][coefficient_index + 1]
                mean = y_l + (x_0 - x_l) / (x_r - x_l) * (y_r - y_l)
                Δ = abs(y_0 - mean) * r_max ** exponents[coefficient_index]
                if not math.isnan(Δ):
                    error += Δ
    return error


try:
    rootdir = Path(sys.argv[1])
except IndexError:
    rootdir = Path(os.path.dirname(__file__))/"../../data/db"

db_files = glob.glob(str(rootdir/"*.xml"))

distortion_data = collect_distortion_data(db_files)
create_distortion_plots(distortion_data)
print(calculate_interpolation_error(distortion_data, (4, 3, 2), 1))
