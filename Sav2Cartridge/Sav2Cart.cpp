/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Sav2Cart.cpp

#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdint.h>

#ifndef WIN32
#include <string.h>
#define sprintf_s snprintf
#define _stricmp  strcasecmp
#endif

#ifdef _MSC_VER
#define OPTIONCHAR '/'
#define OPTIONSTR "/"
#else
#define OPTIONCHAR '-'
#define OPTIONSTR "-"
#endif

#include "Sav2Cart.h"
#include "lzsa/lib.h"
#include "lzsa/shrink_inmem.h"


//////////////////////////////////////////////////////////////////////


size_t EncodeRLE(const uint8_t * source, size_t sourceLength, uint8_t * buffer, size_t bufferLength)
{
    size_t destOffset = 0;
    size_t seqBlockOffset = 0;
    size_t seqBlockSize = 1;
    size_t varBlockOffset = 0;
    size_t varBlockSize = 1;
    uint8_t prevByte = source[0];
    size_t currOffset = 0;
    size_t codedSizeTotal = 0;
    while (currOffset < sourceLength)
    {
        currOffset++;
        uint8_t currByte = (currOffset < sourceLength) ? source[currOffset] : ~prevByte;

        if ((currOffset == sourceLength) ||
            (currByte != prevByte && seqBlockSize > 31) ||
            (currByte != prevByte && seqBlockSize > 1 && prevByte == 0) ||
            (currByte != prevByte && seqBlockSize > 1 && prevByte == 0xff) ||
            (seqBlockSize == 0x1fff || varBlockSize - seqBlockSize == 0x1fff))
        {
            if (varBlockOffset < seqBlockOffset)
            {
                size_t varSize = varBlockSize - seqBlockSize;
                if (currOffset == sourceLength && seqBlockSize < 2)
                    varSize = varBlockSize;  // Special case at the end of input stream
                size_t codedSize = varSize + ((varSize < 256 / 8) ? 1 : 2);
                //printf("RLE  at\t%06o\tVAR  %06o  %06o  %06o\t", varBlockOffset + 512, destOffset, varSize, codedSize);
                codedSizeTotal += codedSize;
                if (destOffset + codedSize < bufferLength)
                {
                    uint8_t flagByte = 0x40;
                    if (varSize < 256 / 8)
                    {
                        //printf("%02x ", (uint8_t)(flagByte | varSize));
                        buffer[destOffset++] = (uint8_t)(flagByte | varSize);
                    }
                    else
                    {
                        //printf("%02x ", (uint8_t)(0x80 | flagByte | ((varSize & 0x1f00) >> 8)));
                        buffer[destOffset++] = (uint8_t)(0x80 | flagByte | ((varSize & 0x1f00) >> 8));
                        //printf("%02x ", (uint8_t)(varSize & 0xff));
                        buffer[destOffset++] = (uint8_t)(varSize & 0xff);
                    }
                    for (size_t offset = varBlockOffset; offset < varBlockOffset + varSize; offset++)
                    {
                        //printf("%02x ", source[offset]);
                        buffer[destOffset++] = source[offset];
                    }
                }
                //printf("\n");
            }
            if ((varBlockOffset < seqBlockOffset && seqBlockSize > 1) ||
                (varBlockOffset == seqBlockOffset && varBlockSize == seqBlockSize))
            {
                size_t codedSize = ((seqBlockSize < 256 / 8) ? 1 : 2) + ((prevByte == 0 || prevByte == 255) ? 0 : 1);
                //printf("RLE  at\t%06o\tSEQ  %06o  %06o  %06o\t%02x\n", seqBlockOffset + 512, destOffset, seqBlockSize, codedSize, prevByte);
                codedSizeTotal += codedSize;
                if (destOffset + codedSize < bufferLength)
                {
                    uint8_t flagByte = ((prevByte == 0) ? 0 : ((prevByte == 255) ? 0x60 : 0x20));
                    if (seqBlockSize < 256 / 8)
                        buffer[destOffset++] = (uint8_t)(flagByte | seqBlockSize);
                    else
                    {
                        buffer[destOffset++] = (uint8_t)(0x80 | flagByte | ((seqBlockSize & 0x1f00) >> 8));
                        buffer[destOffset++] = (uint8_t)(seqBlockSize & 0xff);
                    }
                    if (prevByte != 0 && prevByte != 255)
                        buffer[destOffset++] = prevByte;
                }
            }

            seqBlockOffset = varBlockOffset = currOffset;
            seqBlockSize = varBlockSize = 1;

            prevByte = currByte;
            continue;
        }

        varBlockSize++;
        if (currByte == prevByte)
        {
            seqBlockSize++;
        }
        else
        {
            seqBlockSize = 1;  seqBlockOffset = currOffset;
        }

        prevByte = currByte;
    }

    //printf("RLE Source size: %d  Coded size: %d  Dest offset: %d\n", sourceLength, codedSizeTotal, destOffset);
    printf("RLE input size %lu. bytes\n", sourceLength);
    printf("RLE output size %lu. bytes (%1.2f %%)\n", codedSizeTotal, codedSizeTotal * 100.0 / sourceLength);
    return codedSizeTotal;
}

