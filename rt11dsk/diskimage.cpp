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

#include "rt11dsk.h"
#include <time.h>
#include "diskimage.h"


//////////////////////////////////////////////////////////////////////

struct CCachedBlock
{
    int     nBlock;
    void*   pData;
    bool    bChanged;
    clock_t cLastUsage;  // GetTickCount() for last usage
};


//////////////////////////////////////////////////////////////////////
// Структуры данных, представляющие информацию о томе RT-11

/* Types for rtFileEntry 'status' */
#define RT11_STATUS_TENTATIVE   256     /* Temporary file */
#define RT11_STATUS_EMPTY       512     /* Marks empty space */
#define RT11_STATUS_PERM        1024    /* A "real" file */
#define RT11_STATUS_ENDMARK     2048    /* Marks the end of file entries */

// Структура для хранения разобранной строки каталога
struct CVolumeCatalogEntry
{
public:  // Упакованные поля записи
    uint16_t status;    // See RT11_STATUS_xxx constants
    uint16_t datepac;   // Упакованное поле даты
    uint16_t start;     // File start block number
    uint16_t length;    // File length in 512-byte blocks
public:  // Распакованные поля записи
    char name[7];  // File name - 6 characters
    char ext[4];   // File extension - 3 characters

public:
    CVolumeCatalogEntry();
    void Unpack(uint16_t const * pSrc, uint16_t filestartblock);  // Распаковка записи из каталога
    void Pack(uint16_t* pDest);   // Упаковка записи в каталог
    void Print();  // Печать строки каталога на консоль
};

// Структура данных для сегмента каталога
struct CVolumeCatalogSegment
{
public:
    uint16_t segmentblock;  // Блок на диске, в котором расположен этот сегмент каталога
    uint16_t entriesused;   // Количество использованых записей каталога
public:
    uint16_t nextsegment;   // Номер следующего сегмента
    uint16_t start;         // Номер блока, с которого начинаются файлы этого сегмента
    // Массив записей каталога, размером в максимально возможное кол-во записей для этого сегмента
    CVolumeCatalogEntry* catalogentries;
};


//////////////////////////////////////////////////////////////////////

static uint16_t g_segmentBuffer[512];


