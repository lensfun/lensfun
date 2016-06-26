#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""This script takes the location of a file archive (e.g. a tarball) as its
argument and converts it into an extracted set of files in the same directory.
Moreover, all image files are renamed so that the most important image
parameters like the focal length are included into the filename.

Besides the archive file, it expects a file ``originator.json`` in the same
directory which contains only one string, which is the email address of the
uploader.

It takes the SMTP credentials from the first line in the file
``/var/www/.authinfo``.  Its content may be::

    machine mail.example.com login me243242 port 587 password reallysecret

This program is called from the Apache process and is detached from it.  In the
background, it processes the uploaded archive while the user is presented a
success message in their browser.

Important: for this program to work, the directories
/var/cache/apache2/calibrate and the directory with the above mentioned files
must be writable for the user as which this script runs (probably www-data)!
"""

import hashlib, sys, os, subprocess, json, re, multiprocessing, smtplib
from email.mime.text import MIMEText

filepath = sys.argv[1]
directory = os.path.abspath(os.path.dirname(filepath))
cache_dir = os.path.join("/var/cache/apache2/calibrate", os.path.basename(directory))
email_address = json.load(open(os.path.join(directory, "originator.json")))
authinfo = open("/var/www/.authinfo").readlines()[0].split()
authinfo = dict(zip(authinfo[::2], authinfo[1::2]))

def send_email(to, subject, body):
    message = MIMEText(body, _charset = "iso-8859-1")
    me = "Torsten Bronger <bronger@physik.rwth-aachen.de>"
    message["Subject"] = subject
    message["From"] = me
    message["To"] = to
    smtp_connection = smtplib.SMTP(authinfo["machine"], authinfo["port"])
    smtp_connection.starttls()
    smtp_connection.login(authinfo["login"], authinfo["password"])
    smtp_connection.sendmail(me, [to], message.as_string())

def send_error_email():
    send_email(email_address, "Problems with your calibration images upload", """Hi!

There has been issues with your calibration images upload.
Please visit

    {}

Thank you!

(This is an automatically generated message.)
-- 
Torsten Bronger, aquisgrana, europa vetus
                   Jabber ID: torsten.bronger@jabber.rwth-aachen.de
                                  or http://bronger-jmp.appspot.com
""".format("http://wilson.bronger.org/calibration/results/" + os.path.basename(directory)))

def send_success_email():
    send_email("Torsten Bronger <bronger@physik.rwth-aachen.de>", "New calibration images from " + email_address,
               """Hidy-Ho!

New calibration images arrived from <{}>, see

    {}

-- 
Torsten Bronger, aquisgrana, europa vetus
                   Jabber ID: torsten.bronger@jabber.rwth-aachen.de
                                  or http://bronger-jmp.appspot.com
