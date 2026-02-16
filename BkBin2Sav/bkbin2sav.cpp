// BK-0010 binary to PDP-11 .sav format
//
// by BlaireCas, see https://github.com/blairecas/scripts/blob/main/bkbin2sav.cpp


#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


FILE *      fin;
FILE *      fout;

uint16_t *  inbuf;
int         input_size;
uint16_t    bin_start;
uint16_t    bin_length;

uint8_t     block0[512];


// exit with error
//
void exit_with_msg ( char* s )
{
    if (fin) fclose(fin);
    if (fout) fclose(fout);
    printf(s);
    exit(1);
}


// remove .ext from filename
//
void strip_ext(char *fname)
{
    char *end = fname + strlen(fname);
    while (end > fname && *end != '.') --end;
    if (end > fname) *end = '\0';
}


// guess what, main function
//
int main ( int argc, char* argv[])
{
    // print usage
    if (argc < 2)
    {
        printf("\nBK-0010 binary .bin -> PDP-11 .sav\n");
        printf("Use: bkbin2sav file.bin [file.sav]\n");
        return 0;
    }

    // open input file
    char* infname = argv[1];
    fin = fopen(infname, "rb");
    if (!fin) exit_with_msg("(!) unable to open input file\n");

    // open output file
    char outfname[256];
    if (argc >= 3)
    {
        strcpy(outfname, argv[2]);
    }
    else
    {
        strcpy(outfname, infname);
        strip_ext(outfname);
        strcat(outfname, ".sav");
    }
    fout = fopen(outfname, "wb");
    if (!fout) exit_with_msg("(!) unable to open output file\n");

    // input file size
    fseek(fin, 0L, SEEK_END);
    input_size = ftell(fin);
    if (input_size <= 0x004) exit_with_msg("(!) input file too small (less than 5 bytes)\n");
    if (input_size > 0xF000) exit_with_msg("(!) input file too big (^_^ it can't be BK binary)\n");
    fseek(fin, 0L, SEEK_SET);
    fread(&bin_start, 2, 1, fin);
    fread(&bin_length, 2, 1, fin);
    printf("BK binary header - start 0%o (0x%X), length 0%o (0x%X)\n", bin_start, bin_start, bin_length, bin_length);
    if (bin_start != 0x200) exit_with_msg("(!) use 01000 (0x200) as a starting addr in BK binary\n");
    if (bin_length != (input_size - 4)) exit_with_msg("(!) binary header size is incorrect\n");

    int aligned_size = ((bin_length + 511) / 512) * 512;
    printf("aligned data size = 0%o (0x%X) bytes\n", aligned_size, aligned_size);

    // read input file
    inbuf = (uint16_t*) malloc(aligned_size);
    memset(inbuf, 0, aligned_size);
    int input_readed = fread(inbuf, 1, bin_length, fin);
    if (input_readed != bin_length) exit_with_msg("(!) unable to read input file\n");

    // construct .sav block-0
    // starting addr = loading addr in bin
    block0[0x20] = bin_start & 0xFF;
    block0[0x21] = (bin_start >> 8) & 0xFF;
    // stack pointer = 0x200 (01000)
    block0[0x22] = 0x00;
    block0[0x23] = 0x02;
    // program high limit
    uint16_t high_limit = bin_start + aligned_size;
    block0[0x28] = high_limit & 0xFF;
    block0[0x29] = (high_limit >> 8) & 0xFF;
    // 0xF0-0xFF - bitmask area - to load blocks from file [11111000][...] bytes, bits are readed from high to low
    int adr = high_limit - 2;
    int block0_addr = 0xF0;
    uint8_t block0_byte = 0;
    int rot_count = 0;
    while (adr >= 0)
    {
        block0_byte = (block0_byte >> 1) | 0x80;
        rot_count++;
        if (rot_count >= 8)
        {
            rot_count = 0;
            block0[block0_addr] = block0_byte;
            block0_addr++;
            block0_byte = 0;
        }
        adr -= 512;
    }
    block0[block0_addr] = block0_byte;

    // write out
    if (!fwrite(block0, 1, 512, fout)) exit_with_msg("(!) unable to write output file header");
    if (!fwrite(inbuf, 1, aligned_size, fout)) exit_with_msg("(!) unable to write output file data");

    fclose(fin);
    fclose(fout);

    return 0;
}