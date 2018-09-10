/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// hardimage.cpp : HDD image utilities

#include "stdafx.h"
#include "hardimage.h"
#include "diskimage.h"

//////////////////////////////////////////////////////////////////////

struct CPartitionInfo
{
    long    offset;     // Offset from file start
    WORD    blocks;     // Size in blocks

public:
    void Print(int number);
};

//////////////////////////////////////////////////////////////////////

// Inverts 512 bytes in the buffer
static void InvertBuffer(void* buffer)
{
    DWORD* p = (DWORD*) buffer;
    for (int i = 0; i < 128; i++)
    {
        *p = ~(*p);
        p++;
    }
}

// Verifies UKNC HDD home block checksum
static DWORD CheckHomeBlockChecksum(void* buffer)
{
    WORD* p = (WORD*) buffer;
    DWORD crc = 0;
    for (int i = 0; i < 255; i++)
    {
        crc += (DWORD) * p;
        p++;
    }
    crc += ((DWORD) * p) << 16;

    return crc;
}

//////////////////////////////////////////////////////////////////////

static BYTE g_hardbuffer[512];

CHardImage::CHardImage()
{
    m_okReadOnly = m_okInverted = false;
    m_fpFile = NULL;
    m_lFileSize = 0;
    m_drivertype = HDD_DRIVER_UNKNOWN;
    m_nSectorsPerTrack = m_nSidesPerTrack = m_nPartitions = 0;
    m_pPartitionInfos = NULL;
    m_okChecksum = false;
}

CHardImage::~CHardImage()
{
    Detach();
}

bool CHardImage::Attach(LPCTSTR sImageFileName)
{
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

    // Get file size
    ::fseek(m_fpFile, 0, SEEK_END);
    m_lFileSize = ::ftell(m_fpFile);

    // Read first 512 bytes
    ::fseek(m_fpFile, 0, SEEK_SET);
    size_t lBytesRead = ::fread(g_hardbuffer, 1, 512, m_fpFile);
    if (lBytesRead != 512)
    {
        wprintf(_T("Failed to read first 512 bytes of the hard disk image file.\n"));
        _exit(-1);
    }

    // Check for inverted image
    BYTE test = 0xff;
    for (int i = 0x1f0; i <= 0x1fb; i++)
        test &= g_hardbuffer[i];
    m_okInverted = (test == 0xff);
    // Invert the buffer if needed
    if (m_okInverted)
        InvertBuffer(g_hardbuffer);

    // Calculate and verify checksum
    DWORD checksum = CheckHomeBlockChecksum(g_hardbuffer);
    //wprintf(_T("Home block checksum is 0x%08lx.\n"), checksum);
    m_okChecksum = checksum == 0;
    if (checksum != 0)
        wprintf(_T("Home block checksum is incorrect!\n"));

    m_nSectorsPerTrack = g_hardbuffer[0];
    m_nSidesPerTrack = g_hardbuffer[1];

    m_drivertype = HDD_DRIVER_UNKNOWN;
    WORD wdwaittime = ((WORD*)g_hardbuffer)[0122 / 2];
    WORD wdhidden = ((WORD*)g_hardbuffer)[0124 / 2];
    if (wdwaittime != 0 || wdhidden != 0)
        m_drivertype = HDD_DRIVER_WD;

    // Count partitions
    int count = 0;
    long totalblocks = 0;
    for (int i = 1; i < 24; i++)
    {
        WORD blocks = *((WORD*)g_hardbuffer + i);
        if (blocks == 0) break;
        if (blocks + totalblocks > (m_lFileSize / 512) - 1)
            break;
        count++;
        totalblocks += blocks;
    }

    m_nPartitions = count;
    if (m_nPartitions > 0)
    {
        m_pPartitionInfos = (CPartitionInfo*) ::calloc(m_nPartitions, sizeof(CPartitionInfo));

        // Prepare m_pPartitionInfos
        long offset = 512;
        for (int i = 0; i < m_nPartitions; i++)
        {
            m_pPartitionInfos[i].offset = offset;
            WORD blocks = *((WORD*)g_hardbuffer + i + 1);
            m_pPartitionInfos[i].blocks = blocks;
            offset += blocks * 512;
        }
    }

    return true;
}

void CHardImage::Detach()
{
    if (m_fpFile != NULL)
    {
        ::fclose(m_fpFile);
        m_fpFile = NULL;
    }
}

bool CHardImage::PrepareDiskImage(int partition, CDiskImage* pdiskimage)
{
    if (partition < 0 || partition >= m_nPartitions)
        return false;  // Wrong partition number

    CPartitionInfo* pinfo = m_pPartitionInfos + partition;
    return pdiskimage->Attach(m_fpFile, pinfo->offset, pinfo->blocks, m_okReadOnly);
}

void CHardImage::PrintImageInfo()
{
    wprintf(_T("Image file size: %ld bytes, %ld blocks\n"), m_lFileSize, m_lFileSize / 512);
    wprintf(_T("Disk geometry: %d sectors/track, %d heads\n"), m_nSectorsPerTrack, m_nSidesPerTrack);
}

void CHardImage::PrintPartitionTable()
{
    wprintf(_T("  #  Blocks  Bytes      Offset\n"));
    wprintf(_T("---  ------  ---------  ----------\n"));

    long blocks = 0;
    for (int i = 0; i < m_nPartitions; i++)
    {
        m_pPartitionInfos[i].Print(i);
        blocks += m_pPartitionInfos[i].blocks;
    }

    wprintf(_T("---  ------  ---------  ----------\n"));

    wprintf(_T("     %6ld\n"), blocks);
}

