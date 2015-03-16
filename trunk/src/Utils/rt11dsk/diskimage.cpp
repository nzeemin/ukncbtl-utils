/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// diskimage.cpp : Disk image utilities

#include "stdafx.h"
#include <time.h>
#include "diskimage.h"
#include "rad50.h"

//////////////////////////////////////////////////////////////////////

struct CCachedBlock
{
    int     nBlock;
    void*   pData;
    bool    bChanged;
    clock_t cLastUsage;  // GetTickCount() for last usage
};


//////////////////////////////////////////////////////////////////////
// ��������� ������, �������������� ���������� � ���� RT-11

/* Types for rtFileEntry 'status' */
#define RT11_STATUS_TENTATIVE   256     /* Temporary file */
#define RT11_STATUS_EMPTY       512     /* Marks empty space */
#define RT11_STATUS_PERM        1024    /* A "real" file */
#define RT11_STATUS_ENDMARK     2048    /* Marks the end of file entries */

// ��������� ��� �������� ����������� ������ ��������
struct CVolumeCatalogEntry
{
public:  // ����������� ���� ������
    WORD status;    // See RT11_STATUS_xxx constants
    WORD datepac;   // ����������� ���� ����
    WORD start;     // File start block number
    WORD length;    // File length in 512-byte blocks
public:  // ������������� ���� ������
    TCHAR name[7];  // File name - 6 characters
    TCHAR ext[4];   // File extension - 3 characters

public:
    CVolumeCatalogEntry();
    void Unpack(WORD const * pSrc, WORD filestartblock);  // ���������� ������ �� ��������
    void Pack(WORD* pDest);   // �������� ������ � �������
    void Print();  // ������ ������ �������� �� �������
};

// ��������� ������ ��� �������� ��������
struct CVolumeCatalogSegment
{
public:
    WORD segmentblock;  // ���� �� �����, � ������� ���������� ���� ������� ��������
    WORD entriesused;   // ���������� ������������� ������� ��������
public:
    WORD nextsegment;   // ����� ���������� ��������
    WORD start;         // ����� �����, � �������� ���������� ����� ����� ��������
    // ������ ������� ��������, �������� � ����������� ��������� ���-�� ������� ��� ����� ��������
    CVolumeCatalogEntry* catalogentries;
};


//////////////////////////////////////////////////////////////////////

static WORD g_segmentBuffer[512];


void CDiskImage::UpdateCatalogSegment(CVolumeCatalogSegment* pSegment)
{
    BYTE* pBlock1 = (BYTE*) GetBlock(pSegment->segmentblock);
    memcpy(g_segmentBuffer, pBlock1, 512);
    BYTE* pBlock2 = (BYTE*) GetBlock(pSegment->segmentblock + 1);
    memcpy(g_segmentBuffer + 256, pBlock2, 512);
    WORD* pData = g_segmentBuffer;

    pData += 5;  // ���������� ��������� ��������
    for (int entryno = 0; entryno < m_volumeinfo.catalogentriespersegment; entryno++)
    {
        CVolumeCatalogEntry* pEntry = pSegment->catalogentries + entryno;

        pEntry->Pack(pData);

        pData += m_volumeinfo.catalogentrylength;
    }

    memcpy(pBlock1, g_segmentBuffer, 512);
    MarkBlockChanged(pSegment->segmentblock);
    memcpy(pBlock2, g_segmentBuffer + 256, 512);
    MarkBlockChanged(pSegment->segmentblock + 1);
}


//////////////////////////////////////////////////////////////////////

CDiskImage::CDiskImage()
{
    m_okReadOnly = false;
    m_fpFile = NULL;
    m_okCloseFile = true;
    m_lStartOffset = 0;
    m_nTotalBlocks = m_nCacheBlocks = 0;
    m_pCache = NULL;
}

CDiskImage::~CDiskImage()
{
    Detach();
}

