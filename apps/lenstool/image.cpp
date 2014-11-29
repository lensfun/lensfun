/*
    Miscelaneous image manipulations
    Copyright (C) 2001 by Andrew Zabolotny
*/

#include "image.h"
#include <zlib.h>
#include <png.h>
#include <stdlib.h>
#include <string.h>
#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#include <math.h>

float rint (float x)
{
     return floor (x + 0.5f);
}

float trunc (float x)
{
  return (((x) < 0) ? ceil ((x)) : floor ((x)));
} 
#define unlink _unlink
#else
#include <unistd.h>
#include <math.h>
#endif

// A support of 3 gives an overall sharper looking image, but
// it is a) slower b) gives more sharpening artefacts
#define LANCZOS_SUPPORT    2
// Lanczos kernel is precomputed in a table with this resolution
// The value below seems to be enough for HQ upscaling up to eight times
#define LANCZOS_TABLE_RES  256

template<typename T> static inline T square (T x)
{
    return x * x;
}

float *Image::lanczos_func = NULL;
int Image::lanczos_func_use = 0;

Image::Image () :
    file (NULL), lanczos_func_in_use (false), image (NULL)
{
}

Image::~Image ()
{
    Close ();
    Free ();
}

bool Image::Open (const char *fName)
{
    file = fopen (fName, "rb");
    if (!file)
        return false;

    fseek (file, 0, SEEK_END);
    filesize = ftell (file);
    fseek (file, 0, SEEK_SET);

    return true;
}

void Image::Close ()
{
    if (file)
    {
        fclose (file);
        file = NULL;
    }
}

void Image::Free ()
{
    delete [] image;
    image = NULL;
    if (lanczos_func_in_use && !--lanczos_func_use)
    {
        delete [] lanczos_func;
        lanczos_func = NULL;
        lanczos_func_in_use = false;
    }
}

bool Image::LoadPNG ()
{
    png_structp png;
    png_infop info;

    size_t rowbytes, exp_rowbytes;
    png_bytep *row_pointers;

    Free ();

    png = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
        return false;

    info = png_create_info_struct (png);
    if (!info)
    {
        png_destroy_read_struct (&png, (png_infopp) NULL, (png_infopp) NULL);
        png = NULL;
        return false;
    }

    png_init_io (png, file);

    if (setjmp (png_jmpbuf(png)))
        // If we get here, we had a problem reading the file
        goto nomem;

    png_read_info (png, info);

    // Get picture info
    png_uint_32 Width, Height;
    int bit_depth, color_type;

    png_get_IHDR (png, info, &Width, &Height, &bit_depth, &color_type,
                  NULL, NULL, NULL);

    if (bit_depth > 8)
        // tell libpng to strip 16 bit/color files down to 8 bits/color
        png_set_strip_16 (png);
    else if (bit_depth < 8)
        // Expand pictures with less than 8bpp to 8bpp
        png_set_packing (png);

    switch (color_type)
    {
        case PNG_COLOR_TYPE_GRAY:
        case PNG_COLOR_TYPE_GRAY_ALPHA:
            png_set_gray_to_rgb (png);
            break;
        case PNG_COLOR_TYPE_PALETTE:
            png_set_palette_to_rgb (png);
            break;
        case PNG_COLOR_TYPE_RGB:
        case PNG_COLOR_TYPE_RGB_ALPHA:
            break;
        default:
            goto nomem;
    }

    // If there is no alpha information, fill with 0xff
    if (!(color_type & PNG_COLOR_MASK_ALPHA))
    {
        // Expand paletted or RGB images with transparency to full alpha
        // channels so the data will be available as RGBA quartets.
        if (png_get_valid (png, info, PNG_INFO_tRNS))
            png_set_tRNS_to_alpha (png);
        else
            png_set_filler (png, 0xff, PNG_FILLER_AFTER);
    }

    // Update structure with the above settings
    png_read_update_info (png, info);

    // Allocate the memory to hold the image
    image = new RGBpixel [(width = Width) * (height = Height)];
    if (!image)
        goto nomem;
    exp_rowbytes = Width * sizeof (RGBpixel);

    rowbytes = png_get_rowbytes (png, info);
    if (rowbytes != exp_rowbytes)
        goto nomem;                         // Yuck! Something went wrong!

    row_pointers = new png_bytep [Height];

    if (!row_pointers
        || setjmp (png_jmpbuf(png)))             // Set a new exception handler
    {
        delete [] row_pointers;
    nomem:
        png_destroy_read_struct (&png, &info, (png_infopp) NULL);
        Free ();
        return false;
    }

    for (png_uint_32 row = 0; row < Height; row++)
        row_pointers [row] = ((png_bytep)image) + row * rowbytes;

    // Read image data
    png_read_image (png, row_pointers);

    // read rest of file, and get additional chunks in info_ptr
    png_read_end (png, (png_infop)NULL);

    // Free the row pointers array that is not needed anymore
    delete [] row_pointers;

    png_destroy_read_struct (&png, &info, (png_infopp) NULL);

    return true;
}

