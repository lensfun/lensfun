/*
    Image modifier implementation: perspective correction functions
    Copyright (C) 2015 by Torsten Bronger <bronger@physik.rwth-aachen.de>
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <iostream>
#include "windows/mathconstants.h"

using std::atan;
using std::atan2;
using std::cos;
using std::fabs;
using std::fmod;
using std::pow;
using std::sin;
using std::sqrt;

fvector normalize (float x, float y)
{
    fvector result (2);
    float norm = sqrt (pow (x, 2) + pow(y, 2));
    result [0] = x / norm;
    result [1] = y / norm;
    return result;
}

void central_projection (fvector coordinates, float plane_distance, float &x, float &y)
{
    float stretch_factor = plane_distance / coordinates [2];
    x = coordinates [0] * stretch_factor;
    y = coordinates [1] * stretch_factor;
}

fvector svd (matrix M)
{
    const int n = M [0].size();
    fvector S2 (n);
    int  i, j, k, estimated_column_rank = n, counter = n, iterations = 0,
        max_cycles = (n < 120) ? 30 : n / 4;
    float epsilon = std::numeric_limits<float>::epsilon() * 10,
        e2 = 10 * n * pow (epsilon, 2),
        threshold = 0.1 * epsilon,
        vt, p, x0, y0, q, r, c0, s0, d1, d2;

    M.resize (2 * n, fvector(n));
    for (i = 0; i < n; i++)
        M [n + i][i] = 1;

    while (counter != 0 && iterations++ <= max_cycles)
    {
        counter = estimated_column_rank * (estimated_column_rank - 1) / 2;
        for (j = 0; j < estimated_column_rank - 1; j++)
            for (k = j + 1; k < estimated_column_rank; k++)
            {
                p = q = r = 0;
                for (i = 0; i < n; i++)
                {
                    x0 = M [i][j];
                    y0 = M [i][k];
                    p += x0 * y0;
                    q += pow (x0, 2);
                    r += pow (y0, 2);
                }
                S2 [j] = q;
                S2 [k] = r;
                if (q >= r) {
                    if (q <= e2 * S2 [0] || fabs (p) <= threshold * q)
                        counter--;
                    else
                    {
                        p /= q;
                        r = 1 - r / q;
                        vt = sqrt (4 * pow (p, 2) + pow (r, 2));
                        c0 = sqrt (0.5 * (1 + r / vt));
                        s0 = p / (vt * c0);
                        for (i = 0; i < 2 * n; i++)
                        {
                            d1 = M [i][j];
                            d2 = M [i][k];
                            M [i][j] = d1 * c0 + d2 * s0;
                            M [i][k] = - d1 * s0 + d2 * c0;
                        }
                    }
                }
                else
                {
                    p /= r;
                    q = q / r - 1;
                    vt = sqrt (4 * pow (p, 2) + pow (q, 2));
                    s0 = sqrt (0.5 * (1 - q / vt));
                    if (p < 0)
                        s0 = - s0;
                    c0 = p / (vt * s0);
                    for (i = 0; i < 2 * n; i++)
                    {
                        d1 = M [i][j];
                        d2 = M [i][k];
                        M [i][j] = d1 * c0 + d2 * s0;
                        M [i][k] = - d1 * s0 + d2 * c0;
                    }
                }
            }
        while (estimated_column_rank > 2 &&
               S2 [estimated_column_rank - 1] <=
               S2 [0] * threshold + pow (threshold, 2))
            estimated_column_rank--;
    }
    if (iterations > max_cycles)
        g_warning ("[Lensfun] SVD: Iterations did non converge");

    fvector result;
    for (matrix::iterator it = M.begin() + n; it != M.end(); ++it)
        result.push_back ((*it) [n - 1]);
    return result;
}

void ellipse_analysis (fvector x, fvector y, float f_normalized, float &x_v, float &y_v,
                       float &center_x, float &center_y)
{
    matrix M;
    float a, b, c, d, f, g, _D, x0, y0, phi, _N, _S, _R, a_, b_, radius_vertex;

    // Taken from http://math.stackexchange.com/a/767126/248694
    for (int i = 0; i < 5; i++)
    {
        fvector row (6);
        row [0] = pow (x [i], 2);
        row [1] = x [i] * y [i];
        row [2] = pow (y [i], 2);
        row [3] = x [i];
        row [4] = y [i];
        row [5] = 1;
        M.push_back (row);
    }
    fvector parameters = svd (M);
    /* Taken from http://mathworld.wolfram.com/Ellipse.html, equation (15)
       onwards. */
    a = parameters [0];
    b = parameters [1] / 2;
    c = parameters [2];
    d = parameters [3] / 2;
    f = parameters [4] / 2;
    g = parameters [5];

    _D = pow (b, 2) - a * c;
    x0 = (c * d - b * f) / _D;
    y0 = (a * f - b * d) / _D;

    phi = 1/2 * atan (2 * b / (a - c));
    if (a > c)
        phi += M_PI_2;

    _N = 2 * (a * pow (f, 2) + c * pow (d, 2) + g * pow(b, 2) - 2 * b * d * f - a * c * g) / _D;
    _S = sqrt (pow ((a - c), 2) + 4 * pow (b, 2));
    _R = a + c;
    a_ = sqrt (_N / (_S - _R));
    b_ = sqrt (_N / (- _S - _R));
    // End taken from mathworld
    if (a_ < b_)
    {
        float temp;
        temp = a_;
        a_ = b_;
        b_ = temp;
        phi -= M_PI_2;
    }
    /* Normalize to -π/2..π/2 so that the vertex half-plane is top or bottom
       rather than e.g. left or right. */
    phi = fmod (phi + M_PI_2, M_PI) - M_PI_2;

    /* Negative sign because vertex at top (negative y values) should be
       default. */
    radius_vertex = - f_normalized / sqrt (pow (a_ / b_, 2) - 1);
    if ((x [0] - x0) * (y [1] - y0) < (x [1] - x0) * (y [0] - y0))
        radius_vertex *= -1;

    x_v = radius_vertex * sin(phi);
    y_v = radius_vertex * cos(phi);
    center_x = x0;
    center_y = y0;
}

