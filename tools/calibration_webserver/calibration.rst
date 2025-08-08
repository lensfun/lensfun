.. -*- mode: rst; coding: utf-8; ispell-local-dictionary: "british" -*-

=======================
Calibration HOWTO
=======================

.. contents::

This document describes succinctly how to process uploaded images into Lensfun
calibration data.  It contains the necessary steps as well as my own
experiences with hundreds of calibrations of uploads.

If you like to help with calibrations, you are more than welcome.  Please
contact `Torsten Bronger`_ for getting access to everything.  See also the
terse `workflow description`_.

.. _Torsten Bronger: mailto:bronger@physik.rwth-aachen.de
.. _workflow description:
   https://github.com/lensfun/lensfun/blob/master/tools/calibration_webserver/workflow.rst


The setting
============

While all code is in the Lensfun repository, the calibration webserver only
runs on ``http://wilson.bronger.org``.  There, people can upload files, and the
uploads are pre-processed.  At the same time, a cronjob on this machine does
some work two minutes after the hour, every hour.  More on that later.

You have to set up an Nextcloud client.  Moreover, you must have Hugin
installed.  Darktable or another Lensfun-using program is needed for
calibration verification.  Finally, Lensfun >= 0.3 is necessary.  Contact
`Torsten`_ if you have problems installing everything.  Put the Lensfun
repository on GitHub on your watch list.

.. _Torsten: mailto:bronger@physik.rwth-aachen.de


Analysing incoming data
===========================

So, somebody uploaded images.  They appear in a new directory in the Nextcloud
share.  The directory name is something like “215f9b_miller”.  It starts with a
hash and ends with the email name of the uploader (everything before the “@”).
The cronjob on wilson.bronger.org creates a GitHub issue with the hash in the
title.

If you want to work on the data, self-assign the issue first.  Whenever you
have questions along the way, put them in a comment to that issue and ping
Torsten by including “@bronger” in the comment.

Then, have a look at the RAW images.  In some cases, they are unusable for
calibration.  I've seen people uploading images of wildlife animals, beautiful
landscapes, and even their girlfriend.  In such a case, add a comment like::

    @uploader: I could not use your images because they don't contain straight
    lines.  See http://wilson.bronger.org/calibration for detailed
    instructions.

Sometimes, we have already the data for the lens(es).  Then, comment like::

    @uploader: Lensfun's database already contains the data for the lens you
    submitted.  Please answer to this email if you have trouble with it.

In both cases, add the “unsuccessful” label to the issue.  Done.


Communication
=================

If you need to contact the uploader, do so by email by default.  If the
uploader prefers to take part in the GitHub issue instead, they will add a
comment like “I am the uploader” to the issue (they are told so by email).

For general questions, feel free to contact Torsten Bronger in an issue with
“@bronger”.


Select and sort images
=========================

Copy the upload directory out of Nextcloud to a local place.  This way, you can
work in the directory without triggering the Nextcloud client.

I recommend to create up to four top-level directories: “distortion”, “tca”,
“vignetting”, and “not_used”.  Then, move the images into these directories.
Mostly, the original directory structure is already quite close to this.
However, often uploaders made way too many images.  So, we have to make a tight
selection.


Distortion
----------

Number of necessary images:

:prime: 1
:zoom:  5
:super zoom/bridge camera: 7
:tele zoom: 4

These numbers are only guidelines.  It is all about the changes from focal
length to focal length: The larger the change, the closer the focal lengths
should be together.  In other words, the change from image to image should be
about the same.

For example, a typical sequence for a 18-55mm is: 18, 19, 21, 35, 55.  Being
*very* close at the wide-angle end is very common.  Of course, both extreme
ends of a zoom must be included.

A frequent problem is that the zoom switches from barrel to pincushion but the
uploader provided no image close to the turning point (instead, only images
which are *strongly* barrel or pincushion).  Then, you should request for
another image.

Now about the most critical thing: **Straight lines**.  We need two horizontal
lines, assuming the image has landscape orientation.  One as close to the
top/bottom image border as possible, and one ⅓ on the way from the centre to
the top/bottom border.  They must run from the left to the right border.  See
some `example images`_.  You may refer to the example images if the uploader
really failed to provide the lines.

.. _example images: http://wilson.bronger.org/calibration/target_tips

The target must be trustworthy.  A 300-years-old brick wall is not.  A pasture
fence is not.  The kerb of a pavement is not.  The horizon line is not.  In
contrast, a modern building is perfect.  Note that some targets are suitable
for shorter focal lengths but unsuitable for tele lens images because then, you
can see the imperfections in the lines clearly.

The lines should be uninterrupted but don't be too strict here.


TCA
---

I most cases, the very same image set selected for distortion can also be used
for TCA.  Then, just copy the files in the ``tca`` folder.  TCA images must be
as sharp as possible, also in the corners.  There should be black/white edges
but anything that is not strongly saturated (red, green, blue) is fine.
Straight lines are not necessary.


Vignetting
----------