static inline int isqr (int x)
{ return x * x; }

bool Image::SavePNG (const char *fName)
{
    /* Remove the file in the case it exists and it is a link */
    unlink (fName);
    /* open the file */
    FILE *fp = fopen (fName, "wb");
    if (fp == NULL)
        return false;

    png_structp png = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png)
    {
    error1:
        fclose (fp);
        return false;
    }

    /* Allocate/initialize the image information data. */
    png_infop info = png_create_info_struct (png);
    if (info == NULL)
    {
    error2:
        png_destroy_write_struct (&png, &info);
        goto error1;
    }

    /* Catch processing errors */
    if (setjmp(png_jmpbuf(png)))
        /* If we get here, we had a problem writing the file */
        goto error2;

    /* Set up the output function */
    png_init_io (png, fp);

    /* Set the image information here.  Width and height are up to 2^31,
     * bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
     * the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
     * PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
     * or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
     * PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
     * currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
     */
    int colortype, rowlen, bits;
    colortype = PNG_COLOR_TYPE_RGB_ALPHA;
    rowlen = width * sizeof (RGBpixel);
    bits = 8;

    png_set_IHDR (png, info, width, height, bits, colortype,
                  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    /* if we are dealing with a color image then */
    png_color_8 sig_bit;
    memset (&sig_bit, 0, sizeof (sig_bit));
    sig_bit.red = 8;
    sig_bit.green = 8;
    sig_bit.blue = 8;

    /* if the image has an alpha channel then */
    if (colortype & PNG_COLOR_MASK_ALPHA)
        sig_bit.alpha = bits;
    png_set_sBIT (png, info, &sig_bit);

    /* Write the file header information. */
    png_write_info (png, info);

    /* Get rid of filler (OR ALPHA) bytes, pack XRGB/RGBX/ARGB/RGBA into
     * RGB (4 channels -> 3 channels). The second parameter is not used.
     */
    if (!(colortype & PNG_COLOR_MASK_ALPHA))
        png_set_filler (png, 0, PNG_FILLER_AFTER);

    /* The easiest way to write the image (you may have a different memory
     * layout, however, so choose what fits your needs best).  You need to
     * use the first method if you aren't handling interlacing yourself.
     */
    png_bytep *row_pointers = new png_bytep [height];
    unsigned char *ImageData = (unsigned char *)image;
    for (unsigned i = 0; i < height; i++)
        row_pointers [i] = ImageData + i * rowlen;

    /* One of the following output methods is REQUIRED */
    png_write_image (png, row_pointers);

    /* It is REQUIRED to call this to finish writing the rest of the file */
    png_write_end (png, info);

    /* clean up after the write, and free any memory allocated */
    png_destroy_write_struct (&png, &info);

    /* Free the row pointers */
    delete [] row_pointers;

    /* close the file */
    fclose (fp);

    /* that's it */
    return true;
}

void Image::Resize (unsigned newwidth, unsigned newheight)
{
    Free ();
    image = new RGBpixel [(width = newwidth) * (height = newheight)];
}

void Image::InitInterpolation (InterpolationMethod method)
{
    switch (method)
    {
        case I_NEAREST:
            fGetR = GetR_n;
            fGetG = GetG_n;
            fGetB = GetB_n;
            fGet = Get_n;
            break;
        case I_BILINEAR:
            fGetR = GetR_b;
            fGetG = GetG_b;
            fGetB = GetB_b;
            fGet = Get_b;
            break;
        case I_LANCZOS:
            fGetR = GetR_l;
            fGetG = GetG_l;
            fGetB = GetB_l;
            fGet = Get_l;

            if (!lanczos_func)
            {
                // Precompute the function for faster interpolation
                lanczos_func = new float [LANCZOS_SUPPORT * LANCZOS_SUPPORT * LANCZOS_TABLE_RES];
                for (int i = 0; i < LANCZOS_SUPPORT * LANCZOS_SUPPORT * LANCZOS_TABLE_RES; i++)
                {
                    float d = sqrt (float (i) / LANCZOS_TABLE_RES);
                    if (d == 0.0)
                        lanczos_func [i] = 1.0;
                    else
                        lanczos_func [i] =
                            (LANCZOS_SUPPORT * sin (M_PI * d) *
                             sin ((M_PI / LANCZOS_SUPPORT) * d)) /
                            (M_PI * M_PI * d * d);
                }
            }
            if (!lanczos_func_in_use)
            {
                lanczos_func_in_use = true;
                lanczos_func_use++;
            }
            break;
    }
}

