=======================
Calibration webserver
=======================

This `Django`_ application enables you to collect lens calibration images in a
well-organised manner.  It is only partially customisable with settings.
(Something that could certainly be improved.)  Much of the local adaption has
to be realised by changing the source code itself.

.. _Django: https://www.djangoproject.com


Configuration
===============

* Some important prerequisites are Python 3, Django 1.9, exiv2, Exiftool,
  unrar, 7zip, unzip, PyGithub, ImageMagick, Apache, and Python WSGI.
* Adjust the settings in ``settings.py``.  This is an ordinary `Django
  settings`_ file.  Settings you should change are ``ALLOWED_HOSTS`` and
  ``SECRET_KEY``.  (The ``SECRET_KEY`` is not used by this appication
  currently, but this may change in a later version, so set the ``SECRET_KEY``
  to a new value kanyway.)
* Adjust the values in ``calibration_webserver.ini``.  This file must be moved
  to the home directory of the user under which the webserver process runs
  (e.g. ``/var/www``).
* The directory ``cache_root`` must be writable to the webserver user.
* The script ``follow_db_changes.py`` should run as a cronjob, e.g. once
  per hour.

.. _Django settings: https://docs.djangoproject.com/en/1.9/ref/settings/


Usage with Apache
=====================

In a ``<VirtualHost>`` section, you should insert something like::

    WSGIScriptAlias /calibration /path/to/calibration_webserver/django.wsgi
    WSGIDaemonProcess calibration lang='en_US.UTF-8' locale='en_US.UTF-8' display-name=%{GROUP}
    WSGIProcessGroup calibration
    Alias /calibration/static /var/www/calibration

..  LocalWords:  www login WSGIScriptAlias WSGIDaemonProcess lang UTF
..  LocalWords:  WSGIProcessGroup
