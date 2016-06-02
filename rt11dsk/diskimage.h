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

struct CCachedBlock;
struct CVolumeInformation;
struct CVolumeCatalogSegment;

//////////////////////////////////////////////////////////////////////

/* Size of each RT-11 disk block, 512 or 0x200 bytes */
#define RT11_BLOCK_SIZE         512

#define NETRT11_IMAGE_HEADER_SIZE  256


//////////////////////////////////////////////////////////////////////
// Структура для хранения информации о томе
struct CVolumeInformation
{
    char volumeid[13];
    char ownername[13];
    char systemid[13];
    WORD firstcatalogblock;
    WORD systemversion;
    WORD catalogextrawords;
    WORD catalogentrylength;
    WORD catalogentriespersegment;
    WORD catalogsegmentcount;
    WORD lastopenedsegment;
    // Массив сегментов
    CVolumeCatalogSegment* catalogsegments;
    WORD catalogentriescount;  // Количество валидных записей каталога, включая завершающую ENDMARK

public:
    CVolumeInformation();
    ~CVolumeInformation();
};


//////////////////////////////////////////////////////////////////////
// Образ диска в формате .dsk либо .rtd

class CDiskImage
{
protected:
    FILE*           m_fpFile;
    bool            m_okCloseFile;   // true - close m_fpFile in Detach(), false - do not close it
    bool            m_okReadOnly;
    long            m_lStartOffset;  // First block start offset in the image file
    int             m_nTotalBlocks;  // Total blocks in the image
    int             m_nCacheBlocks;  // Cache size in blocks
    CCachedBlock*   m_pCache;
    CVolumeInformation m_volumeinfo;

public:
    CDiskImage();
    ~CDiskImage();

public:
    bool Attach(LPCTSTR sFileName);
    bool Attach(FILE* fpfile, long offset, int blocks, bool readonly);
    void Detach();

public:
    int IsReadOnly() const { return m_okReadOnly; }
    int GetBlockCount() const { return m_nTotalBlocks; }

public:
    void PrintCatalogDirectory();
    void PrintTableHeader();
    void PrintTableFooter();
    void* GetBlock(int nBlock);
    void MarkBlockChanged(int nBlock);
    void FlushChanges();
    void DecodeImageCatalog();
    void UpdateCatalogSegment(CVolumeCatalogSegment* pSegment);
    void SaveEntryToExternalFile(LPCTSTR sFileName);
    void SaveAllEntriesToExternalFiles();
    void AddFileToImage(LPCTSTR sFileName);

private:
    void PostAttach();
    long GetBlockOffset(int nBlock) const;

};


//////////////////////////////////////////////////////////////////////
