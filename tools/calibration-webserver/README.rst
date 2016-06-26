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


General
-------

* Some important prerequisites are Python 3, Django 1.9, exiv2, Exiftool,
  unrar, 7zip, unzip, ImageMagick, Apache, and Python WSGI.
* The directory ``/var/cache/apache2/calibrate`` must be writable to the user
  ``www-user``.
* The first line of the file ``/var/www/.authinfo`` takes the SMTP credentials
  used for sending mails.  Its content may be::

    machine mail.example.com login me243242 port 587 password reallysecret


In ``settings.py``
------------------

This is an ordinary `Django settings`_ file.  Settings you should change are
``ADMINS``, ``ALLOWED_HOSTS``, and ``SECRET_KEY``.  (The ``SECRET_KEY`` is not
used by this appication currently, but this may change in a later version, so
set the ``SECRET_KEY`` to a new value kanyway.)

.. _Django settings: https://docs.djangoproject.com/en/1.9/ref/settings/


In ``process_upload.py``
---------------------------

Since this script runs as a stand-alone program rather than a module in a
Django-based process, it cannot easily access the Django settings.  (I don't
claim that this would be impossible, though.)  Thus, you have to adapt it by
changing its source code:

* In ``send_email``, you have to change the value of the variable “``me``”.
* In ``send_error_email``, you have to change the signature in the email text
  as well as the URL.
* In ``send_success_email``, you have to change the recipient email address.


In ``calibration/views.py``
---------------------------

* In ``send_success_email``, you have to change the recipient email address.


Templates
---------

In the templates ``error.html``, ``pending.html``, ``success.html``, and
``upload.html``, you have to replace Bronger's email address with your own.


Usage with Apache
=====================

In a ``<VirtualHost>`` section, you should insert something like::

    WSGIScriptAlias /calibration /path/to/calibration-webserver/django.wsgi
    WSGIDaemonProcess calibration lang='en_US.UTF-8' locale='en_US.UTF-8' display-name=%{GROUP}
    WSGIProcessGroup calibration

..  LocalWords:  www login WSGIScriptAlias WSGIDaemonProcess lang UTF
..  LocalWords:  WSGIProcessGroup
