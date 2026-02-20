/**************************************************************
 Huffman decoder for FCU archiver format, based on:
   lzhuf encoder/decoder class by cz6don, based on:
     lzhuf.c
     written by Haruyasu Yoshizaki 11/20/1988
     some minor changes 4/6/1989
     comments translated by Haruhiko Okumura 4/7/1989
     Back to ansi C rewritten by OK2JBG
**************************************************************/

#include <cstdint>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>

//#include "lzhuf.h"

int16_t ReadChar();
void InitTree();
int16_t GetBit();
int16_t GetByte();
void StartHuff();
void reconst();
void update(int16_t c);
int  DecodeChar();
int  DecodePosition();

uint16_t *freq;     // frequency table
uint16_t *prnt;     // pointers to parent nodes, except for the
// elements [T..T + N_CHAR - 1] which are used to get
// the positions of leaves corresponding to the codes.
uint16_t *son;      // pointers to child nodes (son[], son[] + 1)

uint16_t getbuf, putbuf;
uint8_t getlen, putlen;

uint8_t *text_buf;
uint16_t match_position, match_length, *lson, *rson, *dad;

uint8_t *dataOut, *input;
uint32_t length, inPos, outPos, outLen;


// table for encoding and decoding the upper 6 bits of position
// for decoding
uint8_t d_code[256] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
    0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A, 0x0A,
    0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B, 0x0B,
    0x0C, 0x0C, 0x0C, 0x0C, 0x0D, 0x0D, 0x0D, 0x0D,
    0x0E, 0x0E, 0x0E, 0x0E, 0x0F, 0x0F, 0x0F, 0x0F,
    0x10, 0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11,
    0x12, 0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13,
    0x14, 0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15,
    0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x19, 0x19, 0x1A, 0x1A, 0x1B, 0x1B,
    0x1C, 0x1C, 0x1D, 0x1D, 0x1E, 0x1E, 0x1F, 0x1F,
    0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23,
    0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
    0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B,
    0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
};

uint8_t d_len[256] =
{
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
};


// LZSS compression

#define N               4096        // buffer size
#define F               60          // lookahead buffer size
#define THRESHOLD       2
const uint16_t NIL =    0xFFFE;    // leaf of tree

// Huffman coding

#define N_CHAR          (256 - THRESHOLD + F)
// kinds of characters (character code = 0..N_CHAR-1)
#define T               (N_CHAR * 2 - 1)        // size of table
#define R               (T - 1)     // position of root
#define MAX_FREQ        0x8000      // updates tree when the root frequency comes to this value.

#define ALLOC_STEP 1024


void lzhuf_init()
{
    dataOut = NULL;
    input = NULL;
    length = 0;
    inPos = outPos = 0;

    freq = (uint16_t*)calloc(T + 1, sizeof(uint16_t));
    prnt = (uint16_t*)calloc(T + N_CHAR, sizeof(uint16_t));
    son = (uint16_t*)calloc(T, sizeof(uint16_t));
    text_buf = (uint8_t*)calloc(N + F - 1, sizeof(uint8_t));
    lson = (uint16_t*)calloc(N + 1, sizeof(uint16_t));
    rson = (uint16_t*)calloc(N + 257, sizeof(uint16_t));
    dad = (uint16_t*)calloc(N + 1, sizeof(uint16_t));
}

void lzhuf_free()
{
    //NOTE: We not freeing dataOut: it's user's code responsibility

    free(freq);
    free(prnt);
    free(son);
    free(text_buf);
    free(lson);
    free(rson);
    free(dad);
}

int16_t ReadChar()
{
    if (inPos < outLen)
    {
        return input[inPos++];
    }
    return -1;
}

void InitTree()  // initialize trees
{
    for (int16_t i = N + 1; i <= N + 256; i++)
        rson[i] = NIL;                  // root
    for (int16_t i = 0; i < N; i++)
        dad[i] = NIL;                   // node
}

// FCU PDP-11 compatible bit reader
// - reads 16-bit little-endian words
// - emits bits MSB first from each word
int16_t GetBit()
{
    if (getlen == 0)
    {
        int lo = ReadChar();
        int hi = ReadChar();

        if (lo < 0) lo = 0;
        if (hi < 0) hi = 0;

        getbuf = (uint16_t)(lo | (hi << 8));

        getlen = 16;
    }

    int16_t bit = (getbuf >> 15) & 1;  // take MSB
    getbuf <<= 1;
    getlen--;

    return bit;
}
int16_t GetByte()
{
    uint16_t value = 0;
    for (int i = 0; i < 8; i++)
    {
        value = (value << 1) | GetBit();
    }

    return value;
}

