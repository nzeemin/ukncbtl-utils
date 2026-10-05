/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// diskimage.cpp : RT-11 disk image access for the WCX plugin

#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "rt11dsk.h"
#include "diskimage.h"
#include "rt11date.h"

#define MAX_IMAGE_FILE_SIZE     (64 * 1024 * 1024)
#define MIN_IMAGE_BLOCKS        10


//////////////////////////////////////////////////////////////////////

static bool IsNameChar(wchar_t ch)
{
    return (ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9') || ch == L'$';
}

bool Rt11ParseFileName(const wchar_t* name, uint16_t r50[3])
{
    const wchar_t* pslash = wcsrchr(name, L'\\');
    const wchar_t* pslash2 = wcsrchr(name, L'/');
    if (pslash2 > pslash) pslash = pslash2;
    const wchar_t* base = (pslash == nullptr) ? name : pslash + 1;

    const wchar_t* pdot = wcsrchr(base, L'.');
    size_t namelen = (pdot == nullptr) ? wcslen(base) : (size_t)(pdot - base);
    size_t extlen = (pdot == nullptr) ? 0 : wcslen(pdot + 1);
    if (namelen == 0 || namelen > 6 || extlen > 3)
        return false;

    char buf[10];
    memset(buf, ' ', 9);
    buf[9] = 0;
    for (size_t i = 0; i < namelen + extlen; i++)
    {
        bool isext = i >= namelen;
        wchar_t ch = isext ? pdot[1 + (i - namelen)] : base[i];
        if (ch >= L'a' && ch <= L'z') ch = ch - L'a' + L'A';
        if (!IsNameChar(ch))
            return false;
        buf[isext ? 6 + (i - namelen) : i] = (char)ch;
    }
    irad50(9, buf, r50);
    return true;
}


//////////////////////////////////////////////////////////////////////

CDiskImage::CDiskImage()
{
    m_okReadOnly = false;
    m_lStartOffset = 0;
    m_okInterleaving = false;
    m_nTotalBlocks = 0;
    m_result = DISK_OK;
}

CDiskImage::~CDiskImage()
{
    Detach();
}

// Load the specified disk image file
bool CDiskImage::Attach(const wchar_t* fileName, long offset, bool interleaving)
{
    Detach();
    m_fileName = fileName;
    m_okInterleaving = interleaving;

    FILE* fp = ::_wfopen(fileName, L"rb");
    if (fp == nullptr)
    {
        m_result = DISK_ERR_OPEN;
        return false;
    }
    ::fseek(fp, 0, SEEK_END);
    long lFileSize = ::ftell(fp);
    ::fseek(fp, 0, SEEK_SET);
    if (lFileSize < MIN_IMAGE_BLOCKS * RT11_BLOCK_SIZE || lFileSize > MAX_IMAGE_FILE_SIZE)
    {
        ::fclose(fp);
        m_result = DISK_ERR_FORMAT;
        return false;
    }
    m_image.resize(lFileSize);
    size_t lBytesRead = ::fread(&m_image[0], 1, lFileSize, fp);
    ::fclose(fp);
    if ((long)lBytesRead != lFileSize)
    {
        m_image.clear();
        m_result = DISK_ERR_READ;
        return false;
    }

    // Check whether the file can be written
    FILE* fpw = ::_wfopen(fileName, L"r+b");
    m_okReadOnly = (fpw == nullptr);
    if (fpw != nullptr)
        ::fclose(fpw);

    if (offset >= 0)
        m_lStartOffset = offset;
    else  // Try to detect the offset: 000240 at the beginning of the block 0
    {
        m_lStartOffset = 0;
        if (m_image[0] == 0xA0 && m_image[1] == 0)
            m_lStartOffset = 0;
        else if (m_image[128] == 0xA0 && m_image[128 + 1] == 0)
            m_lStartOffset = 128;
        else if (m_image[256] == 0xA0 && m_image[256 + 1] == 0)
            m_lStartOffset = 256;
    }

    m_nTotalBlocks = (int)((lFileSize - m_lStartOffset) / RT11_BLOCK_SIZE);
    if (interleaving)
        m_nTotalBlocks -= 10;
    if (m_nTotalBlocks < MIN_IMAGE_BLOCKS)
    {
        Detach();
        m_result = DISK_ERR_FORMAT;
        return false;
    }
    m_dirty.assign(m_nTotalBlocks, 0);

    if (!DecodeImageCatalog())
    {
        Detach();
        m_result = DISK_ERR_FORMAT;
        return false;
    }

    m_result = DISK_OK;
    return true;
}

void CDiskImage::Detach()
{
    Flush();
    m_image.clear();
    m_dirty.clear();
    m_nTotalBlocks = 0;
    m_volumeinfo = CVolumeInformation();
}

long CDiskImage::GetBlockOffset(int nBlock) const
{
    long foffset = ((long)nBlock) * RT11_BLOCK_SIZE;

    if (m_okInterleaving)  // MS0515 sector interleaving
    {
        int track = nBlock / 10;
        int sector = nBlock % 10;
        int sector_00 = (track * 2) % 10;
        sector = (sector_00 + sector * 2) % 10;
        if (nBlock % 10 >= 5) sector++;
        track++;
        if (track > 79) track = 0;
        foffset = long(track * 10 + sector) * RT11_BLOCK_SIZE;
    }

    foffset += m_lStartOffset;
    return foffset;
}

// Each block is 256 words, 512 bytes; block 0 is the boot block, block 1 is the home block
uint8_t* CDiskImage::GetBlock(int nBlock)
{
    if (nBlock < 0 || nBlock >= m_nTotalBlocks)
        return nullptr;
    long foffset = GetBlockOffset(nBlock);
    if (foffset < 0 || (size_t)foffset + RT11_BLOCK_SIZE > m_image.size())
        return nullptr;
    return &m_image[foffset];
}

void CDiskImage::MarkBlockChanged(int nBlock)
{
    if (nBlock >= 0 && nBlock < (int)m_dirty.size())
        m_dirty[nBlock] = 1;
}

EDiskResult CDiskImage::Flush()
{
    bool changed = false;
    for (size_t i = 0; i < m_dirty.size(); i++)
        if (m_dirty[i]) { changed = true; break; }
    if (!changed)
        return DISK_OK;
    if (m_okReadOnly)
        return DISK_ERR_READONLY;

    FILE* fp = ::_wfopen(m_fileName.c_str(), L"r+b");
    if (fp == nullptr)
        return DISK_ERR_OPEN;

    EDiskResult result = DISK_OK;
    for (int i = 0; i < (int)m_dirty.size(); i++)
    {
        if (!m_dirty[i]) continue;

        long foffset = GetBlockOffset(i);
        if (::fseek(fp, foffset, SEEK_SET) != 0 ||
            ::fwrite(&m_image[foffset], 1, RT11_BLOCK_SIZE, fp) != RT11_BLOCK_SIZE)
        {
            result = DISK_ERR_WRITE;
            break;
        }
        m_dirty[i] = 0;
    }
    if (::fclose(fp) != 0 && result == DISK_OK)
        result = DISK_ERR_WRITE;
    return result;
}


//////////////////////////////////////////////////////////////////////
// Catalog

// Read catalog segment, 2 blocks = 512 words
bool CDiskImage::ReadSegment(int block, uint16_t* buf)
{
    uint8_t* pBlock1 = GetBlock(block);
    uint8_t* pBlock2 = GetBlock(block + 1);
    if (pBlock1 == nullptr || pBlock2 == nullptr)
        return false;
    memcpy(buf, pBlock1, RT11_BLOCK_SIZE);
    memcpy(buf + 256, pBlock2, RT11_BLOCK_SIZE);
    return true;
}

void CDiskImage::WriteSegment(int block, const uint16_t* buf)
{
    uint8_t* pBlock1 = GetBlock(block);
    uint8_t* pBlock2 = GetBlock(block + 1);
    if (pBlock1 == nullptr || pBlock2 == nullptr)
        return;
    if (memcmp(pBlock1, buf, RT11_BLOCK_SIZE) != 0)
    {
        memcpy(pBlock1, buf, RT11_BLOCK_SIZE);
        MarkBlockChanged(block);
    }
    if (memcmp(pBlock2, buf + 256, RT11_BLOCK_SIZE) != 0)
    {
        memcpy(pBlock2, buf + 256, RT11_BLOCK_SIZE);
        MarkBlockChanged(block + 1);
    }
}

bool CDiskImage::DecodeImageCatalog()
{
    m_volumeinfo = CVolumeInformation();

    // Home block
    const uint8_t* pHomeSector = GetBlock(1);
    if (pHomeSector == nullptr)
        return false;
    uint16_t nFirstCatalogBlock = pHomeSector[0724];  // Should be 6
    if (nFirstCatalogBlock > 10)
        return false;
    if (nFirstCatalogBlock == 0) nFirstCatalogBlock = 6;
    m_volumeinfo.firstcatalogblock = nFirstCatalogBlock;
    m_volumeinfo.systemversion = pHomeSector[0726];
    strncpy(m_volumeinfo.volumeid, (const char*)pHomeSector + 0730, 12);
    strncpy(m_volumeinfo.ownername, (const char*)pHomeSector + 0744, 12);
    strncpy(m_volumeinfo.systemid, (const char*)pHomeSector + 0760, 12);

    // The first catalog segment
    uint16_t segbuf[512];
    if (!ReadSegment(nFirstCatalogBlock, segbuf))
        return false;
    uint16_t nSegmentCount = segbuf[0];
    if (nSegmentCount == 0 || nSegmentCount > 31)
        return false;
    uint16_t nExtraBytes = segbuf[3];
    if (nExtraBytes > 128)
        return false;
    uint16_t nExtraWords = (nExtraBytes + 1) / 2;
    uint16_t nEntryLength = 7 + nExtraWords;  // Total catalog entry length, in words
    uint16_t nEntriesPerSegment = (512 - 5) / nEntryLength;
    m_volumeinfo.catalogsegmentcount = nSegmentCount;
    m_volumeinfo.catalogextrawords = nExtraWords;
    m_volumeinfo.catalogentrylength = nEntryLength;
    m_volumeinfo.catalogentriespersegment = nEntriesPerSegment;
    // The catalog is followed by the data: the first file starts after all catalog segments
    if (segbuf[4] != nFirstCatalogBlock + 2 * nSegmentCount)
        return false;

    uint16_t nCatalogBlock = nFirstCatalogBlock;
    for (int guard = 0; ; guard++)
    {
        if (guard >= nSegmentCount)  // Segment chain is longer than expected
            return false;

        CVolumeCatalogSegment segment;
        segment.segmentblock = nCatalogBlock;
        segment.nextsegment = segbuf[1];
        segment.start = segbuf[4];

        const uint16_t* pCatalog = segbuf + 5;
        uint16_t nFileStartBlock = segment.start;
        for (int i = 0; i < nEntriesPerSegment; i++, pCatalog += nEntryLength)
        {
            CVolumeCatalogEntry entry;
            entry.Unpack(pCatalog, nFileStartBlock);
            segment.entries.push_back(entry);
            if (entry.status == RT11_STATUS_ENDMARK)
                break;
            nFileStartBlock += entry.length;
        }
        uint16_t nNextSegment = segment.nextsegment;
        m_volumeinfo.segments.push_back(segment);

        if (nNextSegment == 0) break;  // End of the segment chain
        if (nNextSegment > nSegmentCount)
            return false;

        nCatalogBlock = nFirstCatalogBlock + (nNextSegment - 1) * 2;
        if (!ReadSegment(nCatalogBlock, segbuf))
            return false;
    }

    return true;
}

void CDiskImage::UpdateCatalogSegment(int segno)
{
    const CVolumeCatalogSegment& segment = m_volumeinfo.segments[segno];
    uint16_t segbuf[512];
    if (!ReadSegment(segment.segmentblock, segbuf))
        return;

    int entrylength = m_volumeinfo.catalogentrylength;
    uint16_t* pCatalog = segbuf + 5;  // Skip the segment header
    size_t i = 0;
    for (; i < segment.entries.size(); i++, pCatalog += entrylength)
        segment.entries[i].Pack(pCatalog, entrylength);
    for (; i < m_volumeinfo.catalogentriespersegment; i++, pCatalog += entrylength)
        memset(pCatalog, 0, entrylength * sizeof(uint16_t));

    WriteSegment(segment.segmentblock, segbuf);
}

void CDiskImage::UpdateAllCatalogSegments()
{
    for (int i = 0; i < (int)m_volumeinfo.segments.size(); i++)
        UpdateCatalogSegment(i);
}

// Join adjacent empty entries
void CDiskImage::MergeEmptyEntries()
{
    for (size_t s = 0; s < m_volumeinfo.segments.size(); s++)
    {
        std::vector<CVolumeCatalogEntry>& entries = m_volumeinfo.segments[s].entries;
        size_t i = 0;
        while (i + 1 < entries.size())
        {
            if (entries[i].IsEmpty() && entries[i + 1].IsEmpty() &&
                entries[i].length + entries[i + 1].length <= 0xFFFF)
            {
                entries[i].length += entries[i + 1].length;
                entries.erase(entries.begin() + i + 1);
            }
            else
                i++;
        }
    }
}

bool CDiskImage::FindFile(const uint16_t r50[3], int* pSegment, int* pIndex) const
{
    for (size_t s = 0; s < m_volumeinfo.segments.size(); s++)
    {
        const std::vector<CVolumeCatalogEntry>& entries = m_volumeinfo.segments[s].entries;
        for (size_t i = 0; i < entries.size(); i++)
        {
            if (!entries[i].IsFile()) continue;
            if (memcmp(entries[i].namerad50, r50, sizeof(entries[i].namerad50)) != 0) continue;
            *pSegment = (int)s;
            *pIndex = (int)i;
            return true;
        }
    }
    return false;
}

// Find the best fit empty entry: the smallest one of enough length.
// Entries that need a new catalog slot are skipped when the segment is full.
bool CDiskImage::FindEmptySlot(uint16_t blocks, int* pSegment, int* pIndex, bool* pCatalogFull) const
{
    bool found = false;
    uint16_t bestlength = 0;
    *pCatalogFull = false;
    for (size_t s = 0; s < m_volumeinfo.segments.size(); s++)
    {
        const std::vector<CVolumeCatalogEntry>& entries = m_volumeinfo.segments[s].entries;
        for (size_t i = 0; i < entries.size(); i++)
        {
            const CVolumeCatalogEntry& entry = entries[i];
            if (!entry.IsEmpty() || entry.length < blocks) continue;
            if ((int)entry.start + blocks > m_nTotalBlocks) continue;
            if (entry.length > blocks && entries.size() + 1 > m_volumeinfo.catalogentriespersegment)
            {
                *pCatalogFull = true;
                continue;
            }
            if (!found || entry.length < bestlength)
            {
                found = true;
                bestlength = entry.length;
                *pSegment = (int)s;
                *pIndex = (int)i;
            }
        }
    }
    return found;
}

// Put the file into an empty slot; the catalog and the image are not changed on failure
EDiskResult CDiskImage::InsertFile(const uint16_t r50[3], const uint8_t* data, size_t size,
                                   uint16_t blocks, time_t mtime)
{
    int segno, index;
    bool catalogfull;
    if (!FindEmptySlot(blocks, &segno, &index, &catalogfull))
        return catalogfull ? DISK_ERR_CATALOG_FULL : DISK_ERR_NO_SPACE;

    std::vector<CVolumeCatalogEntry>& entries = m_volumeinfo.segments[segno].entries;
    CVolumeCatalogEntry empty = entries[index];
    for (int b = 0; b < blocks; b++)  // Check all blocks before any change
        if (GetBlock(empty.start + b) == nullptr)
            return DISK_ERR_FORMAT;

    CVolumeCatalogEntry file;
    file.status = RT11_STATUS_PERM;
    memcpy(file.namerad50, r50, sizeof(file.namerad50));
    file.start = empty.start;
    file.length = blocks;
    file.datepac = clock2rt11date(mtime);
    if (empty.length == blocks)
        entries[index] = file;
    else
    {
        entries[index].start += blocks;
        entries[index].length -= blocks;
        entries.insert(entries.begin() + index, file);
    }

    for (int b = 0; b < blocks; b++)
    {
        uint8_t* pBlock = GetBlock(file.start + b);
        size_t offset = (size_t)b * RT11_BLOCK_SIZE;
        size_t chunk = size - offset < RT11_BLOCK_SIZE ? size - offset : RT11_BLOCK_SIZE;
        memcpy(pBlock, data + offset, chunk);
        if (chunk < RT11_BLOCK_SIZE)
            memset(pBlock + chunk, 0, RT11_BLOCK_SIZE - chunk);
        MarkBlockChanged(file.start + b);
    }
    return DISK_OK;
}

EDiskResult CDiskImage::AddFile(const uint16_t r50[3], const uint8_t* data, size_t size, time_t mtime)
{
    if (m_okReadOnly)
        return DISK_ERR_READONLY;
    size_t nblocks = (size + RT11_BLOCK_SIZE - 1) / RT11_BLOCK_SIZE;
    if (nblocks > RT11_MAX_FILE_BLOCKS)
        return DISK_ERR_TOO_BIG;
    uint16_t blocks = (uint16_t)nblocks;

    int segno, index;
    if (FindFile(r50, &segno, &index))  // Replace the existing file
    {
        CVolumeCatalogEntry& old = m_volumeinfo.segments[segno].entries[index];
        if (old.length == blocks)  // Same size: overwrite the file content
        {
            for (int b = 0; b < blocks; b++)
                if (GetBlock(old.start + b) == nullptr)
                    return DISK_ERR_FORMAT;
            for (int b = 0; b < blocks; b++)
            {
                uint8_t* pBlock = GetBlock(old.start + b);
                size_t offset = (size_t)b * RT11_BLOCK_SIZE;
                size_t chunk = size - offset < RT11_BLOCK_SIZE ? size - offset : RT11_BLOCK_SIZE;
                memcpy(pBlock, data + offset, chunk);
                if (chunk < RT11_BLOCK_SIZE)
                    memset(pBlock + chunk, 0, RT11_BLOCK_SIZE - chunk);
                MarkBlockChanged(old.start + b);
            }
            old.datepac = clock2rt11date(mtime);
            UpdateCatalogSegment(segno);
            return DISK_OK;
        }

        // Different size: free the old entry and allocate the space again; restore the entry on failure
        uint16_t oldstatus = old.status;
        old.status = RT11_STATUS_EMPTY;
        EDiskResult result = InsertFile(r50, data, size, blocks, mtime);
        if (result != DISK_OK)
        {
            m_volumeinfo.segments[segno].entries[index].status = oldstatus;
            return result;
        }
        MergeEmptyEntries();
        UpdateAllCatalogSegments();
        return DISK_OK;
    }

    EDiskResult result = InsertFile(r50, data, size, blocks, mtime);
    if (result != DISK_OK)
        return result;
    UpdateAllCatalogSegments();
    return DISK_OK;
}

EDiskResult CDiskImage::RemoveFile(const uint16_t r50[3])
{
    if (m_okReadOnly)
        return DISK_ERR_READONLY;
    int segno, index;
    if (!FindFile(r50, &segno, &index))
        return DISK_ERR_NOT_FOUND;

    m_volumeinfo.segments[segno].entries[index].status = RT11_STATUS_EMPTY;
    MergeEmptyEntries();
    UpdateAllCatalogSegments();
    return DISK_OK;
}


//////////////////////////////////////////////////////////////////////

CVolumeInformation::CVolumeInformation()
{
    memset(volumeid, 0, sizeof(volumeid));
    memset(ownername, 0, sizeof(ownername));
    memset(systemid, 0, sizeof(systemid));
    firstcatalogblock = systemversion = 0;
    catalogextrawords = catalogentrylength = catalogentriespersegment = catalogsegmentcount = 0;
}


//////////////////////////////////////////////////////////////////////

CVolumeCatalogEntry::CVolumeCatalogEntry()
{
    status = 0;
    start = length = datepac = 0;
    memset(namerad50, 0, sizeof(namerad50));
}

void CVolumeCatalogEntry::Unpack(uint16_t const * pCatalog, uint16_t filestartblock)
{
    start = filestartblock;
    status = pCatalog[0];
    namerad50[0] = pCatalog[1];
    namerad50[1] = pCatalog[2];
    namerad50[2] = pCatalog[3];
    length  = pCatalog[4];
    // pCatalog[5] - channel (lsb), job (msb) - used for tentative files only
    datepac = pCatalog[6];
}

void CVolumeCatalogEntry::Pack(uint16_t* pCatalog, int entrylength) const
{
    pCatalog[0] = status;
    memcpy(pCatalog + 1, namerad50, sizeof(namerad50));
    pCatalog[4] = length;
    pCatalog[5] = 0;
    pCatalog[6] = datepac;
    for (int i = 7; i < entrylength; i++)  // Extra words
        pCatalog[i] = 0;
}

bool CVolumeCatalogEntry::IsFile() const
{
    return (status & RT11_STATUS_PERM) == RT11_STATUS_PERM;
}

bool CVolumeCatalogEntry::IsEmpty() const
{
    return status == RT11_STATUS_EMPTY;
}

void CVolumeCatalogEntry::GetName(char* buf) const
{
    uint16_t r50[3] = { namerad50[0], namerad50[1], namerad50[2] };
    char t[10];
    r50asc(9, r50, t);
    for (int i = 0; i < 9; i++)  // Replace the symbols not allowed in a host file name
    {
        char ch = t[i];
        bool ok = ch == ' ' || ch == '$' || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9');
        if (!ok) t[i] = '_';
    }
    int namelen = 6;
    while (namelen > 0 && t[namelen - 1] == ' ') namelen--;
    int extlen = 3;
    while (extlen > 0 && t[6 + extlen - 1] == ' ') extlen--;

    int n = 0;
    if (namelen == 0)
        buf[n++] = '_';
    memcpy(buf + n, t, namelen);
    n += namelen;
    if (extlen > 0)
    {
        buf[n++] = '.';
        memcpy(buf + n, t + 6, extlen);
        n += extlen;
    }
    buf[n] = 0;
}


//////////////////////////////////////////////////////////////////////
