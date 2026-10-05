/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// diskimage.h : RT-11 disk image access for the WCX plugin
// The whole image is loaded into memory; changes are written back by Flush().

#pragma once

#include <stdint.h>
#include <time.h>
#include <string>
#include <vector>

/* Size of each RT-11 disk block, 512 or 0x200 bytes */
#define RT11_BLOCK_SIZE         512
#define RT11_MAX_FILE_BLOCKS    0xFFFF

class CDiskImage;

//////////////////////////////////////////////////////////////////////

enum EDiskResult
{
    DISK_OK = 0,
    DISK_ERR_OPEN,          // Cannot open the image file
    DISK_ERR_READ,          // Cannot read the image file
    DISK_ERR_WRITE,         // Cannot write the image file
    DISK_ERR_FORMAT,        // Not a valid RT-11 volume
    DISK_ERR_NOT_FOUND,     // File not found in the image
    DISK_ERR_NO_SPACE,      // No free space for the file
    DISK_ERR_CATALOG_FULL,  // No free slot in the catalog segment (segment split is not implemented)
    DISK_ERR_READONLY,      // The image file is read-only
    DISK_ERR_BAD_NAME,      // The file name cannot be represented as RT-11 6.3 name
    DISK_ERR_TOO_BIG        // The file is too big for RT-11
};

// Parse the file name (path is ignored) as RT-11 6.3 name, upper cased; result is 3 words of radix-50
bool Rt11ParseFileName(const wchar_t* name, uint16_t r50[3]);

//////////////////////////////////////////////////////////////////////

// Parsed catalog entry
struct CVolumeCatalogEntry
{
public:  // Packed fields
    uint16_t status;        // See RT11_STATUS_xxx constants
    uint16_t datepac;       // Packed date
    uint16_t start;         // File start block number; calculated, not stored in the catalog
    uint16_t length;        // File length in 512-byte blocks
    uint16_t namerad50[3];  // Filename in radix50: 6.3

public:
    CVolumeCatalogEntry();
    void Unpack(uint16_t const * pSrc, uint16_t filestartblock);
    void Pack(uint16_t* pDest, int entrylength) const;
    bool IsFile() const;
    bool IsEmpty() const;
    void GetName(char* buf) const;  // File name as "NAME.EXT", buf must have 12 chars at least
};

struct CVolumeCatalogSegment
{
    uint16_t segmentblock;  // Disk block where the segment is located
    uint16_t nextsegment;   // Next segment number, 0 for the last segment
    uint16_t start;         // Start block of the first file of the segment
    std::vector<CVolumeCatalogEntry> entries;   // Including the end marker entry
    CVolumeCatalogSegment() : segmentblock(0), nextsegment(0), start(0) { }
};

struct CVolumeInformation
{
    char volumeid[13];
    char ownername[13];
    char systemid[13];
    uint16_t firstcatalogblock;
    uint16_t systemversion;
    uint16_t catalogextrawords;
    uint16_t catalogentrylength;        // In words
    uint16_t catalogentriespersegment;
    uint16_t catalogsegmentcount;
    std::vector<CVolumeCatalogSegment> segments;
    CVolumeInformation();
};

//////////////////////////////////////////////////////////////////////

class CDiskImage
{
protected:
    std::wstring    m_fileName;
    std::vector<uint8_t> m_image;       // Whole image file content
    std::vector<uint8_t> m_dirty;       // Changed block flags, by block number
    bool            m_okReadOnly;
    long            m_lStartOffset;     // First block start offset in the image file
    bool            m_okInterleaving;   // Sector interleaving used for MS0515 disks
    int             m_nTotalBlocks;
    EDiskResult     m_result;
    CVolumeInformation m_volumeinfo;

public:
    CDiskImage();
    ~CDiskImage();

public:
    // Load the image file. offset < 0 means detect the start offset automatically.
    bool Attach(const wchar_t* fileName, long offset = -1, bool interleaving = false);
    void Detach();
    EDiskResult Flush();    // Write changed blocks to the image file

public:
    bool IsReadOnly() const { return m_okReadOnly; }
    int GetBlockCount() const { return m_nTotalBlocks; }
    EDiskResult GetLastResult() const { return m_result; }
    const CVolumeInformation& GetVolumeInfo() const { return m_volumeinfo; }
    uint8_t* GetBlock(int nBlock);  // nullptr if the block is out of range
    void MarkBlockChanged(int nBlock);

public:
    bool FindFile(const uint16_t r50[3], int* pSegment, int* pIndex) const;
    // Add the file or replace the existing one
    EDiskResult AddFile(const uint16_t r50[3], const uint8_t* data, size_t size, time_t mtime);
    EDiskResult RemoveFile(const uint16_t r50[3]);

private:
    long GetBlockOffset(int nBlock) const;
    bool ReadSegment(int block, uint16_t* buf);
    void WriteSegment(int block, const uint16_t* buf);
    bool DecodeImageCatalog();
    void UpdateCatalogSegment(int segno);
    void UpdateAllCatalogSegments();
    void MergeEmptyEntries();
    bool FindEmptySlot(uint16_t blocks, int* pSegment, int* pIndex, bool* pCatalogFull) const;
    EDiskResult InsertFile(const uint16_t r50[3], const uint8_t* data, size_t size, uint16_t blocks, time_t mtime);
};

//////////////////////////////////////////////////////////////////////
