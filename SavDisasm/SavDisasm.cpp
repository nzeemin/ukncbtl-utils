// SavDisasm.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////
// Disassembler

/// \brief Disassemble one instruction of KM1801VM2 processor
/// \param[in]  pMemory Memory image (we read only words of the instruction)
/// \param[in]  addr    Address of the instruction
/// \param[out] sInstr  Instruction mnemonics buffer - at least 8 characters
/// \param[out] sArg    Instruction arguments buffer - at least 32 characters
/// \return  Number of words in the instruction
uint16_t DisassembleInstruction(uint16_t* pMemory, uint16_t addr, TCHAR* sInstr, TCHAR* sArg);

//////////////////////////////////////////////////////////////////////
// Preliminary function declarations

void PrintWelcome();
void PrintUsage();
bool ParseCommandLine(int argc, _TCHAR* argv[]);

//////////////////////////////////////////////////////////////////////
// Globals

LPCTSTR g_sSavFileName = NULL;
bool g_okShowValues = false;
uint16_t g_wStartAddress = 001000;
uint16_t g_wEndAddress = 0xffff;

//////////////////////////////////////////////////////////////////////

// Названия регистров процессора
const TCHAR* REGISTER_NAME[] = { _T("R0"), _T("R1"), _T("R2"), _T("R3"), _T("R4"), _T("R5"), _T("SP"), _T("PC") };

//////////////////////////////////////////////////////////////////////


void PrintWelcome()
{
    wprintf(_T("SavDisasm Utility  by Nikita Zimin  [%S %S]\n\n"), __DATE__, __TIME__);
}

void PrintUsage()
{
    wprintf(_T("\nUsage:\n"));
    wprintf(_T("    SavDisasm [options] <SavFileFile>\n"));
    wprintf(_T("  Parameters:\n"));
    wprintf(_T("    <SavFileName> is name of .sav file to disassemble\n"));
    wprintf(_T("  Options:\n"));
    wprintf(_T("    -v        Show original values\n"));
    wprintf(_T("    -sXXXXXX  Set disassembly start address (octal)\n"));
    wprintf(_T("    -eXXXXXX  Set disassembly end address (octal)\n"));
}

// Print octal 16-bit value to buffer
// buffer size at least 7 characters
void PrintOctalValue(TCHAR* buffer, WORD value)
{
    for (int p = 0; p < 6; p++)
    {
        int digit = value & 7;
        buffer[5 - p] = _T('0') + (TCHAR)digit;
        value = (value >> 3);
    }
    buffer[6] = 0;
}

// Parse octal value from text
BOOL ParseOctalValue(LPCTSTR text, WORD* pValue)
{
    WORD value = 0;
    TCHAR* pChar = (TCHAR*)text;
    for (int p = 0;; p++)
    {
        if (p > 6) return FALSE;
        TCHAR ch = *pChar;  pChar++;
        if (ch == 0) break;
        if (ch < _T('0') || ch > _T('7')) return FALSE;
        value = (value << 3);
        TCHAR digit = ch - _T('0');
        value += digit;
    }
    *pValue = value;
    return TRUE;
}

bool ParseCommandLine(int argc, _TCHAR* argv[])
{
    for (int argn = 1; argn < argc; argn++)
    {
        LPCTSTR arg = argv[argn];
        if (arg[0] == _T('-') || arg[0] == _T('/'))
        {
            if (arg[1] == 's')
            {
                if (!ParseOctalValue(arg + 2, &g_wStartAddress))
                {
                    wprintf(_T("Failed to parse start address: %s\n"), arg);
                    return false;
                }
            }
            else if (arg[1] == 'e')
            {
                if (!ParseOctalValue(arg + 2, &g_wEndAddress))
                {
                    wprintf(_T("Failed to parse end address: %s\n"), arg);
                    return false;
                }
            }
            else if (arg[1] == 'v')
            {
                g_okShowValues = true;
            }
            else
            {
                wprintf(_T("Unknown option: %s\n"), arg);
                return false;
            }
        }
        else
        {
            if (g_sSavFileName == NULL)
                g_sSavFileName = arg;
            else
            {
                wprintf(_T("Unknown param: %s\n"), arg);
                return false;
            }
        }
    }

    // Parsed options validation
    if (g_sSavFileName == NULL)
    {
        wprintf(_T("<SavFileName> not specified.\n"));
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

        TCHAR bufaddr[8];
        TCHAR bufinstr[8];
        TCHAR bufargs[32];
        TCHAR buffer[128];
        TCHAR value0[8], value1[8], value2[8];

        int length = 0;
        length = DisassembleInstruction(pImage + address / 2, address, bufinstr, bufargs);
        PrintOctalValue(bufaddr, address);
        if (g_okShowValues)
        {
            PrintOctalValue(value0, pImage[address / 2]);
            if (length > 1) PrintOctalValue(value1, pImage[address / 2 + 2]); else *value1 = 0;
            if (length > 2) PrintOctalValue(value2, pImage[address / 2 + 4]); else *value2 = 0;
            wsprintf(buffer, _T("%s\t%s\t%s\t%s\t%-7s\t%s\r\n"), bufaddr, value0, value1, value2, bufinstr, bufargs);
        }
        else
        {
            wsprintf(buffer, _T("%s\t%-7s\t%s\r\n"), bufaddr, bufinstr, bufargs);
        }

        char ascii[256];  *ascii = 0;
        DWORD dwLength = lstrlen(buffer) * sizeof(TCHAR);
        WideCharToMultiByte(CP_ACP, 0, buffer, dwLength, ascii, 256, NULL, NULL);

        ::fwrite(ascii, 1, strlen(ascii), fpOutFile);

        address += length * 2;
        if (address == 0)
            break;
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    PrintWelcome();

    if (!ParseCommandLine(argc, argv))
    {
        PrintUsage();
        return 255;
    }

    FILE* fpFile = ::_wfopen(g_sSavFileName, _T("rb"));
    if (fpFile == NULL)
    {
        wprintf(_T("Failed to open the input file.\n"));
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
        wprintf(_T("Failed to read the input file.\n"));
        ::free(pImage);
        return 255;
    }

    FILE* fpOutFile = ::_wfopen(_T("disasm.txt"), _T("w+b"));
    if (fpOutFile == NULL)
    {
        wprintf(_T("Failed to open the output file.\n"));
        ::free(pImage);
        return 255;
    }

    DisasmSavImage(pImage, (uint16_t)lReadSize, fpOutFile);

    ::free(pImage);

    return 0;
}

//////////////////////////////////////////////////////////////////////
