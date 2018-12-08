/*  This file is part of UKNCBTL.
UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// LZSS.cpp

/* LZSS encoder-decoder, flag bits grouped in bytes */

#include <stdio.h>
#include <stdlib.h>


#define EI 12
#define EJ  4
#define P   2  /* If match length <= P then output one character */
#define N (1 << EI)  /* buffer size */
#define F ((1 << EJ) + P)  /* lookahead buffer size */

int bit_buffer = 0, bit_mask = 1;
unsigned long codecount = 0, textcount = 0;
unsigned char buffer[N * 2];

unsigned char *inputbuffer = 0;
size_t inputsize, inputpos;
unsigned char *outputbuffer = 0;
size_t outputsize, outputpos, flagpos;


void error(void)
{
    printf("LZSS output error; inputpos %lu., outputpos %lu.\n", inputpos, outputpos);
    exit(1);
}

void putflagbit(int f)
{
    if (bit_mask == 1) // allocate byte for flags
    {
        if (outputpos >= outputsize) error();
        flagpos = outputpos;
        outputpos++;  codecount++;
    }
    if (f) bit_buffer |= bit_mask;
    if ((bit_mask <<= 1) == 256)  // save flags to allocated position
    {
        outputbuffer[flagpos] = bit_buffer;
        bit_buffer = 0;  bit_mask = 1;
    }
}

void flush_bit_buffer(void)
{
    if (bit_mask != 1)
    {
        outputbuffer[flagpos] = bit_buffer;
    }
}

void putbyte(int c)
{
    if (outputpos >= outputsize) error();
    outputbuffer[outputpos++] = c;  codecount++;
}

void output1(int c)
{
    putflagbit(0);
    //printf("LZSS 0x%04X output1 0x%02x\n", outputpos, c);
    putbyte(c);
}

void output2(int x, int y)
{
    putflagbit(1);
    int value = (y << 12) | x;
    //printf("LZSS 0x%04X output2 x=%d, y=%d, value=0x%04X\n", outputpos, x, y, value);
    putbyte((value >> 8) & 0xff);
    putbyte(value & 0xff);
}

unsigned long encode(void)
{
    codecount = 0;
    textcount = 0;
    flagpos = 0;  bit_buffer = 0;  bit_mask = 1;
    outputpos = 0;

    int i, j, r, s, bufferend, c;

    for (i = 0; i < N - F; i++) buffer[i] = ' ';
    for (i = N - F; i < N * 2; i++)
    {
        if (inputpos >= inputsize) break;
        c = inputbuffer[inputpos++];
        buffer[i] = c;  textcount++;
    }
    bufferend = i;  r = N - F;  s = 0;
    while (r < bufferend)
    {
        int f1 = (F <= bufferend - r) ? F : bufferend - r;
        int x = 0;  int y = 1;  c = buffer[r];
        for (i = r - 1; i >= s; i--)
            if (buffer[i] == c)
            {
                for (j = 1; j < f1; j++)
                    if (buffer[i + j] != buffer[r + j]) break;
                if (j > y)
                {
                    x = i;  y = j;
                }
            }
        if (y <= P)
        {
            y = 1; output1(c);
        }
        else
            output2((r - x) & (N - 1), y - P - 1);
        r += y;  s += y;
        if (r >= N * 2 - F)
        {
            for (i = 0; i < N; i++) buffer[i] = buffer[i + N];
            bufferend -= N;  r -= N;  s -= N;
            while (bufferend < N * 2)
            {
                if (inputpos >= inputsize) break;
                c = inputbuffer[inputpos++];
                buffer[bufferend++] = c;  textcount++;
            }
        }
    }
    flush_bit_buffer();

    //printf("LZSS inputpos %ld., outputpos %ld.\n", inputpos, outputpos);
    printf("LZSS input size %lu. bytes\n", textcount);
    printf("LZSS output size %lu. bytes (%1.2f %%)\n", codecount, codecount * 100.0 / textcount);

    return codecount;
}

unsigned long decode(void)
{
    textcount = 0;

    int i, j, k, r, c;

    for (i = 0; i < N - F; i++) buffer[i] = ' ';
    r = N - F;
    while (1)
    {
        if (inputpos >= inputsize) break;
        int flags = inputbuffer[inputpos++];
        for (int b = 0; b < 8; b++)
        {
            if ((flags & 1) == 0)
            {
                if (inputpos >= inputsize) break;
                c = inputbuffer[inputpos++];
                outputbuffer[outputpos++] = c;  textcount++;
                buffer[r++] = c;  r &= (N - 1);
            }
            else
            {
                if (inputpos >= inputsize) break;
                int value = (inputbuffer[inputpos++] << 8);
                if (inputpos >= inputsize) break;
                value = value | inputbuffer[inputpos++];
                i = value & 0xfff;  // 12 bits
                j = (value >> 12);  // 4 bits
                for (k = 0; k <= j + P; k++)
                {
                    c = buffer[(r - i) & (N - 1)];
                    if (outputpos >= outputsize) error();
                    outputbuffer[outputpos++] = c;  textcount++;
                    buffer[r++] = c;  r &= (N - 1);
                }
            }

            flags = flags >> 1;
        }
    }

    //printf("LZSS decode inputpos %ld., outputpos %ld.\n", inputpos, outputpos);

    return textcount;
}

size_t lzss_encode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize)
{
    inputbuffer = inbuffer;
    inputsize = insize;
    inputpos = 0;
    outputbuffer = outbuffer;
    outputsize = outsize;

    unsigned long result = encode();

    inputbuffer = 0;
    outputbuffer = 0;

    return (size_t) result;
}

size_t lzss_decode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize)
{
    inputbuffer = inbuffer;
    inputsize = insize;
    inputpos = 0;
    outputbuffer = outbuffer;
    outputsize = outsize;
    outputpos = 0;

    unsigned long result = decode();

    inputbuffer = 0;
    outputbuffer = 0;

    return result;
}
