#!/usr/bin/env python3

#======================================================================

import subprocess, os, glob, datetime, tempfile, argparse
from xml.etree import ElementTree
import re
import argparse

#======================================================================
# Function and class definitions
#----------------------------------------------------------------------

#----------------------------------------------------------------------
def find_best(root, tagname):
    texts = [(len(element.text), element.text) for element in root.findall(tagname) if element.attrib.get("lang") == "en"]
    if texts:
        return sorted(texts)[0][1]
    texts = [(len(element.text), element.text) for element in root.findall(tagname)]
    return sorted(texts)[0][1]

def print_x(value):
    return ' style="background-color: lightgreen;text-align:center">yes' if value else ' style="text-align:center">no'

#----------------------------------------------------------------------
# class to hold camera information
class Camera:
    camera_makers = {}
    def __init__(self, element):
        self.maker = find_best(element, "maker")
        self.model = find_best(element, "model")
        self.crop = float(element.find("cropfactor").text)
        self.camera_makers.setdefault(self.maker, set()).add(self.model)
    def __lt__(self, other):
        return (self.maker, self.model) < (other.maker, other.model)

#----------------------------------------------------------------------
# class to hold lens information
class Lens:
    def __init__(self, element, root, camtype):
        self.maker = find_best(element, "maker")
        self.model = find_best(element, "model")
        if camtype == "compact" or self.model == "Standard":
            mount = element.find("mount").text
            for camera in root.findall("camera"):
                if camera.find("mount").text == mount:
                    self.maker = find_best(camera, "maker")
                    camera = find_best(camera, "model")
                    break
            if self.model == "Standard":
                self.model = "Fixed lens {}".format(camera)
            else:
                self.model = "Fixed lens {}, {}".format(camera, self.model)
        try:
            self.crop = float(element.find("cropfactor").text)
        except:
            self.crop = None
        self.distortion = element.find("calibration/distortion") is not None
        self.tca = element.find("calibration/tca") is not None
        self.vignetting = element.find("calibration/vignetting") is not None
    def __lt__(self, other):
        return (self.maker, self.model, self.crop) < (other.maker, other.model, self.crop)

#======================================================================



#======================================================================
# Main routine
#----------------------------------------------------------------------

# set up the commandline parser and define some arguemnts
parser = argparse.ArgumentParser(description='Create a list of all cameras and lenses in the lensfun database.')
parser.add_argument('db_path', metavar='DB_PATH', help='path to the lensfun database', default='/usr/share/lensfun/', nargs='?')
parser.add_argument('-s', dest='svn', action='store_true', help='download current database from subversion repository')
parser.add_argument('-t', dest='table_only', action='store_true', help='pure table/list without surrounding header and description')
parser.add_argument('-m', dest='markdown', action='store_true', help='output markdown instead of HTML')
parser.add_argument('-o', dest='outfile', action='store', help='output filename and path', default='./lensfun_coverage.html')

cmdline_args = vars(parser.parse_args())


# decide wether to get the most up to date database from svn or use the locally installed database
if cmdline_args['svn']==True:
    XmlDBPath = os.path.join(tempfile.gettempdir(), "lensfun")
    print("Lensfun database is retrieved directly from SVN")
    if not os.path.isdir(XmlDBPath):
        subprocess.check_output(["svn", "checkout", "svn://svn.berlios.de/lensfun/trunk/data/db", XmlDBPath])
    else:
        subprocess.check_output(["svn", "update"])
else:
    XmlDBPath = cmdline_args['db_path']
    print("Lensfun database is searched in "+XmlDBPath)

# parse the database create a list of camera and lens objects
cameras, lenses = [], []
for filename in glob.glob(os.path.join(XmlDBPath,"*.xml")):
    root = ElementTree.parse(filename)

    camtype_search = re.search('.*/(.*)-(.*).xml', filename, re.IGNORECASE)    
    if camtype_search:
        camtype = camtype_search.group(1)
    else:
        camtype = 'generic'

    cameras.extend(Camera(camera_element) for camera_element in root.findall("camera"))
    lenses_list = [Lens(lens_element, root, camtype) for lens_element in root.findall("lens")]
    lenses.extend(lens for lens in lenses_list if lens.distortion or lens.tca or lens.vignetting)

cameras.sort()
lenses.sort(key=lambda Lens: Lens.model.lower())
lenses.sort(key=lambda Lens: Lens.maker.lower())

# finally write the table into the output file
outfile = open(cmdline_args['outfile'], "w")

#----------------------------------------------------------------------
# write HTML table or markdown formatted list
if cmdline_args['markdown'] == False:

    #----------------------------------------------------------------------
    # HTML table
    if cmdline_args['table_only'] == False:
        outfile.write("<html><head><title>LensFun's coverage</title></head><body><h1>LensFun coverage</h1><h2>Lenses (count: {})</h2>"
                  "<p>This table was generated on {} from current LensFun sources.  Your LensFun version may be older, resulting in "
                  "less coverage.  If your lens is not included, see</p><ul><li><a href='/calibration'>Upload calibration pictures</a>"
                  "</li><li><a href='lens_calibration_tutorial/'>Lens calibration for LensFun</a></li></ul>\n".format(
                      len(lenses), datetime.date.today()))

    outfile.write("<table border='1'><thead><tr><th>manufacturer</th><th>model</th><th>crop</th><th>distortion</th><th>TCA</th>"
                  "<th>vignetting</th></tr></thead><tbody>\n")
    number_of_makers = 0
    previous_maker = None

    for lens in lenses:
        if lens.maker.lower() != previous_maker:
            number_of_makers += 1

        outfile.write("""<tr{}><td>{}</td><td>{}</td><td>{}</td><td{}</td><td{}</td><td{}</td></tr>\n""".format(
            ' style="background-color: #eeeeee"' if number_of_makers % 2 else "",
            lens.maker if lens.maker.lower() != previous_maker else "", lens.model, lens.crop or "?",
            print_x(lens.distortion), print_x(lens.tca), print_x(lens.vignetting)))
        previous_maker = lens.maker.lower()

    outfile.write("</tbody></table><h2>Cameras</h2><p>Note that new camera models can be added very easily.  "
              "Contact the LensFun maintainers for this.</p>")

    for maker, cameras in sorted(Camera.camera_makers.items()):
        outfile.write("<p><strong>{}</strong>: {}</p>".format(maker, ", ".join(sorted(cameras))))

    if cmdline_args['table_only'] == False:
        outfile.write("</body></html>")
else:

    #----------------------------------------------------------------------
    # Markdown list

    if cmdline_args['table_only'] == False:
        outfile.write("= LensFun's coverage = \n\n == Lenses (count: {}) ==\n\n"
                  "This list was generated on {} from current LensFun sources.  Your LensFun version may be older, resulting in "
                  "less coverage.  \nIf your lens is not included, see \n\n* <a href='/calibration'>Upload calibration pictures</a>\n"
                  "* <a href='lens_calibration_tutorial/'>Lens calibration for LensFun</a>\n\n".format(
                      len(lenses), datetime.date.today()))

    number_of_makers = 0
    previous_maker = None

    for lens in lenses:
        if lens.maker.lower() != previous_maker:
            number_of_makers += 1

        if lens.maker.lower() != previous_maker:
            outfile.write("\n== {} ==\n\n".format(lens.maker))

        outfile.write("* {} ({}, {}/{}/{})\n".format(lens.model, lens.crop or "?",
            "D" if lens.distortion else "-", 
            "T" if lens.tca else "-",
            "V" if lens.vignetting else "-"))
        previous_maker = lens.maker.lower()

outfile.close()
