/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#include "SavDisasm.h"


#ifdef _MSC_VER
#define OPTIONCHAR '/'
#define OPTIONSTR "/"
#else
#define OPTIONCHAR '-'
#define OPTIONSTR "-"
#endif

typedef std::string string;


//////////////////////////////////////////////////////////////////////
// Preliminary function declarations

void PrintWelcome();
void PrintUsage();
bool ParseCommandLine(int argc, char* argv[]);


//////////////////////////////////////////////////////////////////////
// Globals

string g_sSavFileName;
bool g_okShowValues = false;
uint16_t g_wStartAddress = 001000;
uint16_t g_wEndAddress = 0xffff;
string g_sOutFileName;


//////////////////////////////////////////////////////////////////////


void PrintWelcome()
{
    std::cout << "SavDisasm Utility  by Nikita Zimin  [" << __DATE__ << " " << __TIME__ << "]" << std::endl << std::endl;
}

void PrintUsage()
{
    std::cout << "\nUsage:" << std::endl
            << "    savdisasm [options] <SavFileFile>" << std::endl
            << "  Parameters:" << std::endl
            << "    <SavFileName> is name of .SAV file to disassemble" << std::endl
            << "  Options:" << std::endl
            << "    " OPTIONSTR "o:<OutFileName>  Set output file name" << std::endl
            << "    " OPTIONSTR "s<addr>  Set disassembly start address (octal)" << std::endl
            << "    " OPTIONSTR "e<addr>  Set disassembly end address (octal)" << std::endl
            << "    " OPTIONSTR "v        Show original values" << std::endl;
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
bool ParseOctalValue(const char * text, uint16_t* pValue)
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
        const char * arg = argv[argn];
        if (arg[0] == (OPTIONCHAR))
        {
            char option = (char)tolower(arg[1]);
            switch (option)
            {
            case ('s'):  // Set disasm Start address
                if (!ParseOctalValue(arg + 2, &g_wStartAddress))
                {
                    std::cout << "Failed to parse start address: " << arg << std::endl;
                    return false;
                }
                break;
            case ('e'):  // Set disasm End address
                if (!ParseOctalValue(arg + 2, &g_wEndAddress))
                {
                    std::cout << "Failed to parse end address: " << arg << std::endl;
                    return false;
                }
                break;
            case ('v'):  // Show original Values in the disassembly
                g_okShowValues = true;
                break;
            case ('o'):  // Set Output file name
                {
                    const char * pOuFileName = arg + 2;
                    if (*pOuFileName == (':')) pOuFileName++;  // Skip ':' if found
                    if (*pOuFileName == 0)
                    {
                        std::cout << "Output file name is not specified: " << arg << std::endl;
                        return false;
                    }
                    g_sOutFileName = pOuFileName;
                    break;
                }
            default:
                std::cout << "Unknown option: " << arg << std::endl;
                return false;
            }
        }
        else
        {
            if (g_sSavFileName.empty())
                g_sSavFileName = arg;
            else
            {
                std::cout << "Unknown param: " << arg << std::endl;
                return false;
            }
        }
    }

    // Parsed options validation
    if (g_sSavFileName.empty())
    {
        std::cout << "<SavFileName> not specified." << std::endl;
        return false;
    }

    return true;
}

void DisasmSavImage(uint16_t* pImage, std::ofstream & foutfile)
{
    uint16_t address = g_wStartAddress;

    for (;;)
    {
        if (address >= g_wEndAddress)
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
            sprintf(buffer, "%s\t%s\t%s\t%s\t%-7s\t%s", bufaddr, value0, value1, value2, bufinstr, bufargs);
            //sprintf(buffer, "%s: %s\t%s\t%s\t%-7s\t%s", bufaddr, value0, value1, value2, bufinstr, bufargs);
        }
        else
        {
            sprintf(buffer, "%s\t%-7s\t%s", bufaddr, bufinstr, bufargs);
            //sprintf(buffer, "%s: %-7s\t%s", bufaddr, bufinstr, bufargs);
        }

        foutfile << buffer << std::endl;

        address += (uint16_t)length * 2;
        if (address == 0)
            break;
    }
}

