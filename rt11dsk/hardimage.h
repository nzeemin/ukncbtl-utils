/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// hardimage.h : HDD image utilities headers

//////////////////////////////////////////////////////////////////////

#define RT11_BLOCK_SIZE         512

enum HDDDriverType
{
    HDD_DRIVER_UNKNOWN = 0,
    HDD_DRIVER_ID = 1,
    HDD_DRIVER_WD = 2,
};

struct CPartitionInfo;
class CDiskImage;

class CHardImage
{
protected:
    FILE*           m_fpFile;
    bool            m_okReadOnly;
    bool            m_okInverted;       // Inverted image
    long            m_lFileSize;
    HDDDriverType   m_drivertype;
    int             m_nSectorsPerTrack;
    BYTE            m_nSidesPerTrack;
    int             m_nPartitions;
    CPartitionInfo* m_pPartitionInfos;
    bool            m_okChecksum;

public:
    CHardImage();
    ~CHardImage();

public:
    bool Attach(LPCTSTR sFileName);
    void Detach();
    bool PrepareDiskImage(int partition, CDiskImage* pdiskimage);

public:
    bool IsReadOnly() const { return m_okReadOnly; }
    int GetPartitionCount() const { return m_nPartitions; }
    bool IsChecksum() const { return m_okChecksum; }

public:
    void PrintImageInfo();
    void PrintPartitionTable();
    void SavePartitionToFile(int partition, LPCTSTR filename);
    void UpdatePartitionFromFile(int partition, LPCTSTR filename);
    void InvertImage();
};

//////////////////////////////////////////////////////////////////////
