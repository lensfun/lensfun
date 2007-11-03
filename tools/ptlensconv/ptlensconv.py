#!/usr/bin/python
#
# NOTE: Due to simplifications made during PTLens database development,
# the resulting files are not 100% accurate. Every output from this script
# must be manually reviewed.
#
# Some things to take care of: many lenses available for several platforms
# (for example using the KAF, EFS, Sigma and Nikon mounts) are listed in
# PTLens database in the "genericSLR" group, which is translated as "M42"
# but in fact the lens must have attached a list of really available mounts.
#

import sys;
from optparse import OptionParser;

# This is not the Ultimate Truth, it's rather a try to guess
# what the author of PTLens was meaning to say ...
KnownMounts = {
    "canonEfsSLR": "Canon EF-S",
    "canonSLR":    "Canon EF",
    "genericSLR":  "M42",
    "pentaxSLR":   "Pentax KAF2",
    "contaxSLR":   "Contax/Yashica",
    "minoltaSLR":  "Minolta AF",
    "nikonSLR":    "Nikon F AF",
    "olympusSLR":  "4/3 System",
};


class Error:
    def __init__(self, str):
        self.str = str
    def __str__(self):
        return self.str


def CompatMount (mount, basemount):
    global Mounts
    if not Mounts.has_key (basemount):
        Mounts [basemount] = []
    if mount:
        if not Mounts [basemount].__contains__ (mount):
            Mounts [basemount].append (mount)

def ParseCamera (lines):
    global Cameras, KnownMounts
    cam = {}
    variant = None
    for l in lines:
        l = l.strip ()
        if l [:10] == "menu_make:":
            if not cam.has_key ("maker"):
                cam ["maker"] = l [10:].lstrip ()
        elif l [:11] == "menu_model:":
            variant = l [11:].lstrip ()
            if not cam.has_key ("model"):
                cam ["model"] = variant
        elif l [:10] == "exif_make:":
            l = l [10:].lstrip ()
            if l != "none":
                cam ["maker"] = l
        elif l [:11] == "exif_model:":
            l = l [11:].lstrip ()
            if l != "none":
                cam ["model"] = l
        elif l [:6] == "group:":
            l = l [6:].lstrip ()
            l = KnownMounts.get (l, l)
            if not cam.has_key ("mount"):
                cam ["mount"] = l
                l = None
            CompatMount (l, cam ["mount"])
        elif l [:11] == "multiplier:":
            cam ["cropfactor"] = l [11:].lstrip ()
        else:
            raise Error ("Unknown keyword in camera section: " + l)

    if not cam.has_key ("maker") or not cam.has_key ("model"):
        print "Camera does not have maker or model defined:", cam
        return

    if not cam.has_key ("mount"):
        print "Camera does not have a mount defined:", cam
        return

    if not cam ["model"].lower ().__contains__ (variant.lower ()):
        cam ["variant"] = variant;

    Cameras.append (cam)


def ParseLens (lines):
    global Lenses, KnownMounts
    lens = {}
    for l in lines:
        l = l.strip ()
        if  l [:10] == "menu_lens:":
            lens ["model"] = l [10:].lstrip ()
        elif l [:6] == "group:":
            if not lens.has_key ("mount"):
                lens ["mount"] = []
            l = l [6:].lstrip ()
            lens ["mount"].append (KnownMounts.get (l, l))
        elif l [:11] == "multiplier:":
            lens ["cropfactor"] = l [11:].lstrip ()
        elif l [:10] == "converter_":
            pass # ignored
        elif l [:8] == "cal_abc:":
            if not lens.has_key ("calibration"):
                lens ["calibration"] = []
            l = l [8:].split ()
            l [0] = float (l [0])
            l [1] = float (l [1])
            l [2] = float (l [2])
            l [3] = float (l [3])
            lens ["calibration"].append (l)
        else:
            raise Error ("Unknown keyword in camera section: " + l)

    if not lens.has_key ("model"):
        print "Lens does not have model name defined:", lens
        return

    Lenses.append (lens)


def WriteOut (ofn):
    global Mounts, Cameras, Lenses
    fw = open (ofn, "w")
    fw.write ("<lensdatabase>\n\n")

    for c in Mounts:
        fw.write ("    <mount>\n")
        fw.write ("        <name>%s</name>\n" % c)
        for x in Mounts [c]:
            fw.write ("        <compat>%s</compat>\n" % x)
        fw.write ("    </mount>\n\n")

    for c in Cameras:
        fw.write ("    <camera>\n")
        fw.write ("        <maker>%s</maker>\n" % c ["maker"])
        fw.write ("        <model>%s</model>\n" % c ["model"])
        if c.has_key ("variant"):
            fw.write ("        <variant>%s</variant>\n" % c ["variant"])
        fw.write ("        <mount>%s</mount>\n" % c ["mount"])
        if c.has_key ("cropfactor"):
            fw.write ("        <cropfactor>%s</cropfactor>\n" % c ["cropfactor"])
        fw.write ("    </camera>\n\n")

    for c in Lenses:
        fw.write ("    <lens>\n")
        fw.write ("        <model>%s</model>\n" % c ["model"])
        for m in c ["mount"]:
            fw.write ("        <mount>%s</mount>\n" % m)
        if c.has_key ("cropfactor"):
            fw.write ("        <cropfactor>%s</cropfactor>\n" % c ["cropfactor"])
        fw.write ("        <calibration>\n");
        for x in c ["calibration"]:
            fw.write ("            <distortion model=\"ptlens_rect\" focal=\"%g\" "\
                      "a=\"%g\" b=\"%g\" c=\"%g\" />\n" % (x [0], x [1], x [2], x [3]))
        fw.write ("        </calibration>\n");
        fw.write ("    </lens>\n\n")

    fw.write ("</lensdatabase>\n")


def Parse (fn):
    global opts
    print "Parsing %s ..." % fn
    try:
        fr = open (fn, "r")
        lines = []
        for l in fr.readlines ():
            l = l.strip ()
            if l [:1] == "#":
                continue;
            elif l [:6] == "begin ":
                l = l [6:].lstrip ()
                lines = []
                if l == "camera":
                    func = ParseCamera
                elif l == "lens":
                    func = ParseLens
                else:
                    raise Error ("Unknown section: " + l)
            elif l == "end":
                func (lines)
            else:
                lines.append (l)

        name = fn.partition (".") [0] or ""
        ext = fn.partition (".") [2] or ""

        WriteOut (opts.Output % {"name": name, "ext": ext});

    except Error, e:
        print "FAILED: ", e


parser = OptionParser (usage="%prog [-o FILE] FILE1.txt [FILE2.txt [...]]",
                       version="%prog 0.0.1")
parser.add_option("-o", "--output", dest="Output", default="%(name)s.xml",
                  help="write output to FILE", metavar="FILE")

(opts, args) = parser.parse_args ()

if len (args) == 0:
    parser.print_help ()

for f in args:
    Cameras = []
    Lenses = []
    Mounts = {}
    Parse (f)