int main(int argc, char* argv[])
{
    PrintWelcome();

    if (!ParseCommandLine(argc, argv))
    {
        if (g_sSavFileName.empty())
            PrintUsage();
        return 255;
    }

    if (g_sOutFileName.empty())
    {
        g_sOutFileName = "disasm.txt";
    }

    std::cout << "Input file: " << g_sSavFileName << std::endl;
    std::ifstream savfile(g_sSavFileName.c_str(), std::ifstream::ate | std::ifstream::binary);
    if (savfile.fail())
    {
        std::cout << "Failed to open the input file." << std::endl;
        return 255;
    }

    uint16_t* pImage = (uint16_t*) ::calloc(65536, 1);
    if (pImage == nullptr)
    {
        savfile.close();
        std::cout << "Failed to allocate memory." << std::endl;
        return 255;
    }

    std::streamoff lFileSize = savfile.tellg();
    std::streamoff lReadSize = (lFileSize < 65535) ? lFileSize : 65535;
    savfile.seekg(std::ifstream::beg);
    savfile.read((char*)pImage, lReadSize);
    if (savfile.fail())
    {
        savfile.close();
        std::cout << "Failed to read the input file." << std::endl;
        ::free(pImage);
        return 255;
    }
    savfile.close();
    std::cout << "Input file size:   " << std::setw(6) << std::oct << std::setfill('0') << lFileSize
            << "  " << std::setw(4) << std::hex << lFileSize
            << "  " << std::setw(5) << std::dec << std::setfill(' ') << lFileSize
            << ". bytes" << std::endl;

    // .SAV file analysis
    uint16_t wStartAddr = *(pImage + 040 / 2);
    uint16_t wStackAddr = *(pImage + 042 / 2);
    uint16_t wTopAddr = *(pImage + 050 / 2);
    std::cout << "SAV Start addr:    " << std::setw(6) << std::oct << std::setfill('0') << wStartAddr
            << "  " << std::setw(4) << std::hex << wStartAddr
            << "  " << std::setw(5) << std::dec << std::setfill(' ') << wStartAddr << "." << std::endl;
    std::cout << "SAV Stack addr:    " << std::setw(6) << std::oct << std::setfill('0') << wStackAddr
            << "  " << std::setw(4) << std::hex << wStackAddr
            << "  " << std::setw(5) << std::dec << std::setfill(' ') << wStackAddr << "." << std::endl;
    std::cout << "SAV Top addr:      " << std::setw(6) << std::oct << std::setfill('0') << wTopAddr
            << "  " << std::setw(4) << std::hex << wTopAddr
            << "  " << std::setw(5) << std::dec << std::setfill(' ') << wTopAddr << "." << std::endl;

    std::cout << "Output file: " << g_sOutFileName << std::endl;
    std::ofstream foutfile(g_sOutFileName.c_str(), std::ofstream::out | std::ofstream::binary);
    if (foutfile.fail())
    {
        std::cout << "Failed to open the output file." << std::endl;
        ::free(pImage);
        return 255;
    }

    if (g_wEndAddress > lReadSize) g_wEndAddress = (uint16_t)lReadSize;

    std::cout << "Disasm start:      " << std::setw(6) << std::oct << std::setfill('0') << g_wStartAddress
            << "  " << std::setw(4) << std::hex << g_wStartAddress
            << "  " << std::setw(5) << std::dec << std::setfill(' ') << g_wStartAddress << "." << std::endl;
    std::cout << "Disasm end:        " << std::setw(6) << std::oct << std::setfill('0') << g_wEndAddress
            << "  " << std::setw(4) << std::hex << g_wEndAddress
            << "  " << std::setw(5) << std::dec << std::setfill(' ') << g_wEndAddress << "." << std::endl;

    DisasmSavImage(pImage, foutfile);

    foutfile.flush();

    std::streamoff lOutFileSize = foutfile.tellp();
    foutfile.close();
    std::cout << "Output file size: " << lOutFileSize << ". bytes" << std::endl;

    ::free(pImage);

    return 0;
}


//////////////////////////////////////////////////////////////////////