// Open the specified disk image file
bool CDiskImage::Attach(LPCTSTR sImageFileName)
{
    // ����������, ��� .dsk-����� ��� .rtd-����� - �� ���������� �����
    m_lStartOffset = 0;
    LPCTSTR sImageFilenameExt = wcsrchr(sImageFileName, _T('.'));
    if (sImageFilenameExt != NULL && _wcsicmp(sImageFilenameExt, _T(".rtd")) == 0)
        m_lStartOffset = NETRT11_IMAGE_HEADER_SIZE;
    //NOTE: ����� ����� ���������� �� ����� �����: ������ 512 -- .dsk, ����� ����� 256 ������ 512 -- .rtd

    // Try to open as Normal first, then as ReadOnly
    m_okReadOnly = false;
    m_fpFile = ::_wfopen(sImageFileName, _T("r+b"));
    if (m_fpFile == NULL)
    {
        m_okReadOnly = true;
        m_fpFile = ::_wfopen(sImageFileName, _T("rb"));
        if (m_fpFile == NULL)
            return false;
    }

    // Calculate m_TotalBlocks
    ::fseek(m_fpFile, 0, SEEK_END);
    long lFileSize = ::ftell(m_fpFile);
    m_nTotalBlocks = lFileSize / RT11_BLOCK_SIZE;

    this->PostAttach();

    return true;
}

// Use the given area of the file as a disk image; do not close the file in Detach() method.
bool CDiskImage::Attach(FILE* fpfile, long offset, int blocks, bool readonly)
{
    m_fpFile = fpfile;
    m_okCloseFile = false;
    m_okReadOnly = readonly;
    m_lStartOffset = offset;
    m_nTotalBlocks = blocks;

    this->PostAttach();

    return true;
}

// Actions at the end of Attach() method
void CDiskImage::PostAttach()
{
    // Allocate memory for the cache
    m_nCacheBlocks = 1024;  //NOTE: For up to 1024 blocks, for 512K of data
    if (m_nCacheBlocks > m_nTotalBlocks) m_nCacheBlocks = m_nTotalBlocks;
    m_pCache = (CCachedBlock*) ::malloc(m_nCacheBlocks * sizeof(CCachedBlock));
    ::memset(m_pCache, 0, m_nCacheBlocks * sizeof(CCachedBlock));

    // Initial read: fill half of the cache
    int nBlocks = 10;
    if (nBlocks > m_nTotalBlocks) nBlocks = m_nTotalBlocks;
    for (int i = 1; i <= nBlocks; i++)
    {
        GetBlock(i);
    }
}

void CDiskImage::Detach()
{
    if (m_fpFile != NULL)
    {
        FlushChanges();

        if (m_okCloseFile)
            ::fclose(m_fpFile);
        m_fpFile = NULL;

        // Free cached blocks data
        for (int i = 0; i < m_nCacheBlocks; i++)
        {
            if (m_pCache[i].pData != NULL)
                ::free(m_pCache[i].pData);
        }

        ::free(m_pCache);
    }
}

long CDiskImage::GetBlockOffset(int nBlock) const
{
    long foffset = ((long)nBlock) * RT11_BLOCK_SIZE;
    foffset += m_lStartOffset;
    return foffset;
}

void CDiskImage::FlushChanges()
{
    for (int i = 0; i < m_nCacheBlocks; i++)
    {
        if (!m_pCache[i].bChanged) continue;

        // ��������� �������� � ����� ������
        long foffset = GetBlockOffset(m_pCache[i].nBlock);
        ::fseek(m_fpFile, foffset, SEEK_SET);

        // ���������� ����
        size_t lBytesWritten = ::fwrite(m_pCache[i].pData, 1, RT11_BLOCK_SIZE, m_fpFile);
        if (lBytesWritten != RT11_BLOCK_SIZE)
        {
            wprintf(_T("Failed to write block number %d.\n"), m_pCache[i].nBlock);
            _exit(-1);
        }

        m_pCache[i].bChanged = false;
    }
}

