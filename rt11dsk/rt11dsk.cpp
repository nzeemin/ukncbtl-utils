/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// main.cpp : Defines the entry point for the console application.
//

#include "rt11dsk.h"
#include "diskimage.h"
#include "hardimage.h"


//////////////////////////////////////////////////////////////////////
// Preliminary function declarations

void PrintWelcome();
void PrintUsage();
bool ParseCommandLine(int argc, char * argv[]);

void DoDiskList();
void DoDiskExtractFile();
void DoDiskExtractAllFiles();
void DoDiskAddFile();
void DoDiskDeleteFile();
void DoDiskExtractAllUnusedFiles();
void DoHardInvert();
void DoHardList();
void DoHardExtractPartition();
void DoHardUpdatePartition();
void DoHardPartitionList();
void DoHardPartitionExtractFile();
void DoHardPartitionAddFile();


//////////////////////////////////////////////////////////////////////
// Globals

#ifdef _MSC_VER
#define OPTIONCHAR '/'
#define OPTIONSTR "/"
#else
#define OPTIONCHAR '-'
#define OPTIONSTR "-"
#endif

const char * g_sCommand = nullptr;
const char * g_sImageFileName = nullptr;
const char * g_sFileName = nullptr;
bool    g_okHardCommand = false;
const char * g_sPartition = nullptr;
int     g_nPartition = -1;
long    g_lStartOffset = 0;
bool    g_okInterleaving = false;
bool    g_okHard32M = false;
bool    g_okTrimZeroes = false;

enum CommandRequirements
{
    CMDR_PARAM_FILENAME        = 4,    // Need FileName parameter
    CMDR_PARAM_PARTITION       = 8,    // Need Partition number parameter
    CMDR_IMAGEFILERW           = 32,   // Image file should be writable (not read-only)
};

struct CommandInfo
{
    const char * command;
    bool    okHard;             // true for hard disk image command, false for disk image command
    void    (*commandImpl)();   // Function implementing the option
    int     requirements;       // Command requirements, see CommandRequirements enum
}
static g_CommandInfos[] =
{
    { "l",    false,  DoDiskList,                   },
    { "e",    false,  DoDiskExtractFile,            CMDR_PARAM_FILENAME },
    { "x",    false,  DoDiskExtractAllFiles,        },
    { "a",    false,  DoDiskAddFile,                CMDR_PARAM_FILENAME | CMDR_IMAGEFILERW },
    { "d",    false,  DoDiskDeleteFile,             CMDR_PARAM_FILENAME | CMDR_IMAGEFILERW },
    { "xu",   false,  DoDiskExtractAllUnusedFiles,  },
    { "hi",   true,   DoHardInvert,                 CMDR_IMAGEFILERW },
    { "hl",   true,   DoHardList,                   },
    { "hx",   true,   DoHardExtractPartition,       CMDR_PARAM_PARTITION | CMDR_PARAM_FILENAME },
    { "hu",   true,   DoHardUpdatePartition,        CMDR_PARAM_PARTITION | CMDR_PARAM_FILENAME | CMDR_IMAGEFILERW },
    { "hpl",  true,   DoHardPartitionList,          CMDR_PARAM_PARTITION },
    { "hpe",  true,   DoHardPartitionExtractFile,   CMDR_PARAM_PARTITION | CMDR_PARAM_FILENAME },
    { "hpa",  true,   DoHardPartitionAddFile,       CMDR_PARAM_PARTITION | CMDR_PARAM_FILENAME | CMDR_IMAGEFILERW },
};
static const int g_CommandInfos_count = (int)(sizeof(g_CommandInfos) / sizeof(CommandInfo));

CDiskImage      g_diskimage;
CHardImage      g_hardimage;
CommandInfo*    g_pCommand = nullptr;


//////////////////////////////////////////////////////////////////////


void PrintWelcome()
{
    printf("RT11DSK Utility  by Nikita Zimin  [%s %s]\n\n", __DATE__, __TIME__);
}

