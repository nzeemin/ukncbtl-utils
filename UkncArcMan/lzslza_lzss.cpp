/**************************************************************

Based on LZSS.C -- A Data Compression Program -- by Haruhiko Okumura

**************************************************************/


#include <stdlib.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>


// Memory-buffer LZSS decode for LZA (each input byte XORed with 0xFF).
// Returns number of bytes written to outbuf, or -1 on error (e.g. outbuf too small).
// inbuf: input buffer, inlen: input length
// outbuf: output buffer, outbuf_size: capacity of outbuf
// bufsize: ring buffer size (recommended power of two)
int uza_lzss_decode(
    const unsigned char* inbuf, size_t inlen,
    unsigned char* outbuf, size_t outbuf_size,
    int bufsize)
{
    const int N = bufsize;
    const int F = 18;
    const int THRESHOLD = 2;

    unsigned char* text_buf = (unsigned char*)malloc(N + F - 1);
    if (!text_buf) return -1;

    int i, j, k, r, c;
    unsigned int flags;
    size_t inpos = 0;
    size_t outpos = 0;

    for (i = 0; i < N - F; i++) text_buf[i] = ' ';
    r = N - F;  flags = 0;

    for (;;)
    {
        if (((flags >>= 1) & 256) == 0)
        {
            if (inpos >= inlen) break;
            c = inbuf[inpos++] ^ 0xFF;
            flags = c | 0xFF00;
        }
        if (flags & 1)
        {
            if (inpos >= inlen) break;
            c = inbuf[inpos++] ^ 0xFF;
            if (outpos >= outbuf_size) { free(text_buf); return -1; }
            outbuf[outpos++] = (unsigned char)c;
            text_buf[r++] = (unsigned char)c;
            r &= (N - 1);
        }
        else
        {
            if (inpos >= inlen) break;
            i = inbuf[inpos++] ^ 0xFF;
            if (inpos >= inlen) break;
            j = inbuf[inpos++] ^ 0xFF;
            i |= ((j & 0xF0) << 4);
            j = (j & 0x0F) + THRESHOLD;
            for (k = 0; k <= j; k++)
            {
                c = text_buf[(i + k) & (N - 1)];
                if (outpos >= outbuf_size) { free(text_buf); return -1; }
                outbuf[outpos++] = (unsigned char)c;
                text_buf[r++] = (unsigned char)c;
                r &= (N - 1);
            }
        }
    }

    free(text_buf);
    return (int)outpos;
}

// Memory-buffer LZSS decode for LZS (no XOR).
// Same return semantics as above.
int uzs_lzss_decode(
    const unsigned char* inbuf, size_t inlen,
    unsigned char* outbuf, size_t outbuf_size,
    int bufsize)
{
    const int N = bufsize;
    const int F = 18;
    const int THRESHOLD = 2;

    unsigned char* text_buf = (unsigned char*)malloc(N + F - 1);
    if (!text_buf) return -1;

    int i, j, k, r, c;
    unsigned int flags;
    size_t inpos = 0;
    size_t outpos = 0;

    for (i = 0; i < N - F; i++) text_buf[i] = ' ';
    r = N - F;  flags = 0;

    for (;;)
    {
        if (((flags >>= 1) & 256) == 0)
        {
            if (inpos >= inlen) break;
            c = inbuf[inpos++];
            flags = c | 0xFF00;
        }
        if (flags & 1)
        {
            if (inpos >= inlen) break;
            c = inbuf[inpos++];
            if (outpos >= outbuf_size) { free(text_buf); return -1; }
            outbuf[outpos++] = (unsigned char)c;
            text_buf[r++] = (unsigned char)c;
            r &= (N - 1);
        }
        else
        {
            if (inpos >= inlen) break;
            i = inbuf[inpos++];
            if (inpos >= inlen) break;
            j = inbuf[inpos++];
            i |= ((j & 0xF0) << 4);
            j = (j & 0x0F) + THRESHOLD;
            for (k = 0; k <= j; k++)
            {
                c = text_buf[(i + k) & (N - 1)];
                if (outpos >= outbuf_size) { free(text_buf); return -1; }
                outbuf[outpos++] = (unsigned char)c;
                text_buf[r++] = (unsigned char)c;
                r &= (N - 1);
            }
        }
    }

    free(text_buf);
    return (int)outpos;
}