// ������ ���� - 256 ����, 512 ����
// nBlock = 1..???
void* CDiskImage::GetBlock(int nBlock)
{
    // First lookup the cache
    for (int i = 0; i < m_nCacheBlocks; i++)
    {
        if (m_pCache[i].nBlock == nBlock)
        {
            m_pCache[i].cLastUsage = ::clock();
            return m_pCache[i].pData;
        }
    }

    // Find a free cache slot
    int iEmpty = -1;
    for (int i = 0; i < m_nCacheBlocks; i++)
    {
        if (m_pCache[i].nBlock == 0)
        {
            iEmpty = i;
            break;
        }
    }

    // If a free slot not found then release a slot
    if (iEmpty == -1)
    {
        // Find a non-changed cached block with oldest usage time
        int iCand = -1;
        DWORD maxdiff = 0;
        for (int i = 0; i < m_nCacheBlocks; i++)
        {
            if (!m_pCache[i].bChanged)
            {
                DWORD diff = ::clock() - m_pCache[i].cLastUsage;
                if (diff > maxdiff)
                {
                    maxdiff = diff;
                    iCand = i;
                }
            }
        }
        if (iCand != -1)  // Found
        {
            ::free(m_pCache[iEmpty].pData);
            m_pCache[iEmpty].pData = NULL;
            m_pCache[iEmpty].nBlock = 0;
            m_pCache[iEmpty].bChanged = false;
        }
    }

    if (iEmpty == -1)
    {
        wprintf(_T("Cache is full.\n"));
        _exit(-1);
    }

    m_pCache[iEmpty].nBlock = nBlock;
    m_pCache[iEmpty].bChanged = false;
    m_pCache[iEmpty].pData = ::malloc(RT11_BLOCK_SIZE);
    ::memset(m_pCache[iEmpty].pData, 0, RT11_BLOCK_SIZE);
    m_pCache[iEmpty].cLastUsage = ::clock();

    // Load the block data
    long foffset = GetBlockOffset(nBlock);
    ::fseek(m_fpFile, foffset, SEEK_SET);
    size_t lBytesRead = ::fread(m_pCache[iEmpty].pData, 1, RT11_BLOCK_SIZE, m_fpFile);
    if (lBytesRead != RT11_BLOCK_SIZE)
    {
        wprintf(_T("Failed to read block number %d.\n"), nBlock);
        _exit(-1);
    }

    return m_pCache[iEmpty].pData;
}

void CDiskImage::MarkBlockChanged(int nBlock)
{
    for (int i = 0; i < m_nCacheBlocks; i++)
    {
        if (m_pCache[i].nBlock != nBlock) continue;

        m_pCache[i].bChanged = true;
        m_pCache[i].cLastUsage = ::clock();
        break;
    }
}

