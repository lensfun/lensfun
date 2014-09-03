#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Finds cameras that are known to rawspeed's database but not to Lensfun.
Maybe this could be extended to look for missing cameras in other places, too.
Moreover, it may be good to optionally exclude compact cameras from the result
– their only purpose might be to look for optically equivalent entries in
Lensfun.

Call:

python3 find_missing_cameras.py path/to/lensfun/db path/to/rawspeeds/cameras.xml

"""

import glob, sys, os, re
from xml.etree import ElementTree

pattern = re.compile(r"[a-z]+|\d+|[^ a-z0-9]+")
def normalize_string(string):
    components = sorted(token for token in pattern.findall(string.lower())
                        if token.isalnum() and token != "f" or len(token) > 1)
    return " ".join(components)

def name(element, tag_name):
    for subelement in element.findall(tag_name):
        if not subelement.attrib:
            return normalize_string(subelement.text)

cameras_in_lensfun = set()
for filepath in glob.glob(os.path.join(sys.argv[1], "*.xml")):
    root = ElementTree.parse(filepath).getroot()
    for element in root.findall("camera"):
        cameras_in_lensfun.add(name(element, "model"))

cameras_in_rawspeed = set()
full_data = {}
def add_camera(make, model):
    normalized_name = normalize_string(model)
    cameras_in_rawspeed.add(normalized_name)
    full_data[normalized_name] = (make, model)
root = ElementTree.parse(sys.argv[2]).getroot()
for element in root.findall("Camera"):
    make = element.attrib.get("make")
    add_camera(make, element.attrib.get("model"))
    for alias in element.find("Aliases") or []:
        add_camera(make, alias.text)

missing_cameras = cameras_in_rawspeed - cameras_in_lensfun
for maker, camera in sorted(data for normalized_name, data in full_data.items() if normalized_name in missing_cameras):
    print("{}: {}".format(maker, camera))