// --- // Nearest interpolation // --- //

unsigned char Image::GetR_n (Image *This, float x, float y)
{
    unsigned xi = unsigned (x + 0.5);
    unsigned yi = unsigned (y + 0.5);
    if (xi >= This->width || yi >= This->height)
        return 0;

    RGBpixel *p = This->image + yi * This->width + xi;
    return p->red;
}

unsigned char Image::GetG_n (Image *This, float x, float y)
{
    unsigned xi = unsigned (x + 0.5);
    unsigned yi = unsigned (y + 0.5);
    if (xi >= This->width || yi >= This->height)
        return 0;

    RGBpixel *p = This->image + yi * This->width + xi;
    return p->green;
}

unsigned char Image::GetB_n (Image *This, float x, float y)
{
    unsigned xi = unsigned (x + 0.5);
    unsigned yi = unsigned (y + 0.5);
    if (xi >= This->width || yi >= This->height)
        return 0;

    RGBpixel *p = This->image + yi * This->width + xi;
    return p->blue;
}

void Image::Get_n (Image *This, RGBpixel &out, float x, float y)
{
    unsigned xi = unsigned (x + 0.5);
    unsigned yi = unsigned (y + 0.5);
    if (xi >= This->width || yi >= This->height)
        return;

    RGBpixel *p = This->image + yi * This->width + xi;
    out = *p;
}


// --- // Bi-linear interpolation // --- //

unsigned char Image::GetR_b (Image *This, float x, float y)
{
    // linear interpolation
    unsigned xi = unsigned (x);
    unsigned yi = unsigned (y);
    if (xi >= This->width || yi >= This->height)
        return 0;

    unsigned dx = unsigned ((x - trunc (x)) * 256);
    unsigned dy = unsigned ((y - trunc (y)) * 256);

    RGBpixel *p0 = This->image + yi * This->width + xi;
    RGBpixel *p1 = p0 + This->width;

    unsigned k1, k2;
    k1 = 256 * p0 [0].red   + dx * (int (p0 [1].red  ) - int (p0 [0].red  ));
    k2 = 256 * p1 [0].red   + dx * (int (p1 [1].red  ) - int (p1 [0].red  ));
    return (256 * k1 + dy * (k2 - k1)) >> 16;
}

unsigned char Image::GetG_b (Image *This, float x, float y)
{
    // linear interpolation
    unsigned xi = int (x);
    unsigned yi = int (y);
    if (xi >= This->width || yi >= This->height)
        return 0;

    unsigned dx = unsigned ((x - trunc (x)) * 256);
    unsigned dy = unsigned ((y - trunc (y)) * 256);

    RGBpixel *p0 = This->image + yi * This->width + xi;
    RGBpixel *p1 = p0 + This->width;

    unsigned k1, k2;
    k1 = 256 * p0 [0].green + dx * (int (p0 [1].green) - int (p0 [0].green));
    k2 = 256 * p1 [0].green + dx * (int (p1 [1].green) - int (p1 [0].green));
    return (256 * k1 + dy * (k2 - k1)) >> 16;
}

unsigned char Image::GetB_b (Image *This, float x, float y)
{
    // linear interpolation
    unsigned xi = int (x);
    unsigned yi = int (y);
    if (xi >= This->width || yi >= This->height)
        return 0;

    unsigned dx = unsigned ((x - trunc (x)) * 256);
    unsigned dy = unsigned ((y - trunc (y)) * 256);

    RGBpixel *p0 = This->image + yi * This->width + xi;
    RGBpixel *p1 = p0 + This->width;

    unsigned k1, k2;
    k1 = 256 * p0 [0].blue  + dx * (int (p0 [1].blue ) - int (p0 [0].blue ));
    k2 = 256 * p1 [0].blue  + dx * (int (p1 [1].blue ) - int (p1 [0].blue ));
    return (256 * k1 + dy * (k2 - k1)) >> 16;
}

