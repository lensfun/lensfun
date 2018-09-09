#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import glob, sys, os, inspect, subprocess
from xml.etree import ElementTree
from pathlib import Path


def divide(x, y):
    try:
        return x / y
    except ZeroDivisionError:
        return float("NaN")


def create_distortion_plots(db_files):
    outfile = open("distortion.dat", "w")
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
                for focal, inv_focal, a, b, c in points:
                    print(divide(focal - min_f, max_f - min_f),
                          divide(inv_focal - min_inv_f, max_inv_f - min_inv_f),
                          divide(a - min_a, max_a - min_a),
                          divide(b - min_b, max_b - min_b),
                          divide(c - min_c, max_c - min_c), file=outfile)
                print(file=outfile)

try:
    rootdir = Path(sys.argv[1])
except IndexError:
    rootdir = Path(os.path.dirname(__file__))/"../../data/db"

db_files = glob.glob(str(rootdir/"*.xml"))

create_distortion_plots(db_files)