fvector rotate_rho_delta (float rho, float delta, float x, float y, float z)
{
}

void intersection (fvector x, fvector y, float &x_i, float &y_i)
{
}

float determine_rho_h (float rho, float delta, fvector x_perpendicular_line, fvector y_perpendicular_line, float f_normalized, float center_x, float center_y)
{
}

void calculate_angles(fvector x, fvector y, float f,
                      float normalized_in_millimeters, float &rho, float &delta, float &rho_h,
                      float &f_normalized, float &final_rotation, float &center_of_control_points_x,
                      float &center_of_control_points_y)
{
    const int number_of_control_points = x.size();

    float center_x, center_y;
    if (number_of_control_points == 6)
    {
        center_x = std::accumulate(x.begin(), x.begin() + 4, 0.) / 4;
        center_y = std::accumulate(y.begin(), y.begin() + 4, 0.) / 4;
    }
    else
    {
        center_x = std::accumulate(x.begin(), x.end(), 0.) / number_of_control_points;
        center_y = std::accumulate(y.begin(), y.end(), 0.) / number_of_control_points;
    }

    // FixMe: Is really always an f given (even if it is inaccurate)?
    f_normalized = f / normalized_in_millimeters;
    float x_v, y_v;
    if (number_of_control_points == 5 || number_of_control_points == 7)
        ellipse_analysis (fvector (x.begin(), x.begin() + 5), fvector (y.begin(), y.begin() + 5),
                          f_normalized, x_v, y_v, center_x, center_y);
    else
    {
        intersection (fvector (x.begin(), x.begin() + 4),
                      fvector (y.begin(), y.begin() + 4),
                      x_v, y_v);
        if (number_of_control_points == 8)
        {
            /* The problem is over-determined.  I prefer the fourth line over
               the focal length.  Maybe this is useful in cases where the focal
               length is not known. */
            float x_h, y_h;
            intersection (fvector (x.begin() + 4, x.begin() + 8),
                          fvector (y.begin() + 4, y.begin() + 8),
                          x_h, y_h);
            float radicand = - x_h * x_v - y_h * y_v;
            if (radicand >= 0)
                f_normalized = sqrt (radicand);
        }
    }

    rho = atan (- x_v / f_normalized);
    delta = M_PI_2 - atan (- y_v / sqrt (pow (x_v, 2) + pow (f_normalized, 2)));
    if (rotate_rho_delta (rho, delta, center_x, center_y, f_normalized) [2] < 0)
        // We have to move the vertex into the nadir instead of the zenith.
        delta -= M_PI;

    bool swapped_verticals_and_horizontals = false;

    fvector c(2);
    switch (number_of_control_points) {
    case 4:
    case 6:
    case 8:
    {
        fvector a = normalize (x_v - x [0], y_v - y [0]);
        fvector b = normalize (x_v - x [2], y_v - y [2]);
        c [0] = a [0] + b [0];
        c [1] = a [1] + b [1];
        break;
    }
    case 5:
    {
        c [0] = x_v - center_x;
        c [1] = y_v - center_y;
        break;
    }
    default:
    {
        c [0] = x [5] - x [6];
        c [1] = y [5] - y [6];
    }
    }
    float alpha;
    if (number_of_control_points == 7)
    {
        float x5_, y5_;
        central_projection (rotate_rho_delta (rho, delta, x [5], y [5], f_normalized), f_normalized, x5_, y5_);
        float x6_, y6_;
        central_projection (rotate_rho_delta (rho, delta, x [6], y [6], f_normalized), f_normalized, x6_, y6_);
        alpha = - atan2 (y6_ - y5_, x6_ - x5_);
        if (fabs (c [0]) > fabs (c [1]))
            // Find smallest rotation into horizontal
            alpha = - fmod (alpha - M_PI_2, M_PI) - M_PI_2;
        else
            // Find smallest rotation into vertical
            alpha = - fmod (alpha, M_PI) - M_PI_2;
    }
    else if (fabs (c [0]) > fabs (c [1]))
    {
        swapped_verticals_and_horizontals = true;
        alpha = rho > 0 ? M_PI_2 : - M_PI_2;
    }
    else
        alpha = 0;

    /* Calculate angle of intersection of horizontal great circle with equator,
       after the vertex was moved into the zenith */
    if (number_of_control_points == 4)
    {
        fvector x_perpendicular_line (2), y_perpendicular_line (2);
        if (swapped_verticals_and_horizontals)
        {
            x_perpendicular_line [0] = center_x;
            x_perpendicular_line [1] = center_x;
            y_perpendicular_line [0] = center_y - 1;
            y_perpendicular_line [1] = center_y + 1;
        }
        else
        {
            x_perpendicular_line [0] = center_x - 1;
            x_perpendicular_line [1] = center_x + 1;
            y_perpendicular_line [0] = center_y;
            y_perpendicular_line [1] = center_y;
        }
        rho_h = determine_rho_h (rho, delta, x_perpendicular_line, y_perpendicular_line, f_normalized, center_x, center_y);
        if (isnan (rho_h))
            rho_h = 0;
    }
    else if (number_of_control_points == 5 || number_of_control_points == 7)
        rho_h = 0;
    else
    {
        rho_h = determine_rho_h (rho, delta, fvector(x.begin() + 4, x.begin() + 6),
                                 fvector(y.begin() + 4, y.begin() + 6), f_normalized, center_x, center_y);
        if (isnan (rho_h))
            if (number_of_control_points == 8)
                rho_h = determine_rho_h (rho, delta, fvector(x.begin() + 6, x.begin() + 8),
                                         fvector(y.begin() + 6, y.begin() + 8), f_normalized, center_x, center_y);
            else
                rho_h = 0;
    }
}

