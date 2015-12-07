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

#ifndef __RGBPIXEL_H__
#define __RGBPIXEL_H__

#ifdef HAVE_ENDIAN_H
#  include <endian.h>
#endif

#ifndef __LITTLE_ENDIAN
#  define __LITTLE_ENDIAN 1234
#endif
#ifndef __BIG_ENDIAN
#  define __BIG_ENDIAN 4321
#endif
#if !defined __BYTE_ORDER
#  if defined _HOST_BIG_ENDIAN || defined __BIG_ENDIAN__ || defined WORDS_BIGENDIAN || defined __sgi__ || defined __sgi || defined __powerpc__ || defined sparc || defined __ppc__ || defined __s390__ || defined __s390x__
#    define __BYTE_ORDER __BIG_ENDIAN
#  else
#    define __BYTE_ORDER __LITTLE_ENDIAN
#  endif
#else
#  define __BYTE_ORDER __LITTLE_ENDIAN
#endif

// For optimized performance, we sometimes handle all R/G/B values simultaneously
#if __BYTE_ORDER == __BIG_ENDIAN
#  define RGB_MASK 0xffffff00
#else
#  define RGB_MASK 0x00ffffff
#endif

/**
 * An RGB pixel. Besides R,G,B color components this structure also
 * contains the Alpha channel component, which is used in images
 * (that potentially have an alpha channel).
 */
struct RGBpixel
{
  /// The red, green, blue and alpha components
  unsigned char red, green, blue, alpha;
  /// Constructor (initialize to zero, alpha to 255)
  RGBpixel () /* : red(0), green(0), blue(0), alpha(255) {} */
  { *(unsigned *)this = (unsigned)~RGB_MASK; }
  /// Copy constructor
  RGBpixel (const RGBpixel& p)
  /* : red (p.red), green (p.green), blue (p.blue), alpha (p.alpha) {} */
  { *(unsigned *)this = *(unsigned *)&p; }
  /// Initialize the pixel with some R/G/B value
  RGBpixel (int r, int g, int b) :
    red (r), green (g), blue (b), alpha (255) {}
  /// Compare with an RGBpixel (including alpha value)
  bool operator == (const RGBpixel& p) const
  /* { return (p.red == red) && (p.green == green) && (p.blue == blue); } */
  { return *(unsigned *)this == *(unsigned *)&p; }
  /// Check if the RGBpixel is not equal to another RGBpixel (including alpha)
  bool operator != (const RGBpixel& p) const
  { return !operator == (p); }
  /// Compare with another RGBpixel, but don't take alpha into account
  bool eq (const RGBpixel& p) const
  { return ((*(unsigned *)this) & RGB_MASK) == ((*(unsigned *)&p) & RGB_MASK); }
  /// Get the pixel intensity
  int Intensity ()
  { return (red + green + blue) / 3; }
  /// Assign given red/green/blue values to this pixel
  void Set (const int r, const int g, const int b)
  { red = r; green = g; blue = b; alpha = 255; }
  /// Assign given red/green/blue/alpha values to this pixel
  void Set (const int r, const int g, const int b, const int a)
  { red = r; green = g; blue = b; alpha = a; }
  void Set (const RGBpixel& p)
  /* : red (p.red), green (p.green), blue (p.blue), alpha (p.alpha) {} */
  { *(unsigned *)this = *(unsigned *)&p; }
};

// We don't need RGB_MASK anymore
#undef RGB_MASK

/**
 * Eye sensivity to different color components, from NTSC grayscale equation.
 * The coefficients are multiplied by 100 and rounded towards nearest integer,
 * to facilitate integer math. The squared coefficients are also multiplied
 * by 100 and rounded to nearest integer (thus 173 == 1.73, 242 == 2.42 etc).
 */
/// Red component sensivity
#define R_COEF		173
/// Green component sensivity
#define G_COEF		242
/// Blue component sensivity
#define B_COEF		107

#endif // RGBPIXEL_H__
