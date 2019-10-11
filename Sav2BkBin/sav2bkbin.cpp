/*  This file is part of BKBTL.
BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Sav2BkBin.cpp

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>


#define BYTE unsigned char
#define WORD unsigned short


static char inputfilename[256];
static char outputfilename[256];
static FILE* inputfile;
static FILE* outputfile;


void Convert()
{
    printf("Input file: %s\n", inputfilename);

    inputfile = fopen(inputfilename, "rb");
    if (inputfile == NULL)
    {
        printf("Failed to open input file.");
        return;
    }

    printf("Output file: %s\n", outputfilename);

    outputfile = fopen(outputfilename, "w+b");
    if (outputfile == NULL)
    {
        printf("Failed to open output file.");
        return;
    }

    void * pHeader = calloc(512, 1);

    size_t bytesread = fread(pHeader, 1, 512, inputfile);
    if (bytesread != 512)
    {
        printf("Failed to read the header.");
        free(pHeader);
        return;
    }

    WORD baseAddress = *(((WORD*)pHeader) + 040 / 2);
    WORD lastAddress = *(((WORD*)pHeader) + 050 / 2);
    WORD dataSize = lastAddress + 2 - 01000;
    WORD dataSizeOrig = dataSize;

    printf("SAV Start\t%06o  %04x  %5d\n", baseAddress, baseAddress, baseAddress);
    printf("SAV Top  \t%06o  %04x  %5d\n", lastAddress, lastAddress, lastAddress);
    printf("SAV image size\t%06o  %04x  %5u\n", dataSize, dataSize, dataSize);

    free(pHeader);

    void * pData = calloc(dataSize, 1);

    bytesread = fread(pData, 1, dataSize, inputfile);
    if (bytesread != dataSize)
    {
        printf("Failed to read the data.");
        free(pData);
        return;
    }

    dataSize += 24 * 2;  // for auto-start header

    WORD autoStartBaseAddress = 000720;
    printf("Saving BK header:  %06o %06o\n", autoStartBaseAddress, dataSize);
    fwrite(&autoStartBaseAddress, 1, 2, outputfile);
    fwrite(&dataSize, 1, 2, outputfile);

    printf("Saving auto-start header, 24. words %06o\n", baseAddress);
    for (int i = 0; i < 24; i++)  // write auto-start header
    {
        fwrite(&baseAddress, 1, 2, outputfile);
    }

    printf("Saving image, %u. bytes\n", (unsigned int)dataSize);
    fwrite(pData, 1, dataSizeOrig, outputfile);

    free(pData);

    fclose(inputfile);
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("Usage: sav2bkbin <inputfile.SAV> <outputfile.BIN>");
        return 255;
    }

    strcpy_s(inputfilename, argv[1]);
    strcpy_s(outputfilename, argv[2]);

    Convert();

    printf("Done.\n");
    return 0;
}
