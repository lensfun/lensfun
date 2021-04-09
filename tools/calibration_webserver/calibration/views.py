import hashlib, os, subprocess, json, shutil, mimetypes, re, configparser
from urllib.parse import quote_plus
import django.forms as forms
from django.shortcuts import render
from django.forms.utils import ValidationError
from django.urls import reverse
import django.http
from django.utils.encoding import iri_to_uri
from utils import generate_thumbnail, RawNotFound


config = configparser.ConfigParser()
config.read(os.path.expanduser("~/calibration_webserver.ini"))

upload_directory = config["General"]["uploads_root"]
cache_root = config["General"]["cache_root"]
admin = "{} <{}>".format(config["General"]["admin_name"], config["General"]["admin_email"])
allowed_extensions = (".tar.gz", ".tgz", ".tar.bz2", ".tbz2", ".bz2", ".tar.xz", ".txz", ".tar", ".rar", ".7z", ".zip")
file_extension_pattern = re.compile("(" + "|".join(allowed_extensions).replace(".", "\\.") + ")$", re.IGNORECASE)
webserver_parent_path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
try:
    python_path = os.environ["PYTHONPATH"] + ":" + webserver_parent_path
except KeyError:
    python_path = webserver_parent_path


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


def spawn_daemon(path_to_executable, *args, env=None):
    """Spawns a completely detached subprocess (i.e., a daemon).  Taken from
    http://stackoverflow.com/questions/972362/spawning-process-from-python
    which in turn was inspired by
    <http://code.activestate.com/recipes/278731/>.

    :Parameters:
      - `path_to_executable`: absolute path to the executable to be run
        detached
      - `args`: all arguments to be passed to the subprocess
      - `env`: if not None, the environment variables passed to the process;
        they are added to the ones of the current process, overriding in case
        of collisions

    :type path_to_executable: str
    :type args: list of str
    :type env: dict mapping str to str, or NoneType
    """
    env_ = os.environ.copy()
    if env is not None:
        env_.update(env)
    os.posix_spawn(path_to_executable, [path_to_executable] + list(args), env_)


def store_upload(uploaded_file, email_address, comments):
    hash_ = hashlib.sha1()
    hash_.update(uploaded_file.name.encode("utf-8"))
    hash_.update(str(uploaded_file.size).encode("utf-8"))
    hash_.update(email_address.encode("utf-8"))
    hash_.update(comments.strip().encode("utf-8"))
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
    if comments:
        with open(os.path.join(directory, "comments.txt"), "w") as outfile:
            outfile.write(comments)
    filepath = os.path.join(directory, uploaded_file.name)
    with open(filepath, "wb") as outfile:
        for chunk in uploaded_file.chunks():
            outfile.write(chunk)
    spawn_daemon("/usr/bin/env", "python3",
                 os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "process_upload.py"),
                 "initial", filepath, env={"PYTHONPATH": python_path})
    return id_


class UploadForm(forms.Form):
    compressed_file = forms.FileField(label="Archive with RAW files", help_text="Must be a .tar.gz or a .zip file")
    email_address = forms.EmailField(label="Your email address")
    comments = forms.CharField(label="optional comments", widget=forms.Textarea, required=False,
                               help_text="This text will be included in a public GitHub issue.")

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
            id_ = store_upload(request.FILES["compressed_file"],
                               upload_form.cleaned_data["email_address"], upload_form.cleaned_data["comments"])
            return HttpResponseSeeOther(reverse("show_issues", kwargs={"id_": id_}))
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


class InvalidData(Exception):
    pass

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
        thumbnails.append("/calibration/thumbnails/{0}/{1}?raw={2}".format(
            id_, hash_.hexdigest(), quote_plus(os.path.relpath(data[0], directory))))
    if request.method == "POST":
        exif_forms = [ExifForm(data, i==0, request.POST, prefix=str(i)) for i, data in enumerate(missing_data)]
        latest_lens_model_name = latest_focal_length = latest_aperture = None
        try:
            if not all([exif_form.is_valid() for exif_form in exif_forms]):
                raise InvalidData
            for data, exif_form in zip(missing_data, exif_forms):
                lens_model_name = exif_form.cleaned_data["lens_model_name"] or latest_lens_model_name
                focal_length = exif_form.cleaned_data["focal_length"] or latest_focal_length
                aperture = exif_form.cleaned_data["aperture"] or latest_aperture
                if not (lens_model_name and focal_length and aperture):
                    raise InvalidData
                latest_lens_model_name = lens_model_name
                latest_focal_length = focal_length
                latest_aperture = aperture
                if focal_length == int(focal_length):
                    focal_length = format(int(focal_length), "03")
                else:
                    focal_length = format(focal_length, "05.1f")
                filepath = data[0]
                filename = os.path.basename(filepath)
                os.rename(filepath, os.path.join(os.path.dirname(filepath), "{}--{}mm--{}_{}".format(
                    lens_model_name.replace("/", "__"), focal_length, aperture, filename)))
            json.dump((None, []), open(os.path.join(directory, "result.json"), "w"), ensure_ascii=True)
            shutil.rmtree(os.path.join(cache_root, id_), ignore_errors=True)
            spawn_daemon("/usr/bin/env", "python3",
                         os.path.join(os.path.abspath(os.path.dirname(__file__)), "..", "process_upload.py"),
                         "amended", directory, env={"PYTHONPATH": python_path})

            return render(request, "calibration/success.html")
        except InvalidData:
            pass
    else:
        exif_forms = [ExifForm(data, i==0, prefix=str(i)) for i, data in enumerate(missing_data)]
    return render(request, "calibration/missing_exif.html", {"images": zip(filepaths, thumbnails, exif_forms)})


def thumbnail(request, id_, hash_):
    raw_filepath = os.path.join(upload_directory, id_, request.GET.get("raw"))
    cache_dir = os.path.join(cache_root, id_)
    filepath = os.path.join(cache_dir, hash_ + ".jpeg")
    if not os.path.exists(filepath):
        try:
            generate_thumbnail(raw_filepath, cache_dir)
        except RawNotFound:
            raise django.http.Http404("{}/{}.jpeg".format(id_, hash_))
    response = django.http.HttpResponse()
    response["X-Sendfile"] = filepath
    response["Content-Type"] = mimetypes.guess_type(filepath)[0] or "application/octet-stream"
    response["Content-Length"] = os.path.getsize(filepath)
    return response
