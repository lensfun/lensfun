#!/usr/bin/env python3

"""This script takes the location of a file archive (e.g. a tarball) as its
argument and converts it into an extracted set of files in the same directory.
Moreover, all image files are renamed so that the most important image
parameters like the focal length are included into the filename.  Furthermore, a
GitHub issue is created in the “lensfun/lensfun” repository.  Finally, the
Nextcloud command line client is called for pushing the new files to the
Nextcloud server for the calibrators to pick them up.

Besides the archive file, it expects a file ``originator.json`` in the same
directory which contains only one string, which is the email address of the
uploader.

This script works in two modes of operation:

1. The first argument is "initial" and the second the absolute path to a
   tarball.  This does the thing explained above.
2. The first argument is "amended" and the second the absolute path to a
   directory.  This happens if the first processing resulted in missing EXIF
   data, and the user has provided it, and now the final stages of processing
   (e.g. GitHub issue, Nextcloud synchronisation) can be performed.  The
   directory is in this case the directory into which the original tarball was
   extracted.

This program is called from the Apache process and is detached from it.  In the
background, it processes the uploaded archive while the user is presented a
success message in their browser.
"""

import sys, os, subprocess, json, re, multiprocessing, smtplib, configparser, logging
from email.mime.text import MIMEText
from github import Github
from calibration_webserver import nextcloud, utils


logging.basicConfig(level=logging.DEBUG, filename="/var/log/process_upload.log")


config = configparser.ConfigParser()
config.read(os.path.expanduser("~/calibration_webserver.ini"))

admin = "{} <{}>".format(config["General"]["admin_name"], config["General"]["admin_email"])


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
    logging.debug("Send mail with subject “{}”".format(subject))
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


def send_error_email():
    """Sends an error email to the uploader.  This happens if the upload was
    erroneous (bad file format, not image files in it etc), or if for some or
    all of the images their lens data could not be extracted.
    """
    send_email(email_address, "Problems with your calibration images upload " + upload_id, """Hi!

Thank you for your images upload!  However, There have been issues
with the contents.  Please visit

    {}

Thank you!

(This is an automatically generated message.)
""".format(config["General"]["root_url"] + "/results/" + upload_id))


def send_success_email(issue_link):
    """Sends a success (acknowledge) email to the uploader.

    :param issue_link: URL to the GitHub issue of this calibration images set.

    :type issue_link: str
    """
    send_email(email_address, "Your calibration upload " + upload_id, """Hi!

Thank you for your images upload!  You can follow progress on GitHub
at <{}>.  If you
like to join on GitHub, follow-up to the issue with a short comment
“I am the uploader” or something like that.  Otherwise, we will
discuss any questions regarding your images with you by email.
Either way, you'll get an email when the processing is finished.

(This is an automatically generated message.)
""".format(issue_link))


def sync_with_github():
    """Create of re-open an issue on GitHub for this calibration upload.

    :return:
      the URL to the GitHub issue

    :rtype: str
    """
    upload_hash = upload_id.partition("_")[0]
    title = "Calibration upload " + upload_hash
    for issue in github.lensfun.get_issues(state="all", labels=[github.calibration_request_label]):
        if issue.title == title:
            issue.edit(state="open")
            issue.create_comment("The original uploader has uploaded the very same files again.  It should be discussed "
                                 "with the uploader why this was done.")
            break
    else:
        body = "Calibration images were uploaded to the directory that starts with “`{0}_`”.\n\n" \
               "Please read the [workflow description]" \
               "(https://github.com/lensfun/lensfun/blob/master/tools/calibration_webserver/workflow.rst) for further " \
               "instructions about the calibration.\n".format(upload_hash)
        try:
            comments = open(os.path.join(directory, "comments.txt")).read()
        except FileNotFoundError:
            pass
        else:
            body += "\nAdditional comments by the uploader:\n\n> " + comments.strip().replace("\n", "\n> ")
        issue = github.lensfun.create_issue("Calibration upload {}".format(upload_hash), body=body,
                                            labels=[github.calibration_request_label])
    return issue.html_url


def handle_successful_upload():
    """Make all necessary steps after a successful upload.  This encompasses:

    1. Creation of an GitHub issue.
    2. Synchronisation of the Nextcloud directory.
    3. Sending of a success email to the uploader.
    """
    issue_link = sync_with_github()
    send_success_email(issue_link)
    nextcloud.sync()


