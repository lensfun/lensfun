#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import absolute_import, unicode_literals

import hashlib, os, subprocess, json, shutil, mimetypes
import django.forms as forms
from django.shortcuts import render
from django.forms.util import ValidationError
import django.core.urlresolvers
import django.http
from django.utils.encoding import iri_to_uri


upload_directory = "/mnt/media/raws/Kalibration/uploads"


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


def spawn_daemon(path_to_executable, *args):
    """Spawns a completely detached subprocess (i.e., a daemon).  Taken from
    http://stackoverflow.com/questions/972362/spawning-process-from-python
    which in turn was inspired by
    <http://code.activestate.com/recipes/278731/>.

    :Parameters:
      - `path_to_executable`: absolute path to the executable to be run
        detatched
      - `args`: all arguments to be passed to the subprocess

    :type path_to_executable: str
    :type args: list of str
    """
    try:
        pid = os.fork()
    except OSError, e:
        raise RuntimeError("1st fork failed: %s [%d]" % (e.strerror, e.errno))
    if pid != 0:
        os.waitpid(pid, 0)
        return
    os.setsid()
    try:
        pid = os.fork()
    except OSError, e:
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
    id_ = email_address.partition("@")[0] + "_" + hash_.hexdigest()
    directory = os.path.join(upload_directory, id_)
    shutil.rmtree(directory, ignore_errors=True)
    os.makedirs(directory)
    with open(os.path.join(directory, "originator.json"), "w") as outfile:
        json.dump(email_address, outfile, ensure_ascii=True)
    filepath = os.path.join(directory, uploaded_file.name)
    with open(filepath, "wb") as outfile:
        for chunk in uploaded_file.chunks():
            outfile.write(chunk)
    spawn_daemon("/usr/bin/python3", "/home/bronger/src/calibration/process_upload.py", filepath)
    return id_

class UploadForm(forms.Form):
    compressed_file = forms.FileField(label="Archive with RAW files")
    email_address = forms.EmailField(label="Your email address")

    def clean_compressed_file(self):
        compressed_file = self.cleaned_data["compressed_file"]
        if compressed_file.content_type not in ["application/zip", "application/x-tar"]:
            raise ValidationError("Uploaded file must be ZIP or gzip-ped tarball.")
        return compressed_file

def upload(request):
    if request.method == "POST":
        upload_form = UploadForm(request.POST, request.FILES)
        if upload_form.is_valid():
            id_ = store_upload(request.FILES["compressed_file"], upload_form.cleaned_data["email_address"])
            return HttpResponseSeeOther(django.core.urlresolvers.reverse(show_issues, kwargs={"id_": id_}))
    else:
        upload_form = UploadForm()
    return render(request, "calibration/upload.html", {"title": "Calibration images upload", "upload": upload_form})

class ExifForm(forms.Form):
    lens_model_name = forms.CharField(max_length=255, label="lens model name")
    focal_length = forms.DecimalField(min_value=0.01, max_digits=4, decimal_places=1, label="focal length",
                                      help_text="in mm")
    aperture = forms.DecimalField(min_value=0.01, max_digits=4, decimal_places=1, label="f-stop number")

    def __init__(self, missing_data, *args, **kwargs):
        super(ExifForm, self).__init__(*args, **kwargs)

def show_issues(request, id_):
    directory = os.path.join(upload_directory, id_)
    if not os.path.exists(os.path.join(directory, "originator.json")):
        raise django.http.Http404
    try:
        error, missing_data = json.load(open(os.path.join(directory, "result.json")))
    except IOError:
        return render(request, "calibration/pending.html")
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
        exif_forms = [ExifForm(data, request.POST) for data in missing_data]
        pass
    else:
        exif_forms = [ExifForm(data) for data in missing_data]
    return render(request, "calibration/missing_exif.html", {"images": sorted(zip(filepaths, thumbnails, exif_forms))})

def thumbnail(request, id_, hash_):
    filepath = os.path.join("/var/cache/apache2/calibrate", id_, hash_ + ".jpeg")
    filepath = os.path.join("/tmp/calibrate", id_, hash_ + ".jpeg")
    if not os.path.exists(filepath):
        raise django.http.Http404(filepath)
    response = django.http.HttpResponse()
#    response.write(open(filepath, "rb").read())
    response["X-Sendfile"] = filepath
    response["Content-Type"] = mimetypes.guess_type(filepath)[0] or "application/octet-stream"
    response["Content-Length"] = os.path.getsize(filepath)
    return response
