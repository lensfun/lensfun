=====================
lensfun-convert-lcp
=====================

-----------------------------------
convert LCP files to Lensfun files
-----------------------------------

:Author: This manual page was written by Torsten Bronger <bronger@physik.rwth-aachen.de>
:Date:   2015-05-11
:Manual section: 1

SYNOPSIS
============

| ``lensfun-convert-lcp`` [``--output`` `OUTPUT`] [``--db-path`` `DB_PATH`]
|                         [``--prefer-lcp``] [path]

DESCRIPTION
===============

Lensfun is a library that corrects flaws in photographic images introduced by
the lens.  It also contains a comprehensive database of cameras and lenses
together with their characteristics to be able to apply automatic corrections
of images taken with these devices.  Lensfun is used by darktable, digiKam,
rawstudio, GimpLensfun, UFRaw, and others.

``lensfun-convert-lcp`` is a command-line program that converts LCP files into
Lensfun's XML format.  LCP files contain distortion, TCA, and vignetting
correction data for lenses, just like Lensfun's XML files do.  The LCP file
format is defined by Adobe, and such files are created by e.g Adobe's “Lens
Profile creator”.  ``lensfun-convert-lcp`` reads all LCP files in a given
directory (and in all of its subdirectories) and converts them to one single
Lensfun XML file.  It assumes that one LCP file contains exactly one lens.

By default, it takes existing Lensfun data for each LCP lens from Lensfun's own
DB, and uses the LCP data only for filling gaps.

OPTIONS
=========

``--output`` `OUTPUT`
    The path of output file.  This file is overwritten silently.  It defaults
    to ``~/.local/share/lensfun/_lcps.xml``.

``--db-path`` `DB_PATH`
    The path to the lensfun database.  If given, ``lensfun-convert-lcp`` looks
    only there (not even in subdirectories) for files with the extension
    “.xml”.  If not given, the DB directory discovery works as explained under
    the section “FILES” below.  XML files starting with an underscore are
    ignored.  In particular, this prevents the output file from a previous run
    from being read (if the default file name was used, at least).

``--prefer-lcp``
    If given, prefer LCP data over Lensfun data.  Normally, LCP data is only
    used to fill gaps in Lensfun's data.  This way, Lensfun's data is only used
    for camera and lens metadata, while Lensfun's actual correction data is
    overridden and supplemented by the LCP data.

``path``
    The path to the root directory with the LCP files.  It defaults to “.”.
    The LCP files are recognised by their content, not by their file extension.

FILES
======

It writes by default into ``~/.local/share/lensfun/_lcps.xml``.  Note that it
will silently overwrite this file if it was already existing.  The Lensfun
database is by default searched in these directories (without subdirectories):
``/usr/share/lensfun``, ``/usr/share/local/lensfun``,
``/var/lib/lensfun-updates``, ``~/.local/share/lensfun/updates``, and
``~/.local/share/lensfun`` (with later directories overriding earlier ones).
Note that this database discovery is slightly different from the Lensfun
library's one.

DIAGNOSTICS
===============

``lensfun-convert-lcp`` may print warnings to stdout.  Its exit code is
always 0.

REPORTING BUGS
====================

Report bugs at <https://github.com/lensfun/lensfun/issues>.

COPYRIGHT
=============

Lensfun is Copyright © 2007 Andrew Zabolotny

License of the code: GNU Lesser General Public License, version 3

License of the database: Creative Commons Attribution-Share Alike 3.0 license

SEE ALSO
============

lensfun-update-data(1), g-lensfun-update-data(1), lensfun-add-adapter(1)
