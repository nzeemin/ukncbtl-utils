
#include "main.h"


//////////////////////////////////////////////////////////////////////
// fcu_lzhuf.cpp

void lzhuf_init();
void lzhuf_free();

void lzhuf_decode(uint32_t len, void* data, uint32_t plen);
void* lzhuf_data();
uint32_t lzhuf_length();


//////////////////////////////////////////////////////////////////////


#pragma pack(push, 1)

// FCU archive compressed file record, 14 bytes
struct fcu_item
{
    uint16_t    name[2];        // file name in RAD50
    uint16_t    ext;            // extension in RAD50
    uint16_t    blocks;         // original file size in blocks
    uint16_t    datep;          // file date (RT-11 format), and the PROTECTED flag
    uint8_t     packedexwords;  // number of extra words in compressed part
    uint8_t     keybyte;        // 0xBE
    uint16_t    packedblocks;   // compressed file in blocks

    uint32_t get_compressed_size_bytes() const
    {
        return packedblocks * 512 + packedexwords * 2;
    }
};

#pragma pack(pop)


//////////////////////////////////////////////////////////////////////


uint16_t fcu_checksum(const uint8_t* p, size_t sizewords)
{
    uint16_t sum = 0;
    for (size_t i = 0; i < sizewords; i++)
        sum += get_word(p, i * 2);
    return sum;
}

format_info format_fcu_detect(std::ifstream& file, std::streampos file_size)
{
    format_info info;
    info.detected = false;
    info.header = nullptr;

    if (file_size < 16)
        return info;  // File too short
    //TODO: check if file too long

    // Read the record for the first file
    fcu_item rec;
    file.read((char*)&rec, sizeof(rec));
    //TODO: check if read failed

    if (rec.name[0] == 0)  // word 0 is first three chars of the file name
        return info;

    uint16_t origsizeblocks = rec.blocks;
    if (origsizeblocks == 0)
        return info;

    if (rec.keybyte != 0xBE)
        return info;

    //if (file_size < rec.get_compressed_size_bytes() + 14)
    //    return false;

    info.detected = true;
    return info;
}

format_enumerator format_fcu_enum_start(std::ifstream& file, format_info& finfo, std::streampos file_size)
{
    format_enumerator enumerator;
    enumerator.pfile = &file;
    enumerator.filesize = file_size;
    enumerator.nextoffset = 0;  // offset for the first item

    return enumerator;
}

format_item format_fcu_enum_next(format_enumerator& enumerator)
{
    format_item item = {};

    std::streampos offset = enumerator.nextoffset;
    item.offset = offset;

    if (offset >= enumerator.filesize)
    {
        item.type = ITEMTYPE_EOF;
        return item;
    }

    // read the next record from file
    fcu_item rec;
    enumerator.pfile->seekg(offset);
    enumerator.pfile->read((char*)&rec, sizeof(rec));
    if (enumerator.pfile->fail())
    {
        item.type = ITEMTYPE_ERROR;
        item.errorstr = "File read error.";
        return item;
    }

    uint16_t word0 = rec.name[0];
    if (word0 == 0)  // End of the archive
    {
        item.type = ITEMTYPE_EOF;
        return item;
    }

    if (rec.keybyte != 0xBE)  // bad record, wrong key byte
    {
        item.type = ITEMTYPE_ERROR;
        item.errorstr = "Wrong key byte.";
        return item;
    }

    item.type = ITEMTYPE_FILE;
    item.name[0] = rec.name[0];
    item.name[1] = rec.name[1];
    item.ext = rec.ext;
    item.origsizeblock = rec.blocks;
    item.compsize = rec.get_compressed_size_bytes();
    item.date = rec.datep & 0x7fff;  // get date without PROTECTED bit
    //TODO: checksum = rec.checksum;
    //TODO: PROTECTED bit

    // move enumerator's offset to the next record
    enumerator.nextoffset += 14 + item.compsize;

    return item;
}

format_extract_result format_fcu_extract_file(std::ifstream& file, format_info& finfo, const format_item& item)
{
    format_extract_result result;
    result.pdata = nullptr;
    result.data_size = 0;

    if (item.type != ITEMTYPE_FILE)
    {
        result.errorstr = "Item is not a file.";
        return result;
    }

    int origsize = item.origsizeblock * 512;
    size_t sizecompressed = item.compsize;  // compressed block, plus 2 checksum words
    uint8_t* pcomp = (uint8_t*)calloc(sizecompressed, 1);

    // read compressed block
    file.seekg(item.offset + (std::streampos)14);
    file.read((char*)pcomp, sizecompressed);
    if (file.fail())
    {
        free(pcomp);
        result.errorstr = "File read error";
        return result;
    }

    // decompress
    lzhuf_init();
    lzhuf_decode(origsize, (void*)pcomp, (uint32_t)(sizecompressed - 4));
    uint32_t destsize = lzhuf_length();
    uint8_t* pdecompressed = (uint8_t*)lzhuf_data();

    uint16_t destcrc = fcu_checksum(pdecompressed, origsize / 2);
    lzhuf_free();

    uint16_t wordcrc = get_word(pcomp, sizecompressed - 4);
    //uint16_t wordcrc2 = get_word(pcomp, sizecompressed - 2);

    free(pcomp);

    bool crcgood = destcrc == wordcrc;

    if (!crcgood)
    {
        free(pdecompressed);
        result.errorstr = "Checksum mismatch.";
        return result;
    }

    result.pdata = pdecompressed;
    result.data_size = origsize;
    return result;
}

format_check_result format_fcu_check_file(std::ifstream& file, format_info& finfo, const format_item& item)
{
    format_check_result result;
    result.success = false;

    if (item.type != ITEMTYPE_FILE)
    {
        result.errorstr = "Item is not a file.";
        return result;
    }

    int origsize = item.origsizeblock * 512;
    size_t sizecompressed = item.compsize;  // compressed block, plus 2 checksum words
    uint8_t* pcomp = (uint8_t*)calloc(origsize, 1);

    // read compressed block
    file.seekg(item.offset + (std::streampos)14);
    file.read((char*)pcomp, sizecompressed);
    if (file.fail())
    {
        free(pcomp);
        result.errorstr = "File read error";
        return result;
    }

    // decompress
    lzhuf_init();
    lzhuf_decode(origsize, (void*)pcomp, (uint32_t)(sizecompressed - 4));
    uint32_t destsize = lzhuf_length();
    uint8_t* pdecompressed = (uint8_t*)lzhuf_data();

    uint16_t destcrc = fcu_checksum(pdecompressed, origsize / 2);
    lzhuf_free();

    uint16_t wordcrc = get_word(pcomp, sizecompressed - 4);
    //uint16_t wordcrc2 = get_word(pcomp, sizecompressed - 2);

    free(pdecompressed);
    free(pcomp);

    bool crcgood = destcrc == wordcrc;

    result.success = crcgood;
    if (!crcgood)
        result.errorstr = "Checksum mismatch.";
    return result;
}


//////////////////////////////////////////////////////////////////////
