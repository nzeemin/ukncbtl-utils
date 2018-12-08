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


//////////////////////////////////////////////////////////////////////
// LZSS.cpp

size_t lzss_encode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize);

size_t lzss_decode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize);


//////////////////////////////////////////////////////////////////////


static uint16_t const loader[] =
{
    0000240,  // 000000  000240  NOP
    0012702,  // 000002  012702  MOV     #000104, R2    ; Адрес массива параметров
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
    0001000,  // 000112  001000   ; Адрес в ОЗУ
    0027400,  // 000114  027400   ; Количество слов = 12032. слов = 24064. байт
    0000104,  // 000116
    0177777,  // 000120
};

static uint16_t const loaderRLE[] =
{
    0000240,  // 000000  000240  NOP
    0012702,  // 000002  012702  MOV     #000104, R2    ; Адрес массива параметров
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
    0012701,  // 000046  012701  MOV     #100000, R1
    0100000,  // 000050  100000
    0012702,  // 000052  012702  MOV     #027400, R2
    0027400,  // 000054  027400
    0062103,  // 000056  062103  ADD     (R1)+, R3
    0005503,  // 000060  005503  ADC     R3
    0077203,  // 000062  077203  SOB     R2, 000056
    0020327,  // 000064  020327  CMP     R3, #CHKSUM
    0000000,  // 000066  ?????? <= CHKSUM
    0001343,  // 000070  001343  BNE     000000
    0000413,  // 000072  000413  BR      000122        ; Переход на RLE decoder
    // Запуск загруженной программы на выполнение
    0012706,  // 000074  016706  MOV	#STACK, SP
    0001000,  // 000076  ?????? <= STACK
    0000137,  // 000100  000137  JMP    START   ; Переход на загруженный код
    0001000,  // 000102  ?????? <= START
    // Массив параметров для получения данных с кассеты ПЗУ через канал 2
    0004000,  // 000104  004000   ; Команда (10) и ответ
    0000021,  // 000106  000021   ; Номер кассеты и номер устройства
    0001000,  // 000110  001000   ; Адрес от начала кассеты ПЗУ
    0100000,  // 000112  100000   ; Адрес в ОЗУ == RLESTA
    0027400,  // 000114  027400   ; Количество слов = 12032. слов = 24064. байт
    0000104,  // 000116
    0177777,  // 000120
    // RLE decoder
    0012701,  // 000122  012701      MOV  #RLESTA, R1
    0100000,  // 000124  100000 <= RLESTA                ; start address of RLE block
    0012702,  // 000126  012702      MOV  #1000, R2
    0001000,  // 000130  001000                          ; destination start address
    0112100,  // 000132  112100  $1: MOVB (R1+), R0
    0001757,  // 000134  001757      BEQ  000074         ; 0 => decoding finished, let's start the application
    0010003,  // 000136  010003      MOV  R0, R3         ; prepare counter
    0042703,  // 000140  042703      BIC  #177740, R3    ; only lower 5 bits are significant
    0177740,  // 000142  177740
    0105700,  // 000144  105700      TSTB R0             ; 1-byte command?
    0100002,  // 000146  100002      BPL  $2             ; yes => jump
    0000303,  // 000150  000303      SWAB R3             ; move lower byte to high byte
    0152103,  // 000152  152103      BISB (R1)+, R3      ; set lower byte of counter
    0005004,  // 000154  005003  $2: CLR  R4             ; clear filler
    0142700,  // 000156  142700      BICB #237, R0
    0000237,  // 000160  000237
    0001411,  // 000162  001411      BEQ  $3             ; zero pattern? => jump
    0012704,  // 000164  012704      MOV  #377, R4       ;
    0000377,  // 000166  000377
    0122700,  // 000170  122700      CMPB #140, R0       ; #377 pattern?
    0000140,  // 000172  000140
    0001404,  // 000174  001404      BEQ  $3             ; yes => jump
    0122700,  // 000176  122700      CMPB #40, R0       ; given pattern?
    0000040,  // 000200  000040
    0001004,  // 000202  001004      BNE  $4             ; no => jump
    0112104,  // 000204  112104      MOVB (R1+), R4      ; read the given pattern
    0110422,  // 000206  110422  $3: MOVB R4, (R2+)      ; loop: write pattern to destination
    0077302,  // 000210  077302      SOB  R3, $3         ;
    0000747,  // 000212  000747      BR   $1
    0112104,  // 000214  112104  $4: MOVB (R1+), R4      ; loop: copy bytes to destination
    0110422,  // 000216  110422      MOVB R4, (R2+)
    0077303,  // 000220  077303      SOB  R3, $4
    0000743,  // 000222  000743      BR   $1
};

