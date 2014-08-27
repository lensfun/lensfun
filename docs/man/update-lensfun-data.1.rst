======================
update-lensfun-data
======================

----------------------------
update lensfun's database
----------------------------

:Author: This manual page was written by Torsten Bronger <bronger@physik.rwth-aachen.de>
:Date:   2013-12-23
:Manual section: 1

SYNOPSIS
============

``update-lensfun-data``

DESCRIPTION
===============

Lensfun is a library that corrects flaws in photographic images introduced by
the lens.  It also contains a comprehensive database of cameras and lenses
together with their characteristics to be able to apply automatic corrections
of images taken with these devices.  Lensfun is used by darktable, digiKam,
rawstudio, GimpLensfun, UFRaw, and others.

``update-lensfun-data`` is a command-line program that updates lensfun's
database.  It looks whether a new version is available online, and if this is
the case, it fetches the latest version and installs it locally.  For the
latter, the program needs root permissions.  On many system, you get these by
saying::

    $ sudo update-lensfun-data

and enter the root password.


FILES
======

``update-lensfun-data`` will place the fetched database in
``/var/lib/lensfun/``.  If necessary, it will create this directory.  If there
is already a database, it is replaced fully.  If lensfun detects a database in
this directory, it will use that instead of the default location below
``/usr/...``.

DIAGNOSTICS
===============

``update-lensfun-data`` prints log messages to stdout.

Exit status:

===========  =====================================
    0         if OK,
    1         if no newer version could be found,
    2         if root permissions were missing.
===========  =====================================

Note that you can use these exit codes for detecting whether a new version of
the database is available without actually fetching that database.  You simply
call ``update-lensfun-data`` without root permissions.  If the exit code is
`2`, there is a newer version available.  If it is `1`, there is none.

REPORTING BUGS
====================

Report bugs at <http://sourceforge.net/p/lensfun/bugs/>.

COPYRIGHT
=============

Lensfun is Copyright © 2007 Andrew Zabolotny

License of the code: GNU Lesser General Public License, version 3

License of the database: Creative Commons Attribution-Share Alike 3.0 license

SEE ALSO
============

g-update-lensfun-data(1), lensfun-add-adapter(1)
