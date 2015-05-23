======================
lensfun-add-adapter
======================

--------------------------------------------------------
add lens mount compatibilities to Lensfun's database
--------------------------------------------------------

:Author: This manual page was written by Torsten Bronger <bronger@physik.rwth-aachen.de>
:Date:   2013-12-23
:Manual section: 1

SYNOPSIS
============

| ``lensfun-add-adapter``
| ``lensfun-add-adapter`` [``--maker`` `MAKER`] [``--model`` `MODEL`] [``--mount`` `MOUNT`]
| ``lensfun-add-adapter`` ``--remove-local-mount-config``

DESCRIPTION
===============

Lensfun is a library that corrects flaws in photographic images introduced by
the lens.  It also contains a comprehensive database of cameras and lenses
together with their characteristics to be able to apply automatic corrections
of images taken with these devices.  Lensfun is used by darktable, digiKam,
rawstudio, GimpLensfun, UFRaw, and others.

Lensfun contains information about lens mounts of cameras and available mounts
for lens models.  This way, the calling application can offer the user a list
with lenses that fit on their camera in order to pick one.  Otherwise, you
would see *all* lenses every time.  This can be pretty overwhelming.

But if you bought an adapter for another mount system, you would want to see
lenses of that mount system listed for your camera.  So, you must tell Lensfun
somehow that you own the adapter.  You can do so by calling
``lensfun-add-adapter``.

The easiest way to use it is to call it with no parameters.
``lensfun-add-adapter`` will then enter an interactive mode.  First you have to
enter your camera model.  ``lensfun-add-adapter`` will use that to determine
the destination mount.  In particular, the extended lens lists will show up for
every camera with that mount, not just the one you entered.  Then, you have to
select the mount of the lenses you can now put on your camera, using the
adapter.  That's it.

With the options ``--maker``, ``--model``, and ``--mount``, you can avoid
interactive mode by passing the nevcessary data explicitly.  If you pass only
maker/model or only mount, ``lensfun-add-adapter`` will enter interactive mode
only for the missing data.

You can also use ``lensfun-add-adapter`` to reset your adapter configuration.
Call it with the ``--help`` paremeter to see all functions.

OPTIONS
=========

``--maker`` `MAKER` ``--model`` `MODEL`
    The exact maker and model of the camera the mount of which should be made
    compatible.  You must provide these options only in combination rather than
    only one of them.

``--mount`` `MOUNT`
    The exact name of the mount of the lenses that should be made compatible.

``--remove-local-mount-config``
    Resets the local mount configuration, i.e. the additional mount
    compatibilities that were created by this program.  Effectively, it removes
    the file ``_mount.xml`` from the local Lensfun database of the current
    user.

FILES
=====

``~/.local/share/lensfun/_mounts.xml``
    File with all mount compatibilities that were added by this program.

DIAGNOSTICS
===============

Exit status:

===========  =====================================
    0         successful
    1         invalid command line arguments
    2         invalid input in interactive mode
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

lensfun-update-data(1)
