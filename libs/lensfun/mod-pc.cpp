/*
    Image modifier implementation: perspective correction functions
    Copyright (C) 2015 by Torsten Bronger <bronger@physik.rwth-aachen.de>
*/

#include "config.h"
#include "lensfun.h"
#include "lensfunprv.h"
#include <math.h>
#include <vector>
#include "windows/mathconstants.h"

void calculate_angles(std::vector<float> x_norm, std::vector<float> y_norm, float focal,
                      float NormalizedInMillimeters, float &rho, float &delta, float &rho_h,
                      float &f_normalized, float &final_rotation, float &center_of_control_points_x,
                      float &center_of_control_points_y)
{
    
}

bool lfModifier::Initialize_Perspective_Correction (int *x, int *y, int count, float d)
{
    if (focal_length <= 0 || count != 4 && count != 6 && count != 8)
        return false;
    if (d < -1)
        d = -1;
    if (d > 1)
        d = 1;
    std::vector<float> x_norm, y_norm;
    for (int i = 0; i < count; i++)
    {
        x_norm.push_back (x [i] * NormScale - CenterX);
        y_norm.push_back (y [i] * NormScale - CenterY);
    }

    float rho, delta, rho_h, f_normalized, final_rotation, center_of_control_points_x,
        center_of_control_points_y;
    calculate_angles(x_norm, y_norm, focal_length, NormalizedInMillimeters,
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
        const float poly2 = one_minus_k1 + k1 * (x * x + y * y);

        iocoord [0] = x * poly2;
        iocoord [1] = y * poly2;
    }
}