""".format(email_address, directory))

def write_result_and_exit(error, missing_data=[]):
    result = (error, missing_data)
    json.dump(result, open(os.path.join(directory, "result.json"), "w"), ensure_ascii=True)
    if any(result):
        send_error_email()
    else:
        send_success_email()
    sys.exit()

extension = os.path.splitext(filepath)[1].lower()
try:
    if extension in [".gz", ".tgz"]:
        subprocess.check_call(["tar", "--directory", directory, "-xzf", filepath])
    elif extension in [".bz2", ".tbz2", ".tb2"]:
        subprocess.check_call(["tar", "--directory", directory, "-xjf", filepath])
    elif extension in [".xz", ".txz"]:
        subprocess.check_call(["tar", "--directory", directory, "-xJf", filepath])
    elif extension == ".tar":
        subprocess.check_call(["tar", "--directory", directory, "-xf", filepath])
    elif extension == ".rar":
        subprocess.check_call(["unrar", "x", filepath, directory])
    elif extension == ".7z":
        subprocess.check_call(["7z", "x", "-o" + directory, filepath])
    else:
        # Must be ZIP (else, fail)
        subprocess.check_call(["unzip", filepath, "-d", directory])
except subprocess.CalledProcessError:
    write_result_and_exit("I could not unpack your file.  Supported file formats:\n"
                          ".gz, .tgz, .bz2, .tbz2, .tb2, .xz, .txz, .tar, .rar, .7z, .zip.")
os.remove(filepath)

class InvalidRaw(Exception):
    pass

invalid_lens_model_name_pattern = re.compile(r"^\(\d+\)$|, | or |\|")
raw_file_extensions = ["3fr", "ari", "arw", "bay", "crw", "cr2", "cap", "dcs", "dcr", "dng", "drf", "eip", "erf",
                       "fff", "iiq", "k25", "kdc", "mef", "mos", "mrw", "nef", "nrw", "obm", "orf", "pef", "ptx",
                       "pxn", "r3d", "raf", "raw", "rwl", "rw2", "rwz", "sr2", "srf", "srw", "x3f", "jpg", "jpeg"]
raw_files = []
for root, __, filenames in os.walk(directory):
    for filename in filenames:
        if os.path.splitext(filename)[1].lower()[1:] in raw_file_extensions:
            raw_files.append(os.path.join(root, filename))
raw_files_per_group = len(raw_files) // multiprocessing.cpu_count() + 1
raw_file_groups = []
file_exif_data = {}
while raw_files:
    raw_file_group = raw_files[:raw_files_per_group]
    if raw_file_group:
        raw_file_groups.append(raw_file_group)
    del raw_files[:raw_files_per_group]
def call_exiv2(raw_file_group):
    exiv2_process = subprocess.Popen(
        ["exiv2", "-PEkt", "-g", "Exif.Image.Make", "-g", "Exif.Image.Model",
         "-g", "Exif.Photo.LensModel", "-g", "Exif.Photo.FocalLength", "-g", "Exif.Photo.FNumber",
         "-g", "Exif.NikonLd2.LensIDNumber", "-g", "Exif.Sony2.LensID", "-g", "Exif.NikonLd3.LensIDNumber",
         "-g", "Exif.Nikon3.Lens",
         "-g", "Exif.CanonCs.LensType", "-g", "Exif.Canon.LensModel", "-g", "Exif.Panasonic.LensType",
         "-g", "Exif.PentaxDng.LensType", "-g", "Exif.Pentax.LensType"]
        + raw_file_group, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, error = exiv2_process.communicate()
    if exiv2_process.returncode in [0, 253]:
        pass
    elif exiv2_process.returncode == 1:
        raise InvalidRaw("""I could not read some of your RAW files.\nI attach the error output of exiv2:\n\n"""
                         + error.decode("utf-8").replace(directory + "/", ""))
    result = {}
    for line in output.splitlines():
        # Sometimes, values have trailing rubbish
        line = line.partition(b"\x00")[0].decode("utf-8")
        if "Exif.Photo." in line:
            filepath, data = line.split("Exif.Photo.")
        elif "Exif.Image." in line:
            filepath, data = line.split("Exif.Image.")
        elif "Exif.NikonLd2." in line:
            filepath, data = line.split("Exif.NikonLd2.")
        elif "Exif.NikonLd3." in line:
            filepath, data = line.split("Exif.NikonLd3.")
        elif "Exif.Nikon3." in line:
            filepath, data = line.split("Exif.Nikon3.")
        elif "Exif.Sony2." in line:
            filepath, data = line.split("Exif.Sony2.")
        elif "Exif.CanonCs." in line:
            filepath, data = line.split("Exif.CanonCs.")
        elif "Exif.Canon." in line:
            filepath, data = line.split("Exif.Canon.")
        elif "Exif.Panasonic." in line:
            filepath, data = line.split("Exif.Panasonic.")
        elif "Exif.PentaxDng." in line:
            filepath, data = line.split("Exif.PentaxDng.")
        elif "Exif.Pentax." in line:
            filepath, data = line.split("Exif.Pentax.")
        filepath = filepath.rstrip()
        if not filepath:
            assert len(raw_file_group) == 1
            filepath = raw_file_group[0]
        try:
            fieldname, field_value = data.split(None, 1)
        except ValueError:
            # Field value was empty
            continue
        exif_data = result.setdefault(filepath, [None, None, None, float("nan"), float("nan")])
        if fieldname == "Make":
            exif_data[0] = field_value
        elif fieldname == "Model":
            exif_data[1] = field_value
        elif fieldname in ["LensID", "LensIDNumber", "LensType", "LensModel", "Lens"]:
            if (not exif_data[2] or len(field_value) > len(exif_data[2])) and \
               not invalid_lens_model_name_pattern.search(field_value):
                exif_data[2] = field_value
        elif fieldname == "FocalLength":
            exif_data[3] = float(field_value.partition("mm")[0])
        elif fieldname == "FNumber":
            if field_value != "(0/0)":
                exif_data[4] = float(field_value.partition("F")[2])
    for filepath, exif_data in result.copy().items():
        if not exif_data[2]:
            camera_model = exif_data[1] and exif_data[1].lower() or ""
            if "powershot" in camera_model or "coolpix" in camera_model or "dsc" in camera_model:
                exif_data[2] = "Standard"
            else:
                # Fallback to Exiftool
                data = json.loads(subprocess.check_output(
                    ["exiftool", "-j", "-lensmodel", "-lensid", "-lenstype", filepath],
                    stderr=open(os.devnull, "w")).decode("utf-8"))[0]
                exiftool_lens_model = data.get("LensID") or data.get("LensModel") or data.get("LensType")
                if exiftool_lens_model and "unknown" not in exiftool_lens_model.lower() \
                   and "manual lens" not in exiftool_lens_model.lower():
                    exif_data[2] = exiftool_lens_model
        result[filepath] = tuple(exif_data)
    return result
pool = multiprocessing.Pool()
try:
    for group_exif_data in pool.map(call_exiv2, raw_file_groups):
        file_exif_data.update(group_exif_data)
except InvalidRaw as error:
    write_result_and_exit(error.args[0])
pool.close()
pool.join()


if not file_exif_data:
    write_result_and_exit("No images (at least, no with EXIF data) found in archive.")

cameras = set(exif_data[:2] for exif_data in file_exif_data.values())
if len(cameras) != 1:
    write_result_and_exit("Multiple camera models found.")
else:
    camera = cameras.pop()

missing_data = []
filepath_pattern = re.compile(r"(?P<lens_model>.+)--(?P<focal_length>[0-9.]+)mm--(?P<aperture>[0-9.]+)")
for filepath, exif_data in file_exif_data.items():
    filename = os.path.basename(filepath)
    exif_lens_model, exif_focal_length, exif_aperture = exif_data[2:]
    if not filepath_pattern.match(os.path.splitext(os.path.basename(filepath))[0]):
        if exif_lens_model and exif_focal_length and exif_aperture:
            if exif_focal_length == int(exif_focal_length):
                focal_length = format(int(exif_focal_length), "03")
            else:
                focal_length = format(exif_focal_length, "05.1f")
            os.rename(filepath, os.path.join(os.path.dirname(filepath), "{}--{}mm--{}_{}".format(
                exif_lens_model, focal_length, exif_aperture, filename). \
                      replace(":", "___").replace("/", "__").replace(" ", "_").replace("*", "++").replace("=", "##")))
        else:
            missing_data.append((filepath, exif_lens_model, exif_focal_length, exif_aperture))

if missing_data:
    try:
        os.makedirs(cache_dir)
    except FileExistsError:
        pass
    def generate_thumbnail(raw_filepath):
        hash_ = hashlib.sha1()
        hash_.update(raw_filepath.encode("utf-8"))
        out_filepath = os.path.join(cache_dir, hash_.hexdigest() + ".jpeg")
        if os.path.splitext(raw_filepath)[1].lower() in [".jpeg", ".jpg"]:
            subprocess.Popen(["convert", raw_filepath, "-resize", "131072@", out_filepath]).wait()
        else:
            dcraw = subprocess.Popen(["dcraw", "-h", "-T", "-c", raw_filepath], stdout=subprocess.PIPE)
            subprocess.Popen(["convert", "-", "-resize", "131072@", out_filepath], stdin=dcraw.stdout).wait()
    pool = multiprocessing.Pool()
    pool.map(generate_thumbnail, [data[0] for data in missing_data])
    pool.close()
    pool.join()


write_result_and_exit(None, missing_data)
