#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Transforms a version-1 database into a version-2 database.

1. Assign unique IDs to mounts, cameras, and lenses.  All numbers < 1000 are
   reserved for private/local use.  Anything which is not a positive integer
   (this includes 0) is an invalid ID.

2. <aperture> is renamed to <f-stop>.

3. Move the <cropfactor> element out of <lens> into the <calibration> element
   as an attribute.

4. Introduce a <min-crop-factor> element in <lens> which denotes the maximal
   image circle the lens can illuminate.  Populate this field with the minimal
   calibration cropfactor found.

5. Move the <aspect-ratio> element out of <lens> into the <calibration> element
   as an attribute.

6. Move <real-focal-length> tags as attributes into <distortion>.

7. Collect all <calibration> elements of one lens model in one <lens> entry.

8. Make mount implicit for compact cameras (link <lens> to <camera> directly).

9. Add informative "camera" attribute to <calibration>.

Manual postprocessing necessary:

1. Some lenses may have a too big <min-crop-factor>.  (For example, FF lenses
   for which Lensfun only had got APS-C calibrations yet.)

2. Alias entries must be merged.

3. Merge multiple camera entries for multiple crop factors in one.
"""

import sys, glob, os
from lxml import etree


def bump_up_version(root):
    root.attrib["version"] = "2"


mount_ids = {999}
camera_ids = {999}
lens_ids = {999}

def assign_ids(root):
    for child in root.xpath("mount"):
        next_id = max(mount_ids) + 1
        child.attrib["id"] = str(next_id)
        mount_ids.add(next_id)
    for child in root.xpath("camera"):
        next_id = max(camera_ids) + 1
        child.attrib["id"] = str(next_id)
        camera_ids.add(next_id)
    for child in root.xpath("lens"):
        next_id = max(lens_ids) + 1
        child.attrib["id"] = str(next_id)
        lens_ids.add(next_id)


def rename_aperture_to_f_stop(root):
    for element in root.xpath("//aperture"):
        element.tag = "f-stop"


def get_model(element):
    for model in element.iter("model"):
        if not(model.get("lang")):
            return model.text
    else:
        assert False, "No model found."


def copy_cropfactor_and_move_aspect_ratio(root):
    # I don't delete <cropfactor> here in order to be able to replace it with
    # <min-crop-factor> later.
    for element in root.xpath("lens"):
        try:
            calibration = element.xpath("calibration")[0]
        except IndexError:
            continue
        calibration.set("crop-factor", element.xpath("cropfactor")[0].text)
        try:
            aspect_ratio = element.xpath("aspect-ratio")[0]
        except IndexError:
            pass
        else:
            calibration.set("aspect-ratio", aspect_ratio.text)
            aspect_ratio.getparent().remove(aspect_ratio)


def min_cropfactor(root):
    cropfactors = {}
    for element in root.xpath("lens"):
        cropfactors.setdefault(get_model(element), set()).add(element.xpath("cropfactor")[0].text)
    for element in root.xpath("lens"):
        cropfactor_element = element.xpath("cropfactor")[0]
        cropfactor_element.tag = "min-crop-factor"
        cropfactor_element.text = str(min(cropfactors[get_model(element)]))
    

for path in glob.glob(os.path.join(sys.argv[1], "*.xml")):
    tree = etree.parse(open(path))
    root = tree.getroot()

    bump_up_version(root)
    assign_ids(root)
    rename_aperture_to_f_stop(root)
    copy_cropfactor_and_move_aspect_ratio(root)
    min_cropfactor(root)

    tree.write(open(os.path.join(sys.argv[2], os.path.basename(path)), "wb"))
