#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import absolute_import, unicode_literals

import hashlib, os, subprocess, json, shutil, mimetypes, smtplib, re, configparser
from email.mime.text import MIMEText
import django.forms as forms
from django.shortcuts import render
from django.forms.utils import ValidationError
import django.core.urlresolvers
import django.http
from django.utils.encoding import iri_to_uri


config = configparser.ConfigParser()
config.read(os.path.expanduser("~/calibration_webserver.ini"))

upload_directory = config["General"]["uploads_root"]
admin = "{} <{}>".format(config["General"]["admin_name"], config["General"]["admin_email"])
allowed_extensions = (".tar.gz", ".tgz", ".tar.bz2", ".tbz2", ".bz2", ".tar.xz", ".txz", ".tar", ".rar", ".7z", ".zip")
file_extension_pattern = re.compile("(" + "|".join(allowed_extensions).replace(".", "\\.") + ")$", re.IGNORECASE)


class HttpResponseSeeOther(django.http.HttpResponse):
    """Response class for HTTP 303 redirects.  Unfortunately, Django does the
    same wrong thing as most other web frameworks: it knows only one type of
    redirect, with the HTTP status code 302.

    It can simply be used as a drop-in replacement of HttpResponseRedirect.
    """
    status_code = 303

    def __init__(self, redirect_to):
        super(HttpResponseSeeOther, self).__init__()
        self["Location"] = iri_to_uri(redirect_to)


def send_email(to, subject, body):
    message = MIMEText(body.encode("iso-8859-1"), _charset = "iso-8859-1")
    message["Subject"] = subject
    message["From"] = admin
    message["To"] = to
    smtp_connection = smtplib.SMTP(config["SMTP"]["machine"], config["SMTP"]["port"])
    smtp_connection.starttls()
    smtp_connection.login(config["SMTP"]["login"], config["SMTP"]["password"])
    smtp_connection.sendmail(admin, [to], message.as_string())

def send_success_email(email_address, id_):
    send_email(admin, "New calibration images from " + email_address,
               """Hidy-Ho!

New calibration images arrived from <{}>, see

    {}
""".format(email_address, os.path.join(config["General"]["uploads_root"], id_)))


def spawn_daemon(path_to_executable, *args):
    """Spawns a completely detached subprocess (i.e., a daemon).  Taken from
    http://stackoverflow.com/questions/972362/spawning-process-from-python
    which in turn was inspired by
    <http://code.activestate.com/recipes/278731/>.

    :Parameters:
      - `path_to_executable`: absolute path to the executable to be run
        detached
      - `args`: all arguments to be passed to the subprocess

    :type path_to_executable: str
    :type args: list of str
    """
    try:
        pid = os.fork()
    except OSError as e:
        raise RuntimeError("1st fork failed: %s [%d]" % (e.strerror, e.errno))
    if pid != 0:
        os.waitpid(pid, 0)
        return
    os.setsid()
    try:
        pid = os.fork()
    except OSError as e:
        raise RuntimeError("2nd fork failed: %s [%d]" % (e.strerror, e.errno))
    if pid != 0:
        os._exit(0)
    try:
        maxfd = os.sysconf(b"SC_OPEN_MAX")
    except (AttributeError, ValueError):
        maxfd = 1024
    for fd in range(maxfd):
        try:
           os.close(fd)
        except OSError:
           pass
    os.open(os.devnull, os.O_RDWR)
    os.dup2(0, 1)
    os.dup2(0, 2)
    try:
        os.execv(path_to_executable, [path_to_executable] + list(filter(lambda arg: arg is not None, args)))
    except:
        os._exit(255)

def store_upload(uploaded_file, email_address):
    hash_ = hashlib.sha1()
    hash_.update(uploaded_file.name)
    hash_.update(str(uploaded_file.size))
    hash_.update(email_address)
    id_ = hash_.hexdigest()[:6] + "_" + email_address.partition("@")[0]
    directory = os.path.join(upload_directory, id_)
    try:
        shutil.rmtree(directory)
    except OSError as error:
        message = str(error).partition(":")[0]
        if message == "[Errno 13] Permission denied":
            # Directory has been already chowned to the calibrator.
            return id_
        elif message == "[Errno 2] No such file or directory":
            pass
        else:
            raise
    os.makedirs(directory)
    with open(os.path.join(directory, "originator.json"), "w") as outfile:
        json.dump(email_address, outfile, ensure_ascii=True)
    filepath = os.path.join(directory, uploaded_file.name)
    with open(filepath, "wb") as outfile:
        for chunk in uploaded_file.chunks():
            outfile.write(chunk)
    spawn_daemon("/usr/bin/env", "python3",
                 os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "process_upload.py"), filepath)
    return id_

