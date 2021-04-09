# Django settings for calibration project.

# Build paths inside the project like this: os.path.join(BASE_DIR, ...)
import os, configparser
BASE_DIR = os.path.dirname(os.path.dirname(__file__))

DEBUG = False
TEMPLATE_DEBUG = DEBUG

config = configparser.ConfigParser()
assert config.read(os.path.expanduser("~/calibration_webserver.ini"))

ADMINS = (
    (config["General"]["admin_name"], config["General"]["admin_email"]),
)

EMAIL_HOST = config["SMTP"]["machine"]
EMAIL_PORT = config["SMTP"]["port"]
EMAIL_HOST_USER = config["SMTP"].get("login")
EMAIL_HOST_PASSWORD = config["SMTP"].get("password")
EMAIL_USE_TLS = config["SMTP"].get("TLS", "off").lower() in {"on", "true", "yes"}
DEFAULT_FROM_EMAIL = config["General"]["admin_email"]

#TEST_RUNNER = "django.test.runner.DiscoverRunner"

# Hosts/domain names that are valid for this site; required if DEBUG is False
# See https://docs.djangoproject.com/en//ref/settings/#allowed-hosts
ALLOWED_HOSTS = ["wilson", "wilson.bronger.org", "0.0.0.0"]

# Language code for this installation. All choices can be found here:
# http://www.i18nguy.com/unicode/language-identifiers.html
LANGUAGE_CODE = "en-us"

# If you set this to False, Django will make some optimizations so as not
# to load the internationalization machinery.
USE_I18N = True

# If you set this to False, Django will not format dates, numbers and
# calendars according to the current locale.
USE_L10N = True

# Absolute filesystem path to the directory that will hold user-uploaded files.
# Example: "/home/media/media.lawrence.com/media/"
MEDIA_ROOT = ""

FILE_UPLOAD_TEMP_DIR = config["General"]["upload_temp_path"]

# URL that handles the media served from MEDIA_ROOT. Make sure to use a
# trailing slash.
# Examples: "http://media.lawrence.com/media/", "http://example.com/media/"
MEDIA_URL = ""

# Absolute path to the directory static files should be collected to.
# don't put anything in this directory yourself; store your static files
# in apps" "static/" subdirectories and in STATICFILES_DIRS.
# Example: "/home/media/media.lawrence.com/static/"
STATIC_ROOT = os.path.expanduser("~/calibration")

# URL prefix for static files.
# Example: "http://media.lawrence.com/static/"
STATIC_URL = "/calibration/static/"

# Additional locations of static files
STATICFILES_DIRS = (
    # Put strings here, like "/home/html/static" or "C:/www/django/static".
    # Always use forward slashes, even on Windows.
    # don't forget to use absolute paths, not relative paths.
)

# Make this unique, and don't share it with anybody.
SECRET_KEY = "yyq3%@)4tg6@a86m8s=dopd)i+nu^-ma@p=lzk^su18h@*+hb3"

MIDDLEWARE_CLASSES = (
    "django.middleware.common.CommonMiddleware",
    "django.contrib.sessions.middleware.SessionMiddleware",
    "django.contrib.auth.middleware.AuthenticationMiddleware",
    "django.contrib.messages.middleware.MessageMiddleware",
    # Uncomment the next line for simple clickjacking protection:
    # "django.middleware.clickjacking.XFrameOptionsMiddleware",
)

ROOT_URLCONF = "calibration.urls"

# Python dotted path to the WSGI application used by Django's runserver.
#WSGI_APPLICATION = "calibration.wsgi.application"

TEMPLATES = [
    {
        "BACKEND": "django.template.backends.django.DjangoTemplates",
        "APP_DIRS": True,
        "OPTIONS": {
            "context_processors": ["django.template.context_processors.debug",
                                   "django.template.context_processors.request",
                                   "django.contrib.auth.context_processors.auth",
                                   "django.contrib.messages.context_processors.messages",
                                   "calibration.context_processors.default"]
            }
    }
]

INSTALLED_APPS = (
    "django.contrib.auth",
    "django.contrib.contenttypes",
    "django.contrib.sessions",
    "django.contrib.sites",
    "django.contrib.messages",
    "django.contrib.staticfiles",
    "calibration"
)

# A sample logging configuration. The only tangible logging
# performed by this configuration is to send an email to
# the site admins on every HTTP 500 error when DEBUG=False.
# See http://docs.djangoproject.com/en/dev/topics/logging for
# more details on how to customize your logging configuration.
LOGGING = {
    "version": 1,
    "disable_existing_loggers": False,
    "filters": {
        "require_debug_false": {
            "()": "django.utils.log.RequireDebugFalse"
        }
    },
    "handlers": {
        "mail_admins": {
            "level": "ERROR",
            "filters": ["require_debug_false"],
            "class": "django.utils.log.AdminEmailHandler"
        }
    },
    "loggers": {
        "django.request": {
            "handlers": ["mail_admins"],
            "level": "ERROR",
            "propagate": True,
        },
    }
}
