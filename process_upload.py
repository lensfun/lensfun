#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys, os, subprocess, json, re, multiprocessing, smtplib
from email.mime.text import MIMEText

filepath = sys.argv[1]
directory = os.path.abspath(os.path.dirname(filepath))
email_address = json.load(open(os.path.join(directory, "originator.json")))

def send_email(to, subject, body):
    message = MIMEText(body, _charset = "iso-8859-1")
    me = "Torsten Bronger <bronger@physik.rwth-aachen.de>"
    message["Subject"] = subject
    message["From"] = me
    message["To"] = to
    smtp_connection = smtplib.SMTP_SSL("***REMOVED***")
    smtp_connection.login("***REMOVED***", "***REMOVED***")
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
""".format("http://wilson.homeunix.com/calibration/" + os.path.basename(directory)))

def send_success_email():
    send_email("Torsten Bronger <bronger@physik.rwth-aachen.de>", "Neue Kalibrationsdaten von " + email_address,
               """Hallöchen!

Es liegen neue Kalibrationsdaten von {} vor, siehe

    {}

-- 
Torsten Bronger, aquisgrana, europa vetus
                   Jabber ID: torsten.bronger@jabber.rwth-aachen.de
                                  or http://bronger-jmp.appspot.com
""".format(email_address, directory))

def write_result_and_exit(errors, missing_data=[]):
    if isinstance(errors, str):
        errors = [errors]
    result = (errors, missing_data)
    json.dump(result, open(os.path.join(directory, "result.json"), "w"), ensure_ascii=True)
    if any(result):
        send_error_email()
    else:
        send_success_email()
    sys.exit()

extension = os.path.splitext(filepath)[1].lower()
try:
    if extension == ".gz":
        subprocess.check_call(["tar", "--directory", directory, "-xzf", filepath])
    elif extension == ".zip":
        subprocess.check_call(["unzip", filepath, "-d", directory])
except subprocess.CalledProcessError:
    write_result_and_exit("I could not unpack your file.  Was it really a .tar.gz or ZIP file?")
os.remove(filepath)

raw_file_extensions = ["3fr", "ari", "arw", "bay", "crw", "cr2", "cap", "dcs", "dcr", "dng", "drf", "eip", "erf",
                       "fff", "iiq", "k25", "kdc", "mef", "mos", "mrw", "nef", "nrw", "obm", "orf", "pef", "ptx",
                       "pxn", "r3d", "raf", "raw", "rwl", "rw2", "rwz", "sr2", "srf", "srw", "x3f", "jpg", "tif"]
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
def call_exiftool(raw_file_group):
    data = json.loads(subprocess.check_output(
        ["exiftool", "-j", "-make", "-model", "-lensmodel", "-focallength", "-aperture", "-lensid"] + raw_file_group,
        stderr=open(os.devnull, "w")).decode("utf-8"))
    return dict((single_data["SourceFile"], (
        single_data.get("Make"),
        single_data.get("Model"),
        single_data.get("LensModel") or single_data.get("LensID"),
        float(single_data["FocalLength"].partition("mm")[0]) if "FocalLength" in single_data else None,
        float(single_data["Aperture"]) if "Aperture" in single_data else None))
                for single_data in data)
pool = multiprocessing.Pool()
for group_exif_data in pool.map(call_exiftool, raw_file_groups):
    file_exif_data.update(group_exif_data)
pool.close()
pool.join()


cameras = set(exif_data[:2] for exif_data in file_exif_data.values())
if len(cameras) != 1:
    write_result_and_exit("Multiple camera models found.")
else:
    camera = cameras.pop()

if not file_exif_data:
    write_result_and_exit("No images found in archive.")

errors = []
missing_data = []
filepath_pattern = re.compile(r"(?P<lens_model>.+)--(?P<focal_length>[0-9.]+)mm--(?P<aperture>[0-9.]+)")
for filepath, exif_data in file_exif_data.items():
    filename = os.path.basename(filepath)
    exif_lens_model, exif_focal_length, exif_aperture = exif_data[2:]
    match = filepath_pattern.match(os.path.splitext(os.path.basename(filepath))[0])
    if match:
        filename_lens_model, filename_focal_length, filename_aperture = \
            match.group("lens_model").replace("__", "/").replace("_", " "), float(match.group("focal_length")), \
            float(match.group("aperture"))
        if exif_lens_model and exif_lens_model != filename_lens_model:
            errors.append("{}: Lens model name in file name ({}) doesn't match EXIF data ({}).".format(
                filename, filename_lens_model, exif_lens_model))
        if exif_focal_length and exif_focal_length != filename_focal_length:
            errors.append("{}: Focal length in file name ({}) doesn't match EXIF data ({}).".format(
                filename, filename_focal_length, exif_focal_length))
        if exif_aperture and exif_aperture != filename_aperture:
            errors.append("{}: Aperture in file name ({}) doesn't match EXIF data ({}).".format(
                filename, filename_aperture, exif_aperture))
    elif exif_lens_model and exif_focal_length and exif_aperture:
        os.rename(filepath, os.path.join(os.path.dirname(filepath), "{}--{}mm--{}_{}".format(
            exif_lens_model.replace("/", "__").replace(" ", "_"), exif_focal_length, exif_aperture, filename)))
    else:
        missing_data.append((filename, exif_lens_model, exif_focal_length, exif_aperture))
write_result_and_exit(errors, missing_data)
