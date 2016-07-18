#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Generator for database tarballs, in different Lensfun versions.

This program is intended to run as a cronjob, and possibly be run as needed.
It creates a versions.json file and tarballs in the given output directory.  If
desired, it also pushes its content to sourceforge.de.

If a new database version is created in Lensfun, you must add a new `Converter`
class.  Simply use `From1to0` as a starting point.  You prepend the decorator
`@converter` so that the rest of the program finds the new class.  The rest is
automatic.

Note that this script also creates a database with version 0.  This may be
downloaded manually by people who use Lensfun <= 0.2.8.
"""

import glob, os, subprocess, calendar, json, time, tarfile, io, argparse, shutil
from lxml import etree

root = "/tmp/"

parser = argparse.ArgumentParser(description="Generate tar balls of the Lensfun database, also for older versions.")
parser.add_argument("output_path", help="Directory where to put the XML files.  They are put in the db/ subdirectory.  "
                    "It needn't exist yet.")
parser.add_argument("--upload", action="store_true", help="Upload the files to Sourceforge, too.")
args = parser.parse_args()


class XMLFile:

    def __init__(self, root, filepath):
        self.filepath = filepath
        self.tree = etree.parse(os.path.join(root, filepath))

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

    def write_to_tar(self, tar, timestamp):
        tarinfo = tarfile.TarInfo(self.filepath)
        root = self.tree.getroot()
        self.indent(root)
        content = etree.tostring(root, encoding="utf-8")
        tarinfo.size = len(content)
        tarinfo.mtime = timestamp
        tar.addfile(tarinfo, io.BytesIO(content))


def update_git_repository():
    try:
        os.chdir(root + "lensfun-git")
    except FileNotFoundError:
        os.chdir(root)
        subprocess.check_call(["git", "clone", "git://git.code.sf.net/p/lensfun/code", "lensfun-git"],
                              stdout=open(os.devnull, "w"), stderr=open(os.devnull, "w"))
        db_was_updated = True
    else:
        subprocess.check_call(["git", "fetch"], stdout=open(os.devnull, "w"), stderr=open(os.devnull, "w"))
        changed_files = subprocess.check_output(["git", "diff", "--name-only", "master..origin/master"],
                                                stderr=open(os.devnull, "w")).splitlines()
        db_was_updated = any(filename.startswith("data/db/") for filename in changed_files)

    subprocess.check_call(["git", "checkout", "master"], stdout=open(os.devnull, "w"), stderr=open(os.devnull, "w"))
    subprocess.check_call(["git", "pull"], stdout=open(os.devnull, "w"), stderr=open(os.devnull, "w"))
    return db_was_updated


def fetch_xml_files():
    os.chdir(root + "lensfun-git/data/db")
    xml_filenames = glob.glob("*.xml")
    xml_files = set(XMLFile(os.getcwd(), filename) for filename in xml_filenames)
    timestamp = int(subprocess.check_output(["git", "log", "-1", '--format=%ad', "--date=raw", "--"] + xml_filenames). \
                    decode("utf-8").split()[0])
    return xml_files, timestamp


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
            calibration = lens.find("calibration")
            if calibration is not None:
                for real_focal_length in calibration.findall("real-focal-length"):
                    # Note that while one could convert it to the old
                    # <field-of-view> element, we simply remove it.  It is not
                    # worth the effort.
                    calibration.remove(real_focal_length)
            self.round_aps_c_cropfactor(lens)
        for camera in tree.findall("camera"):
            self.round_aps_c_cropfactor(camera)


@converter
class From2To1(Converter):
    from_version = 2
    to_version = 1

    def __call__(self, tree):
        super().__call__(tree)
        for acm_model in tree.findall("//calibration/*[@model='acm']"):
            acm_model.getparent().remove(acm_model)
        for distortion in tree.findall("//calibration/distortion[@real-focal]"):
            etree.SubElement(distortion.getparent(), "real-focal-length", {"focal": distortion.get("focal"),
                                                                           "real-focal": distortion.get("real-focal")})
            del distortion.attrib["real-focal"]


def generate_database_tarballs(xml_files, timestamp):
    version = current_version
    output_path = os.path.join(args.output_path, "db")
    shutil.rmtree(output_path, ignore_errors=True)
    os.makedirs(output_path)
    metadata = [timestamp, [], []]
    while True:
        metadata[1].insert(0, version)

        tar = tarfile.open(os.path.join(output_path, "version_{}.tar.bz2".format(version)), "w:bz2")
        for xml_file in xml_files:
            xml_file.write_to_tar(tar, timestamp)
        tar.close()

        try:
            converter_instance = converters.pop()
        except IndexError:
            break
        assert converter_instance.from_version == version
        for xml_file in xml_files:
            converter_instance(xml_file.tree)
        version = converter_instance.to_version
    json.dump(metadata, open(os.path.join(output_path, "versions.json"), "w"))
    if args.upload:
        subprocess.check_call(["rsync", "-a", "--delete", output_path if output_path.endswith("/") else output_path + "/",
                               "web.sourceforge.net:/home/project-web/lensfun/htdocs/db"])


db_was_updated = update_git_repository()
if db_was_updated:
    xml_files, timestamp = fetch_xml_files()
    generate_database_tarballs(xml_files, timestamp)
