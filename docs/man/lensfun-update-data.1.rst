======================
lensfun-update-data
======================

----------------------------
update Lensfun's database
----------------------------

:Author: This manual page was written by Torsten Bronger <bronger@physik.rwth-aachen.de>
:Date:   2015-03-11
:Manual section: 1

SYNOPSIS
============

``lensfun-update-data``

DESCRIPTION
===============

Lensfun is a library that corrects flaws in photographic images introduced by
the lens.  It also contains a comprehensive database of cameras and lenses
together with their characteristics to be able to apply automatic corrections
of images taken with these devices.  Lensfun is used by darktable, digiKam,
rawstudio, GimpLensfun, UFRaw, and others.

``lensfun-update-data`` is a command-line program that updates Lensfun's
database.  It looks whether a new version is available online, and if this is
the case, it fetches the latest version and installs it locally.  If called as
root, the database is installed system-wide, otherwise, it is installed in the
user's directory.

FILES
======

``lensfun-update-data`` will place the fetched database in
``/var/lib/lensfun-updates/`` (if called as root) or in
``~/.local/share/lensfun/updates/`` (otherwise).  If necessary, it will create
this directory.  If there is already a database, it is replaced fully.  If
Lensfun detects a database in one of these directories, it will use that
instead of the default location below ``/usr/...``.

DIAGNOSTICS
===============

``lensfun-update-data`` prints log messages to stdout.

Exit status:

===========  =====================================
    0         if OK (updates were installed),
    1         if no newer version could be found,
    3         if no DB location responded validly.
===========  =====================================

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

g-lensfun-update-data(1), lensfun-add-adapter(1)
