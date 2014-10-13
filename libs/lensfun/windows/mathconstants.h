/*
    Math constants definitions for non-POSIX or old compilers.
    Should be included if necessary after <math.h>.
    Copyright (C) 2014 by Torsten Bronger
*/

#ifndef __MATHCONSTANTS_H__
#define __MATHCONSTANTS_H__

#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#ifndef NAN
#define NAN std::numeric_limits<float>::quiet_NaN()
#endif

#ifndef INF
#define INF std::numeric_limits<float>::infinity()
#endif

#endif /* __MATHCONSTANTS_H__ */