static uint16_t const loaderLZSS[] =
{
    0000240,  // 000000  000240  NOP
    0012702,  // 000002  012702  MOV     #000104, R2    ; Адрес массива параметров
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
    0012701,  // 000046  012701  MOV     #100600, R1
    0100600,  // 000050  100600
    0012702,  // 000052  012702  MOV     #027400, R2
    0027400,  // 000054  027400
    0062103,  // 000056  062103  ADD     (R1)+, R3
    0005503,  // 000060  005503  ADC     R3
    0077203,  // 000062  077203  SOB     R2, 000056
    0020327,  // 000064  020327  CMP     R3, #CHKSUM
    0000000,  // 000066  ?????? <= CHKSUM
    0001343,  // 000070  001343  BNE     000000
    0000413,  // 000072  000413  BR      000122        ; Переход на LZSS decoder
    // Запуск загруженной программы на выполнение
    0012706,  // 000074  016706  MOV	#STACK, SP
    0001000,  // 000076  ?????? <= STACK
    0000137,  // 000100  000137  JMP    START   ; Переход на загруженный код
    0001000,  // 000102  ?????? <= START
    // Массив параметров для получения данных с кассеты ПЗУ через канал 2
    0004000,  // 000104  004000   ; Команда (10) и ответ
    0000021,  // 000106  000021   ; Номер кассеты и номер устройства
    0001000,  // 000110  001000   ; Адрес от начала кассеты ПЗУ
    0100600,  // 000112  100600   ; Адрес в ОЗУ == LZSSTA
    0027400,  // 000114  027400   ; Количество слов = 12032. слов = 24064. байт
    0000104,  // 000116
    0177777,  // 000120
    // LZSS decoder, from LZSAV.MAC by Ostapenko Alexey
    0012700,  // 000122  012700          MOV    #LZSSTA, R0
    0100600,  // 000124  100600 <= LZSSTA
    0012702,  // 000126  012702          MOV    #1000, R2
    0001000,  // 000130  001000                                 ; destination start address
    0005003,  // 000132  005003          CLR    R3
    0112001,  // 000134  112001  UNPST:  MOVB	(R0)+,R1		;GET FLAGS
    0012704,  // 000136  012704          MOV	#10,R4			;BITS COUNT
    0000010,  // 000140  000010
    0006001,  // 000142  006001  M402:   ROR	R1
    0103402,  // 000144  103402          BCS	403$			;1?
    0112022,  // 000146  112022          MOVB	(R0)+,(R2)+		;MOV uint8_t
    0000417,  // 000150  000417          BR     405$
    0152003,  // 000152  152003  403$:   BISB   (R0)+,R3		;GET LOW uint8_t
    0000303,  // 000154  000303          SWAB   R3
    0152003,  // 000156  152003          BISB	(R0)+,R3		;GET HIGH uint8_t
    0010305,  // 000160  010305          MOV    R3,R5
    0042705,  // 000162  042705          BIC	#170000,R5		;OFFSET FROM CURRENT POSITION
    0170000,  // 000164  170000
    0040503,  // 000166  040503          BIC	R5,R3
    0073327,  // 000170  073327          ASHC	#-12.,R3
    0177764,  // 000172  177764
    0062703,  // 000174  062703          ADD	#3,R3
    0000003,  // 000176  000003
    0005405,  // 000200  005405          NEG	R5
    0060205,  // 000202  060205          ADD	R2,R5			;ADRESS
    0112522,  // 000204  112522  404$:   MOVB   (R5)+,(R2)+
    0077302,  // 000206  077302          SOB	R3,404$
    0020227,  // 000210  020227  405$:   CMP	R2,(PC)+
    0000000,  // 000212  ?????? <= CTOP
    0001402,  // 000214  001402          BEQ	406$
    0077427,  // 000216  077427          SOB	R4,M402
    0000745,  // 000220  000745          BR     UNPST
    0000724,  // 000222  000724  406$:   BR     LAUNCH
};


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

char inputfilename[256];
char outputfilename[256];
FILE* inputfile;
FILE* outputfile;
uint8_t* pFileImage = NULL;
uint8_t* pCartImage = NULL;
uint16_t wStartAddr;
uint16_t wStackAddr;
uint16_t wTopAddr;

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("Usage: Sav2Cart <inputfile.SAV> <outputfile.BIN>\n");
        return 255;
    }

    strcpy(inputfilename, argv[1]);
    strcpy(outputfilename, argv[2]);

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
        if (inputfileSize > 24576)
        {
            printf("Input file is too big for cartridge: %u. bytes, max 24576. bytes\n", inputfileSize);
        }
        else  // Copy SAV image as is, add loader
        {
            ::memcpy(pCartImage, pFileImage, inputfileSize);

            // Prepare the loader
            memcpy(pCartImage, loader, sizeof(loader));
            *((uint16_t*)(pCartImage + 0074)) = wStackAddr;
            *((uint16_t*)(pCartImage + 0100)) = wStartAddr;

            break;  // Finished encoding with plain copy
        }

        // RLE
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
            memcpy(pCartImage, loaderRLE, sizeof(loaderRLE));
            *((uint16_t*)(pCartImage + 0076)) = wStackAddr;
            *((uint16_t*)(pCartImage + 0102)) = wStartAddr;

            break;  // Finished encoding with RLE
        }

        // LZSS
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
            memcpy(pCartImage, loaderLZSS, sizeof(loaderLZSS));
            *((uint16_t*)(pCartImage + 0076)) = wStackAddr;
            *((uint16_t*)(pCartImage + 0102)) = wStartAddr;
            *((uint16_t*)(pCartImage + 0212)) = 01000 + savImageSize;  // CTOP
            //printf("LZSS CTOP = %06o\n", 01000 + savImageSize);

            break;  // Finished encoding with LZSS
        }

        return 255;  // All attempts failed
    }

    // Calculate checksum
    uint16_t* pData = ((uint16_t*)(pCartImage + 01000));
    uint16_t wChecksum = 0;
    for (int i = 0; i < 027400; i++)
    {
        uint16_t src = wChecksum;
        uint16_t src2 = *pData;
        wChecksum += src2;
        if (((src & src2) | ((src ^ src2) & ~wChecksum)) & 0100000)  // if Carry
            wChecksum++;
        pData++;
    }
    *((uint16_t*)(pCartImage + 0066)) = wChecksum;

    ::free(pFileImage);

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
