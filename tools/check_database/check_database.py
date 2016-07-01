#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Checks sanity of the database.  Pass the path to the database on the command
line.

If you want to add a check, just add a "check_..." function to this program.
It must take a set of ElementTree roots.
"""

import glob, sys, os, inspect, subprocess
from xml.etree import ElementTree

ERROR_FOUND = False

##########
# Helper functions

def normalize_string(string):
    return " ".join(string.lower().split())

def name(element, tag_name):
    for subelement in element.findall(tag_name):
        if not subelement.attrib:
            return normalize_string(subelement.text)


##########
# Database check_... functions

def check_primary_keys_uniqueness(db_files):
    global ERROR_FOUND
    
    roots = set()
    for filepath in db_files:
        roots.add(ElementTree.parse(filepath).getroot())

    lenses, mounts, cameras = set(), set(), set()
    for root in roots:
        for element in root.findall("lens"):
            lens = (name(element, "maker"), name(element, "model"), float(element.find("cropfactor").text))
            if lens in lenses:
                print("ERROR: Double primary key for lens!  {}".format(lens))
                ERROR_FOUND = True
            else:
                lenses.add(lens)
        for element in root.findall("mount"):
            mount = name(element, "name")
            if mount in mounts:
                print("ERROR: Double primary key for mounts!  {}".format(mount))
                ERROR_FOUND = True
            else:
                mounts.add(mount)
        for element in root.findall("camera"):
            camera = (name(element, "maker"), name(element, "model"), name(element, "variant"))
            if camera in cameras:
                print("ERROR: Double primary key for cameras!  {}".format(camera))
                ERROR_FOUND = True
            else:
                cameras.add(camera)

def check_xmllint(db_files):
    global ERROR_FOUND
    db_path = os.path.split(db_files[0])[0]
    for filepath in db_files:
        err = subprocess.call(["xmllint", 
                "--valid", "--noout",
                "--schema", os.path.join(db_path,"lensfun-database.xsd"),
                filepath,], stderr=open(os.devnull, 'wb'), stdout=open(os.devnull, 'wb'))

        if err is not 0:
            print("ERROR: xmllint check failed for " + filepath)
            ERROR_FOUND= True
            


##########
# Main program

db_files = glob.glob(os.path.join(sys.argv[1], "*.xml"))

for check_function in [function for function in globals().copy().values()
                       if inspect.isfunction(function) and function.__name__.startswith("check_")]:
    check_function(db_files)

if ERROR_FOUND:
    sys.exit(1)
else:
    sys.exit(0)