Normally, you have the same focal lengths for vignetting as for distortion
(which is not a strict rule).  I recommend five images per focal length:
Minimal f-stop, plus one stop, plus one stop, plus one stop, and maximal
f-stop.  But four images are also sufficient.

You can really evaluate vignetting images only *after* the calibration.


Create the TIFFs and lenses.txt
=================================

Call `calibrate.py`_ from the root directory of the upload.  It creates a TIFF
for every RAW because Hugin can't read RAWs.  Moreover, it creates a file
``lenses.txt`` that you should open in a text editor.

.. _calibrate.py:
   https://github.com/lensfun/lensfun/blob/master/tools/calibrate/calibrate.py


Measure distortion with Hugin
================================

My `screencast`_ explains how to use Hugin to get distortion parameters.  Add
them to ``lenses.txt`` for each focal length.  Unfortunately, the screencast
uses an older version of Hugin.  Newer version have a quite different GUI but
the workflow is the same.  The most prominent difference is that the settings
necessary in the “Optimizer” tab are now partly in the “Images” tab.

.. _screencast: https://vimeo.com/51999287


Hugin patch
-----------

Unfortunately for calibration, newer Hugin versions draw lines between control
points, which cannot be switched off.  If you cannot live with that (I can't),
you may use a 2014 version of Hugin.  Alternatively, you can modify Hugin's
source code and compile Hugin yourself: In
``hugin/src/hugin1/hugin/CPImageCtrl.cpp``, at ``DisplayedControlPoint::Draw``
(line 246 in Hugin 2018.0.0), you have to comment out the following line:

.. code-block:: c++

    if(m_line)
    {
    //    DrawLine(dc);    <----
        DrawCross(dc, p, l);
        DrawCross(dc, p2, l);
    }


a, b, c or only b-only?
------------------

This is something not covered in the screencast but it is important because it
can save you much time.  You can select all three distortion parameters a, b, c
for the optimisation, or only b.  If you choose b-only, *one line is enough*.
This means half the effort for you.  I adopted the following workflow:

1. If it is a non-shitty prime, b-only.
2. For a zoom, I start at the smallest focal length with a, b, c.
3. I keep a “Preview panorama” window open (not “Fast preview panorama”), and
   click on “Fit”.  This works only after I selected “Rectangular” projection
   in the “Stitcher” tab.
4. For every focal length, I first mark only the outer line.  Then, I check the
   optimisation with only b, and then, with a, b, c.  If the difference is
   smaller than a factor 1.5, and overall smaller than 3, I use the b-only
   result.  If not, I mark the second line and use a, b, c.
5. Once I have switched to b-only, I stay with it for larger focal length,
   (unless the correction is obviously ugly).

In ``lenses.txt``, you can add a line ``0, 0.1234, 0`` for b-only focal
lengths.  If all entries are b-only, insert only b and leave out the zeros
altogether.


Create Lensfun data
======================

In ``lenses.txt``, replace the ``<tags>`` with correct content.  Then, call
``calibrate.py`` again.  It produces a file called ``lensfun.xml``.

Extract the lens information from it (i.e. everything except the first and last
line), and in your own Lensfun branch (in your own account or the Lensfun
project doesn't matter, just pushed somewhere on GitHub), insert it into the
proper XML file in Lensfun's database.  Prepend an XML comment of the form

::

     <!-- Taken with Canon 6D -->

to the data.

Often, you may want to fine-tune the lens model name.  Lensfun normalises names
before any matching, so you have some freedom.  For example, upper/lowercase
can be changed arbitrarily.  Any single “f” is ignored, so you may change
``10-18mm 2.8`` into ``10-18mm f/2.8``.  If there was a tele converter
involved, you must add “converter” into the name so that Lensfun does not try
to derive allowed focal lengths from the lens name.  Ordering of parts in the
lens name is completely unimportant for matching.  As are single punctuation
characters.  You may even add things (e.g. ``10-18`` into ``10-18mm``) but be
conservative here.  *Never* drop something from what exiv2 says (except for
punctuation); this would thwart matching.

In case of compact cameras, you also have to create an entry for the camera.
Copy the latest existing ``<camera>`` entry in the file and edit it.

If TCA is weak, or the TCA images were not so great, call ``calibrate.py`` with
the option ``--simple-tca``.


Check the results
===================

Copy ``lensfun.xml`` into a directory where Lensfun can find it,
e.g. ``~/.local/share/lensfun``, and start e.g. Darktable to check your
results.  The lines should be straightened, and TCA and vignetting gone.  In
particular for vignetting, there must not be an overcorrection at the rim.


Finally
========

Replace the upload directory on Nextcloud with your new version.  Take care to
remove all TIFFs first because they are huge.  I use the following clean-up
script::

    #!/bin/sh
    find . -name "*.tiff" -exec rm {} \;
    find . -name "*.tca" -exec rm {} \;
    find . -name "*.gp" -exec rm {} \;
    find . -name "*.dat" -exec rm {} \;
    find . -name "*.xmp" -exec rm {} \;

Commit your results to the Lensfun's master branch on GitHub.  Or, if you want
feedback first, create a pull request.
