
#include "main.h"

#ifdef _MSC_VER
#define OPTIONCHAR '/'
#define OPTIONSTR "/"
#else
#define OPTIONCHAR '-'
#define OPTIONSTR "-"
#endif

typedef std::string string;


string g_sInputFileName;
string g_sOutFileName;
uint16_t g_wBaseAddress = 0;
uint16_t g_wStartAddress = 0;
uint16_t g_wEndAddress = 0xffff;

std::array<uint8_t, 65536> g_memdmp;
std::string g_commanddisasm;


void PrintWelcome()
{
    std::cout << "Z80-to-PDP11 code converter utility  by nzeemin  [" << __DATE__ << " " << __TIME__ << "]" << std::endl;
    std::cout << "WARNING: This is NOT a reliable convertor, use on your own risk, review all the results!!!" << std::endl << std::endl;
}

void PrintUsage()
{
    std::cout << "\nUsage:" << std::endl
            << "    recompiler [options] <InputFileFile>" << std::endl
            << "  Parameters:" << std::endl
            << "    <InputFileName> is name of memory image with Z80 code" << std::endl
            << "  Options:" << std::endl
            << "    " OPTIONSTR "o:<OutFileName>  Set output file name" << std::endl
            << "    " OPTIONSTR "b<addr>  Set base address for the input file (4-digit hex)" << std::endl
            << "    " OPTIONSTR "s<addr>  Set conversion start address (4-digit hex)" << std::endl
            << "    " OPTIONSTR "e<addr>  Set conversion end address (4-digit hex)" << std::endl;
}

// Parse hex value from text
bool ParseHexValue(const char* text, uint16_t* pValue)
{
    uint16_t value = 0;
    char* pChar = (char*)text;
    for (int p = 0;; p++)
    {
        if (p > 4) return false;
        char ch = *pChar;  pChar++;
        if (ch == 0) break;
        if (ch >= '0' && ch <= '9')
        {
            value = (value << 4);
            char digit = ch - '0';
            value = value + digit;
        }
        else if (ch >= 'a' && ch <= 'f')
        {
            value = (value << 4);
            char digit = ch - 'a' + 10;
            value = value + digit;
        }
        else if (ch >= 'A' && ch <= 'F')
        {
            value = (value << 4);
            char digit = ch - 'A' + 10;
            value = value + digit;
        }
        else
            return false;
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
            case ('b'):  // Set Base address
                if (!ParseHexValue(arg + 2, &g_wBaseAddress))
                {
                    std::cout << "Failed to parse base address: " << arg << std::endl;
                    return false;
                }
                break;
            case ('s'):  // Set Start address
                if (!ParseHexValue(arg + 2, &g_wStartAddress))
                {
                    std::cout << "Failed to parse conversion start address: " << arg << std::endl;
                    return false;
                }
                break;
            case ('e'):  // Set End address
                if (!ParseHexValue(arg + 2, &g_wEndAddress))
                {
                    std::cout << "Failed to parse conversion end address: " << arg << std::endl;
                    return false;
                }
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
            if (g_sInputFileName.empty())
                g_sInputFileName = arg;
            else
            {
                std::cout << "Unknown param: " << arg << std::endl;
                return false;
            }
        }
    }

    // Parsed options validation
    if (g_sInputFileName.empty())
    {
        std::cout << "<InputFileName> is not specified." << std::endl;
        return false;
    }

    return true;
}

int z80disasm(string& str, uint8_t* bytes)
{
    char buffer[18];

    int cmdlen = DAsm(buffer, bytes);

    str = buffer;

    return cmdlen;
}

void printcommandhex(std::ostream& sout, int addr, int cmdlen)
{
    std::ios_base::fmtflags ff(std::cout.flags());  // store flags

    for (int i = 0; i < 5; i++)
    {
        if (i < cmdlen)
        {
            uint8_t b = g_memdmp[addr + i];
            sout << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)b << " ";
            sout.flags(ff);  // restore flags
        }
        else
        {
            sout << "   ";
        }
    }
}

