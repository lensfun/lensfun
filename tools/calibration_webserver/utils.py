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
    try:
        os.mkdir(cache_dir)
    except FileExistsError:
        pass
    processes = []
    if os.path.splitext(raw_filepath)[1].lower() in [".tif", ".tiff", ".jpeg", ".jpg"]:
        processes.append(subprocess.Popen(["convert", raw_filepath, "-resize", "131072@", out_filepath]))
    else:
        dcraw = subprocess.Popen(["dcraw_emu", "-h", "-T", "-c", raw_filepath], stdout=subprocess.PIPE)
        processes.append(dcraw)
        processes.append(subprocess.Popen(["convert", "-", "-resize", "131072@", out_filepath], stdin=dcraw.stdout))
    return_codes = [process.wait() for process in processes]
    if any(code != 0 for code in return_codes):
        raise RawNotFound
