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

#include "rt11dsk.h"
#include "hardimage.h"
#include "diskimage.h"

//////////////////////////////////////////////////////////////////////

struct CPartitionInfo
{
    long    offset;     // Offset from file start
    uint16_t blocks;     // Size in blocks

public:
    void Print(int number);
};

//////////////////////////////////////////////////////////////////////

// Inverts 512 bytes in the buffer
static void InvertBuffer(void* buffer)
{
    uint32_t* p = (uint32_t*) buffer;
    for (int i = 0; i < 128; i++)
    {
        *p = ~(*p);
        p++;
    }
}

// Verifies UKNC HDD home block checksum
static uint32_t CheckHomeBlockChecksum(void* buffer)
{
    uint16_t* p = (uint16_t*) buffer;
    uint32_t crc = 0;
    for (int i = 0; i < 255; i++)
    {
        crc += (uint32_t) * p;
        p++;
    }
    crc += ((uint32_t) * p) << 16;

    return crc;
}

//////////////////////////////////////////////////////////////////////

static uint8_t g_hardbuffer[512];

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

bool CHardImage::Attach(const char * sImageFileName)
{
    // Try to open as Normal first, then as ReadOnly
    m_okReadOnly = false;
    m_fpFile = ::fopen(sImageFileName, "r+b");
    if (m_fpFile == NULL)
    {
        m_okReadOnly = true;
        m_fpFile = ::fopen(sImageFileName, "rb");
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
        printf("Failed to read first 512 bytes of the hard disk image file.\n");
        _exit(-1);
    }

    // Check for inverted image
    uint8_t test = 0xff;
    for (int i = 0x1f0; i <= 0x1fb; i++)
        test &= g_hardbuffer[i];
    m_okInverted = (test == 0xff);
    // Invert the buffer if needed
    if (m_okInverted)
        InvertBuffer(g_hardbuffer);

    // Calculate and verify checksum
    uint32_t checksum = CheckHomeBlockChecksum(g_hardbuffer);
    //wprintf(_T("Home block checksum is 0x%08lx.\n"), checksum);
    m_okChecksum = checksum == 0;
    if (checksum != 0)
        printf("Home block checksum is incorrect!\n");

    m_nSectorsPerTrack = g_hardbuffer[0];
    m_nSidesPerTrack = g_hardbuffer[1];

    m_drivertype = HDD_DRIVER_UNKNOWN;
    uint16_t wdwaittime = ((uint16_t*)g_hardbuffer)[0122 / 2];
    uint16_t wdhidden = ((uint16_t*)g_hardbuffer)[0124 / 2];
    if (wdwaittime != 0 || wdhidden != 0)
        m_drivertype = HDD_DRIVER_WD;

    // Count partitions
    int count = 0;
    long totalblocks = 0;
    for (int i = 1; i < 24; i++)
    {
        uint16_t blocks = *((uint16_t*)g_hardbuffer + i);
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
            uint16_t blocks = *((uint16_t*)g_hardbuffer + i + 1);
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
    printf("Image file size: %ld bytes, %ld blocks\n", m_lFileSize, m_lFileSize / 512);
    printf("Disk geometry: %d sectors/track, %d heads\n", m_nSectorsPerTrack, m_nSidesPerTrack);
}

void CHardImage::PrintPartitionTable()
{
    printf("  #  Blocks  Bytes      Offset\n"
           "---  ------  ---------  ----------\n");

    long blocks = 0;
    for (int i = 0; i < m_nPartitions; i++)
    {
        m_pPartitionInfos[i].Print(i);
        blocks += m_pPartitionInfos[i].blocks;
    }

    printf("---  ------  ---------  ----------\n");

    printf("     %6ld\n", blocks);
}

void CHardImage::SavePartitionToFile(int partition, const char * filename)
{
    if (partition < 0 || partition >= m_nPartitions)
    {
        printf("Wrong partition number specified.\n");
        return;
    }

    // Open output file
    FILE* foutput = NULL;
    errno_t err = fopen_s(&foutput, filename, "wb");
    if (err != 0)
    {
        printf("Failed to open output file %s: error %d\n", filename, err);
        return;
    }

    CPartitionInfo* pPartInfo = m_pPartitionInfos + partition;
    printf("Extracting partition number %d to file %s\n", partition, filename);
    printf("Saving %d blocks, %ld bytes.\n", pPartInfo->blocks, ((uint32_t)pPartInfo->blocks) * RT11_BLOCK_SIZE);

    // Copy data
    ::fseek(m_fpFile, pPartInfo->offset, SEEK_SET);
    for (int i = 0; i < pPartInfo->blocks; i++)
    {
        size_t lBytesRead = ::fread(g_hardbuffer, sizeof(uint8_t), RT11_BLOCK_SIZE, m_fpFile);
        if (lBytesRead != RT11_BLOCK_SIZE)
        {
            printf("Failed to read hard disk image file.\n");
            fclose(foutput);
            return;
        }

        if (m_okInverted)
            InvertBuffer(g_hardbuffer);

        size_t nBytesWritten = fwrite(g_hardbuffer, sizeof(uint8_t), RT11_BLOCK_SIZE, foutput);
        if (nBytesWritten != RT11_BLOCK_SIZE)
        {
            printf("Failed to write to output file.\n");
            fclose(foutput);
            return;
        }
    }
    fclose(foutput);

    printf("\nDone.\n");
}

void CHardImage::UpdatePartitionFromFile(int partition, const char * filename)
{
    if (partition < 0 || partition >= m_nPartitions)
    {
        printf("Wrong partition number specified.\n");
        return;
    }

    //TODO: Check if m_okReadOnly

    // Open input file
    FILE* finput = NULL;
    errno_t err = fopen_s(&finput, filename, "rb");
    if (err != 0)
    {
        printf("Failed to open input file %s: error %d\n", filename, err);
        return;
    }

    CPartitionInfo* pPartInfo = m_pPartitionInfos + partition;
    printf("Updating partition number %d from file %s\n", partition, filename);

    // Get input file size, compare to the partition size
    ::fseek(finput, 0, SEEK_END);
    long lFileLength = ::ftell(finput);
    if (lFileLength != ((long)pPartInfo->blocks) * RT11_BLOCK_SIZE)
    {
        printf("The input file has wrong size: %ld, expected %ld.\n", lFileLength, ((long)pPartInfo->blocks) * RT11_BLOCK_SIZE);
        fclose(finput);
        return;
    }

    printf("Copying %d blocks, %ld bytes.\n", pPartInfo->blocks, ((uint32_t)pPartInfo->blocks) * RT11_BLOCK_SIZE);

    // Copy data
    ::fseek(finput, 0, SEEK_SET);
    ::fseek(m_fpFile, pPartInfo->offset, SEEK_SET);
    for (int i = 0; i < pPartInfo->blocks; i++)
    {
        size_t lBytesRead = ::fread(g_hardbuffer, sizeof(uint8_t), RT11_BLOCK_SIZE, finput);
        if (lBytesRead != RT11_BLOCK_SIZE)
        {
            printf("Failed to read input file.\n");
            fclose(finput);
            return;
        }

        if (m_okInverted)
            InvertBuffer(g_hardbuffer);

        size_t nBytesWritten = fwrite(g_hardbuffer, sizeof(uint8_t), RT11_BLOCK_SIZE, m_fpFile);
        if (nBytesWritten != RT11_BLOCK_SIZE)
        {
            printf("Failed to write to hard image file.\n");
            fclose(finput);
            return;
        }
    }
    fclose(finput);

    printf("\nDone.\n");
}

void CHardImage::InvertImage()
{
    long blocks = m_lFileSize / RT11_BLOCK_SIZE;
    printf("Inverting %ld blocks, %ld bytes.\n", blocks, blocks * RT11_BLOCK_SIZE);

    for (long i = 0; i < blocks; i++)
    {
        long offset = i * RT11_BLOCK_SIZE;

        ::fseek(m_fpFile, offset, SEEK_SET);
        size_t lBytesRead = ::fread(g_hardbuffer, sizeof(uint8_t), RT11_BLOCK_SIZE, m_fpFile);
        if (lBytesRead != RT11_BLOCK_SIZE)
        {
            printf("Failed to read hard disk image file.\n");
            return;
        }

        InvertBuffer(g_hardbuffer);

        ::fseek(m_fpFile, offset, SEEK_SET);
        size_t nBytesWritten = fwrite(g_hardbuffer, sizeof(uint8_t), RT11_BLOCK_SIZE, m_fpFile);
        if (nBytesWritten != RT11_BLOCK_SIZE)
        {
            printf("Failed to write to hard disk image file.\n");
            return;
        }
    }

    printf("\nDone.\n");
}


//////////////////////////////////////////////////////////////////////

void CPartitionInfo::Print(int number)
{
    long bytes = ((long)blocks) * 512;
    printf("%3d  %6d %10ld  0x%08lx\n", number, blocks, bytes, offset);
}


//////////////////////////////////////////////////////////////////////
