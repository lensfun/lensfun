#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import unicode_literals, division, absolute_import

import numpy
from scipy.optimize.minpack import leastsq
from matplotlib import pyplot

ND = 1
f = 210
d = 30

k1_vig = -0.93
k2_vig = 0.63
k3_vig = -0.34


def error_function(p, x, y):
    k1, k2, k3 = p
    fitted_values = 1 + k1 * x**2 + k2 * x**4 + k3 * x**6
    difference = y - fitted_values
    result = []
    for value, fitted_y in zip(difference, fitted_values):
        if fitted_y < 0.01:
            result.append(10 * value)
        if value < 0:
            result.append(value)
        else:
            result.append(3.3 * value)
    return numpy.array(result)

def get_nd_parameters(k1, k2, k3, nd, focal_length, sensor_diagonal):
    x = numpy.arange(0, 1, 0.001)
    y_vig = 1 + k1 * x**2 + k2 * x**4 + k3 * x**6
    y_filter = 10**(nd * (1 - numpy.sqrt(1 + x**2 / (2 * focal_length / sensor_diagonal)**2)))
    y_total = y_vig * y_filter
    return leastsq(error_function, [k1, k2, k3], args=(x, y_total))[0]

k1, k2, k3 = get_nd_parameters(k1_vig, k2_vig, k3_vig, ND, f, d)

#pyplot.plot(x, y_vig)
#pyplot.plot(x, y_filter)
pyplot.plot(x, y_total)
pyplot.plot(x, y_vig_new(x, k1, k2, k3))
pyplot.xlim(0, 1)
pyplot.grid(True)
pyplot.show()
