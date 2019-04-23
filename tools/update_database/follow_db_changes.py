#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""Generator for database tarballs, in different Lensfun versions.

This program is intended to run as a cronjob, and possibly be run as needed.
It creates a versions.json file and tarballs in the given output directory.  If
desired, it also pushes its content to sourceforge.de.  The
``calibration_webserver`` package must be in the PYTHONPATH.

Since this script reads the same configuration file as the calibration
webserver in $HOME, it should run as the webserver user.  If this is not
feasible, you have to duplicate the INI file.

If a new database version is created in Lensfun, you must add a new `Converter`
class.  Simply use `From1to0` as a starting point.  You prepend the decorator
`@converter` so that the rest of the program finds the new class.  The rest is
automatic.

Note that this script also creates a database with version 0.  This may be
downloaded manually by people who use Lensfun <= 0.2.8.
"""

import glob, os, subprocess, calendar, json, time, tarfile, io, argparse, shutil, configparser, smtplib, textwrap
from subprocess import DEVNULL
from email.mime.text import MIMEText
from lxml import etree
from github import Github
from calibration_webserver import owncloud


parser = argparse.ArgumentParser(description="Generate tar balls of the Lensfun database, also for older versions.")
parser.add_argument("output_path", help="Directory where to put the XML files.  They are put in the db/ subdirectory.  "
                    "It needn't exist yet.")
parser.add_argument("--upload", action="store_true", help="Upload the files to Sourceforge, too.")
args = parser.parse_args()

config = configparser.ConfigParser()
assert config.read(os.path.expanduser("~/calibration_webserver.ini")), os.path.expanduser("~/calibration_webserver.ini")

github = Github(config["GitHub"]["login"], config["GitHub"]["password"])
lensfun = github.get_organization("lensfun").get_repo("lensfun")
calibration_request_label = lensfun.get_label("calibration request")
successful_label = lensfun.get_label("successful")
unsuccessful_label = lensfun.get_label("unsuccessful")
admin = "{} <{}>".format(config["General"]["admin_name"], config["General"]["admin_email"])
root = "/tmp/"


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
        subprocess.check_call(["git", "clone", "https://github.com/lensfun/lensfun.git", "lensfun-git"],
                              stdout=DEVNULL, stderr=DEVNULL)
        os.chdir(root + "lensfun-git")
        db_was_updated = True
    else:
        subprocess.check_call(["git", "fetch"], stdout=DEVNULL, stderr=DEVNULL)
        changed_files = subprocess.check_output(["git", "diff", "--name-only", "master..origin/master"],
                                                stderr=DEVNULL).decode("utf-8").splitlines()
        db_was_updated = any(filename.startswith("data/db/") for filename in changed_files)

    subprocess.check_call(["git", "checkout", "master"], stdout=DEVNULL, stderr=DEVNULL)
    subprocess.check_call(["git", "reset", "--hard", "origin/master"], stdout=DEVNULL, stderr=DEVNULL)
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
                               config["SourceForge"]["login"] + "@web.sourceforge.net:/home/project-web/lensfun/htdocs/db"])


def send_email(to, subject, body):
    """Sends an email using the SMTP configuration given in the INI file.  The
    sender is always the administrator, also as given in the INI file.

    :param to: recipient of the email
    :param subject: subject of the email
    :param body: body text of the email

    :type to: str
    :type subject: str
    :type body: str
    """
    message = MIMEText(body)
    message["Subject"] = subject
    message["From"] = admin
    message["To"] = to
    smtp_connection = smtplib.SMTP(config["SMTP"]["machine"], config["SMTP"]["port"])
    if config["SMTP"].get("TLS", "off").lower() in {"on", "true", "yes"}:
        smtp_connection.starttls()
    if "login" in config["SMTP"]:
        smtp_connection.login(config["SMTP"]["login"], config["SMTP"]["password"])
    smtp_connection.sendmail(admin, [to, config["General"]["admin_email"]], message.as_string())


class OriginatorFileNotReadable(Exception):
    def __init__(self):
        super().__init__("The upload directory (with the uploader's email address) could not be found, "
                         "and/or the originator.json file was not present or not readable.")


def get_upload_data(upload_hash):
    uploads_root = config["General"]["uploads_root"]
    for directory in os.listdir(uploads_root):
        if directory.partition("_")[0] == upload_hash:
            path = os.path.join(uploads_root, directory)
            try:
                return path, json.load(open(os.path.join(path, "originator.json")))
            except (FileNotFoundError, PermissionError, json.JSONDecodeError):
                raise OriginatorFileNotReadable
    else:
        raise OriginatorFileNotReadable


def process_issue(issue, successful):
    issue.remove_from_labels(successful_label if successful else unsuccessful_label)
    if successful:
        body = """Dear uploader,

your upload has been processed and the results were added to
Lensfun's database.  You – like every Lensfun user – can install the
results locally by calling “lensfun-update-data” on the command
line.

{}Please respond to this email if you have any further questions.

Thank you again for your contribution!

(This is an automatically generated message.)
"""
    else:
        body = """Dear uploader,

your upload has been processed but unfortunately, it could not be
used for calibration in its present form.  Please read the
instructions at http://wilson.bronger.org/calibration carefully and
consider a re-upload.

{}Please respond to this email if you have any further questions.

Thank you for your work so far nevertheless!

(This is an automatically generated message.)
"""
    upload_hash = issue.title.split()[-1]
    upload_path, uploader_email = get_upload_data(upload_hash)
    for comment in issue.get_comments().reversed:
        if comment.body.startswith("@uploader"):
            calibrator_comment = comment.body[len("@uploader"):]
            if calibrator_comment.startswith(":"):
                calibrator_comment = calibrator_comment[1:]
            calibrator_comment = textwrap.fill(calibrator_comment.strip(), width=68)
            body = body.format("Additional information from the calibrator:\n" + calibrator_comment + "\n\n")
            break
    else:
        body = body.format("")
    send_email(uploader_email, "Your calibration upload {} has been processed".format(upload_hash), body)
    if config["General"].get("archive_path"):
        destination = config["General"]["archive_path"]
        if not successful:
            destination = os.path.join(destination, "unusable_" + os.path.basename(upload_path))
        shutil.move(upload_path, destination)
    issue.edit(state="closed")


def close_github_issues():
    for issue in lensfun.get_issues(state="all", labels=[calibration_request_label, successful_label]):
        try:
            process_issue(issue, successful=True)
        except (OriginatorFileNotReadable, OSError) as error:
            issue.create_comment(str(error))
    for issue in lensfun.get_issues(state="all", labels=[calibration_request_label, unsuccessful_label]):
        try:
            process_issue(issue, successful=False)
        except (OriginatorFileNotReadable, OSError) as error:
            issue.create_comment(str(error))


db_was_updated = update_git_repository()
if db_was_updated:
    xml_files, timestamp = fetch_xml_files()
    generate_database_tarballs(xml_files, timestamp)
owncloud.sync()
close_github_issues()
owncloud.sync()
