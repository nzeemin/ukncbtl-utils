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
    inputfile = fopen(inputfilename, "rb");
    if (inputfile == NULL)
    {
        printf("Failed to open input file.");
        return;
    }

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
        return;
    }

    WORD baseAddress = *(((WORD*)pHeader) + 040 / 2);
    WORD lastAddress = *(((WORD*)pHeader) + 050 / 2);
    WORD dataSize = lastAddress + 2 - 01000;

    free(pHeader);

    void * pData = calloc(dataSize, 1);

    bytesread = fread(pData, 1, dataSize, inputfile);
    if (bytesread != dataSize)
    {
        printf("Failed to read the data.");
        return;
    }

    dataSize += 24 * 2;  // for auto-start header

    WORD autoStartBaseAddress = 000720;
    fwrite(&autoStartBaseAddress, 1, 2, outputfile);
    fwrite(&dataSize, 1, 2, outputfile);

    for (int i = 0; i < 24; i++)  // write auto-start header
    {
        fwrite(&baseAddress, 1, 2, outputfile);
    }

    fwrite(pData, 1, dataSize, outputfile);

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

    strcpy(inputfilename, argv[1]);
    strcpy(outputfilename, argv[2]);

    printf("Input:\t\t%s\n", inputfilename);
    printf("Output:\t\t%s\n", outputfilename);

    Convert();

    return 0;
}
