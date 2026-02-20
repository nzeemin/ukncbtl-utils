
#define _CRT_SECURE_NO_WARNINGS

#include "main.h"


//////////////////////////////////////////////////////////////////////
// lzslza_lzss.cpp

int uza_lzss_decode(
    const unsigned char* inbuf, size_t inlen,
    unsigned char* outbuf, size_t outbuf_size,
    int bufsize);
int uzs_lzss_decode(
    const unsigned char* inbuf, size_t inlen,
    unsigned char* outbuf, size_t outbuf_size,
    int bufsize);


//////////////////////////////////////////////////////////////////////


#pragma pack(push, 1)

struct lzslza_header
{
    uint16_t    signature;      // 0x4F23 = "LZS" or 0x4F11 = "LZA"
    char        version[2];     // version number, for example "21" or "22"
    uint16_t    buffersize;     // size of the ring buffer: 2048 or 4096
    uint16_t    date;           // archive create date
    uint16_t    password;       // password or 0
    uint16_t    reserved;       // 0
    uint16_t    catsize;        // archive catalog size; 0 = no catalog
    uint16_t    catblock;       // archive catalog start block; 0 = no catalog
};

struct lzslza_item
{
    uint16_t    name[2];        // file name in RAD50
    uint16_t    ext;            // extension in RAD50
    uint16_t    blocks;         // original file size in blocks
    uint16_t    date;           // file date (RT-11 format)
    uint16_t    checksum;       // packed file checksum
    uint32_t    packedsize;     // packed file size in bytes
};

#pragma pack(pop)


//////////////////////////////////////////////////////////////////////



uint16_t lzslza_checksum(const uint8_t* p, size_t sizewords)
{
    uint16_t sum = 0;
    for (size_t i = 0; i < sizewords; i++)
        sum += (uint16_t)~get_word(p, i * 2);
    return sum;
}

format_info format_lzslza_detect(std::ifstream & file, std::streampos file_size)
{
    format_info info;
    info.detected = false;
    info.header = nullptr;

    if (file_size < 18)
        return info;  // File too short
    //TODO: check if file too long

    // Read the file header
    std::vector<uint8_t> vec(16);
    file.read((char*)&vec[0], 16);
    //TODO: check if read failed

    const lzslza_header* ph = (const lzslza_header*) vec.data();
    uint16_t headmethod = ph->signature;
    if (headmethod != 0x4F23 && headmethod != 0x4F11)
    {
        // "File header method not LZS or LZA"
        return info;
    }

    info.header = calloc(sizeof(lzslza_header), 1);
    memcpy(info.header, ph, sizeof(lzslza_header));

    info.detected = true;
    return info;
}

format_enumerator format_lzslza_enum_start(std::ifstream& file, format_info& finfo, std::streampos file_size)
{
    format_enumerator enumerator;
    enumerator.pfile = &file;
    enumerator.filesize = file_size;
    enumerator.nextoffset = 16;  // offset for the first item

    return enumerator;
}

format_item format_lzslza_enum_next(format_enumerator& enumerator)
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
    lzslza_item rec;
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

    if (rec.packedsize >= 32768 * 1024)
    {
        item.type = ITEMTYPE_ERROR;
        item.errorstr = "Wrong compressed size.";
        return item;
    }

    item.type = ITEMTYPE_FILE;
    item.name[0] = rec.name[0];
    item.name[1] = rec.name[1];
    item.ext = rec.ext;
    item.origsizeblock = rec.blocks;
    item.compsize = rec.packedsize;
    item.date = rec.date;
    item.packedchecksum = rec.checksum;

    // move enumerator's offset to the next record
    enumerator.nextoffset += 16 + rec.packedsize;
    if (enumerator.nextoffset & 1) enumerator.nextoffset += 1;  // alignment by 2

    return item;
}