def write_result_and_exit(error, missing_data=[]):
    """Write ``result.json``, send mail to the uploader, and terminate this
    program.

    :param error: error message for the uploader; ``None`` if no error
      occurred.
    :param missing_data: files for which no EXIF lens data could be determined.

    :type error: str or NoneType
    :type missing_data: list of (str, str or NoneType, float or NoneType, float
      or NoneType)
    """
    result = (error, missing_data)
    json.dump(result, open(os.path.join(directory, "result.json"), "w"), ensure_ascii=True)
    if any(result):
        send_error_email()
    else:
        handle_successful_upload()
    sys.exit()


class InvalidArchive(ValueError):
    def __init__(self):
        super().__init__("InvalidArchive")

def extract_archive():
    """Extracts the archive (e.g. the tarball) which was uploaded.  Afterwards, the
    archive file is deleted.
    """
    def protect_files(protected_files):
        result = {}
        for filename in protected_files:
            path = os.path.join(directory, filename)
            try:
                result[filename] = open(path, "rb").read()
            except FileNotFoundError:
                result[filename] = None
            else:
                os.remove(path)
        return result
    def restore_files(protected_files):
        for filename, contents in protected_files.items():
            path = os.path.join(directory, filename)
            if os.path.exists(path):
                index = 1
                while os.path.exists("{}.{}".format(path, index)):
                    index += 1
                os.rename(path, "{}.{}".format(path, index))
            if contents is not None:
                open(path, "wb").write(contents)
    def call_unpacker(arguments):
        return subprocess.run(arguments, check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True)
    protected_files = protect_files({"originator.json", "comments.txt"})
    extension = os.path.splitext(filepath)[1].lower()
    try:
        if extension in [".gz", ".tgz"]:
            call_unpacker(["tar", "--directory", directory, "-xzf", filepath])
        elif extension in [".bz2", ".tbz2", ".tb2"]:
            call_unpacker(["tar", "--directory", directory, "-xjf", filepath])
        elif extension in [".xz", ".txz"]:
            call_unpacker(["tar", "--directory", directory, "-xJf", filepath])
        elif extension == ".tar":
            call_unpacker(["tar", "--directory", directory, "-xf", filepath])
        elif extension == ".rar":
            call_unpacker(["unrar", "x", filepath, directory])
        elif extension == ".7z":
            call_unpacker(["7z", "x", "-o" + directory, filepath])
        elif extension == ".zip":
            call_unpacker(["unzip", filepath, "-x", "/", "-d", directory])
        else:
            raise InvalidArchive
    except subprocess.CalledProcessError as error:
        send_email(admin, "Error when extracting calibration upload " + upload_id,
                   "Error: {}\n\nstdout: {}\nstderr: {}".format(
                       error, repr(error.stdout), repr(error.stderr)))
        restore_files(protected_files)
        write_result_and_exit("Unpacking your file resulted in an error.")
    except InvalidArchive:
        restore_files(protected_files)
        write_result_and_exit("I could not unpack your file.  Supported file formats:\n"
                              ".gz, .tgz, .bz2, .tbz2, .tb2, .xz, .txz, .tar, .rar, .7z, .zip.")
    finally:
        restore_files(protected_files)
    os.remove(filepath)


class InvalidRaw(Exception):
    """Raised if a RAW could not be parsed by exiv2.
    """
    pass


invalid_lens_model_name_pattern = re.compile(r"^\(\d+\)$|, | or |\||manual lens|unknown", re.IGNORECASE)
"""Lens model names which must be assumed to be invalid like “(234)”.
"""