void CDiskImage::DecodeImageCatalog()
{
    memset(&m_volumeinfo, 0, sizeof(m_volumeinfo));

    // ������ Home Block
    BYTE* pHomeSector = (BYTE*) GetBlock(1);
    WORD nFirstCatalogBlock = pHomeSector[0724];  // ��� ������ ���� ���� ����� 6
    if (nFirstCatalogBlock == 0) nFirstCatalogBlock = 6;
    m_volumeinfo.firstcatalogblock = nFirstCatalogBlock;
    m_volumeinfo.systemversion = pHomeSector[0726];
    const char* sVolumeId = (const char*) pHomeSector + 0730;
    strncpy_s(m_volumeinfo.volumeid, 13, sVolumeId, 12);
    const char* sOwnerName = (const char*) pHomeSector + 0744;
    strncpy_s(m_volumeinfo.ownername, 13, sOwnerName, 12);
    const char* sSystemId = (const char*) pHomeSector + 0760;
    strncpy_s(m_volumeinfo.systemid, 13, sSystemId, 12);

    // ������ ������� ����� ��������
    BYTE* pBlock1 = (BYTE*) GetBlock(nFirstCatalogBlock);
    memcpy(g_segmentBuffer, pBlock1, 512);
    BYTE* pBlock2 = (BYTE*) GetBlock(nFirstCatalogBlock + 1);
    memcpy(g_segmentBuffer + 256, pBlock2, 512);
    WORD* pCatalogSector = g_segmentBuffer;
    m_volumeinfo.catalogsegmentcount = pCatalogSector[0];
    m_volumeinfo.lastopenedsegment = pCatalogSector[2];
    WORD nExtraBytesLength = pCatalogSector[3];
    WORD nExtraWordsLength = (nExtraBytesLength + 1) / 2;
    m_volumeinfo.catalogextrawords = nExtraWordsLength;
    WORD nEntryLength = 7 + nExtraWordsLength;  // Total catalog entry length, in words
    m_volumeinfo.catalogentrylength = nEntryLength;
    WORD nEntriesPerSegment = (512 - 5) / nEntryLength;
    m_volumeinfo.catalogentriespersegment = nEntriesPerSegment;

    // �������� ������ ��� ������ ���������
    m_volumeinfo.catalogsegments = (CVolumeCatalogSegment*) ::malloc(
        sizeof(CVolumeCatalogSegment) * m_volumeinfo.catalogsegmentcount);
    memset(m_volumeinfo.catalogsegments, 0,
        sizeof(CVolumeCatalogSegment) * m_volumeinfo.catalogsegmentcount);

    //TODO: ��� ��������� ������ ������� �������� �������� ���������� �������:
    //      ���� ������� ���������� ����� 1 � � ���������� ��������� ��������� ���� �������� (������ 6),
    //      �� ���������� ���������� ����� 5. ����� ������� RT-11 �������� ���� ������� �� ������.

    WORD nCatalogEntriesCount = 0;
    WORD nCatalogSegmentNumber = 1;
    CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments;

    WORD nCatalogBlock = nFirstCatalogBlock;
    for (;;)
    {
        pSegment->segmentblock = nCatalogBlock;

        WORD nStartBlock = pCatalogSector[4];  // ����� �����, � �������� ���������� ����� ����� ��������
        pSegment->start = pCatalogSector[4];
        //wprintf(_T("Segment %d start block: %d\n"), nCatalogSegmentNumber, nStartBlock);
        pSegment->nextsegment = pCatalogSector[1];
        //wprintf(_T("Next segment:           %d\n"), pSegment->nextsegment);

        // �������� ������ ��� ������ ��������
        pSegment->catalogentries = (CVolumeCatalogEntry*) ::malloc(
            sizeof(CVolumeCatalogEntry) * nEntriesPerSegment);
        memset(pSegment->catalogentries, 0,
            sizeof(CVolumeCatalogEntry) * nEntriesPerSegment);

        CVolumeCatalogEntry* pEntry = pSegment->catalogentries;
        WORD* pCatalog = pCatalogSector + 5;  // ������ �������� ������
        WORD nFileStartBlock = nStartBlock;
        WORD entriesused = 0;
        for (;;)  // ���� �� ������� ������� �������� ��������
        {
            nCatalogEntriesCount++;

            pEntry->Unpack(pCatalog, nFileStartBlock);

            if (pEntry->status == RT11_STATUS_ENDMARK)
                break;

            nFileStartBlock += pEntry->length;
            pEntry++;
            pCatalog += nEntryLength;
            if (pCatalog - pCatalogSector > 256 * 2 - nEntryLength)  // ������� ����������
                break;
        }
        pSegment->entriesused = entriesused;

        if (pSegment->nextsegment == 0) break;  // ����� ������� ���������

        // ��������� � ���������� �������� ��������
        nCatalogBlock = nFirstCatalogBlock + (pSegment->nextsegment - 1) * 2;
        pBlock1 = (BYTE*) GetBlock(nCatalogBlock);
        memcpy(g_segmentBuffer, pBlock1, 512);
        pBlock2 = (BYTE*) GetBlock(nCatalogBlock + 1);
        memcpy(g_segmentBuffer + 256, pBlock2, 512);
        pCatalogSector = g_segmentBuffer;
        nCatalogSegmentNumber = pSegment->nextsegment;
        pSegment++;
    }

    m_volumeinfo.catalogentriescount = nCatalogEntriesCount;
}

