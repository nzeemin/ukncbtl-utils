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

#include <time.h>
#ifdef _MSC_VER
  #include <stdio.h>
  #define unlink(fn) _unlink(fn)
#else
  #include <unistd.h>
#endif
#include <assert.h>
#include "rt11dsk.h"
#include "diskimage.h"
#include "rt11date.h"
#include "hostfile.h"
#include <cctype>


//////////////////////////////////////////////////////////////////////

static uint16_t g_segmentBuffer[512];

void CVolumeCatalogEntry::Store(CHostFile* hf_p, CDiskImage* di_p)
{
    // Изменяем существующую запись каталога
    length = hf_p->rt11_sz;
    strncpy(name, hf_p->name(), 6); // FIXME
    name[6] = 0;
    strncpy(ext, hf_p->ext(), 3);
    ext[3] = 0;
    datepac = clock2rt11date(hf_p->mtime_sec);
    status = RT11_STATUS_PERM;
    uint16_t nFileStartBlock = start;  // Начиная с какого блока размещается новый файл
    uint16_t nBlock = nFileStartBlock;
    // Сохраняем новый файл по-блочно
    for (int block = 0; block < hf_p->rt11_sz; block++)
    {
        uint8_t* pFileBlockData = ((uint8_t*) hf_p->data) + block * RT11_BLOCK_SIZE;
        uint8_t* pData = (uint8_t*) di_p->GetBlock(nBlock);
        ::memcpy(pData, pFileBlockData, RT11_BLOCK_SIZE);
        // Сообщаем что блок был изменен
        di_p->MarkBlockChanged(nBlock);
        nBlock++;
    }
}

void CDiskImage::UpdateCatalogSegment(int segm_idx)
{
    printf("Updating catalog segment #%d...\n", segm_idx);
    assert(segm_idx < m_volumeinfo.catalogsegmentcount && segm_idx >= 0);
    CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segm_idx;
    uint8_t* pBlock1 = (uint8_t*) GetBlock(pSegment->segmentblock);
    memcpy(g_segmentBuffer, pBlock1, 512);
    uint8_t* pBlock2 = (uint8_t*) GetBlock(pSegment->segmentblock + 1);
    memcpy(g_segmentBuffer + 256, pBlock2, 512);
    uint16_t* pData = g_segmentBuffer;

    pData += 5;  // Пропускаем заголовок сегмента
    for (int m_file_idx = 0; m_file_idx < m_volumeinfo.catalogentriespersegment; m_file_idx++)
    {
        CVolumeCatalogEntry* pEntry = pSegment->catalogentries + m_file_idx;

        pEntry->Pack(pData);

        pData += m_volumeinfo.catalogentrylength;
    }

    memcpy(pBlock1, g_segmentBuffer, 512);
    MarkBlockChanged(pSegment->segmentblock);
    memcpy(pBlock2, g_segmentBuffer + 256, 512);
    MarkBlockChanged(pSegment->segmentblock + 1);
}

#if 0
// Parse sFileName as RT11 file name, 6.3 format
// Resulting filename and fileext are uppercased strings.
static void ParseFileName63(const char * sFileName, char * filename, char * fileext)
{
    const char * sFilenameExt = strrchr(sFileName, '.');
    const char * sFilenamePath = strrchr(sFileName, '/');
    if (sFilenameExt == nullptr)
    {
        printf("Wrong filename format: %s\n", sFileName);
        return;
    }
    if (sFilenamePath != nullptr)
    {
        sFileName = sFilenamePath + 1;
    }
    size_t nFilenameLength = sFilenameExt - sFileName;
    if (nFilenameLength == 0 || nFilenameLength > 6)
    {
        printf("Wrong filename format: %s\n", sFileName);
        return;
    }
    size_t nFileextLength = strlen(sFileName) - nFilenameLength - 1;
    if (nFileextLength == 0 || nFileextLength > 3)
    {
        printf("Wrong filename format: %s\n", sFileName);
        return;
    }
    for (int i = 0; i < 6; i++) filename[i] = ' ';
    for (uint16_t i = 0; i < nFilenameLength; i++) filename[i] = (char)toupper(sFileName[i]);
    filename[6] = 0;
    for (int i = 0; i < 3; i++) fileext[i] = ' ';
    for (uint16_t i = 0; i < nFileextLength; i++) fileext[i] = (char)toupper(sFilenameExt[i + 1]);
    fileext[3] = 0;
}
#endif