void Image::Get_b (Image *This, RGBpixel &out, float x, float y)
{
    // linear interpolation
    unsigned xi = unsigned (x);
    unsigned yi = unsigned (y);
    if (xi >= This->width || yi >= This->height)
    {
        out.red = out.green = out.blue = 0;
        return;
    }

    unsigned dx = unsigned ((x - trunc (x)) * 256);
    unsigned dy = unsigned ((y - trunc (y)) * 256);

    RGBpixel *p0 = This->image + yi * This->width + xi;
    RGBpixel *p1 = p0 + This->width;

    unsigned k1, k2;

    k1 = 256 * p0 [0].red   + dx * (int (p0 [1].red  ) - int (p0 [0].red  ));
    k2 = 256 * p1 [0].red   + dx * (int (p1 [1].red  ) - int (p1 [0].red  ));
    out.red = (256 * k1 + dy * (k2 - k1)) >> 16;

    k1 = 256 * p0 [0].green + dx * (int (p0 [1].green) - int (p0 [0].green));
    k2 = 256 * p1 [0].green + dx * (int (p1 [1].green) - int (p1 [0].green));
    out.green = (256 * k1 + dy * (k2 - k1)) >> 16;

    k1 = 256 * p0 [0].blue  + dx * (int (p0 [1].blue ) - int (p0 [0].blue ));
    k2 = 256 * p1 [0].blue  + dx * (int (p1 [1].blue ) - int (p1 [0].blue ));
    out.blue = (256 * k1 + dy * (k2 - k1)) >> 16;
}

// --- // Lanczos 2 interpolation // --- //

unsigned char Image::GetR_l (Image *This, float x, float y)
{
    float xs = rint (x) - LANCZOS_SUPPORT;
    float ys = rint (y) - LANCZOS_SUPPORT;
    float xe = xs + LANCZOS_SUPPORT * 2;
    float ye = ys + LANCZOS_SUPPORT * 2;

    float norm = 0.0;
    float sum = 0.0;
    RGBpixel *img = This->image + (long (ys) * This->width + long (xs));

    if (xs >= 0 && ys >= 0 && xe < This->width && ye < This->height)
        for (; ys <= ye; ys += 1.0)
        {
            for (float xc = xs; xc <= xe; xc += 1.0, img++)
            {
                float d = square (x - xc) + square (y - ys);
                if (d >= LANCZOS_SUPPORT * LANCZOS_SUPPORT)
                    continue;

                d = lanczos_func [int (d * LANCZOS_TABLE_RES)];
                norm += d;
                sum += d * img->red;
            }
            img += This->width - LANCZOS_SUPPORT * 2 - 1;
        }
    else
    {
        for (; ys <= ye; ys += 1.0)
        {
            if (ys < 0 || ys >= This->height)
            {
                img += This->width;
                continue;
            }

            for (float xc = xs; xc <= xe; xc += 1.0, img++)
            {
                if (xc < 0 || xc >= This->width)
                    continue;

                float d = square (x - xc) + square (y - ys);
                if (d >= LANCZOS_SUPPORT * LANCZOS_SUPPORT)
                    continue;

                d = lanczos_func [int (d * LANCZOS_TABLE_RES)];
                norm += d;
                sum += d * img->red;
            }
            img += This->width - LANCZOS_SUPPORT * 2 - 1;
        }
        if (norm == 0.0)
            return 0;
    }

    int r = int (sum / norm);
    return r > 255 ? 255 : r < 0 ? 0 : r;
}

unsigned char Image::GetG_l (Image *This, float x, float y)
{
    float xs = rint (x) - LANCZOS_SUPPORT;
    float ys = rint (y) - LANCZOS_SUPPORT;
    float xe = xs + LANCZOS_SUPPORT * 2;
    float ye = ys + LANCZOS_SUPPORT * 2;

    float norm = 0.0;
    float sum = 0.0;
    RGBpixel *img = This->image + (long (ys) * This->width + long (xs));

    if (xs >= 0 && ys >= 0 && xe < This->width && ye < This->height)
        for (; ys <= ye; ys += 1.0)
        {
            for (float xc = xs; xc <= xe; xc += 1.0, img++)
            {
                float d = square (x - xc) + square (y - ys);
                if (d >= LANCZOS_SUPPORT * LANCZOS_SUPPORT)
                    continue;

                d = lanczos_func [int (d * LANCZOS_TABLE_RES)];
                norm += d;
                sum += d * img->green;
            }
            img += This->width - LANCZOS_SUPPORT * 2 - 1;
        }
    else
    {
        for (; ys <= ye; ys += 1.0)
        {
            if (ys < 0 || ys >= This->height)
            {
                img += This->width;
                continue;
            }

            for (float xc = xs; xc <= xe; xc += 1.0, img++)
            {
                if (xc < 0 || xc >= This->width)
                    continue;

                float d = square (x - xc) + square (y - ys);
                if (d >= LANCZOS_SUPPORT * LANCZOS_SUPPORT)
                    continue;

                d = lanczos_func [int (d * LANCZOS_TABLE_RES)];
                norm += d;
                sum += d * img->green;
            }
            img += This->width - LANCZOS_SUPPORT * 2 - 1;
        }
        if (norm == 0.0)
            return 0;
    }

    int r = int (sum / norm);
    return r > 255 ? 255 : r < 0 ? 0 : r;
}