size_t DecodeRLE(const uint8_t * source, size_t sourceLength, uint8_t * buffer, size_t bufferLength)
{
    size_t currOffset = 0;
    size_t destOffset = 0;
    uint8_t filler = 0;
    while (currOffset < sourceLength)
    {
        uint8_t first = source[currOffset++];
        if (first == 0)
            break;
        size_t count = 0;
        if ((first & 0x80) == 0)  // 1-byte command
            count = first & 0x1f;
        else  // 2-byte command
            count = (((size_t)first & 0x1f) << 8) | source[currOffset++];
        switch (first & 0x60)
        {
        case 0x00:
            filler = 0;
            for (size_t i = 0; i < count; i++)
                buffer[destOffset++] = filler;
            break;
        case 0x60:
            filler = 0xff;
            for (size_t i = 0; i < count; i++)
                buffer[destOffset++] = filler;
            break;
        case 0x20:
            filler = source[currOffset++];
            for (size_t i = 0; i < count; i++)
                buffer[destOffset++] = filler;
            break;
        case 0x40:
            for (size_t i = 0; i < count; i++)
                buffer[destOffset++] = source[currOffset++];
            break;
        }
    }

    return destOffset;
}


//////////////////////////////////////////////////////////////////////


enum
{
    OPTION_COMPRESSION_NONE  = 0x0100,
    OPTION_COMPRESSION_RLE   = 0x0200,
    OPTION_COMPRESSION_LZSS  = 0x0400,
    OPTION_COMPRESSION_LZ4   = 0x0800,
    OPTION_COMPRESSION_LZSA1 = 0x1000,
    OPTION_COMPRESSION_LZSA2 = 0x2000,
    OPTION_COMPRESSION_MASK  = 0xff00
};

int options = 0;

char inputfilename[256] = { 0 };
char outputfilename[256] = { 0 };

FILE* inputfile;
FILE* outputfile;
uint8_t* pFileImage = NULL;
uint8_t* pCartImage = NULL;
uint16_t wStartAddr;
uint16_t wStackAddr;
uint16_t wTopAddr;

bool ParseCommandLine(int argc, char* argv[])
{
    for (int argn = 1; argn < argc; argn++)
    {
        const char* arg = argv[argn];
        if (arg[0] == OPTIONCHAR)
        {
            if (_stricmp(arg + 1, "none") == 0)
                options |= OPTION_COMPRESSION_NONE;
            else if (_stricmp(arg + 1, "rle") == 0)
                options |= OPTION_COMPRESSION_RLE;
            else if (_stricmp(arg + 1, "lzss") == 0)
                options |= OPTION_COMPRESSION_LZSS;
            else if (_stricmp(arg + 1, "lz4") == 0)
                options |= OPTION_COMPRESSION_LZ4;
            else if (_stricmp(arg + 1, "lzsa1") == 0)
                options |= OPTION_COMPRESSION_LZSA1;
            else if (_stricmp(arg + 1, "lzsa2") == 0)
                options |= OPTION_COMPRESSION_LZSA2;
            else
            {
                printf("Unknown option: %s\n", arg);
                return false;
            }
        }
        else
        {
            if (inputfilename[0] == 0)
                strcpy(inputfilename, arg);
            else if (outputfilename[0] == 0)
                strcpy(outputfilename, arg);
        }
    }

    if ((options & OPTION_COMPRESSION_MASK) == 0)
        options |= OPTION_COMPRESSION_MASK;  // Compression is not specified => try all of them

    // Validate options
    if (inputfilename[0] == 0 || outputfilename[0] == 0)
        return false;

    return true;
}

