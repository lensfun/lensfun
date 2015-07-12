#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""This program demonstrates how perspective control could work in Lensfun.  It
must be converted to Lensfun C++ code of course.  However, its structure is
already very similar to how Lensfun is structured.

Usage:
    python3 perspective_control.py <JSON file>

The JSON file has the following structure:

[
    "image.jpeg",
    18,
    1.534,
    6,
    [   8,   59,  289,  229,   8,  289],
    [ 188,  154,  187,  154, 188,  187]
]

Here, "image.jpeg" is the filename of the image file, 18 is the focal length in
mm, 1.534 is the crop factor, 6 is a scaling parameter, the first array are the
x values of the control points, and the second array are the y values.  The (x,
y) values must be image pixel coordinates.  There are 4, 6, or 8 points
allowed, see `initialize_perspective_correction` below.
"""

import sys, subprocess, os, array, json, multiprocessing, tempfile
from math import sin, cos, tan, atan, atan2, sqrt, copysign, acos, log
from math import pi as π

def read_ppm(input_file, read_data=True):
    def read_token(skip_trailing_whitespace=True):
        nonlocal current_character
        token = b""
        while current_character:
            if current_character == b"#":
                while current_character and current_character not in b"\r\n":
                    current_character = input_file.read(1)
            elif current_character.isspace():
                if skip_trailing_whitespace:
                    while current_character.isspace():
                        current_character = input_file.read(1)
                return token
            else:
                token += current_character
            current_character = input_file.read(1)
    state = "start"
    current_character = input_file.read(1)
    magic_number = read_token()
    assert magic_number == b"P6"
    width = int(read_token())
    height = int(read_token())
    max_value = int(read_token(skip_trailing_whitespace=False))
    assert max_value == 255
    if read_data:
        image_data = array.array("B")
        image_data.fromfile(input_file, width * height * 3)
        image_data.byteswap()
        return image_data, width, height
    else:
        return width, height

def read_image_file(filepath):
    convert_process = subprocess.Popen(
        ["convert", filepath, "-set", "colorspace", "RGB", "ppm:-"], stderr=open(os.devnull, "w"), stdout=subprocess.PIPE)
    return read_ppm(convert_process.stdout)

def write_image_file(image_data, width, height, filepath):
    image_data.byteswap()
    with tempfile.NamedTemporaryFile(delete=False) as outfile:
        outfile.write("P6\n{} {}\n255\n".format(width, height).encode("ascii") + image_data.tobytes())
    subprocess.call(["convert", outfile.name, "-set", "colorspace", "RGB", filepath])
    image_data.byteswap()


def ellipse_analysis(x, y, f_normalized):
    import numpy

    # Taken from http://math.stackexchange.com/a/767126/248694
    M = numpy.array([[x[i]**2, x[i] * y[i], y[i]**2, x[i], y[i], 1] for i in range(5)])
    U, σ, V_ = numpy.linalg.svd(M)
    assert all(σ > 1e-15)
    # Taken from http://mathworld.wolfram.com/Ellipse.html, equation (15)
    # onwards.
    a, b, c, d, f, g = V_[-1].reshape(-1)
    b, d, f = b/2, d/2, f/2

    _D = b**2 - a * c
    x0 = (c * d - b * f) / _D
    y0 = (a * f - b * d) / _D

    φ = 1/2 * atan(2 * b / (a - c))
    if a > c:
        φ += π/2

    _N = 2 * (a * f**2 + c * d**2 + g * b**2 - 2 * b * d * f - a * c * g) / _D
    _S = sqrt((a - c)**2 + 4 * b**2)
    _R = a + c
    a_ = sqrt(_N / (_S - _R))
    b_ = sqrt(_N / (- _S - _R))
    # End taken from mathworld
    if a_ < b_:
        a_, b_ = b_, a_
        φ -= π/2
    # Normalize to -π/2..π/2 so that the vertex half-plane is top or bottom
    # rather than e.g. left or right.
    φ = (φ + π/2) % π - π/2

    # Negative sign because vertex at top (negative y values) should be
    # default.
    radius_vertex = - f_normalized / sqrt((a_ / b_)**2 - 1)
    if (x[0] - x0) * (y[1] - y0) < (x[1] - x0) * (y[0] - y0):
        radius_vertex *= -1

    return radius_vertex * sin(φ), radius_vertex * cos(φ), x0, y0


def intersection(x1, y1, x2, y2, x3, y3, x4, y4):
    A = x1 * y2 - y1 * x2
    B = x3 * y4 - y3 * x4
    C = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)

    numerator_x = (A * (x3 - x4) - B * (x1 - x2))
    numerator_y = (A * (y3 - y4) - B * (y1 - y2))
    try:
        return numerator_x / C, numerator_y / C
    except ZeroDivisionError:
        return math.copysign(float("inf"), numerator_x), math.copysign(float("inf"), numerator_y)


# In the following, I refer to these two rotation matrices: (See
# <http://en.wikipedia.org/wiki/Rotation_matrix#In_three_dimensions>.)
#
#         ⎛ 1     0         0   ⎞
# Rₓ(ϑ) = ⎜ 0   cos ϑ   - sin ϑ ⎟
#         ⎝ 0   sin ϑ     cos ϑ ⎠
#
#          ⎛  cos ϑ   0   sin ϑ ⎞
# R_y(ϑ) = ⎜   0      1    0    ⎟
#          ⎝- sin ϑ   0   cos ϑ ⎠
#
#          ⎛ cos ϑ   - sin ϑ  0 ⎞
# R_z(ϑ) = ⎜ sin ϑ     cos ϑ  0 ⎟
#          ⎝   0         0    1 ⎠


def rotate_ρ_δ(ρ, δ, x, y, z):
    # This matrix is: Rₓ(δ) · R_y(ρ)
    A11, A12, A13 = cos(ρ), 0, sin(ρ)
    A21, A22, A23 = sin(ρ) * sin(δ), cos(δ), - cos(ρ) * sin(δ)
    A31, A32, A33 = - sin(ρ) * cos(δ), sin(δ), cos(ρ) * cos(δ)
    return \
        A11 * x + A12 * y + A13 * z, \
        A21 * x + A22 * y + A23 * z, \
        A31 * x + A32 * y + A33 * z

def rotate_ρ_δ_ρh(ρ, δ, ρ_h, x, y, z):
    # This matrix is: R_y(ρₕ) · Rₓ(δ) · R_y(ρ)
    A11, A12, A13 = cos(ρ) * cos(ρ_h) - sin(ρ) * cos(δ) * sin(ρ_h), sin(δ) * sin(ρ_h), sin(ρ) * cos(ρ_h) + cos(ρ) * cos(δ) * sin(ρ_h)
    A21, A22, A23 = sin(ρ) * sin(δ), cos(δ), - cos(ρ) * sin(δ)
    A31, A32, A33 = - cos(ρ) * sin(ρ_h) -  sin(ρ) * cos(δ) * cos(ρ_h), \
                    sin(δ) * cos(ρ_h), \
                    - sin(ρ) * sin(ρ_h) + cos(ρ) * cos(δ) * cos(ρ_h)
    return \
        A11 * x + A12 * y + A13 * z, \
        A21 * x + A22 * y + A23 * z, \
        A31 * x + A32 * y + A33 * z

def determine_ρ_h(ρ, δ, x, y, f_normalized, center_x, center_y):
    x_, y_, z_ = [float("nan"), float("nan")], [float("nan"), float("nan")], [float("nan"), float("nan")]
    x_[0], y_[0], z_[0] = rotate_ρ_δ(ρ, δ, x[0], y[0], f_normalized)
    x_[1], y_[1], z_[1] = rotate_ρ_δ(ρ, δ, x[1], y[1], f_normalized)
    if y_[0] == y_[1]:
        if y_[0] == 0:
            # ρ_h is undefined (horizontal great circle is on the equator).
            return None
        else:
            # The horizontal vanishing point is perfectly to the left/right, so
            # no rotation necessary.
            return 0
    else:
        λ = y_[0] / (y_[0] - y_[1])
        x_h = x_[0] + λ * (x_[1] - x_[0])
        z_h = z_[0] + λ * (z_[1] - z_[0])
        if z_h == 0:
            ρ_h = 0 if x_h > 0 else π
        else:
            ρ_h = π / 2 - atan(x_h / z_h)
        if rotate_ρ_δ_ρh(ρ, δ, ρ_h, center_x, center_y, f_normalized)[2] < 0:
            # We have to move the vertex to the left instead of right
            ρ_h -= π
        return ρ_h

def calculate_angles(x, y, f, normalized_in_millimeters):
    number_of_control_points = len(x)
    # Calculate the center of gravity of the control points
    if number_of_control_points == 6:
        center_x = sum(x[:4]) / 4
        center_y = sum(y[:4]) / 4
    else:
        center_x = sum(x) / number_of_control_points
        center_y = sum(y) / number_of_control_points

    # FixMe: Is really always an f given (even if it is inaccurate)?
    f_normalized = f / normalized_in_millimeters
    if number_of_control_points in [5, 7]:
        x_v, y_v, center_x, center_y = ellipse_analysis(x[:5], y[:5], f_normalized)
    else:
        x_v, y_v = intersection(x[0], y[0], x[1], y[1], x[2], y[2], x[3], y[3])
        if number_of_control_points == 8:
            # The problem is over-determined.  I prefer the fourth line over the
            # focal length.  Maybe this is useful in cases where the focal length
            # is not known.
            x_h, y_h = intersection(x[4], y[4], x[5], y[5], x[6], y[6], x[7], y[7])
            try:
                f_normalized = sqrt(- x_h * x_v - y_h * y_v)
            except ValueError:
                pass
            else:
                print(f_normalized * normalized_in_millimeters, "mm")

    # Calculate vertex in polar coordinates
    ρ = atan(- x_v / f_normalized)
    δ = π / 2 - atan(- y_v / sqrt(x_v**2 + f_normalized**2))
    if rotate_ρ_δ(ρ, δ, center_x, center_y, f_normalized)[2] < 0:
        # We have to move the vertex into the nadir instead of the zenith.
        δ -= π

    swapped_verticals_and_horizontals = False

    if number_of_control_points in [4, 6, 8]:
        a = normalize(x_v - x[0], y_v - y[0])
        b = normalize(x_v - x[2], y_v - y[2])
        c = (a[0] + b[0], a[1] + b[1])
    elif number_of_control_points == 5:
        c = (x_v - center_x, y_v - center_y)
    else:
        c = (x[5] - x[6], y[5] - y[6])
    if number_of_control_points == 7:
        x5_, y5_ = rotate_ρ_δ(ρ, δ, x[5], y[5], f_normalized)
        x6_, y6_ = rotate_ρ_δ(ρ, δ, x[6], y[6], f_normalized)
        α = atan2(y6_ - y5_, x6_ - x5_)
        if abs(c[0]) > abs(c[1]):
            # Find smallest rotation into horizontal
            α = - (α - π/2) % π - π/2
        else:
            # Find smallest rotation into vertical
            α = - α % π - π/2
    elif abs(c[0]) > abs(c[1]):
        swapped_verticals_and_horizontals = True
        α = copysign(π/2, ρ)
    else:
        α = 0

    # Calculate angle of intersection of horizontal great circle with equator,
    # after the vertex was moved into the zenith
    if number_of_control_points == 4:
        if swapped_verticals_and_horizontals:
            x_perpendicular_line = [center_x, center_x]
            y_perpendicular_line = [center_y - 1, center_y + 1]
        else:
            x_perpendicular_line = [center_x - 1, center_x + 1]
            y_perpendicular_line = [center_y, center_y]
        ρ_h = determine_ρ_h(ρ, δ, x_perpendicular_line, y_perpendicular_line, f_normalized, center_x, center_y) or 0
    elif number_of_control_points in [5, 7]:
        ρ_h = 0
    else:
        ρ_h = determine_ρ_h(ρ, δ, x[4:6], y[4:6], f_normalized, center_x, center_y)
        if ρ_h is None and number_of_control_points == 8:
            ρ_h = determine_ρ_h(ρ, δ, x[6:8], y[6:8], f_normalized, center_x, center_y)
        ρ_h = ρ_h or 0
    return ρ, δ, ρ_h, f_normalized, α, center_x, center_y

def generate_rotation_matrix(ρ1, δ, ρ2, d):
    """Returns a rotation matrix which combines three rotations.  First, around the
    y axis by ρ₁, then, around the x axis by δ, and finally, around the y axis
    again by ρ₂.
    """
    # We calculate the quaternion by multiplying the three quaternions for the
    # three rotations (in reverse order).  We use quaternions here to be able
    # to apply the d parameter in a reasonable way.
    s_ρ2, c_ρ2 = sin(ρ2/2), cos(ρ2/2)
    s_δ, c_δ = sin(δ/2), cos(δ/2)
    s_ρ1, c_ρ1 = sin(ρ1/2), cos(ρ1/2)
    w = c_ρ2 * c_δ * c_ρ1 - s_ρ2 * c_δ * s_ρ1
    x = c_ρ2 * s_δ * c_ρ1 + s_ρ2 * s_δ * s_ρ1
    y = c_ρ2 * c_δ * s_ρ1 + s_ρ2 * c_δ * c_ρ1
    z = c_ρ2 * s_δ * s_ρ1 - s_ρ2 * s_δ * c_ρ1
    # Now, decompose the quaternion into θ and the axis unit vector.
    θ = 2 * acos(w)
    if θ > π:
        θ -= 2*π
    s_θ = sin(θ/2)
    x, y, z = x / s_θ, y / s_θ, z / s_θ
    # Apply d
    compression = 10
    θ *= d + 1 if d <= 0 else 1 + 1 / compression * log(compression * d + 1)
    if θ > 0.9 * π:
        θ = 0.9 * π
    elif θ < - 0.9 * π:
        θ = - 0.9 * π
    # Compose the quaternion again.
    w = cos(θ/2)
    s_θ = sin(θ/2)
    x, y, z = x * s_θ, y * s_θ, z * s_θ
    # Convert the quaternion to a rotation matrix, see e.g.
    # <https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion>.  This matrix
    # is (if d=0): R_y(ρ2) · Rₓ(δ) · R_y(ρ1)
    A11, A12, A13 = 1 - 2 * y**2 - 2 * z**2,  2 * x * y - 2 * z * w,    2 * x * z + 2 * y * w
    A21, A22, A23 = 2 * x * y + 2 * z * w,    1 - 2 * x**2 - 2 * z**2,  2 * y * z - 2 * x * w
    A31, A32, A33 = 2 * x * z - 2 * y * w,    2 * y * z + 2 * x * w,    1 - 2 * x**2 - 2 * y**2
    return A11, A12, A13, \
           A21, A22, A23, \
           A31, A32, A33

def normalize(x, y):
    norm = sqrt(x**2 + y**2)
    try:
        return x / norm, y / norm
    except ZeroDivisionError:
        return math.copysign(float("inf"), x), math.copysign(float("inf"), y)

class Modifier:

    """First, the coordinate system.  It is right-handed.  The centre of the image
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
    correction.  It is between -1 and 1, with 0 meaning full correction, 0 no
    correction, and 1 increased rotation by factor 1.25.  For example, if you
    have tilted the camera by 40°, d = -1 means the original tilt of 40°, d = 0
    means no tilt (perfect correction), and d = 1 means a tilt of 10° in the
    opposite direction (over-correction).  This way, one can finetune the slope
    of the lines as well as the aspect ratio.

    """

    def __init__(self, crop_factor, width, height):
        self.size = min(width, height)
        self.crop_factor, self.width, self.height = crop_factor, width, height
        aspect_ratio = width / height if width > height else height / width
        aspect_ratio_correction = sqrt(1 + aspect_ratio**2)
        self.normalized_in_millimeters = sqrt(36**2 + 24**2) / 2 / aspect_ratio_correction / self.crop_factor
        self.norm_scale = 2 / (self.size - 1)
        self.norm_unscale = (self.size - 1) / 2
        self.center_x = width / self.size
        self.center_y = height / self.size

    def initialize(self, focal_length):
        self.focal_length = focal_length

    def initialize_perspective_correction(self, x, y, d):
        """Depending on the number of control points given, there are three possible
        modes:

        4 control points: c0 and c1 define one vertical lines, c2 and c3 the other.
        The focal length is used to get the proper aspect ratio.

        6 control points: c0 to c3 like above.  c4 and c5 define a horizontal line.
        The focal length is used to get the proper aspect ratio.

        8 control points: c0 to c5 like above.  c6 and c7 define a second
        horizontal line.  The focal length given is discarded and calculated from
        the control points.

        5 control points: They all must lie on an ellipse that is actually a
        rotated circle.  If they lie in a clockwise ordering, the vertex is
        assumed to be above the ellipse centre in the image.  If they lie in a
        counter-clockwise ordering, the vertex is assumed to be below the
        ellipse.  The focal length is needed to find the proper vertex.

        7 control points: c0 to c4 like above.  c5 and c6 define a horizontal
        line which is used to rotate the final image nicely.

        If the lines constructed from the first four control points for the 4,
        6 and 8 points case (or the last two for the 7 points case) are more
        horizontal than vertical, in all of the above, "horizontal" and
        "vertical" need to be swapped.

        All control points must be given as pixel coordinates in the original
        image.  In particular, cropping, rotating, shifting, or scaling must be
        switched off or removed by mapping.  For best precision,
        anti-distortion should have been applied already.  Moreover, fisheye
        images must have been transformed into rectilinear of the same focal
        length.  The control points can be written to a sidecar file because
        they describe the perspective correction universally,
        i.e. independently of all corrections that can be activated (scaling,
        anti-distortion, projection transform, etc).

        FixMe: Maybe a static routine should be offered which takes a modifier,
        and an (x, y) pair, and maps them on x and y coordinates as if only the
        anti-distortion and the geometry transformation to rectilinear was
        active.

        The d parameter is supposed to be offered to the user as a slider.  It
        can take values from -1 to +1.  0 denotes the perfect correction.  -1
        is the unchanged image.  +1 is an increase of the tilting angle by 25%.
        """
        if self.focal_length <= 0 or len(x) not in [4, 5, 6, 7, 8] or len(x) != len(y):
            # Don't add any callback
            return False
        if d <= - 1:
            d = - 1
        elif d >= 1:
            d = 1
        # x, y = x[:4], y[:4]  # For testing only four points
        # x[0:6] = x[0], x[2], x[1], x[3], x[0], x[1]  # For testing horizonal mode
        # y[0:6] = y[0], y[2], y[1], y[3], y[0], y[1]  # For testing horizonal mode
        # x[6:8] = [x[1], x[3]]  # for faking an explicit second horizonal line
        # y[6:8] = [y[1], y[3]]  # for faking an explicit second horizonal line
        x = [value * self.norm_scale - self.center_x for value in x]
        y = [value * self.norm_scale - self.center_y for value in y]

        ρ, δ, ρ_h, f_normalized, α, center_of_control_points_x, center_of_control_points_y = \
                calculate_angles(x, y, self.focal_length, self.normalized_in_millimeters)

        # Transform center point to get shift
        z = rotate_ρ_δ_ρh(ρ, δ, ρ_h, 0, 0, f_normalized)[2]
        # If the image centre is too much outside, or even at infinity, take the
        # center of gravity of the control points instead.
        new_image_center = "control points center" if z <= 0 or f_normalized / z > 10 else "old image center"

        # Generate a rotation matrix in forward direction, for getting the
        # proper shift of the image center.
        A11, A12, A13, \
        A21, A22, A23, \
        A31, A32, A33 = generate_rotation_matrix(ρ, δ, ρ_h, d)

        if new_image_center == "old image center":
            x, y, z = A13 * f_normalized, A23 * f_normalized, A33 * f_normalized
        elif new_image_center == "control points center":
            x, y, z = A11 * center_of_control_points_x + A12 * center_of_control_points_y + A13 * f_normalized, \
                      A21 * center_of_control_points_x + A22 * center_of_control_points_y + A23 * f_normalized, \
                      A31 * center_of_control_points_x + A32 * center_of_control_points_y + A33 * f_normalized
        if z <= 0:
            return False
        # This is the mapping scale in the image center
        mapping_scale = f_normalized / z
        # Central projection through the origin
        Δa = - x * mapping_scale
        Δb = - y * mapping_scale

#        print(ρ * 180 / π, δ * 180 / π, ρ_h * 180 / π, α * 90)

        # Finally, generate a rotation matrix in backward (lookup) direction
        A11, A12, A13, \
        A21, A22, A23, \
        A31, A32, A33 = generate_rotation_matrix(- ρ_h, - δ, - ρ, d)

        # Now we append the final rotation by α.  This matrix is: R_y(- ρ) ·
        # Rₓ(- δ) · R_y(- ρₕ) · R_z(α).
        A11, A12, A13 = cos(α) * A11 + sin(α) * A12, - sin(α) * A11 + cos(α) * A12, A13
        A21, A22, A23 = cos(α) * A21 + sin(α) * A22, - sin(α) * A21 + cos(α) * A22, A23
        A31, A32, A33 = cos(α) * A31 + sin(α) * A32, - sin(α) * A31 + cos(α) * A32, A33
        Δa, Δb = cos(α) * Δa + sin(α) * Δb, - sin(α) * Δa + cos(α) * Δb

        # The occurances of mapping_scale here avoid an additional
        # multiplication in the inner loop of perspective_correction_callback.
        self.callback_data = A11 * mapping_scale, A12 * mapping_scale, A13, \
                             A21 * mapping_scale, A22 * mapping_scale, A23, \
                             A31 * mapping_scale, A32 * mapping_scale, A33, \
                             f_normalized, Δa / mapping_scale, Δb / mapping_scale
        return True

    @staticmethod
    def perspective_correction_callback(data, iocoord, offset, count):
        A11, A12, A13, \
        A21, A22, A23, \
        A31, A32, A33 = data[:9]
        f_normalized = data[9]
        Δa, Δb = data[10:12]
        for i in range(count):
            x = iocoord[offset + i * 2] - Δa
            y = iocoord[offset + i * 2 + 1] - Δb
            z_ = A31 * x + A32 * y + A33 * f_normalized
            if z_ > 0:
                x_ = A11 * x + A12 * y + A13 * f_normalized
                y_ = A21 * x + A22 * y + A23 * f_normalized
                # Central projection through the origin
                stretch_factor = f_normalized / z_
                iocoord[offset + i * 2] = x_ * stretch_factor
                iocoord[offset + i * 2 + 1] = y_ * stretch_factor
            else:
                iocoord[offset + i * 2] = iocoord[offset + i * 2 + 1] = float("nan")

    @staticmethod
    def scaling_callback(data, iocoord, offset, count):
        scaling = data[0]
        for i in range(count):
            iocoord[offset + i * 2] *= scaling
            iocoord[offset + i * 2 + 1] *= scaling

    def apply_perspective_correction(self, xu, yu, width, height, res):
        y = yu * self.norm_scale - self.center_y
        offset = 0   # In the C code, res itself is moved forward
        for j in range(height):
            x = xu * self.norm_scale - self.center_x
            for i in range(width):
                res[offset + i * 2] = x
                res[offset + i * 2 + 1] = y
                x += self.norm_scale
            self.scaling_callback([self.scaling_factor], res, offset, width)
            self.perspective_correction_callback(self.callback_data, res, offset, width)
            for i in range(width):
                res[offset] = (res[offset] + self.center_x) * self.norm_unscale
                res[offset + 1] = (res[offset + 1] + self.center_y) * self.norm_unscale
                offset += 2
            y += self.norm_scale

def process_image(json_filepath, d, index):
    image_filepath, f, crop_factor, shrinking, x, y = json.load(open(json_filepath))
    image_filepath = os.path.join(os.path.dirname(json_filepath), image_filepath)
    image_data, width, height = read_image_file(image_filepath)
    modifier = Modifier(crop_factor, width, height)
    modifier.initialize(f)
    if modifier.initialize_perspective_correction(x, y, d):
        res = array.array("f", width * height * 2 * [0])
        modifier.scaling_factor = shrinking
        modifier.apply_perspective_correction(0, 0, width, height, res)
        destination_image_data = array.array("B", width * height * 3 * [0])
        for y in range(height):
            for x in range(width):
                offset = (width * y + x) * 2
                try:
                    a = int(round(res[offset]))
                    b = int(round(res[offset + 1]))
                except ValueError:
                    # NaN
                    continue
                if 0 <= a < width and 0 <= b < height:
                    destination_offset = (width * y + x) * 3
                    image_offset = (width * b + a) * 3
                    destination_image_data[destination_offset] = image_data[image_offset]
                    destination_image_data[destination_offset + 1] = image_data[image_offset + 1]
                    destination_image_data[destination_offset + 2] = image_data[image_offset + 2]
    else:
        destination_image_data = image_data
    basename, extension = os.path.splitext(image_filepath)
    write_image_file(destination_image_data, width, height, "{}_{:03}_{}{}".format(basename, index, d, extension))

pool = multiprocessing.Pool()
results = set()
for filename in sys.argv[1:]:
    results.add(pool.apply_async(process_image, (filename, 0, 0)))
    # for i, d in enumerate(numpy.arange(-1, 1.02, 0.02)):
    #     results.add(pool.apply_async(process_image, (filename, d, i)))
pool.close()
pool.join()
for result in results:
    # Just to get tracebacks if necessary
    result.get()
