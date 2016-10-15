/*
    Image modifier implementation: perspective correction functions
    Copyright (C) 2015 by Torsten Bronger <bronger@physik.rwth-aachen.de>
*/

/*
    First, the coordinate system.  It is right-handed.  The centre of the image
    is the origin of the x-y plane.  x is to the right, y to the bottom, and z
    into the back.  All rotations rotate points in mathematical positive
    direction around the axes.  The axes keep their position (extrinsic
    rotations).

    The general algorithm is as follows: The original image (note that the
    sensor sees everything mirror-inverted, but this is annihilated by the
    back-projection later) is positioned parallely to the x-y plane at z = -
    focal length.  Then, it is central-projected to the full sphere (radius
    equals focal length), so it gets positive z coordinates and can be viewed
    properly from the origin.

    This way, also the vertical vanishing point is on the sphere at (- ρ, 90° -
    δ).  Then, the sphere with the image is rotated around the y axis by ρ so
    that the vanishing point is on the y-z plane with z > 0.  Then, the sphere
    is rotated around the y axis by δ so that the vanishing point is in the
    zenith – where it belongs.

    Next, the intersection of the horizontal control line with the x-z plane is
    determined.  Its rectascension is called 90° - ρₕ.  So, the sphere is
    rotation around the y axis a second time, this time by ρₕ.  Now, the right
    vanishing point truely is to the right.

    The rotated image plane is central-projected back into the sensor plane at
    z = - focal length.  In general, it will be way off the center of the
    image, so that the image must be shifted.  We shift so that the image
    center is a fix point.  If however the image center is not visible in the
    result, or too close to the pole (i.e. magnification > 10), we use the
    center of gravity of the control points instead.

    Finally, the scaling in the center of the image is kept constant.  One can
    finetune with an additinal scaling, but this way, one has a good starting
    point.

    Note that in reality, this whole process is performed backwards because we
    make a lookup in the original picture.  In particular, the rotation by ρ,
    δ, ρₕ becomes - ρₕ, - δ, - ρ.  We still need the forward direction in some
    places.

    And then there is the d parameter which allows finetuning of the
    correction.  It is between -1 and 1, with 0 meaning full correction, -1 no
    correction, and 1 increased rotation by factor 1.24.  For example, if you
    have tilted the camera by 40°, d = -1 means the original tilt of 40°, d = 0
    means no tilt (perfect correction), and d = 1 means a tilt of 10° in the
    opposite direction (over-correction).  This way, one can finetune the slope
    of the lines as well as the aspect ratio.
 */

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <cmath>
#include <vector>
#include <numeric>
#include <stdexcept>
#include "windows/mathconstants.h"

using std::acos;
using std::atan;
using std::atan2;
using std::cos;
using std::fabs;
using std::fmod;
using std::log;
using std::pow;
using std::sin;
using std::sqrt;
using std::isnan;

dvector normalize (double x, double y)
{
    double norm = sqrt (pow (x, 2) + pow (y, 2));
    double temp[] = {x / norm, y / norm};
    return dvector (temp, temp + 2);
}

/* Projects the coordinates on an x-y plane with the distance `plane_distance`
 * from the origin.  The centre of the projection is the origin.
 */
void central_projection (dvector coordinates, double plane_distance, double &x, double &y)
{
    double stretch_factor = plane_distance / coordinates [2];
    x = coordinates [0] * stretch_factor;
    y = coordinates [1] * stretch_factor;
}


class svd_no_convergence: public std::runtime_error
{
public:
    svd_no_convergence() : std::runtime_error ("SVD: Iterations did not converge") {}
};

/* The following SVD implementation is a modified version of an SVD
 * implementation published in “Evaluation of gaussian processes and other
 * methods for non-linear regression”, Carl Edward Rasmussen, 1996.
 */
dvector svd (matrix M)
{
    const int n = M [0].size();
    dvector S2 (n);
    int  i, j, k, estimated_column_rank = n, counter = n, iterations = 0,
        max_cycles = (n < 120) ? 60 : n / 2;
    double epsilon = std::numeric_limits<double>::epsilon(),
        e2 = 10 * n * pow (epsilon, 2),
        threshold = 0.2 * epsilon,
        vt, p, x0, y0, q, r, c0, s0, d1, d2;

    M.resize (2 * n, dvector (n));
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
        throw svd_no_convergence();

    dvector result;
    for (matrix::iterator it = M.begin() + n; it != M.end(); ++it)
        result.push_back ((*it) [n - 1]);
    return result;
}