bool lfModifier::enable_perspective_correction (fvector x, fvector y, float d)
{
    const int number_of_control_points = x.size();
    if (focal_length <= 0 || number_of_control_points != 4 &&
        number_of_control_points != 6 && number_of_control_points != 8)
        return false;
    if (d < -1)
        d = -1;
    if (d > 1)
        d = 1;
    for (int i = 0; i < number_of_control_points; i++)
    {
        x [i] = x [i] * NormScale - CenterX;
        y [i] = y [i] * NormScale - CenterY;
    }

    float rho, delta, rho_h, f_normalized, final_rotation, center_of_control_points_x,
        center_of_control_points_y;
    calculate_angles(x, y, focal_length, NormalizedInMillimeters,
                     rho, delta, rho_h, f_normalized, final_rotation, center_of_control_points_x,
                     center_of_control_points_y);

}

void lfModifier::ModifyCoord_Perspective_Correction (void *data, float *iocoord, int count)
{
    // Rd = Ru * (1 - k1 + k1 * Ru^2)
    const float k1 = *(float *)data;
    const float one_minus_k1 = 1.0 - k1;

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        const float x = iocoord [0];
        const float y = iocoord [1];
        const float poly2 = one_minus_k1 + k1 * (pow (x, 2) + pow (y, 2));

        iocoord [0] = x * poly2;
        iocoord [1] = y * poly2;
    }
}