def call_exiv2(raw_file_group):
    """Extracts the EXIF data from the given RAW image files.  This is a helper
    function for `collect_exif_data`.  It is used for parallelising the work.

    :param raw_file_group: the raw files that should be processed.

    :type raw_file_group: list of str

    :return:
      the extracted EXIF data for the given files, as a dictionary mapping
      filepaths to (camera make, camera model, lens model, focal length, f-stop
      number)

    :rtype: dict mapping str to (str, str, str, float, float)
    """
    exiv2_process = subprocess.Popen(
        ["exiv2", "-PEkt", "-g", "Exif.Image.Make", "-g", "Exif.Image.Model",
         "-g", "Exif.Photo.LensModel", "-g", "Exif.Photo.FocalLength", "-g", "Exif.Photo.FNumber",
         "-g", "Exif.NikonLd2.LensIDNumber", "-g", "Exif.Sony2.LensID", "-g", "Exif.NikonLd3.LensIDNumber",
         "-g", "Exif.Nikon3.Lens", "-g", "Exif.Canon.FocalLength",
         "-g", "Exif.CanonCs.LensType", "-g", "Exif.Canon.LensModel", "-g", "Exif.Panasonic.LensType",
         "-g", "Exif.PentaxDng.LensType", "-g", "Exif.Pentax.LensType"]
        + raw_file_group, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, error = exiv2_process.communicate()
    if exiv2_process.returncode in [0, 253]:
        pass
    elif exiv2_process.returncode == 1:
        raise InvalidRaw("""I could not read some of your RAW files.\nI attach the error output of exiv2:\n\n"""
                         + error.decode("utf-8").replace(directory + "/", ""))
    result = {filepath: [None, None, None, float("nan"), float("nan")] for filepath in raw_file_group}
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
        else:
            field_value = field_value.strip()
        exif_data = result[filepath]
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


def collect_exif_data():
    """Extracts the EXIF data from all RAW image files of the upload.

    :return:
      the extracted EXIF data for the given files, as a dictionary mapping
      filepaths to (camera make, camera model, lens model, focal length, f-stop
      number)

    :rtype: dict mapping str to (str, str, str, float, float)
    """
    raw_file_extensions = ["3fr", "ari", "arw", "bay", "crw", "cr2", "cr3", "cap", "dcs", "dcr", "dng", "drf", "eip",
                           "erf", "fff", "iiq", "k25", "kdc", "mef", "mos", "mrw", "nef", "nrw", "obm", "orf", "pef",
                           "ptx", "pxn", "r3d", "raf", "raw", "rwl", "rw2", "rwz", "sr2", "srf", "srw", "tif", "tiff",
                           "x3f", "jpg", "jpeg"]

    raw_files = []
    ignored_directories = {"__MACOSX"}
    for root, dirnames, filenames in os.walk(directory, topdown=True):
        dirnames[:] = [directory for directory in dirnames if directory not in ignored_directories]
        for filename in filenames:
            if os.path.splitext(filename)[1].lower()[1:] in raw_file_extensions:
                raw_files.append(os.path.join(root, filename))

    file_exif_data = {}
    raw_files_per_group = len(raw_files) // multiprocessing.cpu_count() + 1
    raw_file_groups = []
    while raw_files:
        raw_file_group = raw_files[:raw_files_per_group]
        if raw_file_group:
            raw_file_groups.append(raw_file_group)
        del raw_files[:raw_files_per_group]
    pool = multiprocessing.Pool()
    try:
        for group_exif_data in pool.map(call_exiv2, raw_file_groups):
            file_exif_data.update(group_exif_data)
    except InvalidRaw as error:
        write_result_and_exit(error.args[0])
    pool.close()
    pool.join()
    return file_exif_data


def check_data(file_exif_data):
    """Performs basic checks on the upload contents.  It checks whether at least
    one image is in the upload, and whether exactly one camera was used.  If
    the check fails, `write_result_and_exit` is called and thus the program
    terminated.

    :param file_exif_data: the extracted EXIF data for the given files, as a
      dictionary mapping filepaths to (camera make, camera model, lens model,
      focal length, f-stop number)

    :type file_exif_data: dict mapping str to (str, str, str, float, float)
    """
    if not file_exif_data:
        write_result_and_exit("No images (at least, no with EXIF data) found in archive.")

    cameras = set(exif_data[:2] for exif_data in file_exif_data.values())
    if len(cameras) != 1:
        write_result_and_exit("Multiple camera models found.")


def tag_image_files(file_exif_data):
    """Renames the image files so that essential EXIF data is in the filename.
    Moreover, this function collects files with missing EXIF data, creates
    thumbnails for them, and returns the list of them.

    :param file_exif_data: the extracted EXIF data for the given files, as a
      dictionary mapping filepaths to (camera make, camera model, lens model,
      focal length, f-stop number)

    :type file_exif_data: dict mapping str to (str, str, str, float, float)

    :return:
      All files for which EXIF data is missing, as a list (filepath, lens
      model, focal length, f-stop number).  Anything missing is ``None``.

    :rtype: list of (str, str or NoneType, float or NoneType, float or
      NoneType)
    """
    missing_data = []
    filepath_pattern = re.compile(r"(?P<lens_model>.+)--(?P<focal_length>[0-9.]+)mm--(?P<aperture>[0-9.]+)")
    for filepath, exif_data in file_exif_data.items():
        filename = os.path.basename(filepath)
        exif_lens_model, exif_focal_length, exif_aperture = exif_data[2:]
        if not filepath_pattern.match(os.path.splitext(os.path.basename(filepath))[0]):
            if exif_lens_model and exif_focal_length and exif_aperture:
                logging.debug("All EXIF data found in " + filepath)
                if exif_focal_length == int(exif_focal_length):
                    focal_length = format(int(exif_focal_length), "03")
                else:
                    focal_length = format(exif_focal_length, "05.1f")
                os.rename(filepath, os.path.join(os.path.dirname(filepath), "{}--{}mm--{}_{}".format(
                    exif_lens_model, focal_length, exif_aperture, filename).replace("/", "__")))
            else:
                logging.info("Missing EXIF data in " + filepath)
                missing_data.append((filepath, exif_lens_model, exif_focal_length, exif_aperture))
    if missing_data:
        try:
            os.makedirs(cache_dir)
        except FileExistsError:
            pass
        pool = multiprocessing.Pool()
        pool.starmap(utils.generate_thumbnail, [(data[0], cache_dir) for data in missing_data])
        pool.close()
        pool.join()
    return missing_data


class GithubConfiguration:
    """Holds the GitHub-related variables.  These variables represent the
    connection to the Lensfun project on GitHub.
    """
    def __init__(self):
        self.github = Github(config["GitHub"]["token"])
        self.lensfun = self.github.get_organization("lensfun").get_repo("lensfun")
        self.calibration_request_label = self.lensfun.get_label("calibration request")


def quote_directory(path):
    """Walks through `path` and escapes all names of files and directories.  After
    this operation, the directory can be safely transferred with Nextcloud.
    Moreover, Hugin will not complain about the filenames.  *Important*: This
    function is idempotent.

    :param str path: the path to the directory to be escaped
    """
    def quote_filename_component(name):
        """Escapes `name` so that it can be safely used as a filename in Nextcloud
        or Hugin.  *Important*: This function is idempotent.

        :param str name: the name to be escaped

        :returns:
          the escaped name

        :rtype: str
        """
        assert "/" not in name
        intermediate_name = name.replace(":", "___").replace(" ", "_").replace("*", "++").replace("=", "##")
        result = ""
        for char in intermediate_name:
            if char in ';%?><|"~&':
                result += f"{{{ord(char)}}}"
            else:
                result += char
        logging.debug(f"quoting {name} into {result}")
        return result

    logging.debug(f"Quoting directory {path}")
    for root, dirnames, filenames in os.walk(path, topdown=False):
        for filename in filenames + dirnames:
            quoted_filename = quote_filename_component(filename)
            if quoted_filename != filename:
                old, new = os.path.join(root, filename), os.path.join(root, quoted_filename)
                logging.info(f"renaming '{old}' into {new}")
                os.rename(old, new)
    # FixMe: The following four lines are superfluous if the assertion is never
    # triggered.
    quoted_path = "/".join(quote_filename_component(component) for component in path.split("/"))
    if quoted_path != path:
        assert False, (quoted_path, path)
        os.rename(path, quoted_path)


logging.info("Started process_upload with arguments: {}".format(sys.argv[1:]))
operation = sys.argv[1]
if operation == "initial":
    filepath = sys.argv[2]
    directory = os.path.abspath(os.path.dirname(filepath))
    upload_id = os.path.basename(directory)
    try:
        cache_dir = os.path.join(config["General"]["cache_root"], upload_id)
        email_address = json.load(open(os.path.join(directory, "originator.json")))
        github = GithubConfiguration()

        extract_archive()
        quote_directory(directory)
        file_exif_data = collect_exif_data()
        check_data(file_exif_data)
        missing_data = tag_image_files(file_exif_data)
        write_result_and_exit(None, missing_data)
    except Exception as error:
        error_description = "{}: {}".format(repr(error), str(error))
        logging.critical(error_description)
        send_email(admin, "Error in calibration upload " + upload_id, error_description)
    logging.info("Successfully exited process_upload")
elif operation == "amended":
    directory = sys.argv[2]
    quote_directory(directory)
    upload_id = os.path.basename(directory)
    try:
        email_address = json.load(open(os.path.join(directory, "originator.json")))
        github = GithubConfiguration()
        handle_successful_upload()
    except Exception as error:
        error_description = "{}: {}".format(repr(error), str(error))
        logging.critical(error_description)
        send_email(admin, "Error in calibration upload " + upload_id, error_description)
    logging.info("Successfully exited process_upload")
else:
    logging.critical("Invalid operation")
    raise Exception("Invalid operation")
