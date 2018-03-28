#!/usr/bin/python3

import sys, os, argparse
from xml.etree import ElementTree
from pathlib import Path


parser = argparse.ArgumentParser(description="Find min/max values of parameters in the DB.")
parser.add_argument("db_root", help="path to the DB")
parser.add_argument("tag", choices={"distortion", "tca", "vignetting"})
parser.add_argument("model", choices={"ptlens", "poly3", "acm", "pa", "poly5", "linear"})
parser.add_argument("parameter", choices={"a", "b", "c", "k1", "k2", "k3", "kr", "kb", "br", "cr", "vr", "bb", "cb", "vb"})
args = parser.parse_args()


values = set()
for root, __, filenames in os.walk(args.db_root):
    root = Path(root)
    for filename in filenames:
        filepath = root/filename
        if filepath.suffix == ".xml":
            tree = ElementTree.parse(str(filepath))
            for element in tree.findall(".//{0.tag}[@model='{0.model}']".format(args)):
                value = element.attrib.get(args.parameter)
                if value is not None:
                    values.add(float(value))
if values:
    print("min: {}, max: {}".format(min(values), max(values)))
else:
    print("No values found.")
