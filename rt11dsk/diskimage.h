/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// diskimage.h : Disk image utilities headers

#include <time.h>
#include "hostfile.h"

struct CCachedBlock;
struct CVolumeInformation;
struct CVolumeCatalogSegment;
class CDiskImage;

//////////////////////////////////////////////////////////////////////

/* Size of each RT-11 disk block, 512 or 0x200 bytes */
#define RT11_BLOCK_SIZE         512

#define NETRT11_IMAGE_HEADER_SIZE  256


//////////////////////////////////////////////////////////////////////

struct CCachedBlock
{
    int     nBlock;
    void*   pData;
    bool    bChanged;
    time_t cLastUsage;  // GetTickCount() for last usage
};


// Структура для хранения разобранной строки каталога
struct CVolumeCatalogEntry
{
public:  // Упакованные поля записи
    uint16_t status;    // See RT11_STATUS_xxx constants
    uint16_t datepac;   // Упакованное поле даты
    uint16_t start;     // File start block number
    uint16_t length;    // File length in 512-byte blocks
    uint16_t namerad50[3];  // filename in radix50: 6.3
public:  // Распакованные поля записи
    char name[8];  // File name - 6 characters
    char ext[4];   // File extension - 3 characters

public:
    CVolumeCatalogEntry();
    void Unpack(uint16_t const * pSrc, uint16_t filestartblock);  // Распаковка записи из каталога
    void Pack(uint16_t* pDest);   // Упаковка записи в каталог
    void Print();  // Печать строки каталога на консоль
    void Store(CHostFile* hf_p, CDiskImage* di_p); // Сохранить файл в образ диска
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
// Структура для хранения информации о томе

struct CVolumeInformation
{
    char volumeid[13];
    char ownername[13];
    char systemid[13];
    uint16_t firstcatalogblock;
    uint16_t systemversion;
    uint16_t catalogextrawords;
    uint16_t catalogentrylength;
    uint16_t catalogentriespersegment;
    uint16_t catalogsegmentcount;
    uint16_t lastopenedsegment;
    // Массив сегментов
    CVolumeCatalogSegment* catalogsegments;
    uint16_t catalogentriescount;  // Количество валидных записей каталога, включая завершающую ENDMARK

public:
    CVolumeInformation();
    ~CVolumeInformation();
};

//////////////////////////////////////////////////////////////////////

enum EIterOp
{
    IT_STOP = 0, // stop the iterator (loops over catalog entries)
    IT_NEXT = 1
};

typedef EIterOp (*lookup_fn_t)(CVolumeCatalogEntry* pEntry, void* opaque);

//////////////////////////////////////////////////////////////////////
// Образ диска в формате .dsk либо .rtd

class CDiskImage
{
protected:
    FILE*           m_fpFile;
    bool            m_okCloseFile;   // true - close m_fpFile in Detach(), false - do not close it
    bool            m_okReadOnly;
    long            m_lStartOffset;  // First block start offset in the image file
    bool            m_okInterleaving;  // Sector interleaving used for MS0515 disks
    int             m_nTotalBlocks;  // Total blocks in the image
    int             m_nCacheBlocks;  // Cache size in blocks
    int             m_seg_idx; // current segment number in the iterator
    int             m_file_idx; // current file index in the iterator
    CCachedBlock*   m_pCache;
    CVolumeInformation m_volumeinfo;

public:
    CDiskImage();
    ~CDiskImage();

public:
    bool Attach(const char * sFileName, long offset = 0, bool interleaving = false);
    bool Attach(FILE* fpfile, long offset, bool interleaving, int blocks, bool readonly);
    void Detach();

public:
    int IsReadOnly() const { return m_okReadOnly; }
    int GetBlockCount() const { return m_nTotalBlocks; }
    int iterSegmentIdx(void) const { return m_seg_idx; }
    int iterFileIdx(void) const { return m_file_idx; }
    int getEntriesPerSegment(void) const
    {
        return m_volumeinfo.catalogentriespersegment;
    }
    bool IsCurrentSegmentFull(void)
    {
        return m_volumeinfo.catalogsegments[m_seg_idx].entriesused ==
               m_volumeinfo.catalogentriespersegment;
    }
public:
    void PrintCatalogDirectory();
    void PrintTableHeader();
    void PrintTableFooter();
    void* GetBlock(int nBlock);
    void MarkBlockChanged(int nBlock);
    void FlushChanges();
    void DecodeImageCatalog();
    void UpdateCatalogSegment(int segno);
    void SaveEntryToExternalFile(const char * sFileName);
    void SaveAllEntriesToExternalFiles();
    void AddFileToImage(const char * sFileName);
    void DeleteFileFromImage(const char * sFileName);
    void SaveAllUnusedEntriesToExternalFiles();
    bool Iterate(lookup_fn_t, void* opaque);

private:
    void PostAttach();
    long GetBlockOffset(int nBlock) const;

};


//////////////////////////////////////////////////////////////////////
