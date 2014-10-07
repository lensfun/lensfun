/*
    Copyright (C) 2001 by Andrew Zabolotny

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __IMAGE_H__
#define __IMAGE_H__

#include "rgbpixel.h"
#include <stdio.h>

#if defined (_MSC_VER) && !defined(CONF_LENSFUN_STATIC)
#if defined auxfun_EXPORTS
#define AF_EXPORT __declspec(dllexport)
#else
#define AF_EXPORT __declspec(dllimport)
#endif
#else
#define AF_EXPORT
#endif

/**
 * This class represents an image object.
 * It provides loading/saving from a PNG file functionality (through libPNG),
 * the rest was cut off ;-)
 */
class AF_EXPORT Image
{
    FILE *file;
    int filesize;
    bool lanczos_func_in_use;
    static float *lanczos_func;
    static int lanczos_func_use;

    unsigned char (*fGetR) (Image *This, float x, float y);
    unsigned char (*fGetG) (Image *This, float x, float y);
    unsigned char (*fGetB) (Image *This, float x, float y);
    void (*fGet) (Image *This, RGBpixel &out, float x, float y);

    // --- Nearest interpolation --- //

    /// Get interpolated red value at given position
    static unsigned char GetR_n (Image *This, float x, float y);
    /// Get interpolated green value at given position
    static unsigned char GetG_n (Image *This, float x, float y);
    /// Get interpolated blue value at given position
    static unsigned char GetB_n (Image *This, float x, float y);
    /// Get interpolated pixel value at given position
    static void Get_n (Image *This, RGBpixel &out, float x, float y);

    // --- Linear interpolation --- //

    /// Get interpolated red value at given position
    static unsigned char GetR_b (Image *This, float x, float y);
    /// Get interpolated green value at given position
    static unsigned char GetG_b (Image *This, float x, float y);
    /// Get interpolated blue value at given position
    static unsigned char GetB_b (Image *This, float x, float y);
    /// Get interpolated pixel value at given position
    static void Get_b (Image *This, RGBpixel &out, float x, float y);

    // --- Lanczos interpolation --- //

    /// Get interpolated red value at given position
    static unsigned char GetR_l (Image *This, float x, float y);
    /// Get interpolated green value at given position
    static unsigned char GetG_l (Image *This, float x, float y);
    /// Get interpolated blue value at given position
    static unsigned char GetB_l (Image *This, float x, float y);
    /// Get interpolated pixel value at given position
    static void Get_l (Image *This, RGBpixel &out, float x, float y);

public:
    /// The actual image data
    RGBpixel *image;
    /// Image size
    unsigned width, height;
    /// The transparent color
    RGBpixel transp;

    enum InterpolationMethod
    {
        /// Nearest interpolation (very fast, very low quality)
        I_NEAREST,
        /// Bi-linear interpolation (fast, low quality)
        I_BILINEAR,
        /// Lanczos interpolation (slow, high quality)
        I_LANCZOS
    };

    /// Initialize the image object
    Image ();
    /// Destroy the image object
    ~Image ();
    /// Open the file and prepare to read from it
    bool Open (const char *fName);
    /// Close the file
    void Close ();
    /// Free the image
    void Free ();
    /// Load next image from file: this completely discards previously loaded one
    bool LoadPNG ();
    /// Save the image into a PNG file in given format
    bool SavePNG (const char *fName);
    /// Check if file is at EOF
    bool AtEOF ()
    { return ftell (file) >= filesize; }
    /// Allocate a new image of given size
    void Resize (unsigned newwidth, unsigned newheight);

    /// Initialize interpolation method
    void InitInterpolation (InterpolationMethod method);

    /// Get interpolated red value at given position
    unsigned char GetR (float x, float y)
    { return fGetR (this, x, y); }
    /// Get interpolated green value at given position
    unsigned char GetG (float x, float y)
    { return fGetG (this, x, y); }
    /// Get interpolated blue value at given position
    unsigned char GetB (float x, float y)
    { return fGetB (this, x, y); }
    /// Get interpolated pixel value at given position
    void Get (RGBpixel &out, float x, float y)
    { fGet (this, out, x, y); }
};

#endif // __IMAGE_H__