format_extract_result format_lzslza_extract_file(std::ifstream& file, format_info& finfo, const format_item& item)
{
    format_extract_result result;
    result.pdata = nullptr;
    result.data_size = 0;

    if (item.type != ITEMTYPE_FILE)
    {
        result.errorstr = "Item is not a file";
        return result;
    }

    int origsize = item.origsizeblock * 512;
    size_t compsizeeven = (item.compsize + 1) / 2 * 2;
    uint8_t* pcomp = (uint8_t*)calloc(compsizeeven, 1);

    // read compressed block
    file.seekg(item.offset + (std::streampos)16);  // at compressed data
    file.read((char*)pcomp, compsizeeven);
    if (file.fail())
    {
        free(pcomp);
        result.errorstr = "File read error";
        return result;
    }

    // check packed file checksum
    uint16_t checksum = lzslza_checksum(pcomp, compsizeeven / 2);

    if (checksum != item.packedchecksum)
    {
        free(pcomp);
        result.errorstr = "Checksum mismatch";
        return result;
    }

    lzslza_header* pheader = (lzslza_header*)finfo.header;
    uint16_t headmethod = pheader->signature;
    uint16_t buffersize = pheader->buffersize;

    uint8_t* pdecomp = (uint8_t*)calloc(origsize, 1);

    int decoderes;
    if (headmethod == 0x4F11)  // LZA
        decoderes = uza_lzss_decode(pcomp, compsizeeven, pdecomp, origsize, buffersize);
    else
        decoderes = uzs_lzss_decode(pcomp, compsizeeven, pdecomp, origsize, buffersize);

    if (decoderes <= 0)
    {
        free(pcomp);
        free(pdecomp);
        result.errorstr = "Decompression error";
        return result;
    }

    free(pcomp);

    result.pdata = pdecomp;
    result.data_size = origsize;
    return result;
}

format_check_result format_lzslza_check_file(std::ifstream& file, format_info& finfo, const format_item& item)
{
    format_check_result result;
    result.success = false;

    if (item.type != ITEMTYPE_FILE)
    {
        result.errorstr = "Item is not a file";
        return result;
    }

    int origsize = item.origsizeblock * 512;
    size_t compsizeeven = (item.compsize + 1) / 2 * 2;
    uint8_t* pcomp = (uint8_t*)calloc(compsizeeven, 1);

    // read compressed block
    file.seekg(item.offset + (std::streampos)16);  // at compressed data
    file.read((char*)pcomp, compsizeeven);
    if (file.fail())
    {
        free(pcomp);
        result.errorstr = "File read error";
        return result;
    }

    // check packed file checksum
    uint16_t checksum = lzslza_checksum(pcomp, compsizeeven / 2);

    if (checksum != item.packedchecksum)
    {
        free(pcomp);
        result.errorstr = "Checksum mismatch";
        return result;
    }

    result.success = true;
    return result;
}