void CHardImage::SavePartitionToFile(int partition, LPCTSTR filename)
{
    if (partition < 0 || partition >= m_nPartitions)
    {
        wprintf(_T("Wrong partition number specified.\n"));
        return;
    }

    // Open output file
    FILE* foutput = NULL;
    errno_t err = _wfopen_s(&foutput, filename, _T("wb"));
    if (err != 0)
    {
        wprintf(_T("Failed to open output file %s: error %d\n"), filename, err);
        return;
    }

    CPartitionInfo* pPartInfo = m_pPartitionInfos + partition;
    wprintf(_T("Extracting partition number %d to file %s\n"), partition, filename);
    wprintf(_T("Saving %d blocks, %ld bytes.\n"), pPartInfo->blocks, ((DWORD)pPartInfo->blocks) * RT11_BLOCK_SIZE);

    // Copy data
    ::fseek(m_fpFile, pPartInfo->offset, SEEK_SET);
    for (int i = 0; i < pPartInfo->blocks; i++)
    {
        size_t lBytesRead = ::fread(g_hardbuffer, sizeof(BYTE), RT11_BLOCK_SIZE, m_fpFile);
        if (lBytesRead != RT11_BLOCK_SIZE)
        {
            wprintf(_T("Failed to read hard disk image file.\n"));
            fclose(foutput);
            return;
        }

        if (m_okInverted)
            InvertBuffer(g_hardbuffer);

        size_t nBytesWritten = fwrite(g_hardbuffer, sizeof(BYTE), RT11_BLOCK_SIZE, foutput);
        if (nBytesWritten != RT11_BLOCK_SIZE)
        {
            wprintf(_T("Failed to write to output file.\n"));
            fclose(foutput);
            return;
        }
    }
    fclose(foutput);

    wprintf(_T("\nDone.\n"));
}

void CHardImage::UpdatePartitionFromFile(int partition, LPCTSTR filename)
{
    if (partition < 0 || partition >= m_nPartitions)
    {
        wprintf(_T("Wrong partition number specified.\n"));
        return;
    }

    //TODO: Check if m_okReadOnly

    // Open input file
    FILE* finput = NULL;
    errno_t err = _wfopen_s(&finput, filename, _T("rb"));
    if (err != 0)
    {
        wprintf(_T("Failed to open input file %s: error %d\n"), filename, err);
        return;
    }

    CPartitionInfo* pPartInfo = m_pPartitionInfos + partition;
    wprintf(_T("Updating partition number %d from file %s\n"), partition, filename);

    // Get input file size, compare to the partition size
    ::fseek(finput, 0, SEEK_END);
    long lFileLength = ::ftell(finput);
    if (lFileLength != ((long)pPartInfo->blocks) * RT11_BLOCK_SIZE)
    {
        wprintf(_T("The input file has wrong size: %ld, expected %ld.\n"), lFileLength, ((long)pPartInfo->blocks) * RT11_BLOCK_SIZE);
        fclose(finput);
        return;
    }

    wprintf(_T("Copying %d blocks, %ld bytes.\n"), pPartInfo->blocks, ((DWORD)pPartInfo->blocks) * RT11_BLOCK_SIZE);

    // Copy data
    ::fseek(finput, 0, SEEK_SET);
    ::fseek(m_fpFile, pPartInfo->offset, SEEK_SET);
    for (int i = 0; i < pPartInfo->blocks; i++)
    {
        size_t lBytesRead = ::fread(g_hardbuffer, sizeof(BYTE), RT11_BLOCK_SIZE, finput);
        if (lBytesRead != RT11_BLOCK_SIZE)
        {
            wprintf(_T("Failed to read input file.\n"));
            fclose(finput);
            return;
        }

        if (m_okInverted)
            InvertBuffer(g_hardbuffer);

        size_t nBytesWritten = fwrite(g_hardbuffer, sizeof(BYTE), RT11_BLOCK_SIZE, m_fpFile);
        if (nBytesWritten != RT11_BLOCK_SIZE)
        {
            wprintf(_T("Failed to write to hard image file.\n"));
            fclose(finput);
            return;
        }
    }
    fclose(finput);

    wprintf(_T("\nDone.\n"));
}

void CHardImage::InvertImage()
{
    long blocks = m_lFileSize / RT11_BLOCK_SIZE;
    wprintf(_T("Inverting %ld blocks, %ld bytes.\n"), blocks, blocks * RT11_BLOCK_SIZE);

    for (long i = 0; i < blocks; i++)
    {
        long offset = i * RT11_BLOCK_SIZE;

        ::fseek(m_fpFile, offset, SEEK_SET);
        size_t lBytesRead = ::fread(g_hardbuffer, sizeof(BYTE), RT11_BLOCK_SIZE, m_fpFile);
        if (lBytesRead != RT11_BLOCK_SIZE)
        {
            wprintf(_T("Failed to read hard disk image file.\n"));
            return;
        }

        InvertBuffer(g_hardbuffer);

        ::fseek(m_fpFile, offset, SEEK_SET);
        size_t nBytesWritten = fwrite(g_hardbuffer, sizeof(BYTE), RT11_BLOCK_SIZE, m_fpFile);
        if (nBytesWritten != RT11_BLOCK_SIZE)
        {
            wprintf(_T("Failed to write to hard disk image file.\n"));
            return;
        }
    }

    wprintf(_T("\nDone.\n"));
}


//////////////////////////////////////////////////////////////////////

void CPartitionInfo::Print(int number)
{
    long bytes = ((long)blocks) * 512;
    wprintf(_T("%3d  %6d %10ld  0x%08lx\n"), number, blocks, bytes, offset);
}


//////////////////////////////////////////////////////////////////////