void ellipse_analysis (dvector x, dvector y, double f_normalized, double &x_v, double &y_v,
                       double &center_x, double &center_y)
{
    matrix M;
    double a, b, c, d, f, g, _D, x0, y0, phi, _N, _S, _R, a_, b_, radius_vertex;

    // Taken from http://math.stackexchange.com/a/767126/248694
    for (int i = 0; i < 5; i++)
    {
        double temp[] = {pow (x [i], 2), x [i] * y [i], pow (y [i], 2), x [i], y [i], 1};
        M.push_back (dvector (temp, temp + 6));
    }
    dvector parameters = svd (M);
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

    phi = 1./2 * atan (2 * b / (a - c));
    if (a > c)
        phi += M_PI_2;

    _N = 2 * (a * pow (f, 2) + c * pow (d, 2) + g * pow (b, 2) - 2 * b * d * f - a * c * g) / _D;
    _S = sqrt (pow ((a - c), 2) + 4 * pow (b, 2));
    _R = a + c;
    a_ = sqrt (_N / (_S - _R));
    b_ = sqrt (_N / (- _S - _R));
    // End taken from mathworld
    if (a_ < b_)
    {
        double temp;
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

    x_v = radius_vertex * sin (phi);
    y_v = radius_vertex * cos (phi);
    center_x = x0;
    center_y = y0;
}

/* Returns the coordinates of the intersection of the lines defined by `x` and
 * `y`.  Both parameters need to be exactly 4 items long.  The first two items
 * defines the one line, the last two the other.
 */
void intersection (dvector x, dvector y, double &x_i, double &y_i)
{
    double A, B, C, numerator_x, numerator_y;

    A = x [0] * y [1] - y [0] * x [1];
    B = x [2] * y [3] - y [2] * x [3];
    C = (x [0] - x [1]) * (y [2] - y [3]) - (y [0] - y [1]) * (x [2] - x [3]);

    numerator_x = (A * (x [2] - x [3]) - B * (x [0] - x [1]));
    numerator_y = (A * (y [2] - y [3]) - B * (y [0] - y [1]));

    x_i = numerator_x / C;
    y_i = numerator_y / C;
}

/*
  In the following, I refer to these two rotation matrices: (See
  <http://en.wikipedia.org/wiki/Rotation_matrix#In_three_dimensions>.)

          ⎛ 1     0         0   ⎞
  Rₓ(ϑ) = ⎜ 0   cos ϑ   - sin ϑ ⎟
          ⎝ 0   sin ϑ     cos ϑ ⎠

           ⎛  cos ϑ   0   sin ϑ ⎞
  R_y(ϑ) = ⎜   0      1    0    ⎟
           ⎝- sin ϑ   0   cos ϑ ⎠

           ⎛ cos ϑ   - sin ϑ  0 ⎞
  R_z(ϑ) = ⎜ sin ϑ     cos ϑ  0 ⎟
           ⎝   0         0    1 ⎠
*/

dvector rotate_rho_delta (double rho, double delta, double x, double y, double z)
{
    // This matrix is: Rₓ(δ) · R_y(ρ)
    double A11, A12, A13, A21, A22, A23, A31, A32, A33;
    A11 = cos (rho);
    A12 = 0;
    A13 = sin (rho);
    A21 = sin (rho) * sin (delta);
    A22 = cos (delta);
    A23 = - cos (rho) * sin (delta);
    A31 = - sin (rho) * cos (delta);
    A32 = sin (delta);
    A33 = cos (rho) * cos (delta);

    dvector result (3);
    result [0] = A11 * x + A12 * y + A13 * z;
    result [1] = A21 * x + A22 * y + A23 * z;
    result [2] = A31 * x + A32 * y + A33 * z;
    return result;
}

dvector rotate_rho_delta_rho_h (double rho, double delta, double rho_h,
                                double x, double y, double z)
{
    // This matrix is: R_y(ρₕ) · Rₓ(δ) · R_y(ρ)
    double A11, A12, A13, A21, A22, A23, A31, A32, A33;
    A11 = cos (rho) * cos (rho_h) - sin (rho) * cos (delta) * sin (rho_h);
    A12 = sin (delta) * sin (rho_h);
    A13 = sin (rho) * cos (rho_h) + cos (rho) * cos (delta) * sin (rho_h);
    A21 = sin (rho) * sin (delta);
    A22 = cos (delta);
    A23 = - cos (rho) * sin (delta);
    A31 = - cos (rho) * sin (rho_h) - sin (rho) * cos (delta) * cos (rho_h);
    A32 = sin (delta) * cos (rho_h);
    A33 = - sin (rho) * sin (rho_h) + cos (rho) * cos (delta) * cos (rho_h);

    dvector result (3);
    result [0] = A11 * x + A12 * y + A13 * z;
    result [1] = A21 * x + A22 * y + A23 * z;
    result [2] = A31 * x + A32 * y + A33 * z;
    return result;
}

double determine_rho_h (double rho, double delta, dvector x, dvector y,
                        double f_normalized, double center_x, double center_y)
{
    dvector p0, p1;
    p0 = rotate_rho_delta (rho, delta, x [0], y [0], f_normalized);
    p1 = rotate_rho_delta (rho, delta, x [1], y [1], f_normalized);
    double x_0 = p0 [0], y_0 = p0 [1], z_0 = p0 [2];
    double x_1 = p1 [0], y_1 = p1 [1], z_1 = p1 [2];
    if (y_0 == y_1)
        return y_0 == 0 ? NAN : 0;
    else
    {
        double Delta_x, Delta_z, x_h, z_h, rho_h;
        double temp[] = {x_1 - x_0, z_1 - z_0, y_1 - y_0};
        central_projection (dvector (temp, temp + 3), - y_0, Delta_x, Delta_z);
        x_h = x_0 + Delta_x;
        z_h = z_0 + Delta_z;
        if (z_h == 0)
            rho_h = x_h > 0 ? 0 : M_PI;
        else
            rho_h = M_PI_2 - atan (x_h / z_h);
        if (rotate_rho_delta_rho_h (rho, delta, rho_h, center_x, center_y, f_normalized) [2] < 0)
            rho_h -= M_PI;
        return rho_h;
    }
}

void calculate_angles (dvector x, dvector y, double &f_normalized,
                       double &rho, double &delta, double &rho_h, double &alpha,
                       double &center_of_control_points_x, double &center_of_control_points_y)
{
    const int number_of_control_points = x.size();

    double center_x, center_y;
    if (number_of_control_points == 6)
    {
        center_x = std::accumulate (x.begin(), x.begin() + 4, 0.) / 4;
        center_y = std::accumulate (y.begin(), y.begin() + 4, 0.) / 4;
    }
    else
    {
        center_x = std::accumulate (x.begin(), x.end(), 0.) / number_of_control_points;
        center_y = std::accumulate (y.begin(), y.end(), 0.) / number_of_control_points;
    }

    double x_v, y_v;
    if (number_of_control_points == 5 || number_of_control_points == 7)
        ellipse_analysis (dvector (x.begin(), x.begin() + 5), dvector (y.begin(), y.begin() + 5),
                          f_normalized, x_v, y_v, center_x, center_y);
    else
    {
        intersection (dvector (x.begin(), x.begin() + 4),
                      dvector (y.begin(), y.begin() + 4),
                      x_v, y_v);
        if (number_of_control_points == 8)
        {
            /* The problem is over-determined.  I prefer the fourth line over
               the focal length.  Maybe this is useful in cases where the focal
               length is not known. */
            double x_h, y_h;
            intersection (dvector (x.begin() + 4, x.begin() + 8),
                          dvector (y.begin() + 4, y.begin() + 8),
                          x_h, y_h);
            double radicand = - x_h * x_v - y_h * y_v;
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

    dvector c (2);
    switch (number_of_control_points) {
    case 4:
    case 6:
    case 8:
    {
        dvector a = normalize (x_v - x [0], y_v - y [0]);
        dvector b = normalize (x_v - x [2], y_v - y [2]);
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
    if (number_of_control_points == 7)
    {
        double x5_, y5_;
        central_projection (rotate_rho_delta (rho, delta, x [5], y [5], f_normalized), f_normalized, x5_, y5_);
        double x6_, y6_;
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
        dvector x_perpendicular_line (2), y_perpendicular_line (2);
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
        rho_h = determine_rho_h (rho, delta, dvector (x.begin() + 4, x.begin() + 6),
                                 dvector (y.begin() + 4, y.begin() + 6), f_normalized, center_x, center_y);
        if (isnan (rho_h))
            if (number_of_control_points == 8)
                rho_h = determine_rho_h (rho, delta, dvector (x.begin() + 6, x.begin() + 8),
                                         dvector (y.begin() + 6, y.begin() + 8), f_normalized, center_x, center_y);
            else
                rho_h = 0;
    }
    center_of_control_points_x = center_x;
    center_of_control_points_y = center_y;
}

/* Returns a rotation matrix which combines three rotations.  First, around the
 * y axis by ρ₁, then, around the x axis by δ, and finally, around the y axis
 * again by ρ₂.
 */
matrix generate_rotation_matrix (double rho_1, double delta, double rho_2, double d)
{
    double s_rho_2, c_rho_2, s_delta, c_delta, s_rho_1, c_rho_1,
        w, x, y, z, theta, s_theta;
    /* We calculate the quaternion by multiplying the three quaternions for the
       three rotations (in reverse order).  We use quaternions here to be able
       to apply the d parameter in a reasonable way. */
    s_rho_2 = sin (rho_2 / 2);
    c_rho_2 = cos (rho_2 / 2);
    s_delta = sin (delta / 2);
    c_delta = cos (delta / 2);
    s_rho_1 = sin (rho_1 / 2);
    c_rho_1 = cos (rho_1 / 2);
    w = c_rho_2 * c_delta * c_rho_1 - s_rho_2 * c_delta * s_rho_1;
    x = c_rho_2 * s_delta * c_rho_1 + s_rho_2 * s_delta * s_rho_1;
    y = c_rho_2 * c_delta * s_rho_1 + s_rho_2 * c_delta * c_rho_1;
    z = c_rho_2 * s_delta * s_rho_1 - s_rho_2 * s_delta * c_rho_1;
    // Now, decompose the quaternion into θ and the axis unit vector.
    theta = 2 * acos (w);
    if (theta > M_PI)
        theta -= 2 * M_PI;
    s_theta = sin (theta / 2);
    x /= s_theta;
    y /= s_theta;
    z /= s_theta;
    const double compression = 10;
    theta *= d <= 0 ? d + 1 : 1 + 1. / compression * log (compression * d + 1);
    if (theta > 0.9 * M_PI)
        theta = 0.9 * M_PI;
    else if (theta < - 0.9 * M_PI)
        theta = - 0.9 * M_PI;
    // Compose the quaternion again.
    w = cos (theta / 2);
    s_theta = sin (theta / 2);
    x *= s_theta;
    y *= s_theta;
    z *= s_theta;
    /* Convert the quaternion to a rotation matrix, see e.g.
       <https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion>.  This matrix
       is (if d=0): R_y(ρ2) · Rₓ(δ) · R_y(ρ1) */
    matrix M (3, dvector (3));
    M [0][0] = 1 - 2 * pow (y, 2) - 2 * pow (z, 2);
    M [0][1] = 2 * x * y - 2 * z * w;
    M [0][2] = 2 * x * z + 2 * y * w;
    M [1][0] = 2 * x * y + 2 * z * w;
    M [1][1] = 1 - 2 * pow (x, 2) - 2 * pow (z, 2);
    M [1][2] = 2 * y * z - 2 * x * w;
    M [2][0] = 2 * x * z - 2 * y * w;
    M [2][1] = 2 * y * z + 2 * x * w;
    M [2][2] = 1 - 2 * pow (x, 2) - 2 * pow (y, 2);
    return M;
}

bool lfModifier::EnablePerspectiveCorrection (float *x, float *y, int count, float d)
{
    if (Reverse)
    {
        g_warning ("[Lensfun] reverse perspective correction is not yet implemented\n");
        return false;
    }
    const int number_of_control_points = count;
    if (number_of_control_points < 4 || number_of_control_points > 8 ||
        FocalLengthNormalized <= 0 && number_of_control_points != 8)
        return false;
    if (d < -1)
        d = -1;
    if (d > 1)
        d = 1;
    dvector x_, y_;
    for (int i = 0; i < number_of_control_points; i++)
    {
        x_.push_back (x [i] * NormScale - CenterX);
        y_.push_back (y [i] * NormScale - CenterY);
    }

    double f_normalized = FocalLengthNormalized;
    double rho, delta, rho_h, alpha, center_of_control_points_x,
        center_of_control_points_y, z;
    try
    {
        calculate_angles (x_, y_, f_normalized, rho, delta, rho_h, alpha,
                          center_of_control_points_x, center_of_control_points_y);
    }
    catch (svd_no_convergence &e)
    {
        g_warning ("[Lensfun] %s", e.what());
        return false;
    }

    // Transform center point to get shift
    z = rotate_rho_delta_rho_h (rho, delta, rho_h, 0, 0, f_normalized) [2];
    /* If the image centre is too much outside, or even at infinity, take the
       center of gravity of the control points instead. */
    enum center_type { old_image_center, control_points_center };
    center_type new_image_center = z <= 0 || f_normalized / z > 10 ? control_points_center : old_image_center;

    /* Generate a rotation matrix in forward direction, for getting the
       proper shift of the image center. */
    matrix A = generate_rotation_matrix (rho, delta, rho_h, d);
    dvector center_coords (3);

    switch (new_image_center) {
    case old_image_center:
    {
        center_coords [0] = A [0][2] * f_normalized;
        center_coords [1] = A [1][2] * f_normalized;
        center_coords [2] = A [2][2] * f_normalized;
        break;
    }
    case control_points_center:
    {
        center_coords [0] = A [0][0] * center_of_control_points_x +
                            A [0][1] * center_of_control_points_y +
                            A [0][2] * f_normalized;
        center_coords [1] = A [1][0] * center_of_control_points_x +
                            A [1][1] * center_of_control_points_y +
                            A [1][2] * f_normalized;
        center_coords [2] = A [2][0] * center_of_control_points_x +
                            A [2][1] * center_of_control_points_y +
                            A [2][2] * f_normalized;
        break;
    }
    }
    if (center_coords [2] <= 0)
        return false;
    // This is the mapping scale in the image center
    double mapping_scale = f_normalized / center_coords [2];

    // Finally, generate a rotation matrix in backward (lookup) direction
    {
        matrix A_ = generate_rotation_matrix (- rho_h, - delta, - rho, d);

        /* Now we append the final rotation by α.  This matrix is: R_y(- ρ) ·
           Rₓ(- δ) · R_y(- ρₕ) · R_z(α). */
        A [0][0] = cos (alpha) * A_ [0][0] + sin (alpha) * A_ [0][1];
        A [0][1] = - sin (alpha) * A_ [0][0] + cos (alpha) * A_ [0][1];
        A [0][2] = A_ [0][2];
        A [1][0] = cos (alpha) * A_ [1][0] + sin (alpha) * A_ [1][1];
        A [1][1] = - sin (alpha) * A_ [1][0] + cos (alpha) * A_ [1][1];
        A [1][2] = A_ [1][2];
        A [2][0] = cos (alpha) * A_ [2][0] + sin (alpha) * A_ [2][1];
        A [2][1] = - sin (alpha) * A_ [2][0] + cos (alpha) * A_ [2][1];
        A [2][2] = A_ [2][2];
    }

    double Delta_a, Delta_b;
    central_projection (center_coords, f_normalized, Delta_a, Delta_b);
    {
        double Delta_a_old = Delta_a;
        Delta_a = cos (alpha) * Delta_a + sin (alpha) * Delta_b;
        Delta_b = - sin (alpha) * Delta_a_old + cos (alpha) * Delta_b;
    }

    /* The occurances of factors and denominators here avoid additional
       operations in the inner loop of perspective_correction_callback. */
    float tmp[] = {A [0][0] * mapping_scale, A [0][1] * mapping_scale, A [0][2] * f_normalized,
                   A [1][0] * mapping_scale, A [1][1] * mapping_scale, A [1][2] * f_normalized,
                   A [2][0] / center_coords [2], A [2][1] / center_coords [2], A [2][2],
                   Delta_a / mapping_scale, Delta_b / mapping_scale};
    AddCoordCallback (ModifyCoord_Perspective_Correction, 300, tmp, sizeof (tmp));
    return true;
}

void lfModifier::ModifyCoord_Perspective_Correction (void *data, float *iocoord, int count)
{
    float *param = (float *)data;
    float A11 = param [0];
    float A12 = param [1];
    float A13 = param [2];
    float A21 = param [3];
    float A22 = param [4];
    float A23 = param [5];
    float A31 = param [6];
    float A32 = param [7];
    float A33 = param [8];
    float Delta_a = param [9];
    float Delta_b = param [10];

    for (float *end = iocoord + count * 2; iocoord < end; iocoord += 2)
    {
        float x, y, z_;
        x = iocoord [0] + Delta_a;
        y = iocoord [1] + Delta_b;
        z_ = A31 * x + A32 * y + A33;
        if (z_ > 0)
        {
            iocoord [0] = (A11 * x + A12 * y + A13) / z_;
            iocoord [1] = (A21 * x + A22 * y + A23) / z_;
        }
        else
            iocoord [0] = iocoord [1] = 1.6e16F;
    }
}

//---------------------------// The C interface //---------------------------//

cbool lf_modifier_enable_perspective_correction (
    lfModifier *modifier, float *x, float *y, int count, float d)
{
    return modifier->EnablePerspectiveCorrection (x, y, count, d);
}
