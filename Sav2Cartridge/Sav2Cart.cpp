/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Sav2Wav.cpp
//
// Originally written by Alexander Alexandrow aka BYTEMAN
// mailto: sash-a@nm.ru
//
// Converted from Delphi 7 to C++ by Nikita Zimin
//

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdint.h>


#define BYTE uint8_t
#define WORD uint16_t


static WORD const loader[] =
{
    0000240,  // 000000  000240  NOP
    0012702,  // 000002  012702  MOV     #000104, R2
    0000104,
    0110062,  // 000006  110062  MOVB    R0, 000003(R2)
    0000003,
    0012701,  // 000012  012701  MOV     #000005, R1
    0000005,
    0012703,  // 000016  012703  MOV     #000116, R3
    0000116,
    0000402,  // 000022  000402  BR      000030
    0112337,  // 000024  112337  MOVB    (R3)+, @#176676
    0176676,
    0105737,  // 000030  105737  TSTB    @#176674
    0176674,
    0100375,  // 000034  100375  BPL     000030
    0077106,  // 000036  077106  SOB     R1, 000024
    0105712,  // 000040  105712  TSTB    (R2)
    0001356,  // 000042  001356  BNE     000000
    // Подсчёт контрольной суммы
    0005003,  // 000044  005003  CLR     R3
    0012701,  // 000046  012701  MOV     #001000, R1
    0001000,  // 000050  001000
    0012702,  // 000052  012702  MOV     #027400, R2
    0027400,  // 000054  027400
    0062103,  // 000056  062103  ADD     (R1)+, R3
    0005503,  // 000060  005503  ADC     R3
    0077203,  // 000062  077203  SOB     R2, 000056
    0020327,  // 000064  020327  CMP     R3, #CHKSUM
    0000000,  // 000066  ?????? <= CHKSUM
    0001343,  // 000070  001343  BNE     000000
    // Запуск загруженной программы на выполнение
    0012706,  // 000072  016706  MOV	#STACK, SP
    0001000,  // 000074  ?????? <= STACK
    0000137,  // 000076  000137  JMP    START   ; Переход на загруженный код
    0001000,  // 000100  ?????? <= START
    0000240,  // 000102 NOP
    // Массив параметров для получения данных с кассеты ПЗУ через канал 2
    0004000,  // 000104  004000   ; Команда (10) и ответ
    0000021,  // 000106  000021   ; Номер кассеты и номер устройства
    0001000,  // 000110  001000   ; Адрес от начала кассеты ПЗУ
    0001000,  // 000112  005566   ; Адрес в ОЗУ
    0027400,  // 000114  027400   ; Количество слов = 12032. слов = 24064. байт
    0000104,
    0177777,
};


char inputfilename[256];
char outputfilename[256];
FILE* inputfile;
FILE* outputfile;
BYTE* pCartImage = NULL;
WORD wStartAddr;
WORD wStackAddr;


int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("Usage: Sav2Cart <inputfile.SAV> <outputfile.BIN>");
        return 255;
    }

    strcpy(inputfilename, argv[1]);
    strcpy(outputfilename, argv[2]);

    printf("Input:\t\t%s\n", inputfilename);
    printf("Output:\t\t%s\n", outputfilename);

    pCartImage = (BYTE*) ::malloc(24576);
    ::memset(pCartImage, 0, 24576);

    inputfile = fopen(inputfilename, "rb");
    if (inputfile == NULL)
    {
        printf("Failed to open the input file.");
        return 255;
    }
    ::fseek(inputfile, 0, SEEK_END);
    uint32_t inputfileSize = ::ftell(inputfile);
    if (inputfileSize > 24576)
    {
        printf("Input file too long; max length is 24576.");
        return 255;
    }

    ::fseek(inputfile, 0, SEEK_SET);
    size_t bytesRead = ::fread(pCartImage, 1, inputfileSize, inputfile);
    if (bytesRead != inputfileSize)
    {
        printf("Failed to read the input file.");
        return 255;
    }

    ::fclose(inputfile);

    wStartAddr = *((WORD*)(pCartImage + 040));
    wStackAddr = *((WORD*)(pCartImage + 042));

    // Prepare the loader
    memcpy(pCartImage, loader, sizeof(loader));

    *((WORD*)(pCartImage + 0074)) = wStackAddr;
    *((WORD*)(pCartImage + 0100)) = wStartAddr;

    // Calculate checksum
    WORD* pData = ((WORD*)(pCartImage + 01000));
    WORD wChecksum = 0;
    for (int i = 0; i < 027400; i++)
    {
        WORD src = wChecksum;
        WORD src2 = *pData;
        wChecksum += src2;
        if (((src & src2) | ((src ^ src2) & ~wChecksum)) & 0100000)  // if Carry
            wChecksum++;
        pData++;
    }
    *((WORD*)(pCartImage + 0066)) = wChecksum;

    outputfile = fopen(outputfilename, "w+b");
    if (outputfile == NULL)
    {
        printf("Failed to open output file.");
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

    return 0;
}
