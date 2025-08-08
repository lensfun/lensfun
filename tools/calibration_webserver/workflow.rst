=======================
Calibration workflow
=======================


Setup
=======


GitHub
------

If you want to help with calibrations, contact `Torsten Bronger`_.  Then, you
get access to Lensfun's GitHub repository.  You need to have an account on
GitHub for this.  Then, watch the `Lensfun repository`_.  New calibration
requests bear the label “calibration request”.

.. _Torsten Bronger: mailto:bronger@physik.rwth-aachen.de
.. _Lensfun repository: https://github.com/lensfun/lensfun

Note that the official Lensfun Git repository still resides on Sourceforge.
The GitHub clone is only used for an easier calibration workflow.  (And to make
my GitHub profile look better.)


Nextcloud
--------

We share the calibration images themselves via Nextcloud.  Any Nextcloud client
should work.  Our server is at ``https://bob.ipv.kfa-juelich.de/nextcloud``.
You get credentials from Torsten.  We cannot make it totally public because it
contains email addresses.


Workflow
===========

1. Assign an unassigned calibration request issue to you.
2. The issue title contains a hash.  The respective directory in Nextcloud
   starts with the same hash.
3. `Work on the calibration`_, probably changing the contents of the
   directory.
4. If you have questions to the uploader, contact them by email or in the
   GitHub issue.  If the uploader prefers GitHub, they will add a comment like
   “I am the uploader” to the issue.
5. If you are done, you may add a comment starting with “@uploader:”.  Please
   don't use this just to say thank you because the automatic email sent will
   include thanks anyway.  Instead, use this feature for comments like “The
   images were good, but the target was not really trustworthy” or “Images of
   ducks on a lake cannot be used for calibration”.  Note that it is not
   necessary to use this feature at all.
6. If the images are unusable for calibration, add the label “unsuccessful” to
   the issue.  You needn’t close it.
7. If you need feedback, file a pull request with the calibration results.
   Else, commit them to the ``master`` branch and mark the issue with the label
   “successful”.
8. A `cronjob`_ on Torsten's computer will remove this label once per hour,
   close the issue, and send an email to the uploader, possibly with the latest
   “@uploader” comment.

.. _Work on the calibration:
   https://github.com/lensfun/lensfun/blob/master/tools/calibration_webserver/calibration.rst
.. _cronjob:
   https://github.com/lensfun/lensfun/blob/master/tools/update_database/follow_db_changes.py