//TODO: Unused, remove
//void format_lzslza_list(std::ifstream& file, std::streampos file_size)
//{
//    // Read the whole file
//    std::vector<uint8_t> vec(file_size);
//    file.read((char*)&vec[0], file_size);
//
//    const lzslza_header* ph = (const lzslza_header*)vec.data();
//    uint16_t headmethod = ph->signature;
//    if (headmethod != 0x4F11 && headmethod != 0x4F23)
//    {
//        std::cout << "File header method not LZS or LZA, aborting." << std::endl;
//        exit(255);
//    }
//    char headsign[4] = { 0 };
//    r50asc(3, (const uint16_t*)ph, headsign);
//    char headvers[3] = { 0 };
//    headvers[0] = ph->version[0];
//    headvers[1] = ph->version[1];
//    std::cout << "Header info: method " << headsign << ", version " << headvers;
//    uint16_t buffersize = ph->buffersize;
//    std::cout << ", buffer size: " << buffersize;
//    char headdate[12] = { 0 };
//    rt11date_str(ph->date, headdate, 11);
//    std::cout << ", date " << headdate << std::endl;
//    //uint16_t archCatSize = ph->catsize;
//    //uint16_t archCatBlk = ph->catblock;
//    //std::cout << "archive catalog size " << archCatSize << ", archive catalog block " << archCatBlk << std::endl;
//
//    if (buffersize != 2048 && buffersize != 4096)
//    {
//        std::cout << "== BAD HEADER: Unexpected buffer size " << buffersize << ". ==" << std::endl;
//        std::cout << "LISTING ABORTED!" << std::endl;
//        exit(255);
//    }
//
//
//    size_t offset = 16;  // header size
//    int file_count = 0;
//    int block_count = 0;
//
//    std::cout << "   Name          Date       Original    Compressed   Offset" << std::endl;
//
//    for (; ; )
//    {
//        const uint8_t* p = vec.data() + offset;
//        const lzslza_item* pitem = (const lzslza_item*)p;
//        uint16_t word0 = pitem->name[0];
//        if (word0 == 0)
//            break;  // End of the archive
//
//        char itemname[7] = { 0 };
//        r50asc(6, pitem->name, itemname);
//        char itemext[4] = { 0 };
//        r50asc(3, &(pitem->ext), itemext);
//        uint16_t srcblocks = pitem->blocks;
//        char itemdate[12] = { 0 };
//        rt11date_str(pitem->date, itemdate, 11);
//        uint16_t itemchksum = pitem->checksum;
//        uint32_t packedSize = pitem->packedsize;
//        if (packedSize >= 32768 * 1024)
//        {
//            std::cout << "== BAD RECORD: Wrong Compressed size. ==" << std::endl;
//            std::cout << "LISTING ABORTED!" << std::endl;
//            break;
//        }
//
//        std::cout << itemname << "." << itemext << "   ";
//        std::cout << itemdate << "  ";
//        std::cout << std::setw(3) << srcblocks << " blocks  " << std::setw(5) << packedSize << " bytes";
//        std::cout << "  " << std::setw(8) << std::hex << offset << std::dec;
//        std::cout << std::endl;
//
//        file_count++;
//        block_count += srcblocks;
//
//        const uint8_t* pcomp = p + 16;  // pointer to compressed data
//
//        //{
//        //    std::ofstream fileraw("raw", std::ios::out | std::ios::binary);
//        //    fileraw.write((const char*)pcomp, packedSize);
//        //    fileraw.close();
//
//        //    FILE* infile = fopen("raw", "rb");
//        //    FILE* outfile = fopen("uncomp", "wb");
//
//        //    if (headmethod == 0x4F11)  // LZA
//        //        uza_lzss_decode(infile, outfile, buffersize);
//        //    else
//        //        uzs_lzss_decode(infile, outfile, buffersize);
//
//        //    fclose(infile);
//        //    fclose(outfile);
//        //}
//        //{
//        //    std::ifstream unpfile("uncomp", std::ios::in | std::ios::binary);
//
//        //    unpfile.seekg(0, std::ios::end);
//        //    std::streampos unpfile_size = unpfile.tellg();
//        //    unpfile.seekg(0, std::ios::beg);
//        //    //std::cout << "    Unpacked file size: " << unpfile_size << " bytes" << std::endl;
//
//        //    std::vector<uint8_t> unpvec(unpfile_size);
//        //    unpfile.read((char*)&unpvec[0], unpfile_size);
//
//        //    unpfile.close();
//
//        //    //uint16_t checksum = calc_checksum(unpvec.data(), (size_t)unpfile_size / 2);
//        //    //std::cout << std::hex << "    Checksums " << itemchksum << " / " << checksum << std::dec << std::endl;
//        //}
//
//        offset += 16 + packedSize;
//        if (offset & 1) offset++;  // alignment by 2
//
//        if (offset >= vec.size())
//            break;
//    }
//
//    std::cout << "  " << file_count << " files, " << block_count << " blocks." << std::endl;
//}


//////////////////////////////////////////////////////////////////////
