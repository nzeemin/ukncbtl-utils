// UkncArcMan.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#define _CRT_SECURE_NO_WARNINGS

#include "main.h"


struct FormatDescriptor
{
    const char* Name;
    FORMAT_DETECT_CALLBACK DetectFunc;
    FORMAT_ENUM_START_CALLBACK EnumStartFunc;
    FORMAT_ENUM_NEXT_CALLBACK EnumNextFunc;
    FORMAT_EXTRACT_FILE_CALLBACK ExtractFileFunc;
    FORMAT_CHECK_FILE_CALLBACK CheckItemFunc;
}
static g_FormatDescriptors[] =
{
    {
        "FCU",
        format_fcu_detect,
        format_fcu_enum_start, format_fcu_enum_next,
        format_fcu_extract_file, format_fcu_check_file
    },
    {
        "LZS/LZA",
        format_lzslza_detect,
        format_lzslza_enum_start, format_lzslza_enum_next,
        format_lzslza_extract_file, format_lzslza_check_file
    },
};
static const size_t g_FormatInfos_count = (size_t)(sizeof(g_FormatDescriptors) / sizeof(FormatDescriptor));


std::string g_sCommand;
std::string g_sInputFileName;

FormatDescriptor* g_pfdesc = nullptr;


// Trim from the end (in place)
inline void rtrim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch)
    {
        return !std::isspace(ch);
    }).base(), s.end());
}

uint16_t get_word(const uint8_t* p, size_t offset)
{
    return ((uint16_t) * (p + offset)) + ((uint16_t)(*(p + offset + 1)) << 8);
}


void command_list_files(std::ifstream& file, format_info& finfo, std::streampos file_size)
{
    std::cout << "  Offset     Name        Date       Original    Compressed\n";
    format_enumerator enumerator = g_pfdesc->EnumStartFunc(file, finfo, file_size);
    int file_count = 0;
    int block_count = 0;
    for (; ; )
    {
        format_item item = g_pfdesc->EnumNextFunc(enumerator);
        if (item.type == ITEMTYPE_EOF)
            break;

        std::cout << std::setw(8) << std::setfill('0') << std::hex << item.offset << std::dec << std::setfill(' ') << "  ";

        if (item.type == ITEMTYPE_ERROR)
        {
            std::cout << "== BAD RECORD: " << item.errorstr << " ==" << std::endl;
            std::cout << "COMMAND ABORTED!" << std::endl;
            break;
        }

        file_count++;
        block_count += item.origsizeblock;

        std::cout << item.get_name_string() << "." << item.get_ext_string() << "  ";
        std::cout << item.get_date_string() << "  ";
        std::cout << std::setw(3) << item.origsizeblock << " blocks  " << std::setw(5) << item.compsize << " bytes";
        std::cout << std::endl;
    }

    std::cout << "  " << file_count << " files, " << block_count << " blocks." << std::endl;
}

void command_extract_files(std::ifstream& file, format_info& finfo, std::streampos file_size)
{
    std::cout << "  Offset     Name        Date       Original    Compressed   Result\n";
    format_enumerator enumerator = g_pfdesc->EnumStartFunc(file, finfo, file_size);
    int file_count = 0;
    int block_count = 0;
    int saved_count = 0, failed_count = 0;
    for (; ; )
    {
        format_item item = g_pfdesc->EnumNextFunc(enumerator);
        if (item.type == ITEMTYPE_EOF)
            break;

        std::cout << std::setw(8) << std::setfill('0') << std::hex << item.offset << std::dec << std::setfill(' ') << "  ";

        if (item.type == ITEMTYPE_ERROR)
        {
            std::cout << "== BAD RECORD: " << item.errorstr << " ==" << std::endl;
            std::cout << "COMMAND ABORTED!" << std::endl;
            break;
        }

        file_count++;
        block_count += item.origsizeblock;

        std::cout << item.get_name_string() << "." << item.get_ext_string() << "  ";
        std::cout << item.get_date_string() << "  ";
        std::cout << std::setw(3) << item.origsizeblock << " blocks  " << std::setw(5) << item.compsize << " bytes";
        std::cout << "   ";

        format_extract_result extractres = g_pfdesc->ExtractFileFunc(file, finfo, item);
        if (extractres.pdata == nullptr)
        {
            failed_count++;
            std::cout << "FAILED: " << extractres.errorstr;
        }
        else
        {
            std::string outfilename = item.get_name_string();
            rtrim(outfilename);
            outfilename += ".";
            outfilename += item.get_ext_string();

            // save the file
            std::ofstream outfile(outfilename, std::ios::out | std::ios::binary);
            if (outfile.fail())
            {
                failed_count++;
                std::cout << "FAILED to open file \"" << outfilename << "\"";
            }
            else
            {
                outfile.write((const char*)extractres.pdata, extractres.data_size);
                if (outfile.fail())
                {
                    failed_count++;
                    std::cout << "FAILED: file write error";
                }
                else
                {
                    saved_count++;
                    std::cout << "Saved " << outfilename;
                }

                outfile.close();
            }

            free(extractres.pdata);
        }

        std::cout << std::endl;
    }

    std::cout << "  " << file_count << " files, " << block_count << " blocks." << std::endl;
    std::cout << "  Saved " << saved_count << " files, failed " << failed_count << " files." << std::endl;
}

