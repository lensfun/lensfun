#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import hashlib, subprocess, os


class RawNotFound(FileNotFoundError):
    pass


def generate_thumbnail(raw_filepath, cache_dir):
    """Generates a thumbnail for the given image.  The thumbnail is written into
    the cache dir, given by ``cache_root`` in the INI file.  This is a helper
    routine for `tag_image_files` in order to make the thumbnail generation
    parallel.

    :param str raw_filepath: filepath of the RAW image file
    :param str cache_dir: root path of the thumbnail cache
    """
    hash_ = hashlib.sha1()
    hash_.update(raw_filepath.encode("utf-8"))
    out_filepath = os.path.join(cache_dir, hash_.hexdigest() + ".jpeg")
    if os.path.splitext(raw_filepath)[1].lower() in [".jpeg", ".jpg"]:
        subprocess.Popen(["convert", raw_filepath, "-resize", "131072@", out_filepath]).wait()
    else:
        dcraw = subprocess.Popen(["dcraw", "-h", "-T", "-c", raw_filepath], stdout=subprocess.PIPE)
        subprocess.Popen(["convert", "-", "-resize", "131072@", out_filepath], stdin=dcraw.stdout).wait()