void PrintUsage()
{
    printf("\nUsage:\n"
           "  Floppy disk image commands:\n"
           "    rt11dsk l <ImageFile>  - list image contents\n"
           "    rt11dsk e <ImageFile> <FileName>  - extract file\n"
           "    rt11dsk a <ImageFile> <FileName>  - add file\n"
           "    rt11dsk x <ImageFile>  - extract all files\n"
           "    rt11dsk d <ImageFile> <FileName>  - delete file\n"
           "    rt11dsk xu <ImageFile>  - extract all unused space\n"
           "  Hard disk image commands:\n"
           "    rt11dsk hl <HddImage>  - list HDD image partitions\n"
           "    rt11dsk hx <HddImage> <Partn> <FileName>  - extract partition to file\n"
           "    rt11dsk hu <HddImage> <Partn> <FileName>  - update partition from the file\n"
           "    rt11dsk hi <HddImage>  - invert HDD image file\n"
           "    rt11dsk hpl <HddImage> <Partn>  - list partition contents\n"
           "    rt11dsk hpe <HddImage> <Partn> <FileName>  - extract file from the partition\n"
           "    rt11dsk hpa <HddImage> <Partn> <FileName>  - add file to the partition\n"
           "  Parameters:\n"
           "    <ImageFile> is UKNC disk image in .dsk or .rtd format\n"
           "    <HddImage>  is UKNC hard disk image file name\n"
           "    <Partn>     is hard disk image partition number, 0..23\n"
           "    <FileName>  is a file name to read from or save to\n"
           "  Options:\n"
           "    " OPTIONSTR "oXXXXX  Set start offset to XXXXX; 0 by default (offsets 128 and 256 are detected by word 000240)\n"
           "    " OPTIONSTR "ms0515  Sector interleaving used for MS0515 disks\n"
           "    " OPTIONSTR "hd32    Hard disk with 32 MB partitions\n"
           "    " OPTIONSTR "trimz   (Extract file commands) Trim trailing zeroes in the last block\n"
          );
}

bool ParseCommandLine(int argc, char * argv[])
{
    for (int argn = 1; argn < argc; argn++)
    {
        const char * arg = argv[argn];
        if (arg[0] == OPTIONCHAR)
        {
            if (arg[1] == 'o')
            {
                if (1 != sscanf(arg + 2, "%ld", &g_lStartOffset))
                {
                    printf("Failed to parse option argument: %s\n", arg);
                    return false;
                }
            }
            else if (strcmp(arg + 1, "ms0515") == 0)
            {
                g_okInterleaving = true;
            }
            else if (strcmp(arg + 1, "hd32") == 0)
            {
                g_okHard32M = true;
            }
            else if (strcmp(arg + 1, "trimz") == 0)
            {
                g_okTrimZeroes = true;
            }
            else
            {
                printf("Unknown option: %s\n", arg);
                return false;
            }
        }
        else
        {
            if (g_sCommand == nullptr)
                g_sCommand = arg;
            else if (g_sImageFileName == nullptr)
                g_sImageFileName = arg;
            else if (g_sCommand[0] == 'h' && g_sPartition == nullptr)
                g_sPartition = arg;
            else if (g_sFileName == nullptr)
                g_sFileName = arg;
            else
            {
                printf("Unknown param: %s\n", arg);
                return false;
            }
        }
    }

    // Parsed options validation
    if (g_sCommand == nullptr)
    {
        printf("Command not specified.\n");
        return false;
    }
    CommandInfo* pcinfo = nullptr;
    for (int i = 0; i < g_CommandInfos_count; i++)
    {
        if (strcmp(g_sCommand, g_CommandInfos[i].command) == 0)
        {
            pcinfo = g_CommandInfos + i;
            break;
        }
    }
    if (pcinfo == nullptr)
    {
        printf("Unknown command: %s\n", g_sCommand);
        return false;
    }
    g_pCommand = pcinfo;

    g_okHardCommand = pcinfo->okHard;
    if (pcinfo->okHard && g_sPartition != nullptr)
    {
        g_nPartition = atoi(g_sPartition);
    }

    // More pre-checks based on command requirements
    if (g_sImageFileName == nullptr)
    {
        printf("Image file not specified.\n");
        return false;
    }
    if ((pcinfo->requirements & CMDR_PARAM_PARTITION) != 0 && g_nPartition < 0)
    {
        printf("Partition number expected.\n");
        return false;
    }
    if ((pcinfo->requirements & CMDR_PARAM_FILENAME) != 0 && g_sFileName == nullptr)
    {
        printf("File name expected.\n");
        return false;
    }
    if ((pcinfo->requirements & CMDR_IMAGEFILERW) != 0 && g_diskimage.IsReadOnly())
    {
        printf("Cannot perform the operation: disk image file is read-only.\n");
        return false;
    }

    return true;
}

