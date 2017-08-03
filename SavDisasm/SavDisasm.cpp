// SavDisasm.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////
// Preliminary function declarations

void PrintWelcome();
void PrintUsage();
bool ParseCommandLine(int argc, char* argv[]);

//////////////////////////////////////////////////////////////////////
// Globals

LPCSTR g_sSavFileName = NULL;
bool g_okShowValues = false;
uint16_t g_wStartAddress = 001000;
uint16_t g_wEndAddress = 0xffff;
char g_sOutFileName[255] = { 0 };

//////////////////////////////////////////////////////////////////////

// Названия регистров процессора
const char* REGISTER_NAME[] = { ("R0"), ("R1"), ("R2"), ("R3"), ("R4"), ("R5"), ("SP"), ("PC") };

//////////////////////////////////////////////////////////////////////


void PrintWelcome()
{
    printf(("SavDisasm Utility  by Nikita Zimin  [%s %s]\n\n"), __DATE__, __TIME__);
}

void PrintUsage()
{
    printf(("\nUsage:\n"));
    printf(("    SavDisasm [options] <SavFileFile>\n"));
    printf(("  Parameters:\n"));
    printf(("    <SavFileName> is name of .SAV file to disassemble\n"));
    printf(("  Options:\n"));
    printf(("    /O:<OutFileName>  Set output file name\n"));
    printf(("    /S<addr>  Set disassembly start address (octal)\n"));
    printf(("    /E<addr>  Set disassembly end address (octal)\n"));
    printf(("    /V        Show original values\n"));
}

// Print octal 16-bit value to buffer
// buffer size at least 7 characters
void PrintOctalValue(char* buffer, uint16_t value)
{
    for (int p = 0; p < 6; p++)
    {
        int digit = value & 7;
        buffer[5 - p] = ('0') + (char)digit;
        value = (value >> 3);
    }
    buffer[6] = 0;
}

// Parse octal value from text
bool ParseOctalValue(LPCSTR text, uint16_t* pValue)
{
    uint16_t value = 0;
    char* pChar = (char*)text;
    for (int p = 0;; p++)
    {
        if (p > 6) return false;
        char ch = *pChar;  pChar++;
        if (ch == 0) break;
        if (ch < ('0') || ch > ('7')) return false;
        value = (value << 3);
        char digit = ch - ('0');
        value += digit;
    }
    *pValue = value;
    return true;
}

bool ParseCommandLine(int argc, char* argv[])
{
    for (int argn = 1; argn < argc; argn++)
    {
        LPCSTR arg = argv[argn];
        if (arg[0] == ('-') || arg[0] == ('/'))
        {
            char option = tolower(arg[1]);
            switch (option)
            {
            case ('s'):  // Set disasm Start address
                if (!ParseOctalValue(arg + 2, &g_wStartAddress))
                {
                    printf(("Failed to parse start address: %s\n"), arg);
                    return false;
                }
                break;
            case ('e'):  // Set disasm End address
                if (!ParseOctalValue(arg + 2, &g_wEndAddress))
                {
                    printf(("Failed to parse end address: %s\n"), arg);
                    return false;
                }
                break;
            case ('v'):  // Show original Values in the disassembly
                g_okShowValues = true;
                break;
            case ('o'):  // Set Output file name
                {
                    LPCSTR pOuFileName = arg + 2;
                    if (*pOuFileName == (':')) pOuFileName++;  // Skip ':' if found
                    if (*pOuFileName == 0)
                    {
                        printf(("Output file name is not specified: %s\n"), arg);
                        return false;
                    }
                    strcpy(g_sOutFileName, pOuFileName);
                    break;
                }
            default:
                printf(("Unknown option: %s\n"), arg);
                return false;
            }
        }
        else
        {
            if (g_sSavFileName == NULL)
                g_sSavFileName = arg;
            else
            {
                printf(("Unknown param: %s\n"), arg);
                return false;
            }
        }
    }

    // Parsed options validation
    if (g_sSavFileName == NULL)
    {
        printf(("<SavFileName> not specified.\n"));
        return false;
    }

    return true;
}

void DisasmSavImage(uint16_t* pImage, uint16_t wImageSize, FILE* fpOutFile)
{
    uint16_t address = g_wStartAddress;

    for (;;)
    {
        if (address >= g_wEndAddress || address >= wImageSize)
            break;

        char bufaddr[8];
        char bufinstr[8];
        char bufargs[32];
        char buffer[128];
        char value0[8], value1[8], value2[8];

        int length = 0;
        length = DisassembleInstruction(pImage + address / 2, address, bufinstr, bufargs);
        PrintOctalValue(bufaddr, address);
        if (g_okShowValues)
        {
            PrintOctalValue(value0, pImage[address / 2]);
            if (length > 1) PrintOctalValue(value1, pImage[address / 2 + 1]); else *value1 = 0;
            if (length > 2) PrintOctalValue(value2, pImage[address / 2 + 2]); else *value2 = 0;
            sprintf_s(buffer, 128, ("%s\t%s\t%s\t%s\t%-7s\t%s\r\n"), bufaddr, value0, value1, value2, bufinstr, bufargs);
        }
        else
        {
            sprintf_s(buffer, 128, ("%s\t%-7s\t%s\r\n"), bufaddr, bufinstr, bufargs);
        }

        ::fwrite(buffer, 1, strlen(buffer), fpOutFile);

        address += length * 2;
        if (address == 0)
            break;
    }
}

int main(int argc, char* argv[])
{
    PrintWelcome();

    if (!ParseCommandLine(argc, argv))
    {
        if (g_sSavFileName == NULL)
            PrintUsage();
        return 255;
    }

    if (*g_sOutFileName == 0)
        strcpy(g_sOutFileName, ("disasm.txt"));

    // Show current settings
    printf(("Input file:\t%s\n"), g_sSavFileName);
    printf(("Output file:\t%s\n"), g_sOutFileName);
    printf(("Start address:\t%06o\n"), g_wStartAddress);
    printf(("End address:\t%06o\n"), g_wEndAddress);

    FILE* fpFile = ::fopen(g_sSavFileName, ("rb"));
    if (fpFile == NULL)
    {
        printf(("Failed to open the input file.\n"));
        return 255;
    }

    uint16_t* pImage = (uint16_t*) ::malloc(65536);
    ::memset(pImage, 0, 65536);

    ::fseek(fpFile, 0, SEEK_END);
    long lFileSize = ::ftell(fpFile);
    long lReadSize = (lFileSize < 65535) ? lFileSize : 65535;
    ::fseek(fpFile, 0, SEEK_SET);
    long lReadSizeFact = ::fread(pImage, 1, lReadSize, fpFile);
    ::fclose(fpFile);
    if (lReadSizeFact != lReadSize)
    {
        printf(("Failed to read the input file.\n"));
        ::free(pImage);
        return 255;
    }

    FILE* fpOutFile = ::fopen(g_sOutFileName, ("w+b"));
    if (fpOutFile == NULL)
    {
        printf(("Failed to open the output file.\n"));
        ::free(pImage);
        return 255;
    }

    DisasmSavImage(pImage, (uint16_t)lReadSize, fpOutFile);

    ::free(pImage);

    return 0;
}

//////////////////////////////////////////////////////////////////////
