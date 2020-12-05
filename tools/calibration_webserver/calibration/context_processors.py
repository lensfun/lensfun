# This file is Python 2.7.

"""Context processor for getting the admin's name and email into the templates.
"""

import os, configparser


config = configparser.ConfigParser()
config.read(os.path.expanduser("~/calibration_webserver.ini"))


def default(request):
    """Injects admin contact information into the template context.

    :param request: the current HTTP Request object

    :type request: HttpRequest

    :return:
      the (additional) context dictionary

    :rtype: dict mapping str to session data
    """
    return {"admin_name": config["General"]["admin_name"], "admin_email": config["General"]["admin_email"]}