void command_check_files(std::ifstream& file, format_info& finfo, std::streampos file_size)
{
    std::cout << "  Offset      Name      Original    Compressed   Check\n";
    format_enumerator enumerator = g_pfdesc->EnumStartFunc(file, finfo, file_size);
    int file_count = 0;
    int block_count = 0;
    int good_count = 0, failed_count = 0;
    for (; ; )
    {
        format_item item = g_pfdesc->EnumNextFunc(enumerator);
        if (item.type == ITEMTYPE_EOF)
            break;

        std::cout << std::setw(8) << std::setfill('0') << std::hex << item.offset << std::dec << std::setfill(' ');
        std::cout << "   ";

        if (item.type == ITEMTYPE_ERROR)
        {
            std::cout << "== BAD RECORD: " << item.errorstr << " ==" << std::endl;
            std::cout << "COMMAND ABORTED!" << std::endl;
            break;
        }

        file_count++;
        block_count += item.origsizeblock;

        std::cout << item.get_name_string() << "." << item.get_ext_string() << "  ";
        std::cout << std::setw(3) << item.origsizeblock << " blocks  " << std::setw(5) << item.compsize << " bytes";
        std::cout << "   ";

        format_check_result checkres = g_pfdesc->CheckItemFunc(file, finfo, item);
        if (checkres.success)
        {
            good_count++;
            std::cout << "OK";
        }
        else
        {
            failed_count++;
            std::cout << "FAILED: " << checkres.errorstr;
        }

        std::cout << std::endl;
    }

    std::cout << "  " << file_count << " files, " << block_count << " blocks." << std::endl;
    std::cout << "  Check passed " << good_count << " files, failed " << failed_count << " files." << std::endl;
}

bool parse_command_line(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        const char* arg = argv[i];
        if (arg[0] == '-')
        {
            std::cout << "Unknown option." << std::endl;
            return false;
        }
        if (g_sCommand.empty())
            g_sCommand = arg;
        else if (g_sInputFileName.empty())
            g_sInputFileName = arg;
    }

    if (g_sCommand.empty())
    {
        std::cout << "Command not specified." << std::endl;
        return false;
    }
    if (g_sInputFileName.empty())
    {
        std::cout << "Input file name is not specified." << std::endl;
        return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    std::cout << "UkncArcMan\n";

    if (!parse_command_line(argc, argv))
    {
        //PrintUsage();
        return 255;
    }

    std::cout << "Command: " << g_sCommand << std::endl;
    std::cout << "Input file: " << g_sInputFileName << std::endl;


    std::ifstream file(g_sInputFileName, std::ios::in | std::ios::binary);
    if (file.fail())
    {
        std::cout << "Failed to open the input file (error " << errno << ": " << strerror(errno) << ")." << std::endl;
        return 1;
    }

    // Detect file size
    file.seekg(0, std::ios::end);
    std::streampos file_size = file.tellg();
    std::cout << "File size: " << file_size << " bytes\n";

    // Trying to recognize the file format
    FormatDescriptor* pfdesc = nullptr;
    format_info finfo;
    for (int i = 0; i < g_FormatInfos_count; i++)
    {
        file.seekg(0, std::ios::beg);

        finfo = g_FormatDescriptors[i].DetectFunc(file, file_size);
        if (finfo.detected)
        {
            pfdesc = g_FormatDescriptors + i;
            break;
        }
        else
        {
            //TODO: destruct format_info
        }
    }
    if (pfdesc == nullptr)
    {
        std::cout << "Format not recognized for file " << g_sInputFileName << std::endl;
        file.close();
        return 2;
    }

    g_pfdesc = pfdesc;
    std::cout << "Detected format: " << g_pfdesc->Name << std::endl;


    // Show file listing using EnumStartFunc/EnumNextFunc
    if (pfdesc->EnumStartFunc == nullptr || g_pfdesc->EnumNextFunc == nullptr)
    {
        std::cout << "Format listing is not implemented for " << g_pfdesc->Name << std::endl;
        file.close();
        exit(1);
    }

    if (g_sCommand == "l")
        command_list_files(file, finfo, file_size);
    else if (g_sCommand == "x")
        command_extract_files(file, finfo, file_size);
    else if (g_sCommand == "t")
        command_check_files(file, finfo, file_size);
    else
    {
        std::cout << "Unknown command: " << g_sCommand << std::endl;
        file.close();
        exit(1);
    }

    file.close();

    return 0;
}
