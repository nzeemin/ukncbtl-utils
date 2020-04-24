/*  This file is part of UKNCBTL.
UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Sav2Cart.h

#include <stdint.h>


//////////////////////////////////////////////////////////////////////
// Loaders.cpp

extern uint16_t const loader[];
extern size_t const loaderSize;

extern uint16_t const loaderRLE[];
extern size_t const loaderRLESize;

extern uint16_t const loaderLZSS[];
extern size_t const loaderLZSSSize;

extern uint16_t const loaderLZ4[];
extern size_t const loaderLZ4Size;

extern uint16_t const loaderLZSA1[];
extern size_t const loaderLZSA1Size;

extern uint16_t const loaderLZSA2[];
extern size_t const loaderLZSA2Size;


//////////////////////////////////////////////////////////////////////
// LZSS.cpp

size_t lzss_encode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize);

size_t lzss_decode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize);


//////////////////////////////////////////////////////////////////////
// LZ4.cpp

size_t lz4_encode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize);

size_t lz4_decode(unsigned char *src, size_t insize, unsigned char *dst, size_t outsize);


//////////////////////////////////////////////////////////////////////
