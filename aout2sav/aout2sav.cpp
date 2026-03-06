#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <iomanip>
#include <map>

struct Options
{
    std::string in_filename;
    std::string out_filename;
    uint16_t stack;
};

void printUsage()
{
    std::cout << "aout2sav utility, compiled at [" << __DATE__ << "]" << std::endl;
    std::cout << "Usage: aout2sav [OPTIONS] FILENAME" << std::endl;
    std::cout << "  FILENAME  object file in a.out format, usually produced with pdp11-aout-ld" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -o NAME, --out-file=NAME  filename to store resulting binary" << std::endl;
    std::cout << "  --stack=VALUE  stack address, NNNNN for decimal, 0NNNNN for octal; default is 001000 octal" << std::endl;
}

bool parseArgs(int argc, char* argv[], Options& opts)
{
    opts.stack = 01000;  // default stack address

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            printUsage();
            return false;
        }
        else if (arg == "-o" || arg == "--out-file")
        {
            if (i + 1 < argc)
            {
                opts.out_filename = argv[++i];
            }
            else
            {
                std::cerr << "Error: " << arg << " requires an argument." << std::endl;
                return false;
            }
        }
        else if (arg.rfind("--out-file=", 0) == 0)
        {
            opts.out_filename = arg.substr(11);
        }
        else if (arg.rfind("--stack=", 0) == 0)
        {
            uint16_t stack = static_cast<uint16_t>(strtoul(arg.substr(8).c_str(), nullptr, 0));
            if (stack == 0)
            {
                std::cerr << "Error: invalid stack value." << std::endl;
                return false;
            }
            opts.stack = stack;
        }
        else if (arg[0] != '-')
        {
            opts.in_filename = arg;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            return false;
        }
    }

    return !opts.in_filename.empty();
}

std::string getMagicDescription(uint16_t magic)
{
    switch (magic)
    {
    case 263: return "normal";              // 0407
    case 264: return "read-only text";      // 0410
    case 265: return "separated I&D";       // 0411
    case 261: return "overlay";             // 0405
    case 280: return "auto-overlay (nonseparate)"; // 0430
    case 281: return "auto-overlay (separate)";    // 0431
    default: return "unknown";
    }
}

int main(int argc, char* argv[])
{
    Options options;

    if (!parseArgs(argc, argv, options))
    {
        if (options.in_filename.empty() && argc > 1)
        {
            return 1;
        }
        printUsage();
        return 1;
    }

    // Read input file
    std::ifstream infile(options.in_filename, std::ios::binary);
    if (!infile)
    {
        std::cerr << "Error: Cannot open input file: " << options.in_filename << std::endl;
        return 1;
    }

    // Get file size
    infile.seekg(0, std::ios::end);
    size_t fileSize = infile.tellg();
    infile.seekg(0, std::ios::beg);

    // Read entire file
    std::vector<uint8_t> bin(fileSize);
    infile.read(reinterpret_cast<char*>(bin.data()), fileSize);
    infile.close();

    if (fileSize < 16)
    {
        std::cerr << "Error: File too small to contain a.out header" << std::endl;
        return 1;
    }

    // Parse header (16 bytes = 8 uint16_t, little-endian)
    uint16_t a_magic    = bin[0]  | (bin[1]  << 8);
    uint16_t a_text     = bin[2]  | (bin[3]  << 8);
    uint16_t a_data     = bin[4]  | (bin[5]  << 8);
    //uint16_t a_bss      = bin[6]  | (bin[7]  << 8);
    //uint16_t a_syms     = bin[8]  | (bin[9]  << 8);
    uint16_t a_entry    = bin[10] | (bin[11] << 8);
    //uint16_t a_unused   = bin[12] | (bin[13] << 8);
    //uint16_t a_flag     = bin[14] | (bin[15] << 8);

    // Print header info
    std::cout << "Header info:" << std::endl;
    std::cout << "  magic number:               " << std::oct << a_magic << std::dec
            << " = " << getMagicDescription(a_magic) << std::endl;
    std::cout << "  size of text segment:       " << a_text << std::endl;
    std::cout << "  size of initialized data:   " << a_data << std::endl;
    //std::cout << "  size of uninitialized data: " << a_bss << std::endl;
    //std::cout << "  size of symbol table:       " << a_syms << std::endl;
    std::cout << "  entry point:                " << std::oct << a_entry << std::dec << " octal" << std::endl;
    //std::cout << "  relocation info stripped:   " << a_flag << std::endl;

    uint16_t text_size = a_text + a_data;
    std::cout << "  total data size:            " << text_size << std::endl;

    std::cout << "Stack address:                " << std::oct << options.stack << std::dec << " octal" << std::endl;

    // Determine output filename
    std::string out_filename;
    if (!options.out_filename.empty())
    {
        out_filename = options.out_filename;
    }
    else
    {
        // Extract base name (remove extension) and convert to uppercase
        size_t dotPos = options.in_filename.find_last_of('.');
        std::string baseName;
        if (dotPos != std::string::npos)
        {
            baseName = options.in_filename.substr(0, dotPos);
        }
        else
        {
            baseName = options.in_filename;
        }

        // Convert to uppercase
        for (char & c : baseName)
        {
            c = std::toupper(static_cast<unsigned char>(c));
        }
        out_filename = baseName + ".SAV";
    }

    // Extract text segment
    // Text segment starts at offset 0o20 = 16 bytes (octal)
    const size_t header_size = 16;  // 0o20
    std::vector<uint8_t> text;

    std::ofstream outfile(out_filename, std::ios::binary);
    if (!outfile)
    {
        std::cerr << "Error: Cannot create output file: " << out_filename << std::endl;
        return 1;
    }

    text.assign(bin.begin() + header_size, bin.begin() + header_size + text_size);

    uint16_t bin_start = a_entry;

    // Align text to block boundary
    uint16_t aligned_size = static_cast<uint16_t>( ((text.size() + 511) / 512) * 512 );
    while (text.size() < aligned_size)
        text.push_back(0);

    // Prepare .SAV header
    std::vector<uint8_t> savheader(512);
    uint8_t* block0 = savheader.data();
    block0[0x20] = bin_start & 0xFF;
    block0[0x21] = (bin_start >> 8) & 0xFF;
    // stack pointer = 0x200 (01000)
    block0[0x22] = options.stack & 0xFF;
    block0[0x23] = (options.stack >> 8) & 0xFF;
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

    outfile.write(reinterpret_cast<const char*>(savheader.data()), savheader.size());

    outfile.write(reinterpret_cast<const char*>(text.data()), text.size());

    size_t outfile_size = outfile.tellp();

    outfile.close();

    std::cout << "Saved file " << out_filename << ", " << outfile_size << " bytes.";

    return 0;
}