class UploadForm(forms.Form):
    compressed_file = forms.FileField(label="Archive with RAW files", help_text="Must be a .tar.gz or a .zip file")
    email_address = forms.EmailField(label="Your email address")

    def clean_compressed_file(self):
        compressed_file = self.cleaned_data["compressed_file"]
        if not file_extension_pattern.search(compressed_file.name):
            raise ValidationError("Uploaded file must be ZIP, tarball, RAR, or 7-zip.")
        return compressed_file

def upload(request):
    if request.method == "POST":
        try:
            upload_form = UploadForm(request.POST, request.FILES)
        except django.http.UnreadablePostError:
            return django.http.HttpResponseBadRequest(b"Upload was incomplete.")
        if upload_form.is_valid():
            id_ = store_upload(request.FILES["compressed_file"], upload_form.cleaned_data["email_address"])
            return HttpResponseSeeOther(django.core.urlresolvers.reverse("show_issues", kwargs={"id_": id_}))
    else:
        upload_form = UploadForm()
    return render(request, "calibration/upload.html", {"title": "Calibration images upload", "upload": upload_form})

class ExifForm(forms.Form):
    lens_model_name = forms.CharField(max_length=255, label="lens model name")
    focal_length = forms.DecimalField(min_value=0.1, max_digits=5, decimal_places=1, label="focal length",
                                      help_text="in mm")
    aperture = forms.DecimalField(min_value=0.1, max_digits=4, decimal_places=1, label="f-stop number")

    def __init__(self, missing_data, first, *args, **kwargs):
        super(ExifForm, self).__init__(*args, **kwargs)
        self.fields["lens_model_name"].widget.attrs["size"] = 60
        self.fields["lens_model_name"].initial, self.fields["focal_length"].initial, self.fields["aperture"].initial = \
            missing_data[1:]
        if not first:
            for fieldname in ["lens_model_name", "focal_length", "aperture"]:
                self.fields[fieldname].required = False

def show_issues(request, id_):
    directory = os.path.join(upload_directory, id_)
    try:
        email_address = json.load(open(os.path.join(directory, "originator.json")))
    except IOError:
        raise django.http.Http404
    try:
        error, missing_data = json.load(open(os.path.join(directory, "result.json")))
    except IOError:
        return render(request, "calibration/pending.html")
    missing_data.sort()
    if error:
        return render(request, "calibration/error.html", {"error": error})
    elif not missing_data:
        return render(request, "calibration/success.html")
    filepaths = [os.path.relpath(data[0], directory) for data in missing_data]
    thumbnails = []
    for data in missing_data:
        hash_ = hashlib.sha1()
        hash_.update(data[0].encode("utf-8"))
        thumbnails.append("/calibration/thumbnails/{0}/{1}".format(id_, hash_.hexdigest()))
    if request.method == "POST":
        exif_forms = [ExifForm(data, i==0, request.POST, prefix=str(i)) for i, data in enumerate(missing_data)]
        if all([exif_form.is_valid() for exif_form in exif_forms]):
            for data, exif_form in zip(missing_data, exif_forms):
                lens_model_name = exif_form.cleaned_data["lens_model_name"] or lens_model_name
                focal_length = exif_form.cleaned_data["focal_length"] or focal_length
                if focal_length == int(focal_length):
                    focal_length = format(int(focal_length), "03")
                else:
                    focal_length = format(focal_length, "05.1f")
                aperture = exif_form.cleaned_data["aperture"] or aperture
                filepath = data[0]
                filename = os.path.basename(filepath)
                os.rename(filepath, os.path.join(os.path.dirname(filepath), "{}--{}mm--{}_{}".format(
                    lens_model_name.replace("*", "++").replace("=", "##").replace(":", "___").replace("/", "__").replace(" ", "_"),
                    focal_length, aperture, filename)))
            json.dump((None, []), open(os.path.join(directory, "result.json"), "w"), ensure_ascii=True)
            shutil.rmtree("/var/cache/apache2/calibrate/" + id_)
            send_success_email(email_address, id_)
            return render(request, "calibration/success.html")
    else:
        exif_forms = [ExifForm(data, i==0, prefix=str(i)) for i, data in enumerate(missing_data)]
    return render(request, "calibration/missing_exif.html", {"images": zip(filepaths, thumbnails, exif_forms)})

def thumbnail(request, id_, hash_):
    filepath = os.path.join("/var/cache/apache2/calibrate", id_, hash_ + ".jpeg")
    if not os.path.exists(filepath):
        raise django.http.Http404(filepath)
    response = django.http.HttpResponse()
    response["X-Sendfile"] = filepath
    response["Content-Type"] = mimetypes.guess_type(filepath)[0] or "application/octet-stream"
    response["Content-Length"] = os.path.getsize(filepath)
    return response
