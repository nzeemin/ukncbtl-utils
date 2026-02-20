#pragma once

#include <cstdint>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>

#include "rt11date.h"


//////////////////////////////////////////////////////////////////////
// RADIX50 convertion rotines headers, see rad50.cpp

void r50asc(int cnt, const uint16_t* r50, char str[]);
void irad50(int cnt, char str[], uint16_t r50[]);


//////////////////////////////////////////////////////////////////////


uint16_t get_word(const uint8_t* p, size_t offset);


//////////////////////////////////////////////////////////////////////


struct format_info
{
    bool detected;                  // indicates that the format is matched
    void* header;                   // pointer to header data or nullptr
};

struct format_enumerator
{
    std::ifstream*  pfile;          // pointer to archive file stream
    std::streampos  filesize;       // total file size
    std::streampos  nextoffset;     // next record position
};

enum format_item_type
{
    ITEMTYPE_EOF = 0,
    ITEMTYPE_ERROR = 1,
    ITEMTYPE_FILE = 2,
};

struct format_item
{
    format_item_type type;          // item type
    uint16_t        name[2];        // file name in RAD50
    uint16_t        ext;            // extension in RAD50
    std::streampos  offset;         // offset for the file record in the archive file
    uint16_t        origsizeblock;  // size of the original file in blocks
    uint32_t        compsize;       // compressed size in bytes
    uint16_t        date;           // date in RT-11 format
    uint16_t        packedchecksum; // checksum of the packed file
    uint16_t        origchecksum;   // checksum of the original file
    std::string     errorstr;       // error description for type = ITEMTYPE_ERROR

    std::string get_name_string() const
    {
        char itemname[7] = { 0 };
        r50asc(6, name, itemname);
        return itemname;
    }
    std::string get_ext_string() const
    {
        char itemext[4] = { 0 };
        r50asc(3, &ext, itemext);
        return itemext;
    }
    std::string get_date_string() const
    {
        char itemdate[12] = { 0 };
        rt11date_str(date, itemdate, sizeof(itemdate) - 1);
        return itemdate;
    }
};

struct format_extract_result
{
    void*           pdata;          // Pointer to extracted file contents, or nullptr
    uint32_t        data_size;      // Extracted data size
    std::string     errorstr;       // Error description for pdata == nullptr
};

struct format_check_result
{
    bool success;
    std::string     errorstr;       // Error description for !success
};


typedef format_info (*FORMAT_DETECT_CALLBACK)(std::ifstream & file, std::streampos file_size);

typedef format_enumerator (*FORMAT_ENUM_START_CALLBACK)(std::ifstream& file, format_info& finfo, std::streampos file_size);

typedef format_item (*FORMAT_ENUM_NEXT_CALLBACK)(format_enumerator& enumerator);

typedef format_extract_result (*FORMAT_EXTRACT_FILE_CALLBACK)(std::ifstream& file, format_info& finfo, const format_item& item);

typedef format_check_result (*FORMAT_CHECK_FILE_CALLBACK)(std::ifstream& file, format_info& finfo, const format_item& item);


//////////////////////////////////////////////////////////////////////


format_info format_lzslza_detect(std::ifstream & file, std::streampos file_size);

format_enumerator format_lzslza_enum_start(std::ifstream& file, format_info& finfo, std::streampos file_size);
format_item format_lzslza_enum_next(format_enumerator& enumerator);

format_extract_result format_lzslza_extract_file(std::ifstream& file, format_info& finfo, const format_item& item);
format_check_result format_lzslza_check_file(std::ifstream& file, format_info& finfo, const format_item& item);


//////////////////////////////////////////////////////////////////////


format_info format_fcu_detect(std::ifstream& file, std::streampos file_size);

format_enumerator format_fcu_enum_start(std::ifstream& file, format_info& finfo, std::streampos file_size);
format_item format_fcu_enum_next(format_enumerator& enumerator);

format_extract_result format_fcu_extract_file(std::ifstream& file, format_info& finfo, const format_item& item);
format_check_result format_fcu_check_file(std::ifstream& file, format_info& finfo, const format_item& item);


//////////////////////////////////////////////////////////////////////