// Calculate string width, knowing about 8-char tabs
int getstringwidth(string& str)
{
    int width = 0;
    for (std::string::iterator it = str.begin(); it != str.end(); ++it)
    {
        char ch = *it;
        if (ch == '\t')
            width = (width + 7) / 8 * 8;
        else
            width++;

    }
    return width;
}

int main(int argc, char* argv[])
{
    std::ios_base::fmtflags coutf(std::cout.flags());  // store flags

    PrintWelcome();

    // Initialization
    g_memdmp.fill(0);

    // Parse command line
    if (!ParseCommandLine(argc, argv))
    {
        if (g_sInputFileName.empty())
            PrintUsage();
        return 255;
    }

    if (g_sOutFileName.empty())
    {
        g_sOutFileName = "converted.txt";
    }

    // Load memory image from the source file
    std::ifstream srcfile(g_sInputFileName, std::ifstream::binary | std::ifstream::ate);
    if (srcfile.fail())
    {
        std::cerr << "Failed to open source file." << std::endl;
        exit(1);
    }

    std::streamoff srcsize = srcfile.tellg();
    srcfile.seekg(0, std::ifstream::beg);

    uint16_t base = g_wBaseAddress;
    //TODO: Check if the file fit in the array

    srcfile.read((char*)g_memdmp.data() + base, srcsize);
    if (srcfile.fail())
    {
        std::cerr << "Failed to read source file." << std::endl;
        exit(1);
    }

    // Create output file
    std::cout << "Output file: " << g_sOutFileName << std::endl;
    std::ofstream foutfile(g_sOutFileName.c_str(), std::ofstream::out);
    if (foutfile.fail())
    {
        std::cout << "Failed to open the output file." << std::endl;
        return 255;
    }
    std::ios_base::fmtflags ff(foutfile.flags());  // store flags

    std::cout << "Convertion start address: 0x"
            << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << (int)g_wStartAddress
            << ", end address: 0x"
            << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << (int)g_wEndAddress << std::endl;
    std::cout.flags(coutf);  // restore flags

    int processedCommands = 0, convertedCommands = 0;
    int addr = g_wStartAddress;
    while (addr <= g_wEndAddress)
    {
        int cmdlen = z80disasm(g_commanddisasm, g_memdmp.data() + addr);

        // Call recompiler
        string result = recomp(addr);
        // Replace first space to tab
        size_t resultspacepos = result.find(" ");
        if (resultspacepos != string::npos)
            result.replace(resultspacepos, 1, "\t");
        // Trim trailing spaces

        foutfile << "L" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << addr << ":\t";
        foutfile.flags(ff);  // restore flags

        //printcommandhex(foutfile, addr, cmdlen);

        if (result.empty())
        {
            result = "???";  // Not converted

            std::cout << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << addr << ":  ";
            std::cout.flags(coutf);  // restore flags
            printcommandhex(std::cout, addr, cmdlen);
            std::cout << "  " << std::setw(18) << std::setfill(' ') << std::left << g_commanddisasm.c_str() << "  ";
            std::cout << std::endl;
            std::cout.flags(coutf);  // restore flags
        }

        foutfile << result.c_str();

        if (result.find("???") == string::npos)  // Fully converted?
            convertedCommands++;

        int tabnum = getstringwidth(result) / 8;
        tabnum = tabnum >= 3 ? 0 : 3 - tabnum;
        for (int i = 0; i < tabnum; i++)
            foutfile << "\t";

        foutfile << "\t; " << g_commanddisasm.c_str();

        foutfile.flags(ff);  // restore flags
        foutfile << std::endl;

        processedCommands++;
        addr += cmdlen;
    }

    std::cout << "Processed commands: " << processedCommands
            << ", converted: " << convertedCommands
            << ", unconverted: " << (processedCommands - convertedCommands) << std::endl;
}
