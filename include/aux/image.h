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

/**
 * This class represents an image object.
 * It provides loading/saving from a PNG file functionality (through libPNG),
 * the rest was cut off ;-)
 */
class Image
{
    FILE *file;
    int filesize;

public:
    /// The actual image data
    RGBpixel *image;
    /// Image size
    unsigned width, height;
    /// The transparent color
    RGBpixel transp;

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

    // --- Linear interpolation --- //

    /// Get interpolated red value at given position
    unsigned char GetR_l (float x, float y);
    /// Get interpolated green value at given position
    unsigned char GetG_l (float x, float y);
    /// Get interpolated blue value at given position
    unsigned char GetB_l (float x, float y);
    /// Get interpolated pixel value at given position
    void Get_l (RGBpixel &out, float x, float y);

    // --- Modified third order B-spline interpolation (A. Gotchev et al) --- //

    /// Get interpolated red value at given position
    unsigned char GetR_s (float x, float y);
    /// Get interpolated green value at given position
    unsigned char GetG_s (float x, float y);
    /// Get interpolated blue value at given position
    unsigned char GetB_s (float x, float y);
    /// Get interpolated pixel value at given position
    void Get_s (RGBpixel &out, float x, float y);
};

#endif // __IMAGE_H__
