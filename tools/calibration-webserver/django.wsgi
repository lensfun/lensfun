#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys

sys.path.append(os.path.dirname(__file__))

os.environ["DJANGO_SETTINGS_MODULE"] = "settings"

from django.core.wsgi import get_wsgi_application
application = get_wsgi_application()
