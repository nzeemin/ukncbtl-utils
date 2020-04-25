/*  This file is part of UKNCBTL.
UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// LZSA.cpp

#include "lzsa/lib.h"
#include "lzsa/shrink_inmem.h"


//////////////////////////////////////////////////////////////////////

size_t lzsa1_encode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize)
{
    const int nFormatVersion = 1;
    unsigned int nFlags = LZSA_FLAG_RAW_BLOCK | LZSA_FLAG_FAVOR_RATIO;
    size_t encodedSize = lzsa_compress_inmem(inbuffer, outbuffer, insize, outsize, nFlags, 3, nFormatVersion);
    return encodedSize;
}

size_t lzsa1_decode(unsigned char *src, size_t insize, unsigned char *dst, size_t outsize)
{
    int nFormatVersion = 1;
    unsigned int nFlags = LZSA_FLAG_RAW_BLOCK | LZSA_FLAG_FAVOR_RATIO;
    size_t decodedSize = lzsa_decompress_inmem(src, dst, insize, 65536, nFlags, &nFormatVersion);
    return decodedSize;
}

size_t lzsa2_encode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize)
{
    const int nFormatVersion = 2;
    unsigned int nFlags = LZSA_FLAG_RAW_BLOCK | LZSA_FLAG_FAVOR_RATIO;
    size_t encodedSize = lzsa_compress_inmem(inbuffer, outbuffer, insize, outsize, nFlags, 3, nFormatVersion);
    return encodedSize;
}

size_t lzsa2_decode(unsigned char *src, size_t insize, unsigned char *dst, size_t outsize)
{
    int nFormatVersion = 2;
    unsigned int nFlags = LZSA_FLAG_RAW_BLOCK | LZSA_FLAG_FAVOR_RATIO;
    size_t decodedSize = lzsa_decompress_inmem(src, dst, insize, 65536, nFlags, &nFormatVersion);
    return decodedSize;
}


//////////////////////////////////////////////////////////////////////
