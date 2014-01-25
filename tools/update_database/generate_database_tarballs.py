#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Generator for database tarballs, in different LensFun versions.

This program is intended to run as a cronjob, and possibly be run as needed.
It creates a versions.json file and tarballs in the given output directory.  If
desired, it also pushes its content to berlios.de.

If a new database version is created in LensFun, you must add a new `Converter`
class.  Simply use `From1to0` as a starting point.  You prepend the decorator
`@converter` so that the rest of the program finds the new class.  The rest is
automatic.

Note that this script also creates a database with version 0.  This is not
really needed but it is a good test.
"""

import glob, os, subprocess, calendar, json, re, time, tarfile, io, argparse, shutil
from xml.etree import ElementTree

timestamp_pattern = re.compile(r"Last Changed Date: (\d{4}-\d\d-\d\d \d\d:\d\d:\d\d) ([+-])(\d\d)(\d\d) ")
root = "/tmp/"

parser = argparse.ArgumentParser(description="Generate tar balls of the LensFun database, also for older versions.")
parser.add_argument("output_path", help="Directory where to put the XML files.  They are put in the db/ subdirectory.  "
                    "It needn't exist yet.")
parser.add_argument("--upload", action="store_true", help="Upload the files to Berlios, too.")
args = parser.parse_args()


class XMLFile:

    def __init__(self, filepath):
        self.filepath = filepath
        self.tree = ElementTree.parse(os.path.join(root, "lensfun-svn/data/db", filepath))

    @staticmethod
    def indent(tree, level=0):
        i = "\n" + level*"    "
        if len(tree):
            if not tree.text or not tree.text.strip():
                tree.text = i + "    "
            if not tree.tail or not tree.tail.strip():
                tree.tail = i
            for tree in tree:
                XMLFile.indent(tree, level + 1)
            if not tree.tail or not tree.tail.strip():
                tree.tail = i
        else:
            if level and (not tree.tail or not tree.tail.strip()):
                tree.tail = i

    def write_to_tar(self, tar):
        tarinfo = tarfile.TarInfo(self.filepath)
        root = self.tree.getroot()
        self.indent(root)
        content = ElementTree.tostring(root, encoding="utf-8")
        tarinfo.size = len(content)
        tarinfo.mtime = timestamp
        tar.addfile(tarinfo, io.BytesIO(content))


def fetch_xml_files():
    try:
        os.chdir(root + "lensfun-svn")
    except FileNotFoundError:
        os.chdir(root)
        subprocess.check_output(["svn", "checkout", "svn://svn.berlios.de/lensfun/trunk", "lensfun-svn"])
    else:
        subprocess.check_output(["svn", "update"])
    os.chdir(root + "lensfun-svn/data/db")
    xml_files = set()
    xml_filepaths = glob.glob("*.xml")
    for filename in xml_filepaths:
        xml_files.add(XMLFile(filename))
    timestamp = 0
    for line in subprocess.check_output(["svn", "info"] + xml_filepaths, env={"LANG": "C"}).decode("utf-8").splitlines():
        match = timestamp_pattern.match(line)
        if match:
            current_timestamp = calendar.timegm(time.strptime(match.group(1), "%Y-%m-%d %H:%M:%S"))
            sign, hours, minutes = match.group(2), int(match.group(3)), int(match.group(4))
            offset = hours * 3600 + minutes * 60
            if sign == "-":
                offset -= 1
            current_timestamp -= offset
            timestamp = max(timestamp, current_timestamp)
    return xml_files, timestamp
xml_files, timestamp = fetch_xml_files()


class Converter:
    from_version = None
    to_version = None
    def __call__(self, tree):
        root = tree.getroot()
        if self.to_version == 0:
            if "version" in root.attrib:
                del root.attrib["version"]
        else:
            root.attrib["version"] = str(self.to_version)
        
converters = []
current_version = 0
def converter(converter_class):
    global current_version
    current_version = max(current_version, converter_class.from_version)
    converters.append(converter_class())
    return converter_class

@converter
class From1To0(Converter):
    from_version = 1
    to_version = 0

    @staticmethod
    def round_aps_c_cropfactor(lens_or_camera):
        element = lens_or_camera.find("cropfactor")
        if element is not None:
            cropfactor = float(element.text)
            if 1.5 < cropfactor < 1.56:
                element.text = "1.5"
            elif 1.6 < cropfactor < 1.63:
                element.text = "1.6"

    def __call__(self, tree):
        super().__call__(tree)
        for lens in tree.findall("lens"):
            element = lens.find("aspect-ratio")
            if element is not None:
                lens.remove(element)
            self.round_aps_c_cropfactor(lens)
        for camera in tree.findall("camera"):
            self.round_aps_c_cropfactor(camera)

output_path = os.path.join(args.output_path, "db")
shutil.rmtree(output_path, ignore_errors=True)
os.makedirs(output_path)
metadata = [timestamp, [], []]
while True:
    metadata[1].insert(0, current_version)

    tar = tarfile.open(os.path.join(output_path, "version_{}.tar.bz2".format(current_version)), "w:bz2")
    for xml_file in xml_files:
        xml_file.write_to_tar(tar)
    tar.close()

    try:
        converter_instance = converters.pop()
    except IndexError:
        break
    assert converter_instance.from_version == current_version
    for xml_file in xml_files:
        converter_instance(xml_file.tree)
    current_version = converter_instance.to_version
json.dump(metadata, open(os.path.join(output_path, "versions.json"), "w"))
if args.upload:
    subprocess.check_call(["rsync", "-a", "--delete", output_path if output_path.endswith("/") else output_path + "/",
                           "shell.berlios.de:/home/groups/lensfun/htdocs/db"])
    subprocess.check_call(["ssh", "shell.berlios.de", "chgrp --recursive lensfun /home/groups/lensfun/htdocs/db ; "
                           "chmod --recursive g+w /home/groups/lensfun/htdocs/db"])