void CDiskImage::PrintTableHeader()
{
    wprintf(_T(" Filename  Blocks  Date      Start    Bytes\n"));
    wprintf(_T("---------- ------  --------- ----- --------\n"));
}
void CDiskImage::PrintTableFooter()
{
    wprintf(_T("---------- ------  --------- ----- --------\n"));
}

void CDiskImage::PrintCatalogDirectory()
{
    wprintf(_T(" Volume: %S\n"), m_volumeinfo.volumeid);
    wprintf(_T(" Owner:  %S\n"), m_volumeinfo.ownername);
    wprintf(_T(" System: %S\n"), m_volumeinfo.systemid);
    wprintf(_T("\n"));
    wprintf(_T(" %d available segments, last opened segment: %d\n"), m_volumeinfo.catalogsegmentcount, m_volumeinfo.lastopenedsegment);
    wprintf(_T("\n"));
    PrintTableHeader();

    WORD nFilesCount = 0;
    WORD nBlocksCount = 0;
    WORD nFreeBlocksCount = 0;
    for (int segmno = 0; segmno < m_volumeinfo.catalogsegmentcount; segmno++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segmno;
        if (pSegment->catalogentries == NULL) continue;
        
        for (int entryno = 0; entryno < m_volumeinfo.catalogentriespersegment; entryno++)
        {
            CVolumeCatalogEntry* pEntry = pSegment->catalogentries + entryno;

            if (pEntry->status == RT11_STATUS_ENDMARK) break;
            if (pEntry->status == 0) continue;
            pEntry->Print();
            if (pEntry->status == RT11_STATUS_EMPTY)
                nFreeBlocksCount += pEntry->length;
            else
            {
                nFilesCount++;
                nBlocksCount += pEntry->length;
            }
        }
    }

    PrintTableFooter();
    wprintf(_T(" %d files, %d blocks\n"), nFilesCount, nBlocksCount);
    wprintf(_T(" %d free blocks\n\n"), nFreeBlocksCount);
}

void CDiskImage::SaveEntryToExternalFile(LPCTSTR sFileName)
{
    // Parse g_sFileName
    LPCTSTR sFilenameExt = wcsrchr(sFileName, _T('.'));
    if (sFilenameExt == NULL)
    {
        wprintf(_T("Wrong filename format: %s\n"), sFileName);
        return;
    }
    size_t nFilenameLength = sFilenameExt - sFileName;
    if (nFilenameLength == 0 || nFilenameLength > 6)
    {
        wprintf(_T("Wrong filename format: %s\n"), sFileName);
        return;
    }
    size_t nFileextLength = wcslen(sFileName) - nFilenameLength - 1;
    if (nFileextLength == 0 || nFileextLength > 3)
    {
        wprintf(_T("Wrong filename format: %s\n"), sFileName);
        return;
    }
    TCHAR filename[7];
    for (int i = 0; i < 6; i++) filename[i] = _T(' ');
    for (WORD i = 0; i < nFilenameLength; i++) filename[i] = sFileName[i];
    filename[6] = 0;
    TCHAR fileext[4];
    for (int i = 0; i < 3; i++) fileext[i] = _T(' ');
    for (WORD i = 0; i < nFileextLength; i++) fileext[i] = sFilenameExt[i + 1];
    fileext[3] = 0;

    // Search for the filename/fileext
    CVolumeCatalogEntry* pFileEntry = NULL;
    for (int segmno = 0; segmno < m_volumeinfo.catalogsegmentcount; segmno++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segmno;
        if (pSegment->catalogentries == NULL) continue;
        
        for (int entryno = 0; entryno < m_volumeinfo.catalogentriespersegment; entryno++)
        {
            CVolumeCatalogEntry* pEntry = pSegment->catalogentries + entryno;

            if (pEntry->status == RT11_STATUS_ENDMARK) break;
            if (pEntry->status == 0) continue;
            if (pEntry->status == RT11_STATUS_EMPTY) continue;

            if (_wcsnicmp(filename, pEntry->name, 6) == 0 &&
                _wcsnicmp(fileext, pEntry->ext, 3) == 0)
            {
                pFileEntry = pEntry;
                break;
            }
        }
    }
    if (pFileEntry == NULL)
    {
        wprintf(_T("Filename not found: %s\n"), sFileName);
        return;
    }
    wprintf(_T("Extracting file:\n\n"));
    PrintTableHeader();
    pFileEntry->Print();
    PrintTableFooter();

    WORD filestart = pFileEntry->start;
    WORD filelength = pFileEntry->length;

    FILE* foutput = NULL;
    errno_t err = _wfopen_s(&foutput, sFileName, _T("wb"));
    if (err != 0)
    {
        wprintf(_T("Failed to open output file %s: error %d\n"), sFileName, err);
        return;
    }

    for (WORD blockpos = 0; blockpos < filelength; blockpos++)
    {
        BYTE* pData = (BYTE*) GetBlock(filestart + blockpos);
        size_t nBytesWritten = fwrite(pData, sizeof(BYTE), RT11_BLOCK_SIZE, foutput);
        //TODO: Check if nBytesWritten < RT11_BLOCK_SIZE
    }

    fclose(foutput);

    wprintf(_T("\nDone.\n"));
}