void CDiskImage::UpdateCatalogSegment(CVolumeCatalogSegment* pSegment)
{
    uint8_t* pBlock1 = (uint8_t*) GetBlock(pSegment->segmentblock);
    memcpy(g_segmentBuffer, pBlock1, 512);
    uint8_t* pBlock2 = (uint8_t*) GetBlock(pSegment->segmentblock + 1);
    memcpy(g_segmentBuffer + 256, pBlock2, 512);
    uint16_t* pData = g_segmentBuffer;

    pData += 5;  // Пропускаем заголовок сегмента
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

// Parse sFileName as RT11 file name, 6.3 format
static void ParseFileName63(const char * sFileName, char * filename, char * fileext)
{
    const char * sFilenameExt = strrchr(sFileName, '.');
    if (sFilenameExt == nullptr)
    {
        printf("Wrong filename format: %s\n", sFileName);
        return;
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
    for (uint16_t i = 0; i < nFilenameLength; i++) filename[i] = sFileName[i];
    filename[6] = 0;
    _strupr_s(filename, 7);
    for (int i = 0; i < 3; i++) fileext[i] = ' ';
    for (uint16_t i = 0; i < nFileextLength; i++) fileext[i] = sFilenameExt[i + 1];
    fileext[3] = 0;
    _strupr_s(fileext, 4);
}


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
            _exit(-1);
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
        printf("Cache is full.\n");
        _exit(-1);
    }

    m_pCache[iEmpty].nBlock = nBlock;
    m_pCache[iEmpty].bChanged = false;
    m_pCache[iEmpty].pData = ::calloc(1, RT11_BLOCK_SIZE);
    if (m_pCache[iEmpty].pData == nullptr)
    {
        printf("Failed to allocate memory for block number %d.\n", nBlock);
        _exit(-1);
    }
    m_pCache[iEmpty].cLastUsage = ::clock();

    // Load the block data
    long foffset = GetBlockOffset(nBlock);
    ::fseek(m_fpFile, foffset, SEEK_SET);
    size_t lBytesRead = ::fread(m_pCache[iEmpty].pData, 1, RT11_BLOCK_SIZE, m_fpFile);
    if (lBytesRead != RT11_BLOCK_SIZE)
    {
        printf("Failed to read block number %d.\n", nBlock);
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

    // Разбор Home Block
    uint8_t* pHomeSector = (uint8_t*) GetBlock(1);
    uint16_t nFirstCatalogBlock = pHomeSector[0724];  // Это должен быть блок номер 6
    if (nFirstCatalogBlock > 10)
    {
        printf("First catalog block is %d, out of range.\n", nFirstCatalogBlock);
        _exit(-1);
    }
    if (nFirstCatalogBlock == 0) nFirstCatalogBlock = 6;
    m_volumeinfo.firstcatalogblock = nFirstCatalogBlock;
    m_volumeinfo.systemversion = pHomeSector[0726];
    const char* sVolumeId = (const char*) pHomeSector + 0730;
    strncpy_s(m_volumeinfo.volumeid, 13, sVolumeId, 12);
    const char* sOwnerName = (const char*) pHomeSector + 0744;
    strncpy_s(m_volumeinfo.ownername, 13, sOwnerName, 12);
    const char* sSystemId = (const char*) pHomeSector + 0760;
    strncpy_s(m_volumeinfo.systemid, 13, sSystemId, 12);

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
        _exit(-1);
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

void CDiskImage::PrintCatalogDirectory()
{
    printf(" Volume: %s\n", m_volumeinfo.volumeid);
    printf(" Owner:  %s\n", m_volumeinfo.ownername);
    printf(" System: %s\n", m_volumeinfo.systemid);
    printf("\n");
    printf(" %d available segments, last opened segment: %d\n", m_volumeinfo.catalogsegmentcount, m_volumeinfo.lastopenedsegment);
    printf("\n");
    PrintTableHeader();

    uint16_t nFilesCount = 0;
    uint16_t nBlocksCount = 0;
    uint16_t nFreeBlocksCount = 0;
    for (int segmno = 0; segmno < m_volumeinfo.catalogsegmentcount; segmno++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segmno;
        if (pSegment->catalogentries == nullptr) continue;

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
    printf(" %d files, %d blocks\n", nFilesCount, nBlocksCount);
    printf(" %d free blocks\n\n", nFreeBlocksCount);
}

void CDiskImage::SaveEntryToExternalFile(const char * sFileName)
{
    // Parse sFileName
    char filename[7];
    char fileext[4];
    ParseFileName63(sFileName, filename, fileext);

    // Search for the filename/fileext
    CVolumeCatalogEntry* pFileEntry = nullptr;
    for (int segmno = 0; segmno < m_volumeinfo.catalogsegmentcount; segmno++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segmno;
        if (pSegment->catalogentries == nullptr) continue;

        for (int entryno = 0; entryno < m_volumeinfo.catalogentriespersegment; entryno++)
        {
            CVolumeCatalogEntry* pEntry = pSegment->catalogentries + entryno;

            if (pEntry->status == RT11_STATUS_ENDMARK) break;
            if (pEntry->status == 0) continue;
            if (pEntry->status == RT11_STATUS_EMPTY) continue;

            if (_strnicmp(filename, pEntry->name, 6) == 0 &&
                _strnicmp(fileext, pEntry->ext, 3) == 0)
            {
                pFileEntry = pEntry;
                break;
            }
        }
    }
    if (pFileEntry == nullptr)
    {
        printf("Filename not found: %s\n", sFileName);
        return;
    }
    printf("Extracting file:\n\n");
    PrintTableHeader();
    pFileEntry->Print();
    PrintTableFooter();

    // Collect file name + ext without trailing spaces
    char sfilename[11];
    strcpy(sfilename, pFileEntry->name);
    char * p = sfilename + 5;
    while (p > sfilename && *p == ' ') p--;
    p++;
    *p = '.';
    p++;
    strcpy(p, pFileEntry->ext);

    uint16_t filestart = pFileEntry->start;
    uint16_t filelength = pFileEntry->length;

    FILE* foutput = nullptr;
    foutput = fopen(sfilename, "wb");
    if (foutput == nullptr)
    {
        printf("Failed to open output file %s: error %d\n", sFileName, errno);
        return;
    }

    for (uint16_t blockpos = 0; blockpos < filelength; blockpos++)
    {
        uint8_t* pData = (uint8_t*) GetBlock(filestart + blockpos);
        size_t nBytesWritten = fwrite(pData, sizeof(uint8_t), RT11_BLOCK_SIZE, foutput);
        if (nBytesWritten < RT11_BLOCK_SIZE)
        {
            printf("Failed to write output file\n");  //TODO: Show error number
            return;
        }
    }

    fclose(foutput);

    printf("\nDone.\n");
}

void CDiskImage::SaveAllEntriesToExternalFiles()
{
    printf("Extracting files:\n\n");
    PrintTableHeader();

    for (int segmno = 0; segmno < m_volumeinfo.catalogsegmentcount; segmno++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segmno;
        if (pSegment->catalogentries == nullptr) continue;

        for (int entryno = 0; entryno < m_volumeinfo.catalogentriespersegment; entryno++)
        {
            CVolumeCatalogEntry* pEntry = pSegment->catalogentries + entryno;

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
                    return;
                }
            }

            fclose(foutput);
        }
    }
    PrintTableFooter();

    printf("\nDone.\n");
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
//NOTE: Пока НЕ проверяем что файл с таким именем уже есть, и НЕ выдаем ошибки
void CDiskImage::AddFileToImage(const char * sFileName)
{
    // Parse sFileName
    char filename[7];
    char fileext[4];
    ParseFileName63(sFileName, filename, fileext);

    // Открываем помещаемый файл на чтение
    FILE* fpFile = ::fopen(sFileName, "rb");
    if (fpFile == nullptr)
    {
        printf("Failed to open the file.");
        return;
    }

    // Определяем длину файла, с учетом округления до полного блока
    ::fseek(fpFile, 0, SEEK_END);
    long lFileLength = ::ftell(fpFile);  // Точная длина файла
    uint16_t nFileSizeBlocks =  // Требуемая ширина свободного места в блоках
        (uint16_t) ((lFileLength + RT11_BLOCK_SIZE - 1) / RT11_BLOCK_SIZE);
    uint32_t dwFileSize =  // Длина файла с учетом округления до полного блока
        ((uint32_t) nFileSizeBlocks) * RT11_BLOCK_SIZE;
    //TODO: Проверка на файл нулевой длины
    //TODO: Проверка, не слишком ли длинный файл для этого тома

    // Выделяем память и считываем данные файла
    void* pFileData = ::calloc(dwFileSize, 1);
    ::fseek(fpFile, 0, SEEK_SET);
    size_t lBytesRead = ::fread(pFileData, 1, lFileLength, fpFile);
    if (lBytesRead != lFileLength)
    {
        printf("Failed to read the file.\n");
        _exit(-1);
    }
    ::fclose(fpFile);

    printf("File size is %ld bytes or %d blocks\n", lFileLength, nFileSizeBlocks);

    // Перебираются все записи каталога, пока не будет найдена пустая запись длины >= dwFileLength
    //TODO: Выделить в отдельную функцию и искать наиболее подходящую запись, с минимальной разницей по длине
    CVolumeCatalogEntry* pFileEntry = nullptr;
    CVolumeCatalogSegment* pFileSegment = nullptr;
    for (int segmno = 0; segmno < m_volumeinfo.catalogsegmentcount; segmno++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segmno;
        if (pSegment->catalogentries == nullptr) continue;

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
    if (pFileEntry == nullptr)
    {
        printf("Empty catalog entry with %d or more blocks not found\n", nFileSizeBlocks);
        free(pFileData);
        return;
    }
    printf("Found empty catalog entry with %d blocks:\n\n", pFileEntry->length);
    PrintTableHeader();
    pFileEntry->Print();
    PrintTableFooter();

    // Определяем, нужна ли новая запись каталога
    bool okNeedNewCatalogEntry = (pFileEntry->length != nFileSizeBlocks);
    CVolumeCatalogEntry* pEmptyEntry = nullptr;
    if (okNeedNewCatalogEntry)
    {
        // Проверяем, нужно ли для новой записи каталога открывать новый сегмент каталога
        if (pFileSegment->entriesused == m_volumeinfo.catalogentriespersegment)
        {
            printf("New catalog segment needed - not implemented now, sorry.\n");
            free(pFileData);
            return;
        }

        // Сдвигаем записи сегмента начиная с пустой на одну вправо - освобождаем место под новую запись
        int fileentryindex = (int) (pFileEntry - pFileSegment->catalogentries);
        int totalentries = m_volumeinfo.catalogentriespersegment;
        memmove(pFileEntry + 1, pFileEntry, (totalentries - fileentryindex - 1) * sizeof(CVolumeCatalogEntry));

        // Новая пустая запись каталога
        pEmptyEntry = pFileEntry + 1;
        // Заполнить данные новой записи каталога
        pEmptyEntry->status = RT11_STATUS_EMPTY;
        pEmptyEntry->start = pFileEntry->start + nFileSizeBlocks;
        pEmptyEntry->length = pFileEntry->length - nFileSizeBlocks;
        pEmptyEntry->datepac = pFileEntry->datepac;
    }

    // Изменяем существующую запись каталога
    pFileEntry->length = nFileSizeBlocks;
    strcpy_s(pFileEntry->name, 7, filename);
    strcpy_s(pFileEntry->ext, 4, fileext);
    pFileEntry->datepac = 0;
    pFileEntry->status = RT11_STATUS_PERM;

    printf("\nCatalog entries to update:\n\n");
    PrintTableHeader();
    pFileEntry->Print();
    if (pEmptyEntry != nullptr) pEmptyEntry->Print();
    PrintTableFooter();

    // Сохраняем новый файл поблочно
    printf("\nWriting file data...\n");
    uint16_t nFileStartBlock = pFileEntry->start;  // Начиная с какого блока размещается новый файл
    uint16_t nBlock = nFileStartBlock;
    for (int block = 0; block < nFileSizeBlocks; block++)
    {
        uint8_t* pFileBlockData = ((uint8_t*) pFileData) + block * RT11_BLOCK_SIZE;
        uint8_t* pData = (uint8_t*) GetBlock(nBlock);
        memcpy(pData, pFileBlockData, RT11_BLOCK_SIZE);
        // Сообщаем что блок был изменен
        MarkBlockChanged(nBlock);

        nBlock++;
    }
    free(pFileData);

    // Сохраняем сегмент каталога на диск
    printf("Updating catalog segment...\n");
    UpdateCatalogSegment(pFileSegment);

    FlushChanges();

    printf("\nDone.\n");
}

// Удаление файла
// Алгоритм:
//   Перебираются все записи каталога, пока не будет найдена запись данного файла
//   Запись каталога помечается как удалённая
void CDiskImage::DeleteFileFromImage(const char * sFileName)
{
    // Parse sFileName
    char filename[7];
    char fileext[4];
    ParseFileName63(sFileName, filename, fileext);

    // Search for the filename/fileext
    CVolumeCatalogEntry* pFileEntry = nullptr;
    CVolumeCatalogSegment* pFileSegment = nullptr;
    for (int segmno = 0; segmno < m_volumeinfo.catalogsegmentcount; segmno++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segmno;
        if (pSegment->catalogentries == nullptr) continue;

        for (int entryno = 0; entryno < m_volumeinfo.catalogentriespersegment; entryno++)
        {
            CVolumeCatalogEntry* pEntry = pSegment->catalogentries + entryno;

            if (pEntry->status == RT11_STATUS_ENDMARK) break;
            if (pEntry->status == 0) continue;
            if (pEntry->status == RT11_STATUS_EMPTY) continue;

            if (_strnicmp(filename, pEntry->name, 6) == 0 &&
                _strnicmp(fileext, pEntry->ext, 3) == 0)
            {
                pFileEntry = pEntry;
                pFileSegment = pSegment;
                break;
            }
        }
    }
    if (pFileEntry == nullptr || pFileSegment == nullptr)
    {
        printf("Filename not found: %s\n", sFileName);
        return;
    }
    printf("Deleting file:\n\n");
    PrintTableHeader();
    pFileEntry->Print();
    PrintTableFooter();

    // Изменяем существующую запись каталога
    pFileEntry->status = RT11_STATUS_EMPTY;

    // Сохраняем сегмент каталога на диск
    printf("Updating catalog segment...\n");
    UpdateCatalogSegment(pFileSegment);

    FlushChanges();

    printf("\nDone.\n");
}

void CDiskImage::SaveAllUnusedEntriesToExternalFiles()
{
    printf("Extracting files:\n\n");
    PrintTableHeader();

    int unusedno = 0;
    for (int segmno = 0; segmno < m_volumeinfo.catalogsegmentcount; segmno++)
    {
        CVolumeCatalogSegment* pSegment = m_volumeinfo.catalogsegments + segmno;
        if (pSegment->catalogentries == nullptr) continue;

        bool okSegmentChanged = false;
        for (int entryno = 0; entryno < m_volumeinfo.catalogentriespersegment; entryno++)
        {
            CVolumeCatalogEntry* pEntry = pSegment->catalogentries + entryno;

            if (pEntry->status == RT11_STATUS_ENDMARK) break;
            if (pEntry->status != RT11_STATUS_EMPTY) continue;

            pEntry->Print();

            unusedno++;
            char filename[20];
            printf_s(filename, 20, "UNUSED%02d", unusedno);

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
                int blockno = filestart + blockpos;
                if (blockno >= m_nTotalBlocks)
                {
                    printf("WARNING: For file %s block %d is beyond the end of the image file.\n", filename, blockno);
                    break;
                }
                uint8_t* pData = (uint8_t*)GetBlock(blockno);
                size_t nBytesWritten = fwrite(pData, sizeof(uint8_t), RT11_BLOCK_SIZE, foutput);
                //TODO: Check if nBytesWritten < RT11_BLOCK_SIZE
            }

            fclose(foutput);
        }
    }

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
}

void CVolumeCatalogEntry::Unpack(uint16_t const * pCatalog, uint16_t filestartblock)
{
    start = filestartblock;
    status = pCatalog[0];
    uint16_t namerad50[3];
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
        char datestr[10];
        rtDateStr(datepac, datestr);
        printf("%s.%s  %5d  %s %5d %8d\n",
               name, ext, length, datestr, start, length * RT11_BLOCK_SIZE);
    }
}


//////////////////////////////////////////////////////////////////////
