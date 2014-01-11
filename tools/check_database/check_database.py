#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Checks sanity of the database.  Pass the path to the database on the command
line.

If you want to add a check, just add a "check_..." function to this program.
It must take a set of ElementTree roots.
"""

import glob, sys, os, inspect
from xml.etree import ElementTree

def normalize_string(string):
    return " ".join(string.lower().split())

def name(element, tag_name):
    for subelement in element.findall(tag_name):
        if not subelement.attrib:
            return normalize_string(subelement.text)


def check_primary_keys_uniqueness(roots):
    lenses, mounts, cameras = set(), set(), set()
    for root in roots:
        for element in root.findall("lens"):
            lens_mounts = frozenset(normalize_string(mount.text) for mount in element.findall("mount"))
            lens = (name(element, "maker"), name(element, "model"), lens_mounts, float(element.find("cropfactor").text))
            if lens in lenses:
                print("Double primary key for lens!  {}".format(lens))
            else:
                lenses.add(lens)
        for element in root.findall("mount"):
            mount = name(element, "name")
            if mount in mounts:
                print("Double primary key for mounts!  {}".format(mount))
            else:
                mounts.add(mount)
        for element in root.findall("camera"):
            camera = (name(element, "maker"), name(element, "model"), name(element, "variant"))
            if camera in cameras:
                print("Double primary key for cameras!  {}".format(camera))
            else:
                cameras.add(camera)


roots = set()
for filepath in glob.glob(os.path.join(sys.argv[1], "*.xml")):
    roots.add(ElementTree.parse(filepath).getroot())
for check_function in [function for function in globals().copy().values()
                       if inspect.isfunction(function) and function.__name__.startswith("check_")]:
    check_function(roots)