//////////////////////////////////////////////////////////////////////

CDiskImage::CDiskImage()
{
    m_okReadOnly = false;
    m_fpFile = nullptr;
    m_okCloseFile = true;
    m_lStartOffset = 0;
    m_nTotalBlocks = m_nCacheBlocks = 0;
    m_pCache = nullptr;
}

CDiskImage::~CDiskImage()
{
    Detach();
}

// Open the specified disk image file
bool CDiskImage::Attach(const char * sImageFileName, long offset)
{
    m_lStartOffset = offset;
    if (m_lStartOffset <= 0)
    {
        m_lStartOffset = 0;
        // Определяем, это .dsk-образ или .rtd-образ - по расширению файла
        const char * sImageFilenameExt = strrchr(sImageFileName, '.');
        if (sImageFilenameExt != nullptr && _stricmp(sImageFilenameExt, ".rtd") == 0)
            m_lStartOffset = NETRT11_IMAGE_HEADER_SIZE;
        //NOTE: Можно также определять по длине файла: кратна 512 -- .dsk, длина минус 256 кратна 512 -- .rtd
    }

    // Try to open as Normal first, then as ReadOnly
    m_okReadOnly = false;
    m_fpFile = ::fopen(sImageFileName, "r+b");
    if (m_fpFile == nullptr)
    {
        m_okReadOnly = true;
        m_fpFile = ::fopen(sImageFileName, "rb");
        if (m_fpFile == nullptr)
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
    m_nCacheBlocks = 1600;  //NOTE: For up to 1600 blocks, for 800K of data
    if (m_nCacheBlocks > m_nTotalBlocks) m_nCacheBlocks = m_nTotalBlocks;
    m_pCache = (CCachedBlock*) ::calloc(m_nCacheBlocks, sizeof(CCachedBlock));

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
    if (m_fpFile != nullptr)
    {
        FlushChanges();

        if (m_okCloseFile)
            ::fclose(m_fpFile);
        m_fpFile = nullptr;

        // Free cached blocks data
        for (int i = 0; i < m_nCacheBlocks; i++)
        {
            if (m_pCache[i].pData != nullptr)
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

        // Вычисляем смещение в файле образа
        long foffset = GetBlockOffset(m_pCache[i].nBlock);
        ::fseek(m_fpFile, foffset, SEEK_SET);

        // Записываем блок
        size_t lBytesWritten = ::fwrite(m_pCache[i].pData, 1, RT11_BLOCK_SIZE, m_fpFile);
        if (lBytesWritten != RT11_BLOCK_SIZE)
        {
            printf("Failed to write block number %d.\n", m_pCache[i].nBlock);
            exit(-1);
        }

        m_pCache[i].bChanged = false;
    }
}

// Каждый блок - 256 слов, 512 байт
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
        uint32_t maxdiff = 0;
        for (int i = 0; i < m_nCacheBlocks; i++)
        {
            if (!m_pCache[i].bChanged)
            {
                uint32_t diff = ::clock() - m_pCache[i].cLastUsage;
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
            m_pCache[iEmpty].pData = nullptr;
            m_pCache[iEmpty].nBlock = 0;
            m_pCache[iEmpty].bChanged = false;
        }
    }

    if (iEmpty == -1)
    {
        fprintf(stderr, "Cache is full.\n");
        exit(-1);
    }

    m_pCache[iEmpty].nBlock = nBlock;
    m_pCache[iEmpty].bChanged = false;
    m_pCache[iEmpty].pData = ::calloc(1, RT11_BLOCK_SIZE);
    if (m_pCache[iEmpty].pData == nullptr)
    {
        printf("Failed to allocate memory for block number %d.\n", nBlock);
        exit(-1);
    }
    m_pCache[iEmpty].cLastUsage = ::clock();

    // Load the block data
    long foffset = GetBlockOffset(nBlock);
    ::fseek(m_fpFile, foffset, SEEK_SET);
    size_t lBytesRead = ::fread(m_pCache[iEmpty].pData, 1, RT11_BLOCK_SIZE, m_fpFile);
    if (lBytesRead != RT11_BLOCK_SIZE)
    {
        printf("Failed to read block number %d.\n", nBlock);
        exit(-1);
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

    // Разбор Home Block
    uint8_t* pHomeSector = (uint8_t*) GetBlock(1);
    uint16_t nFirstCatalogBlock = pHomeSector[0724];  // Это должен быть блок номер 6
    if (nFirstCatalogBlock > 10)
    {
        printf("First catalog block is %d, out of range.\n", nFirstCatalogBlock);
        exit(-1);
    }
    if (nFirstCatalogBlock == 0) nFirstCatalogBlock = 6;
    m_volumeinfo.firstcatalogblock = nFirstCatalogBlock;
    m_volumeinfo.systemversion = pHomeSector[0726];
    const char* sVolumeId = (const char*) pHomeSector + 0730;
    strncpy(m_volumeinfo.volumeid, sVolumeId, 12);
    const char* sOwnerName = (const char*) pHomeSector + 0744;
    strncpy(m_volumeinfo.ownername, sOwnerName, 12);
    const char* sSystemId = (const char*) pHomeSector + 0760;
    strncpy(m_volumeinfo.systemid, sSystemId, 12);

    // Разбор первого блока каталога
    uint8_t* pBlock1 = (uint8_t*) GetBlock(nFirstCatalogBlock);
    memcpy(g_segmentBuffer, pBlock1, 512);
    uint8_t* pBlock2 = (uint8_t*) GetBlock(nFirstCatalogBlock + 1);
    memcpy(g_segmentBuffer + 256, pBlock2, 512);
    uint16_t* pCatalogSector = g_segmentBuffer;
    m_volumeinfo.catalogsegmentcount = pCatalogSector[0];
    m_volumeinfo.lastopenedsegment = pCatalogSector[2];
    uint16_t nExtraBytesLength = pCatalogSector[3];
    uint16_t nExtraWordsLength = (nExtraBytesLength + 1) / 2;
    m_volumeinfo.catalogextrawords = nExtraWordsLength;
    uint16_t nEntryLength = 7 + nExtraWordsLength;  // Total catalog entry length, in words
    m_volumeinfo.catalogentrylength = nEntryLength;
    uint16_t nEntriesPerSegment = (512 - 5) / nEntryLength;
    m_volumeinfo.catalogentriespersegment = nEntriesPerSegment;
    if (m_volumeinfo.catalogsegmentcount == 0 || m_volumeinfo.catalogsegmentcount > 31)
    {
        printf("Catalog segment count is %d, out of range (1..31).\n", m_volumeinfo.catalogsegmentcount);
        exit(-1);
    }

    // Получаем память под список сегментов
    m_volumeinfo.catalogsegments = (CVolumeCatalogSegment*) ::calloc(
            m_volumeinfo.catalogsegmentcount, sizeof(CVolumeCatalogSegment));

    //TODO: Для заголовка самого первого сегмента каталога существует правило:
    //      если удвоить содержимое слова 1 и к результату прибавить начальный блок каталога (обычно 6),
    //      то получиться содержимое слова 5. Таким образом RT-11 отличает свой каталог от чужого.

    uint16_t nCatalogEntriesCount = 0;
    uint16_t nCatalogSegmentNumber = 1;
    CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments;

    uint16_t nCatalogBlock = nFirstCatalogBlock;
    for (;;)
    {
        pSegment->segmentblock = nCatalogBlock;

        uint16_t nStartBlock = pCatalogSector[4];  // Номер блока, с которого начинаются файлы этого сегмента
        pSegment->start = pCatalogSector[4];
        //printf("Segment %d start block: %d\n", nCatalogSegmentNumber, nStartBlock);
        pSegment->nextsegment = pCatalogSector[1];
        //printf("Next segment:           %d\n", pSegment->nextsegment);

        // Выделяем память под записи сегмента
        pSegment->catalogentries = (CVolumeCatalogEntry*) ::calloc(
                nEntriesPerSegment, sizeof(CVolumeCatalogEntry));

        CVolumeCatalogEntry* pEntry = pSegment->catalogentries;
        uint16_t* pCatalog = pCatalogSector + 5;  // Начало описаний файлов
        uint16_t nFileStartBlock = nStartBlock;
        uint16_t entriesused = 0;
        for (;;)  // Цикл по записям данного сегмента каталога
        {
            nCatalogEntriesCount++;

            pEntry->Unpack(pCatalog, nFileStartBlock);

            if (pEntry->status == RT11_STATUS_ENDMARK)
                break;

            nFileStartBlock += pEntry->length;
            pEntry++;
            pCatalog += nEntryLength;
            if (pCatalog - pCatalogSector > 256 * 2 - nEntryLength)  // Сегмент закончился
                break;
        }
        pSegment->entriesused = entriesused;

        if (pSegment->nextsegment == 0) break;  // Конец цепочки сегментов

        // Переходим к следующему сегменту каталога
        nCatalogBlock = nFirstCatalogBlock + (pSegment->nextsegment - 1) * 2;
        pBlock1 = (uint8_t*) GetBlock(nCatalogBlock);
        memcpy(g_segmentBuffer, pBlock1, 512);
        pBlock2 = (uint8_t*) GetBlock(nCatalogBlock + 1);
        memcpy(g_segmentBuffer + 256, pBlock2, 512);
        pCatalogSector = g_segmentBuffer;
        nCatalogSegmentNumber = pSegment->nextsegment;
        pSegment++;
    }

    m_volumeinfo.catalogentriescount = nCatalogEntriesCount;
}

void CDiskImage::PrintTableHeader()
{
    printf(" Filename  Blocks  Date      Start    Bytes\n"
           "---------- ------  --------- ----- --------\n");
}
void CDiskImage::PrintTableFooter()
{
    printf("---------- ------  --------- ----- --------\n");
}

////////////////////////////////////////////////////////////////////////
struct d_print
{
    uint16_t nFilesCount;
    uint16_t nBlocksCount;
    uint16_t nFreeBlocksCount;
};

EIterOp cb_print_entries(CVolumeCatalogEntry* pEntry, void* opaque)
{
    struct d_print* p = (struct d_print*)opaque;

    pEntry->Print();
    if (pEntry->status == RT11_STATUS_EMPTY)
        p->nFreeBlocksCount += pEntry->length;
    else
    {
        p->nFilesCount++;
        p->nBlocksCount += pEntry->length;
    }
    return IT_NEXT;
}

// Do iteration over m_voduleinfo (segments / file entries)
// return:
//   - true if iteration was stopped by IT_STOP, i.e. some item found
//   - false othesize
bool CDiskImage::Iterate(lookup_fn_t lookup, void* opaque)
{
    for (m_seg_idx = 0; m_seg_idx < m_volumeinfo.catalogsegmentcount; m_seg_idx++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + m_seg_idx;
        if (pSegment->catalogentries == nullptr) continue;

        for (m_file_idx = 0; m_file_idx < m_volumeinfo.catalogentriespersegment; m_file_idx++)
        {
            CVolumeCatalogEntry* pEntry = pSegment->catalogentries + m_file_idx;

            if (pEntry->status == RT11_STATUS_ENDMARK) break;
            if (pEntry->status == 0) continue;
            switch (lookup(pEntry, opaque))
            {
                case IT_STOP:
                    return true;
                case IT_NEXT:
                    continue;
            }
        }
    }
    return false;
}

void CDiskImage::PrintCatalogDirectory()
{
    printf(" Volume: %s\n", m_volumeinfo.volumeid);
    printf(" Owner:  %s\n", m_volumeinfo.ownername);
    printf(" System: %s\n", m_volumeinfo.systemid);
    printf("\n");
    printf(" %d available segments, last opened segment: %d\n",
           m_volumeinfo.catalogsegmentcount, m_volumeinfo.lastopenedsegment);
    printf("\n");
    PrintTableHeader();

    struct d_print  res;
    memset(&res, 0, sizeof(res));
    Iterate(cb_print_entries, (void*)&res);

    PrintTableFooter();
    printf(" %d files, %d blocks\n", res.nFilesCount, res.nBlocksCount);
    printf(" %d free blocks\n\n", res.nFreeBlocksCount);
}

////////////////////////////////////////////////////////////////////////
struct d_save_one
{
    CHostFile*  hf_p;
    CDiskImage* di_p;
};

EIterOp cb_save_one(CVolumeCatalogEntry* pEntry, void* opaque)
{
    struct d_save_one*  r = (struct d_save_one*)opaque;

    if ((pEntry->status & RT11_STATUS_PERM) != RT11_STATUS_PERM)
        return IT_NEXT;
    // compare name
    if (memcmp(pEntry->namerad50, r->hf_p->rt11_fn, sizeof(pEntry->namerad50)) != 0)
        return IT_NEXT;
    printf("Extracting file:\n\n");
    r->di_p->PrintTableHeader();
    pEntry->Print();
    r->di_p->PrintTableFooter();

    // Collect file name + ext without trailing spaces
    char sfilename[11];
    char *p = sfilename;
    char *s = pEntry->name;
    while(*s)
        *p++ = ::tolower(*s++);
    // remove trailing spaces
    while(*--p == ' ' && p >= sfilename);
    *++p = '.'; 
    s = pEntry->ext;
    while(*s && *s != ' ')
        *++p = ::tolower(*s++);
    *++p = 0;

    uint16_t filestart = pEntry->start;
    uint16_t filelength = pEntry->length;

    FILE* foutput = ::fopen(r->hf_p->host_fn, "wb");
    if (foutput == nullptr)
    {
        fprintf(stderr, "Failed to open output file %s: error %d\n", r->hf_p->host_fn, errno);
        return IT_STOP;
    }

    for (uint16_t blockpos = 0; blockpos < filelength; blockpos++)
    {
        uint8_t* pData = (uint8_t*) r->di_p->GetBlock(filestart + blockpos);
        size_t nBytesWritten = ::fwrite(pData, sizeof(uint8_t), RT11_BLOCK_SIZE, foutput);
        if (nBytesWritten < RT11_BLOCK_SIZE)
        {
            fprintf(stderr, "Failed to write output file\n");  //TODO: Show error number
            ::fclose(foutput);
            ::unlink(r->hf_p->host_fn);
            return IT_STOP;
        }
    }
    ::fclose(foutput);
    return IT_STOP;
}

void CDiskImage::SaveEntryToExternalFile(const char * sFileName)
{
    CHostFile   hf(sFileName);
    struct d_save_one   res;
    res.hf_p = &hf;
    res.di_p = this;

    if (!hf.ParseFileName63())
        return; // error
    if (!Iterate(cb_save_one, &res))
    {
        fprintf(stderr, "Filename not found: %s\n", sFileName);
        return;
    }
    printf("\nDone.\n");
}

void CDiskImage::SaveAllEntriesToExternalFiles()
{
    printf("Extracting files:\n\n");
    PrintTableHeader();

    for (int m_seg_idx = 0; m_seg_idx < m_volumeinfo.catalogsegmentcount; m_seg_idx++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + m_seg_idx;
        if (pSegment->catalogentries == nullptr) continue;

        for (int m_file_idx = 0; m_file_idx < m_volumeinfo.catalogentriespersegment; m_file_idx++)
        {
            CVolumeCatalogEntry* pEntry = pSegment->catalogentries + m_file_idx;

            if (pEntry->status == RT11_STATUS_ENDMARK) break;
            if (pEntry->status == 0) continue;
            if (pEntry->status == RT11_STATUS_EMPTY) continue;

            pEntry->Print();

            // Collect file name + ext without trailing spaces
            char filename[11];
            strcpy(filename, pEntry->name);
            char * p = filename + 5;
            while (p > filename && *p == ' ') p--;
            p++;
            *p = '.';
            p++;
            strcpy(p, pEntry->ext);

            uint16_t filestart = pEntry->start;
            uint16_t filelength = pEntry->length;

            FILE* foutput = fopen(filename, "wb");
            if (foutput == nullptr)
            {
                printf("Failed to open output file %s: error %d\n", filename, errno);
                return;
            }

            for (uint16_t blockpos = 0; blockpos < filelength; blockpos++)
            {
                uint8_t* pData = (uint8_t*)GetBlock(filestart + blockpos);
                size_t nBytesWritten = fwrite(pData, sizeof(uint8_t), RT11_BLOCK_SIZE, foutput);
                if (nBytesWritten < RT11_BLOCK_SIZE)
                {
                    printf("Failed to write output file\n");  //TODO: Show error number
                    fclose(foutput);
                    return;
                }
            }

            fclose(foutput);
        }
    }
    PrintTableFooter();

    printf("\nDone.\n");
}

////////////////////////////////////////////////////////////////////////
struct d_add_one
{
    CHostFile*  hf_p;
    CDiskImage* di_p;
};

EIterOp cb_replace_one(CVolumeCatalogEntry* pEntry, void* opaque)
{
    struct d_add_one*  r = (struct d_add_one*)opaque;

    if ((pEntry->status & RT11_STATUS_PERM) != RT11_STATUS_PERM)
        return IT_NEXT;
    // compare name
    if (memcmp(pEntry->namerad50, r->hf_p->rt11_fn, sizeof(pEntry->namerad50)) != 0)
        return IT_NEXT;
    // Проверить имя файла
    if (pEntry->length != r->hf_p->rt11_sz)
    {
        fprintf(stderr, "File exists with different size (%d): %.6s.%.3s",
                pEntry->length, r->hf_p->name(), r->hf_p->ext());
        exit(-1);
    }
    // ok, меняем контент
    printf("\nCatalog entries to update:\n\n");
    r->di_p->PrintTableHeader();
    pEntry->Print();
    r->di_p->PrintTableFooter();

    printf("\nWriting file data...\n");
    pEntry->Store(r->hf_p, r->di_p);
    // Сохраняем сегмент каталога на диск
    r->di_p->UpdateCatalogSegment(r->di_p->iterSegmentIdx());
    return IT_STOP;
}

EIterOp cb_new_one(CVolumeCatalogEntry* pEntry, void* opaque)
{
    struct d_add_one*  r = (struct d_add_one*)opaque;

    if ((pEntry->status & RT11_STATUS_EMPTY) != RT11_STATUS_EMPTY)
        return IT_NEXT;
    // Проверить  размер свободного пространства
    // TODO найти наиболее подходящий пустой:
    // a) с таким же размером в блоках
    // b) с минимальным размером блока, к-й больше требуемого
    if (pEntry->length < r->hf_p->rt11_sz)
        return IT_NEXT;
    // Ok, есть пустой слот
    // Проверяем, нужно ли для новой записи каталога открывать новый сегмент каталога
    if (r->di_p->IsCurrentSegmentFull())
    {
        // FIXME
        fprintf(stderr, "New catalog segment needed - not implemented now, sorry.\n");
        exit(-1);
    }

    printf("\nCatalog entries to update:\n\n");
    r->di_p->PrintTableHeader();
    pEntry->Print();
    r->di_p->PrintTableFooter();

    if (pEntry->length > r->hf_p->rt11_sz)
    {
        // Сдвигаем записи сегмента начиная с пустой на одну вправо - освобождаем место под новую запись
        int fileentryindex = r->di_p->iterFileIdx();
        int totalentries = r->di_p->getEntriesPerSegment();
        // Новая пустая запись каталога
        CVolumeCatalogEntry *pEmptyEntry = pEntry + 1;
        memmove(pEmptyEntry, pEntry, (totalentries - fileentryindex - 1) * sizeof(CVolumeCatalogEntry));

        // Заполнить данные новой записи каталога
        pEmptyEntry->status = RT11_STATUS_EMPTY;
        pEmptyEntry->start = pEntry->start + r->hf_p->rt11_sz;
        pEmptyEntry->length = pEntry->length - r->hf_p->rt11_sz;
        pEmptyEntry->datepac = pEntry->datepac;
    }

    printf("\nWriting file data...\n");
    pEntry->Store(r->hf_p, r->di_p);
    // Сохраняем сегмент каталога на диск
    r->di_p->UpdateCatalogSegment(r->di_p->iterSegmentIdx());
    return IT_STOP;
}

// Помещение файла в образ.
// Алгоритм:
//   Помещаемый файл считывается в память
//   Перебираются все записи каталога, пока не будет найдена пустая запись большей или равной длины
//   Если нужно, в конце каталога создается новая пустая запись
//   В файл образа прописывается измененная запись каталога
//   Если нужно, в файл образа прописывается новая пустая запись каталога
//   В файл образа прописываются блоки нового файла
//NOTE: Пока НЕ обрабатываем ситуацию открытия нового блока каталога - выходим по ошибке
//NOTE: Проверяем что файл с таким именем уже есть, 
//      если длина не совпадает, то выходим по ошибке
void CDiskImage::AddFileToImage(const char * sFileName)
{
    CHostFile   hfile(sFileName);
    struct d_add_one   res;
    res.hf_p = &hfile;
    res.di_p = this;


    if (!hfile.ParseFileName63())
        return; // error
    if (!hfile.read())
        return; // error
    // попытаемся заменить существующий файл
    if (!Iterate(cb_replace_one, &res))
    {
        // ищем пустое место и вставляем там файл
        if (!Iterate(cb_new_one, &res))
        {
            fprintf(stderr, "Unable to find empty space for: %s\n", sFileName);
            return;
        }
    }
    FlushChanges();
    printf("\nDone.\n");
}

////////////////////////////////////////////////////////////////////////
struct d_remove_one
{
    CHostFile*  hf_p;
    CDiskImage* di_p;
};

EIterOp cb_remove_one(CVolumeCatalogEntry* pEntry, void* opaque)
{
    struct d_remove_one*  r = (struct d_remove_one*)opaque;

    if ((pEntry->status & RT11_STATUS_PERM) != RT11_STATUS_PERM)
        return IT_NEXT;
    // compare name
    if (memcmp(pEntry->namerad50, r->hf_p->rt11_fn, sizeof(pEntry->namerad50)) != 0)
        return IT_NEXT;
    // FIXME
    printf("Deleting file:\n\n");
    r->di_p->PrintTableHeader();
    pEntry->Print();
    r->di_p->PrintTableFooter();

    // Изменяем существующую запись каталога
    pEntry->status = RT11_STATUS_EMPTY;

    // Сохраняем сегмент каталога на диск
    printf("Updating catalog segment...\n");
    r->di_p->UpdateCatalogSegment(r->di_p->iterSegmentIdx());

    return IT_STOP;
}

// Удаление файла
// Алгоритм:
//   Перебираются все записи каталога, пока не будет найдена запись данного файла
//   Запись каталога помечается как удалённая
void CDiskImage::DeleteFileFromImage(const char * sFileName)
{
    CHostFile   hf(sFileName);
    struct d_remove_one   res;
    res.hf_p = &hf;
    res.di_p = this;

    if (!hf.ParseFileName63())
        return; // error
    if (!Iterate(cb_remove_one, &res))
    {
        fprintf(stderr, "Filename not found: %s\n", sFileName);
        return;
    }
    FlushChanges();

    printf("\nDone.\n");
}

////////////////////////////////////////////////////////////////////////
struct d_save_unused
{
    CDiskImage* di_p;
    uint16_t    unusedno;
};

EIterOp cb_save_unused(CVolumeCatalogEntry* pEntry, void* opaque)
{
    struct d_save_unused    *r = (struct d_save_unused*)opaque;

    if ((pEntry->status & RT11_STATUS_EMPTY) != RT11_STATUS_EMPTY)
        return IT_NEXT;
    pEntry->Print();

    r->unusedno++;
    char filename[20];
    sprintf(filename, "UNUSED%02d", r->unusedno);

    uint16_t filestart = pEntry->start;
    uint16_t filelength = pEntry->length;

    FILE* foutput = ::fopen(filename, "wb");
    if (foutput == nullptr)
    {
        fprintf(stderr, "Failed to open output file %s: error %d\n", filename, errno);
        return IT_STOP;
    }

    for (uint16_t blockpos = 0; blockpos < filelength; blockpos++)
    {
        int blockno = filestart + blockpos;
        if (blockno >= r->di_p->GetBlockCount())
        {
            fprintf(stderr, "WARNING: For file %s block %d is beyond the end "
                    "of the image file.\n", filename, blockno);
            return IT_STOP;
        }
        uint8_t* pData = (uint8_t*)r->di_p->GetBlock(blockno);
        size_t nBytesWritten = ::fwrite(pData, sizeof(uint8_t), RT11_BLOCK_SIZE, foutput);
        if (nBytesWritten < RT11_BLOCK_SIZE)
        {
            fprintf(stderr, "Failed to write output file\n");  //TODO: Show error number
            ::fclose(foutput);
            return IT_STOP;
        }
    }
    ::fclose(foutput);
    return IT_NEXT;
}

void CDiskImage::SaveAllUnusedEntriesToExternalFiles()
{
    struct d_save_unused    res;
    res.di_p = this;
    res.unusedno = 0;

    printf("Extracting files:\n\n");
    PrintTableHeader();

    Iterate(cb_save_unused, &res);

    PrintTableFooter();
    FlushChanges();

    printf("\nDone.\n");
}

//////////////////////////////////////////////////////////////////////

CVolumeInformation::CVolumeInformation()
{
    memset(volumeid, 0, sizeof(volumeid));
    memset(ownername, 0, sizeof(ownername));
    memset(systemid, 0, sizeof(systemid));
    firstcatalogblock = systemversion = 0;
    catalogextrawords = catalogentrylength = catalogentriespersegment = catalogsegmentcount = 0;
    lastopenedsegment = 0;
    catalogsegments = nullptr;
    catalogentriescount = 0;
}

CVolumeInformation::~CVolumeInformation()
{
    if (catalogsegments != nullptr)
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
    // FIXME pCatalog[5] - channel (lsb), job (msb) - used for E_TENT
    datepac = pCatalog[6];

    if (status != RT11_STATUS_EMPTY && status != RT11_STATUS_ENDMARK)
    {
        char*   p = name;
        if (status == RT11_STATUS_TENTATIVE)
        {
            *p++ = '!';
        }
        r50asc(6, namerad50, p);
        p[6] = 0;
        r50asc(3, namerad50 + 2, ext);
        ext[3] = 0;
    }
}

void CVolumeCatalogEntry::Pack(uint16_t* pCatalog)
{
    pCatalog[0] = status;
    if (status == RT11_STATUS_EMPTY || status == RT11_STATUS_ENDMARK)
    {
        memset(pCatalog + 1, 0, sizeof(uint16_t) * 3);
    }
    else
    {
        uint16_t namerad50[3];
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
    {
        printf("< UNUSED >  %5d            %5d %8d\n",
               length, start, length * RT11_BLOCK_SIZE);
    }
    else
    {
        char datestr[16];
        rt11date_str(datepac, datestr, sizeof(datestr));
        printf("%s.%s  %5d  %s %5d %8d\n",
               name, ext, length, datestr, start, length * RT11_BLOCK_SIZE);
    }
}


//////////////////////////////////////////////////////////////////////