int main(int argc, char* argv[])
{
    PrintWelcome();

    if (!ParseCommandLine(argc, argv))
    {
        PrintUsage();
        return 255;
    }

    // Подключение к файлу образа
    if (g_okHardCommand)
    {
        if (!g_hardimage.Attach(g_sImageFileName, g_okHard32M))
        {
            printf("Failed to open the image file.\n");
            return 255;
        }
    }
    else
    {
        if (!g_diskimage.Attach(g_sImageFileName, g_lStartOffset, g_okInterleaving))
        {
            printf("Failed to open the image file.\n");
            return 255;
        }
    }

    // Main task
    g_pCommand->commandImpl();

    // Завершение работы с файлом
    g_diskimage.Detach();
    g_hardimage.Detach();

    return 0;
}


//////////////////////////////////////////////////////////////////////


void DoDiskList()
{
    g_diskimage.DecodeImageCatalog();
    g_diskimage.PrintCatalogDirectory();
}

void DoDiskExtractFile()
{
    g_diskimage.DecodeImageCatalog();
    g_diskimage.SaveEntryToExternalFile(g_sFileName, g_okTrimZeroes);
}

void DoDiskExtractAllFiles()
{
    g_diskimage.DecodeImageCatalog();
    g_diskimage.SaveAllEntriesToExternalFiles();
}

void DoDiskAddFile()
{
    g_diskimage.DecodeImageCatalog();
    g_diskimage.AddFileToImage(g_sFileName);
}

void DoDiskDeleteFile()
{
    g_diskimage.DecodeImageCatalog();
    g_diskimage.DeleteFileFromImage(g_sFileName);
}

void DoDiskExtractAllUnusedFiles()
{
    g_diskimage.DecodeImageCatalog();
    g_diskimage.SaveAllUnusedEntriesToExternalFiles();
}

void DoHardInvert()
{
    g_hardimage.PrintImageInfo();
    printf("\n");
    g_hardimage.InvertImage();
}

void DoHardList()
{
    if (!g_hardimage.IsChecksum())
    {
        printf("Cannot perform the operation: home block checksum is incorrect.\n");
        return;
    }

    g_hardimage.PrintImageInfo();
    printf("\n");
    g_hardimage.PrintPartitionTable();
    printf("\n");
}

void DoHardExtractPartition()
{
    if (!g_hardimage.IsChecksum())
    {
        printf("Cannot perform the operation: home block checksum is incorrect.\n");
        return;
    }

    g_hardimage.SavePartitionToFile(g_nPartition, g_sFileName);
}

void DoHardUpdatePartition()
{
    if (!g_hardimage.IsChecksum())
    {
        printf("Cannot perform the operation: home block checksum is incorrect.\n");
        return;
    }

    g_hardimage.UpdatePartitionFromFile(g_nPartition, g_sFileName);
}

void DoHardPartitionList()
{
    if (!g_hardimage.IsChecksum())
    {
        printf("Cannot perform the operation: home block checksum is incorrect.\n");
        return;
    }

    if (!g_hardimage.PrepareDiskImage(g_nPartition, &g_diskimage))
    {
        printf("Failed to prepare partition disk image.\n");
        return;
    }

    g_diskimage.DecodeImageCatalog();
    g_diskimage.PrintCatalogDirectory();
}

void DoHardPartitionExtractFile()
{
    if (!g_hardimage.IsChecksum())
    {
        printf("Cannot perform the operation: home block checksum is incorrect.\n");
        return;
    }

    if (!g_hardimage.PrepareDiskImage(g_nPartition, &g_diskimage))
    {
        printf("Failed to prepare partition disk image.\n");
        return;
    }

    g_diskimage.DecodeImageCatalog();
    g_diskimage.SaveEntryToExternalFile(g_sFileName, g_okTrimZeroes);
}

void DoHardPartitionAddFile()
{
    if (!g_hardimage.IsChecksum())
    {
        printf("Cannot perform the operation: home block checksum is incorrect.\n");
        return;
    }

    if (!g_hardimage.PrepareDiskImage(g_nPartition, &g_diskimage))
    {
        printf("Failed to prepare partition disk image.\n");
        return;
    }

    g_diskimage.DecodeImageCatalog();
    g_diskimage.AddFileToImage(g_sFileName);
}


//////////////////////////////////////////////////////////////////////