// ��������� ����� � �����.
// ��������:
//   ���������� ���� ����������� � ������
//   ������������ ��� ������ ��������, ���� �� ����� ������� ������ ������ ������� ��� ������ �����
//   ���� �����, � ����� �������� ��������� ����� ������ ������
//   � ���� ������ ������������� ���������� ������ ��������
//   ���� �����, � ���� ������ ������������� ����� ������ ������ ��������
//   � ���� ������ ������������� ����� ������ �����
//NOTE: ���� �� ������������ �������� �������� ������ ����� �������� - ������� �� ������
//NOTE: ���� �� ��������� ��� ���� � ����� ������ ��� ����, � �� ������ ������
void CDiskImage::AddFileToImage(LPCTSTR sFileName)
{
    // Parse g_sFileName
    LPCTSTR sFilenameExt = wcsrchr(sFileName, _T('.'));
    if (sFilenameExt == NULL)
    {
        wprintf(_T("Wrong filename format: %s\n"), sFileName);
        return;
    }
    size_t nFilenameLength = sFilenameExt - sFileName;
    if (nFilenameLength == 0 || nFilenameLength > 6)
    {
        wprintf(_T("Wrong filename format: %s\n"), sFileName);
        return;
    }
    size_t nFileextLength = wcslen(sFileName) - nFilenameLength - 1;
    if (nFileextLength == 0 || nFileextLength > 3)
    {
        wprintf(_T("Wrong filename format: %s\n"), sFileName);
        return;
    }
    TCHAR filename[7];
    for (int i = 0; i < 6; i++) filename[i] = _T(' ');
    for (WORD i = 0; i < nFilenameLength; i++) filename[i] = sFileName[i];
    filename[6] = 0;
    _wcsupr_s(filename, 7);
    TCHAR fileext[4];
    for (int i = 0; i < 3; i++) fileext[i] = _T(' ');
    for (WORD i = 0; i < nFileextLength; i++) fileext[i] = sFilenameExt[i + 1];
    fileext[3] = 0;
    _wcsupr_s(fileext, 4);

    // ��������� ���������� ���� �� ������
    FILE* fpFile = ::_wfopen(sFileName, _T("rb"));
    if (fpFile == NULL)
    {
        wprintf(_T("Failed to open the file."));
        return;
    }

    // ���������� ����� �����, � ������ ���������� �� ������� �����
    ::fseek(fpFile, 0, SEEK_END);
    long lFileLength = ::ftell(fpFile);  // ������ ����� �����
    WORD nFileSizeBlocks =  // ��������� ������ ���������� ����� � ������
        (WORD) (lFileLength + RT11_BLOCK_SIZE - 1) / RT11_BLOCK_SIZE;
    DWORD dwFileSize =  // ����� ����� � ������ ���������� �� ������� �����
        ((DWORD) nFileSizeBlocks) * RT11_BLOCK_SIZE;
    //TODO: �������� �� ���� ������� �����
    //TODO: ��������, �� ������� �� ������� ���� ��� ����� ����

    // �������� ������ � ��������� ������ �����
    void* pFileData = ::malloc(dwFileSize);
    memset(pFileData, 0, dwFileSize);
    ::fseek(fpFile, 0, SEEK_SET);
    size_t lBytesRead = ::fread(pFileData, 1, lFileLength, fpFile);
    if (lBytesRead != lFileLength)
    {
        wprintf(_T("Failed to read the file.\n"));
        _exit(-1);
    }
    ::fclose(fpFile);

    wprintf(_T("File size is %ld bytes or %d blocks\n"), lFileLength, nFileSizeBlocks);

    // ������������ ��� ������ ��������, ���� �� ����� ������� ������ ������ ����� >= dwFileLength
    //TODO: �������� � ��������� ������� � ������ �������� ���������� ������, � ����������� �������� �� �����
    CVolumeCatalogEntry* pFileEntry = NULL;
    CVolumeCatalogSegment* pFileSegment = NULL;
    for (int segmno = 0; segmno < m_volumeinfo.catalogsegmentcount; segmno++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segmno;
        if (pSegment->catalogentries == NULL) continue;
        
        for (int entryno = 0; entryno < m_volumeinfo.catalogentriespersegment; entryno++)
        {
            CVolumeCatalogEntry* pEntry = pSegment->catalogentries + entryno;

            if (pEntry->status == RT11_STATUS_ENDMARK) break;
            if (pEntry->status == 0) continue;

            if (pEntry->status == RT11_STATUS_EMPTY && pEntry->length >= nFileSizeBlocks)
            {
                pFileEntry = pEntry;
                pFileSegment = pSegment;
                break;
            }
        }
    }
    if (pFileEntry == NULL)
    {
        wprintf(_T("Empty catalog entry with %d or more blocks not found\n"), nFileSizeBlocks);
        free(pFileData);
        return;
    }
    wprintf(_T("Found empty catalog entry with %d blocks:\n\n"), pFileEntry->length);
    PrintTableHeader();
    pFileEntry->Print();
    PrintTableFooter();

    // ����������, ����� �� ����� ������ ��������
    bool okNeedNewCatalogEntry = (pFileEntry->length != nFileSizeBlocks);
    CVolumeCatalogEntry* pEmptyEntry = NULL;
    if (okNeedNewCatalogEntry)
    {
        // ���������, ����� �� ��� ����� ������ �������� ��������� ����� ������� ��������
        if (pFileSegment->entriesused == m_volumeinfo.catalogentriespersegment)
        {
            wprintf(_T("New catalog segment needed - not implemented now, sorry.\n"));
            free(pFileData);
            return;
        }

        // �������� ������ �������� ������� � ������ �� ���� ������ - ����������� ����� ��� ����� ������
        int fileentryindex = (int) (pFileEntry - pFileSegment->catalogentries);
        int totalentries = m_volumeinfo.catalogentriespersegment;
        memmove(pFileEntry + 1, pFileEntry, (totalentries - fileentryindex - 1) * sizeof(CVolumeCatalogEntry));

        // ����� ������ ������ ��������
        pEmptyEntry = pFileEntry + 1;
        // ��������� ������ ����� ������ ��������
        pEmptyEntry->status = RT11_STATUS_EMPTY;
        pEmptyEntry->start = pFileEntry->start + nFileSizeBlocks;
        pEmptyEntry->length = pFileEntry->length - nFileSizeBlocks;
        pEmptyEntry->datepac = pFileEntry->datepac;
    }

    // �������� ������������ ������ ��������
    pFileEntry->length = nFileSizeBlocks;
    wcscpy_s(pFileEntry->name, 7, filename);
    wcscpy_s(pFileEntry->ext, 4, fileext);
    pFileEntry->datepac = 0;
    pFileEntry->status = RT11_STATUS_PERM;

    wprintf(_T("\nCatalog entries to update:\n\n"));
    PrintTableHeader();
    pFileEntry->Print();
    if (pEmptyEntry != NULL) pEmptyEntry->Print();
    PrintTableFooter();

    // ��������� ����� ���� ��������
    wprintf(_T("\nWriting file data...\n"));
    WORD nFileStartBlock = pFileEntry->start;  // ������� � ������ ����� ����������� ����� ����
    WORD nBlock = nFileStartBlock;
    for (int block = 0; block < nFileSizeBlocks; block++)
    {
        BYTE* pFileBlockData = ((BYTE*) pFileData) + block * RT11_BLOCK_SIZE;
        BYTE* pData = (BYTE*) GetBlock(nBlock);
        memcpy(pData, pFileBlockData, RT11_BLOCK_SIZE);
        // �������� ��� ���� ��� �������
        MarkBlockChanged(nBlock);
        
        nBlock++;
    }
    free(pFileData);

    // ��������� ������� �������� �� ����
    wprintf(_T("Updating catalog segment...\n"));
    UpdateCatalogSegment(pFileSegment);

    FlushChanges();

    wprintf(_T("\nDone.\n"));
}