uint16_t CalcCheckum(const uint16_t* pData, int nWords)
{
    uint16_t wChecksum = 0;
    for (int i = 0; i < nWords; i++)
    {
        uint16_t src = wChecksum;
        uint16_t src2 = *pData;
        wChecksum += src2;
        if (((src & src2) | ((src ^ src2) & ~wChecksum)) & 0100000)  // if Carry
            wChecksum++;
        pData++;
    }
    return wChecksum;
}

int main(int argc, char* argv[])
{
    if (!ParseCommandLine(argc, argv))
    {
        printf(
            "Usage: Sav2Cart [options] <inputfile.SAV> <outputfile.BIN>\n"
            "Options:\n"
            "\t" OPTIONSTR "none  - try to fit non-compressed\n"
            "\t" OPTIONSTR "rle   - try RLE compression\n"
            "\t" OPTIONSTR "lzss  - try LZSS compression\n"
            "\t" OPTIONSTR "lz4   - try LZ4 compression\n"
            "\t" OPTIONSTR "lzsa1 - try LZSA1 compression\n"
            "\t" OPTIONSTR "lzsa2 - try LZSA2 compression\n"
            "\t(no compression options) - try all on-by-one until fit");
        return 255;
    }

    printf("Input file: %s\n", inputfilename);

    inputfile = fopen(inputfilename, "rb");
    if (inputfile == nullptr)
    {
        printf("Failed to open the input file (%d).", errno);
        return 255;
    }
    ::fseek(inputfile, 0, SEEK_END);
    uint32_t inputfileSize = ::ftell(inputfile);

    pFileImage = (uint8_t*) ::malloc(inputfileSize);

    ::fseek(inputfile, 0, SEEK_SET);
    size_t bytesRead = ::fread(pFileImage, 1, inputfileSize, inputfile);
    if (bytesRead != inputfileSize)
    {
        printf("Failed to read the input file.");
        return 255;
    }
    ::fclose(inputfile);
    printf("Input file size %u. bytes\n", inputfileSize);

    wStartAddr = *((uint16_t*)(pFileImage + 040));
    wStackAddr = *((uint16_t*)(pFileImage + 042));
    wTopAddr = *((uint16_t*)(pFileImage + 050));
    printf("SAV Start\t%06ho  %04x  %5d\n", wStartAddr, wStartAddr, wStartAddr);
    printf("SAV Stack\t%06ho  %04x  %5d\n", wStackAddr, wStackAddr, wStackAddr);
    printf("SAV Top  \t%06ho  %04x  %5d\n", wTopAddr, wTopAddr, wTopAddr);
    size_t savImageSize = ((size_t)wTopAddr + 2 - 01000);
    printf("SAV image size\t%06ho  %04lx  %5lu\n", (uint16_t)savImageSize, savImageSize, savImageSize);

    pCartImage = (uint8_t*) ::calloc(65536, 1);
    if (pCartImage == NULL)
    {
        printf("Failed to allocate memory.");
        return 255;
    }

    for (;;)  // breaking the block when we prepared the output image somehow
    {
        // Plain copy
        if (options & OPTION_COMPRESSION_NONE)
        {
            if (inputfileSize > 24576)
            {
                printf("Input file is too big for cartridge: %u. bytes, max 24576. bytes\n", inputfileSize);
            }
            else  // Copy SAV image as is, add loader
            {
                ::memcpy(pCartImage, pFileImage, inputfileSize);

                // Prepare the loader
                memcpy(pCartImage, loader, loaderSize);
                *((uint16_t*)(pCartImage + 0074)) = wStackAddr;
                *((uint16_t*)(pCartImage + 0100)) = wStartAddr;
                *((uint16_t*)(pCartImage + 0066)) = CalcCheckum((uint16_t*)(pCartImage + 01000), 027400);
                break;  // Finished encoding with plain copy
            }
        }

        // RLE
        if (options & OPTION_COMPRESSION_RLE)
        {
            ::memset(pCartImage, 0, 65536);
            size_t rleCodedSize = EncodeRLE(pFileImage + 512, savImageSize, pCartImage + 512, 24576 - 512);
            if (rleCodedSize > 24576 - 512)
            {
                printf("RLE encoded size too big: %lu. bytes, max %d. bytes\n", rleCodedSize, 24576 - 512);
            }
            else  // Use RLE compression
            {
                // Trying to decode to make sure encoder works fine
                uint8_t* pTempBuffer = (uint8_t*) ::calloc(savImageSize, 1);
                if (pTempBuffer == NULL)
                {
                    printf("Failed to allocate memory.");
                    return 255;
                }
                size_t decodedSize = DecodeRLE(pCartImage + 512, 24576 - 512, pTempBuffer, savImageSize);
                for (size_t offset = 0; offset < savImageSize; offset++)
                {
                    if (pTempBuffer[offset] == pFileImage[512 + offset])
                        continue;

                    printf("RLE decode failed at offset %06ho (%02x != %02x)\n", (uint16_t)(512 + offset), pTempBuffer[offset], pFileImage[512 + offset]);
                    return 255;
                }
                ::free(pTempBuffer);
                printf("RLE decode check done, decoded size %lu. bytes\n", decodedSize);

                ::memcpy(pCartImage, pFileImage, 512);

                // Prepare the loader
                memcpy(pCartImage, loaderRLE, loaderRLESize);
                *((uint16_t*)(pCartImage + 0076)) = wStackAddr;
                *((uint16_t*)(pCartImage + 0102)) = wStartAddr;
                *((uint16_t*)(pCartImage + 0066)) = CalcCheckum((uint16_t*)(pCartImage + 01000), 027400);
                break;  // Finished encoding with RLE
            }
        }

        // LZSS
        if (options & OPTION_COMPRESSION_LZSS)
        {
            ::memset(pCartImage, 0, 65536);
            size_t lzssCodedSize = lzss_encode(pFileImage + 512, savImageSize, pCartImage + 512, 65536 - 512);
            if (lzssCodedSize > 24576 - 512)
            {
                printf("LZSS encoded size too big: %lu. bytes, max %d. bytes\n", lzssCodedSize, 24576 - 512);
            }
            else
            {
                // Trying to decode to make sure encoder works fine
                uint8_t* pTempBuffer = (uint8_t*) ::calloc(65536, 1);
                if (pTempBuffer == NULL)
                {
                    printf("Failed to allocate memory.");
                    return 255;
                }
                size_t decodedSize = lzss_decode(pCartImage + 512, lzssCodedSize, pTempBuffer, 65536);
                for (size_t offset = 0; offset < savImageSize; offset++)
                {
                    if (pTempBuffer[offset] == pFileImage[512 + offset])
                        continue;

                    printf("LZSS decode failed at offset %06ho 0x%04x (%02x != %02x)\n", (uint16_t)(512 + offset), (uint16_t)(512 + offset), pTempBuffer[offset], pFileImage[512 + offset]);
                    return 255;
                }
                ::free(pTempBuffer);

                printf("LZSS decode check done, decoded size %lu. bytes\n", decodedSize);

                ::memcpy(pCartImage, pFileImage, 512);

                // Prepare the loader
                memcpy(pCartImage, loaderLZSS, loaderLZSSSize);
                *((uint16_t*)(pCartImage + 0076)) = wStackAddr;
                *((uint16_t*)(pCartImage + 0102)) = wStartAddr;
                *((uint16_t*)(pCartImage + 0212)) = 01000 + savImageSize;  // CTOP
                *((uint16_t*)(pCartImage + 0066)) = CalcCheckum((uint16_t*)(pCartImage + 01000), 027400);
                //printf("LZSS CTOP = %06o\n", 01000 + savImageSize);

                break;  // Finished encoding with LZSS
            }
        }

        // LZ4
        if (options & OPTION_COMPRESSION_LZ4)
        {
            ::memset(pCartImage, -1, 65536);
            size_t lz4CodedSize = lz4_encode(pFileImage + 512, savImageSize, pCartImage + 512, 65536 - 512);
            if (lz4CodedSize > 24576 - 512)
            {
                printf("LZ4 encoded size too big: %lu. bytes, max %d. bytes\n", lz4CodedSize, 24576 - 512);
            }
            else
            {
                // Trying to decode to make sure encoder works fine
                uint8_t* pTempBuffer = (uint8_t*) ::calloc(65536, 1);
                if (pTempBuffer == NULL)
                {
                    printf("Failed to allocate memory.");
                    return 255;
                }
                size_t decodedSize = lz4_decode(pCartImage + 512, lz4CodedSize, pTempBuffer, 65536);
                if (decodedSize != savImageSize) printf("failed, LZ4 decoded size = %lu (must be: %lu)\n", decodedSize, savImageSize);
                for (size_t offset = 0; offset < savImageSize; offset++)
                {
                    if (pTempBuffer[offset] == pFileImage[512 + offset])
                        continue;

                    printf("LZ4 decode failed at offset %06ho 0x%04x (%02x != %02x)\n", (uint16_t)(512 + offset),
                           (uint16_t)(512 + offset), pTempBuffer[offset], pFileImage[512 + offset]);
                    return 255;
                }
                ::free(pTempBuffer);

                printf("LZ4 decode check done, decoded size %lu. bytes\n", decodedSize);

                ::memcpy(pCartImage, pFileImage, 512);

                // Prepare the loader
                memcpy(pCartImage, loaderLZ4, loaderLZ4Size);
                *((uint16_t*)(pCartImage + 0076)) = wStackAddr;
                *((uint16_t*)(pCartImage + 0102)) = wStartAddr;
                *((uint16_t*)(pCartImage + 0130)) = wStartAddr;
                *((uint16_t*)(pCartImage + 0066)) = CalcCheckum((uint16_t*)(pCartImage + 01000), 027400);
                break;  // Finished encoding with LZ4
            }
        }

        // LZSA1
        if (options & OPTION_COMPRESSION_LZSA1)
        {
            ::memset(pCartImage, -1, 65536);
            int nFormatVersion = 1;
            unsigned int nFlags = LZSA_FLAG_RAW_BLOCK | LZSA_FLAG_FAVOR_RATIO;
            size_t encodedSize = lzsa_compress_inmem(
                    pFileImage + 512, pCartImage + 512, savImageSize, 65536 - 512, nFlags, 3, nFormatVersion);
            printf("LZSA1 output size %lu. bytes (%1.2f %%)\n", encodedSize, encodedSize * 100.0 / savImageSize);
            if (encodedSize > 24576 - 512)
            {
                printf("LZSA1 encoded size too big: %lu. bytes, max %d. bytes\n", encodedSize, 24576 - 512);
            }
            else
            {
                // Trying to decode to make sure encoder works fine
                uint8_t* pTempBuffer = (uint8_t*) ::calloc(65536, 1);
                if (pTempBuffer == NULL)
                {
                    printf("Failed to allocate memory.");
                    return 255;
                }
                int nFormatVersion = 1;
                size_t decodedSize = lzsa_decompress_inmem(
                        pCartImage + 512, pTempBuffer, encodedSize, 65536, nFlags, &nFormatVersion);
                if (decodedSize != savImageSize) printf("failed, LZSA1 decoded size = %lu (must be: %lu)\n", decodedSize, savImageSize);
                for (size_t offset = 0; offset < savImageSize; offset++)
                {
                    if (pTempBuffer[offset] == pFileImage[512 + offset])
                        continue;

                    printf("LZSA1 decode failed at offset %06ho 0x%04x (%02x != %02x)\n", (uint16_t)(512 + offset),
                           (uint16_t)(512 + offset), pTempBuffer[offset], pFileImage[512 + offset]);
                    return 255;
                }
                ::free(pTempBuffer);

                printf("LZSA1 decode check done, decoded size %lu. bytes\n", decodedSize);

                ::memcpy(pCartImage, pFileImage, 512);

                // Prepare the loader
                memcpy(pCartImage, loaderLZSA1, loaderLZSA1Size);
                uint16_t wLZWords = (encodedSize + 1) / 2;  // How many words to copy from the cartridge
                uint16_t wLZStart = 0160000 - wLZWords * 2 - 0100;  // Address where to copy to from the cartridge
                *((uint16_t*)(pCartImage + 0050)) = wLZStart;
                *((uint16_t*)(pCartImage + 0054)) = wLZWords;
                *((uint16_t*)(pCartImage + 0066)) = CalcCheckum((uint16_t*)(pCartImage + 01000), 027400);
                *((uint16_t*)(pCartImage + 0076)) = wStackAddr;
                *((uint16_t*)(pCartImage + 0102)) = wStartAddr;
                *((uint16_t*)(pCartImage + 0112)) = wLZStart;
                *((uint16_t*)(pCartImage + 0114)) = wLZWords;
                *((uint16_t*)(pCartImage + 0124)) = wLZStart;
                break;  // Finished encoding with LZSA1
            }
        }

        // LZSA2
        if (options & OPTION_COMPRESSION_LZSA2)
        {
            ::memset(pCartImage, -1, 65536);
            int nFormatVersion = 2;
            unsigned int nFlags = LZSA_FLAG_RAW_BLOCK | LZSA_FLAG_FAVOR_RATIO;
            size_t encodedSize = lzsa_compress_inmem(
                    pFileImage + 512, pCartImage + 512, savImageSize, 65536 - 512, nFlags, 3, nFormatVersion);
            printf("LZSA2 output size %lu. bytes (%1.2f %%)\n", encodedSize, encodedSize * 100.0 / savImageSize);
            if (encodedSize > 24576 - 512)
            {
                printf("LZSA2 encoded size too big: %lu. bytes, max %d. bytes\n", encodedSize, 24576 - 512);
            }
            else
            {
                // Trying to decode to make sure encoder works fine
                uint8_t* pTempBuffer = (uint8_t*) ::calloc(65536, 1);
                if (pTempBuffer == NULL)
                {
                    printf("Failed to allocate memory.");
                    return 255;
                }
                size_t decodedSize = lzsa_decompress_inmem(
                        pCartImage + 512, pTempBuffer, encodedSize, 65536, nFlags, &nFormatVersion);
                if (decodedSize != savImageSize) printf("failed, LZSA2 decoded size = %lu (must be: %lu)\n", decodedSize, savImageSize);
                for (size_t offset = 0; offset < savImageSize; offset++)
                {
                    if (pTempBuffer[offset] == pFileImage[512 + offset])
                        continue;

                    printf("LZSA2 decode failed at offset %06ho 0x%04x (%02x != %02x)\n", (uint16_t)(512 + offset),
                           (uint16_t)(512 + offset), pTempBuffer[offset], pFileImage[512 + offset]);
                    return 255;
                }
                ::free(pTempBuffer);

                printf("LZSA2 decode check done, decoded size %lu. bytes\n", decodedSize);

                ::memcpy(pCartImage, pFileImage, 512);

                // Prepare the loader
                memcpy(pCartImage, loaderLZSA2, loaderLZSA2Size);
                uint16_t wLZWords = (encodedSize + 1) / 2;  // How many words to copy from the cartridge
                uint16_t wLZStart = 0160000 - wLZWords * 2 - 0100;  // Address where to copy to from the cartridge
                *((uint16_t*)(pCartImage + 0050)) = wLZStart;
                *((uint16_t*)(pCartImage + 0054)) = wLZWords;
                *((uint16_t*)(pCartImage + 0066)) = CalcCheckum((uint16_t*)(pCartImage + 01000), wLZWords);
                *((uint16_t*)(pCartImage + 0074)) = wLZStart;
                *((uint16_t*)(pCartImage + 0110)) = wStackAddr;
                *((uint16_t*)(pCartImage + 0124)) = wLZStart;
                *((uint16_t*)(pCartImage + 0126)) = wLZWords;
                //TODO
                break;  // Finished encoding with LZSA2
            }
        }

        return 255;  // All attempts failed
    }

    ::free(pFileImage);  pFileImage = nullptr;

    printf("Output file: %s\n", outputfilename);
    outputfile = fopen(outputfilename, "wb");
    if (outputfile == nullptr)
    {
        printf("Failed to open output file (%d).", errno);
        return 255;
    }

    size_t bytesWrite = ::fwrite(pCartImage, 1, 24576, outputfile);
    if (bytesWrite != 24576)
    {
        printf("Failed to write to the output file.");
        return 255;
    }
    ::fclose(outputfile);

    ::free(pCartImage);

    printf("Done.\n");
    return 0;
}


//////////////////////////////////////////////////////////////////////
