======================
Calibration statistics
======================


Results
=======

The results of this script with the current database are the following::

    bronger@penny:~/src/lensfun/tools/calibration_statistics$ ./statistics.py --inverse && \
                                                              ./statistics.py --in-focal-length
    [0.06834731424201283, 0.04983392790356207, 0.11328902542292163]
    [0.09085023095526398, 0.058878117602132324, 0.11360064218315158]

This shows:

* Using the focal length as the unit system results in a slightly bigger error.
* The difference is small enough to say that one can use both coordinate systems.
  
The two other combinations of ``--inverse`` and ``-in-focal-length`` are worse, by the way.

If you look at the resulting plot – for example, for the parameter b – you see
that using the focal length for the unit system results in very nicely linear
lines, even better than with ``--inverse``.  Unfortunately, an increased number
of lines don’t follow this trend, resulting in the worse figures above.  I
suspect changes of sign in the original curves result in difficult slopes when
applying the transformation into the focal length unit system.