//////////////////////////////////////////////////////////////////////

CVolumeInformation::CVolumeInformation()
{
    catalogsegments = NULL;
}

CVolumeInformation::~CVolumeInformation()
{
    if (catalogsegments != NULL)
    {
        ::free(catalogsegments);
    }
}


//////////////////////////////////////////////////////////////////////

CVolumeCatalogEntry::CVolumeCatalogEntry()
{
    status = 0;
    memset(name, 0, sizeof(name));
    memset(ext, 0, sizeof(ext));
    start = length = 0;
}

void CVolumeCatalogEntry::Unpack(WORD const * pCatalog, WORD filestartblock)
{
    start = filestartblock;
    status = pCatalog[0];
    WORD namerad50[3];
    namerad50[0] = pCatalog[1];
    namerad50[1] = pCatalog[2];
    namerad50[2] = pCatalog[3];
    length  = pCatalog[4];
    datepac = pCatalog[6];

    if (status != RT11_STATUS_EMPTY && status != RT11_STATUS_ENDMARK)
    {
        r50asc(6, namerad50, name);
        name[6] = 0;
        r50asc(3, namerad50 + 2, ext);
        ext[3] = 0;
    }
}

void CVolumeCatalogEntry::Pack(WORD* pCatalog)
{
    pCatalog[0] = status;
    if (status == RT11_STATUS_EMPTY || status == RT11_STATUS_ENDMARK)
    {
        memset(pCatalog + 1, 0, sizeof(WORD) * 3);
    }
    else
    {
        WORD namerad50[3];
        irad50(6, name, namerad50);
        irad50(3, ext,  namerad50 + 2);
        memcpy(pCatalog + 1, namerad50, sizeof(namerad50));
    }
    pCatalog[4] = length;
    pCatalog[5] = 0;  // Used only for tentative files
    pCatalog[6] = datepac;
}

void CVolumeCatalogEntry::Print()
{
    if (status == RT11_STATUS_EMPTY)
        wprintf(_T("< UNUSED >  %5d            %5d %8d\n"),
                length, start, length * RT11_BLOCK_SIZE);
    else
    {
        TCHAR datestr[10];
        rtDateStr(datepac, datestr);
        wprintf(_T("%s.%s  %5d  %s %5d %8d\n"),
                name, ext, length, datestr, start, length * RT11_BLOCK_SIZE);
    }
}


//////////////////////////////////////////////////////////////////////