unsigned char Image::GetB_l (Image *This, float x, float y)
{
    float xs = rint (x) - LANCZOS_SUPPORT;
    float ys = rint (y) - LANCZOS_SUPPORT;
    float xe = xs + LANCZOS_SUPPORT * 2;
    float ye = ys + LANCZOS_SUPPORT * 2;

    float norm = 0.0;
    float sum = 0.0;
    RGBpixel *img = This->image + (long (ys) * This->width + long (xs));

    if (xs >= 0 && ys >= 0 && xe < This->width && ye < This->height)
        for (; ys <= ye; ys += 1.0)
        {
            for (float xc = xs; xc <= xe; xc += 1.0, img++)
            {
                float d = square (x - xc) + square (y - ys);
                if (d >= LANCZOS_SUPPORT * LANCZOS_SUPPORT)
                    continue;

                d = lanczos_func [int (d * LANCZOS_TABLE_RES)];
                norm += d;
                sum += d * img->blue;
            }
            img += This->width - LANCZOS_SUPPORT * 2 - 1;
        }
    else
    {
        for (; ys <= ye; ys += 1.0)
        {
            if (ys < 0 || ys >= This->height)
            {
                img += This->width;
                continue;
            }

            for (float xc = xs; xc <= xe; xc += 1.0, img++)
            {
                if (xc < 0 || xc >= This->width)
                    continue;

                float d = square (x - xc) + square (y - ys);
                if (d >= LANCZOS_SUPPORT * LANCZOS_SUPPORT)
                    continue;

                d = lanczos_func [int (d * LANCZOS_TABLE_RES)];
                norm += d;
                sum += d * img->blue;
            }
            img += This->width - LANCZOS_SUPPORT * 2 - 1;
        }
        if (norm == 0.0)
            return 0;
    }

    int r = int (sum / norm);
    return r > 255 ? 255 : r < 0 ? 0 : r;
}

void Image::Get_l (Image *This, RGBpixel &out, float x, float y)
{
    float xs = rint (x) - LANCZOS_SUPPORT;
    float ys = rint (y) - LANCZOS_SUPPORT;
    float xe = xs + LANCZOS_SUPPORT * 2;
    float ye = ys + LANCZOS_SUPPORT * 2;

    float norm = 0.0;
    float sumR = 0.0;
    float sumG = 0.0;
    float sumB = 0.0;
    RGBpixel *img = This->image + (long (ys) * This->width + long (xs));

    if (xs >= 0 && ys >= 0 && xe < This->width && ye < This->height)
        for (; ys <= ye; ys += 1.0)
        {
            for (float xc = xs; xc <= xe; xc += 1.0, img++)
            {
                float d = square (x - xc) + square (y - ys);
                if (d >= LANCZOS_SUPPORT * LANCZOS_SUPPORT)
                    continue;

                d = lanczos_func [int (d * LANCZOS_TABLE_RES)];
                norm += d;
                sumR += d * img->red;
                sumG += d * img->green;
                sumB += d * img->blue;
            }
            img += This->width - LANCZOS_SUPPORT * 2 - 1;
        }
    else
    {
        for (; ys <= ye; ys += 1.0)
        {
            if (ys < 0 || ys >= This->height)
            {
                img += This->width;
                continue;
            }

            for (float xc = xs; xc <= xe; xc += 1.0, img++)
            {
                if (xc < 0 || xc >= This->width)
                    continue;

                float d = square (x - xc) + square (y - ys);
                if (d >= LANCZOS_SUPPORT * LANCZOS_SUPPORT)
                    continue;

                d = lanczos_func [int (d * LANCZOS_TABLE_RES)];
                norm += d;
                sumR += d * img->red;
                sumG += d * img->green;
                sumB += d * img->blue;
            }
            img += This->width - LANCZOS_SUPPORT * 2 - 1;
        }
        if (norm == 0.0)
        {
            out.Set (0, 0, 0);
            return;
        }
    }

    int rR = int (sumR / norm);
    int rG = int (sumG / norm);
    int rB = int (sumB / norm);
    out.Set (rR > 255 ? 255 : rR < 0 ? 0 : rR,
             rG > 255 ? 255 : rG < 0 ? 0 : rG,
             rB > 255 ? 255 : rB < 0 ? 0 : rB);
}