// initialization of tree
void StartHuff()
{
    int16_t i, j;

    for (i = 0; i < N_CHAR; i++)
    {
        freq[i] = 1;
        son[i] = i + T;
        prnt[i + T] = i;
    }
    i = 0; j = N_CHAR;
    while (j <= R)
    {
        freq[j] = freq[i] + freq[i + 1];
        son[j] = i;
        prnt[i] = prnt[i + 1] = j;
        i += 2; j++;
    }
    freq[T] = 0xffff;
    prnt[R] = 0;
}

// reconstruction of tree
void reconst()
{
    int16_t i, k;

    // collect leaf nodes in the first half of the table
    // and replace the freq by (freq + 1) / 2.
    int16_t j = 0;
    for (i = 0; i < T; i++)
    {
        if (son[i] >= T)
        {
            freq[j] = (freq[i] + 1) / 2;
            son[j] = son[i];
            j++;
        }
    }
    // begin constructing tree by connecting sons
    for (i = 0, j = N_CHAR; j < T; i += 2, j++)
    {
        k = i + 1;
        uint16_t f = freq[j] = freq[i] + freq[k];
        for (k = j - 1; f < freq[k]; k--);
        k++;
        uint16_t l = (j - k) * 2;
        memmove(&freq[k + 1], &freq[k], l);
        freq[k] = f;
        memmove(&son[k + 1], &son[k], l);
        son[k] = i;
    }
    // connect prnt
    for (i = 0; i < T; i++)
    {
        if ((k = son[i]) >= T)
        {
            prnt[k] = i;
        }
        else
        {
            prnt[k] = prnt[k + 1] = i;
        }
    }
}

// increment frequency of given code by one, and update tree
void update(int16_t c)
{
    if (freq[R] == MAX_FREQ)
        reconst();

    c = prnt[c + T];
    do
    {
        uint16_t k = ++freq[c];

        // if the order is disturbed, exchange nodes
        int16_t l = c + 1;
        if (k > freq[l])
        {
            while (k > freq[++l]);
            l--;
            freq[c] = freq[l];
            freq[l] = k;

            int16_t i = son[c];
            prnt[i] = l;
            if (i < T) prnt[i + 1] = l;

            int16_t j = son[l];
            son[l] = i;

            prnt[j] = c;
            if (j < T) prnt[j + 1] = c;
            son[c] = j;

            c = l;
        }
    }
    while ((c = prnt[c]) != 0);  // repeat up to root
}

int DecodeChar()
{
    uint16_t c = son[R];

    // travel from root to leaf,
    // choosing the smaller child node (son[]) if the read bit is 0,
    // the bigger (son[]+1} if 1
    while (c < T)
    {
        c += GetBit();
        c = son[c];
    }
    c -= T;
    update(c);
    return c;
}

int DecodePosition()
{
    // recover upper 6 bits from table
    uint16_t i = GetByte();
    uint16_t c = ((uint16_t)d_code[i]) << 6;
    uint16_t j = d_len[i];

    // read lower 6 bits verbatim
    j -= 2;
    while (j--)
    {
        i <<= 1;
        i &= 0xFFFF;
        i |= GetBit();
    }
    return c | (i & 0x3f);
}


void lzhuf_decode(uint32_t len, void *data, uint32_t plen)  // decompression
{
    uint8_t *ptr = (uint8_t*)&length;
    int16_t  i, j, k, r, c;
    uint32_t count;

    getbuf = putbuf = 0;
    getlen = putlen = 0;

    outLen = plen;
    length = len;
    if (dataOut)
        free(dataOut);
    dataOut = (uint8_t*)calloc(length, sizeof(uint8_t));
    inPos = outPos = 0;
    input = (uint8_t*)data;

    StartHuff();
    for (i = 0; i < N - F; i++)
        text_buf[i] = ' ';
    r = N - F;
    for (count = 0; count < length; )
    {
        c = DecodeChar();
        if (c < 256)
        {
            //std::cout << "DecodeChar " << std::dec << count << " " << std::hex << c << std::dec << std::endl;
            dataOut[outPos++] = (uint8_t)c;
            text_buf[r++] = (uint8_t)c;
            r &= (N - 1);
            count++;
        }
        else
        {
            //if (count == 3351) _CrtDbgBreak();
            int decpos = DecodePosition();
            //std::cout << "DecodeChar " << std::dec << count << " " << std::hex << c << " " << decpos << std::dec << std::endl;
            i = (r - decpos - 1) & (N - 1);
            j = c - 255 + THRESHOLD;
            for (k = 0; k < j; k++)
            {
                c = text_buf[(i + k) & (N - 1)];
                if (outPos >= length)  // should not be, but possible with wrong length
                    break;
                dataOut[outPos++] = (uint8_t)c;
                text_buf[r++] = (uint8_t)c;
                r &= (N - 1);
                count++;
            }
        }

        //if (count > 1761) break;
    }
}

void* lzhuf_data()
{
    return (void*)dataOut;
}

uint32_t lzhuf_length()
{
    return length;
}
