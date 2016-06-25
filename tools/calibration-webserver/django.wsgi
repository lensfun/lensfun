# -*- mode: python -*-

import os
import sys

sys.path.append("/home/bronger/src/calibration")
sys.stdout = sys.stderr

os.environ["DJANGO_SETTINGS_MODULE"] = "settings"
#os.environ["MPLCONFIGDIR"] = "/var/www/.matplotlib"

from django.core.wsgi import get_wsgi_application
application = get_wsgi_application()
