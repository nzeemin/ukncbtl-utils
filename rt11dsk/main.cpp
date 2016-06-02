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

#include "stdafx.h"
#include "diskimage.h"
#include "hardimage.h"


//////////////////////////////////////////////////////////////////////
// Preliminary function declarations

void PrintWelcome();
void PrintUsage();
bool ParseCommandLine(int argc, _TCHAR* argv[]);

void DoDiskList();
void DoDiskExtractFile();
void DoDiskExtractAllFiles();
void DoDiskAddFile();
void DoHardInvert();
void DoHardList();
void DoHardExtractPartition();
void DoHardUpdatePartition();
void DoHardPartitionList();
void DoHardPartitionExtractFile();
void DoHardPartitionAddFile();


//////////////////////////////////////////////////////////////////////
// Globals

LPCTSTR g_sCommand = NULL;
LPCTSTR g_sImageFileName = NULL;
LPCTSTR g_sFileName = NULL;
bool    g_okHardCommand = false;
LPCTSTR g_sPartition = NULL;
int     g_nPartition = -1;

struct CommandInfo
{
    LPCTSTR command;
    bool    okHard;  // true for hard disk image command, false for disk image command
    void    (*commandImpl)();
}
static g_CommandInfos[] =
{
    { _T("l"),    false,  DoDiskList                    },
    { _T("e"),    false,  DoDiskExtractFile             },
    { _T("x"),    false,  DoDiskExtractAllFiles         },
    { _T("a"),    false,  DoDiskAddFile                 },
    { _T("hi"),   true,   DoHardInvert                  },
    { _T("hl"),   true,   DoHardList                    },
    { _T("hx"),   true,   DoHardExtractPartition        },
    { _T("hu"),   true,   DoHardUpdatePartition         },
    { _T("hpl"),  true,   DoHardPartitionList           },
    { _T("hpe"),  true,   DoHardPartitionExtractFile    },
    { _T("hpa"),  true,   DoHardPartitionAddFile        },
};

CDiskImage      g_diskimage;
CHardImage      g_hardimage;
CommandInfo*    g_pCommand = NULL;


//////////////////////////////////////////////////////////////////////


void PrintWelcome()
{
    wprintf(_T("RT11DSK Utility  by Nikita Zimin  [%S %S]\n\n"), __DATE__, __TIME__);
}

void PrintUsage()
{
    wprintf(_T("\nUsage:\n"));
    wprintf(_T("  Disk image commands:\n"));
    wprintf(_T("    rt11dsk l <ImageFile>  - list image contents\n"));
    wprintf(_T("    rt11dsk e <ImageFile> <FileName>  - extract file\n"));
    wprintf(_T("    rt11dsk a <ImageFile> <FileName>  - add file\n"));
    wprintf(_T("  Hard disk image commands:\n"));
    wprintf(_T("    rt11dsk hi <HddImage>  - invert HDD image file\n"));
    wprintf(_T("    rt11dsk hl <HddImage>  - list HDD image partitions\n"));
    wprintf(_T("    rt11dsk hx <HddImage> <Partn> <FileName>  - extract partition to file\n"));
    wprintf(_T("    rt11dsk hu <HddImage> <Partn> <FileName>  - update partition from the file\n"));
    wprintf(_T("    rt11dsk hpl <HddImage> <Partn>  - list partition contents\n"));
    wprintf(_T("    rt11dsk hpe <HddImage> <Partn> <FileName>  - extract file from the partition\n"));
    wprintf(_T("    rt11dsk hpa <HddImage> <Partn> <FileName>  - add file to the partition\n"));
    wprintf(_T("  Parameters:\n"));
    wprintf(_T("    <ImageFile> is UKNC disk image in .dsk or .rtd format\n"));
    wprintf(_T("    <HddImage>  is UKNC hard disk image file name\n"));
    wprintf(_T("    <Partn>     is hard disk image partition number, 0..23\n"));
    wprintf(_T("    <FileName>  is a file name to read from or save to\n"));
}

bool ParseCommandLine(int argc, _TCHAR* argv[])
{
    for (int argn = 1; argn < argc; argn++)
    {
        LPCTSTR arg = argv[argn];
        if (arg[0] == _T('-') || arg[0] == _T('/'))
        {
            //TODO
            wprintf(_T("Unknown option: %s\n"), arg);
            return false;
        }
        else
        {
            if (g_sCommand == NULL)
                g_sCommand = arg;
            else if (g_sImageFileName == NULL)
                g_sImageFileName = arg;
            else if (g_sCommand[0] == _T('h') && g_sPartition == NULL)
                g_sPartition = arg;
            else if (g_sFileName == NULL)
                g_sFileName = arg;
            else
            {
                wprintf(_T("Unknown param: %s\n"), arg);
                return false;
            }
        }
    }

    // Parsed options validation
    if (g_sCommand == NULL)
    {
        wprintf(_T("Command not specified.\n"));
        return false;
    }
    CommandInfo* pcinfo = NULL;
    for (int i = 0; i < sizeof(g_CommandInfos)/sizeof(CommandInfo); i++)
    {
        if (wcscmp(g_sCommand, g_CommandInfos[i].command) == 0)
        {
            pcinfo = g_CommandInfos + i;
            break;
        }
    }
    if (pcinfo == NULL)
    {
        wprintf(_T("Unknown command: %s.\n"), g_sCommand);
        return false;
    }
    g_pCommand = pcinfo;

    if (pcinfo->okHard)
    {
        g_okHardCommand = true;
        if (g_sPartition != NULL)
        {
            g_nPartition = _wtoi(g_sPartition);
        }
    }

    if (g_sImageFileName == NULL)
    {
        wprintf(_T("Image file not specified.\n"));
        return false;
    }

    return true;
}

int _tmain(int argc, _TCHAR* argv[])
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
        if (!g_hardimage.Attach(g_sImageFileName))
        {
            wprintf(_T("Failed to open the image file.\n"));
            return 255;
        }
    }
    else
    {
        if (!g_diskimage.Attach(g_sImageFileName))
        {
            wprintf(_T("Failed to open the image file.\n"));
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
    if (g_sFileName == NULL)
    {
        wprintf(_T("Output file name expected.\n"));
        return;
    }

    g_diskimage.DecodeImageCatalog();
    g_diskimage.SaveEntryToExternalFile(g_sFileName);
}

void DoDiskExtractAllFiles()
{
    g_diskimage.DecodeImageCatalog();
    g_diskimage.SaveAllEntriesToExternalFiles();
}

void DoDiskAddFile()
{
    if (g_sFileName == NULL)
    {
        wprintf(_T("Input file name expected.\n"));
        return;
    }

    if (g_diskimage.IsReadOnly())
    {
        wprintf(_T("Cannot perform the operation: disk image file is read-only.\n"));
        return;
    }

    g_diskimage.DecodeImageCatalog();
    g_diskimage.AddFileToImage(g_sFileName);
}

void DoHardInvert()
{
    if (g_hardimage.IsReadOnly())
    {
        wprintf(_T("Cannot perform the operation: hard disk image file is read-only.\n"));
        return;
    }

    g_hardimage.PrintImageInfo();
    wprintf(_T("\n"));
    g_hardimage.InvertImage();
}

void DoHardList()
{
    if (!g_hardimage.IsChecksum())
    {
        wprintf(_T("Cannot perform the operation: home block checksum is incorrect.\n"));
        return;
    }

    g_hardimage.PrintImageInfo();
    wprintf(_T("\n"));
    g_hardimage.PrintPartitionTable();
    wprintf(_T("\n"));
}

void DoHardExtractPartition()
{
    if (g_nPartition < 0)
    {
        wprintf(_T("Partition number expected.\n"));
        return;
    }
    if (g_sFileName == NULL)
    {
        wprintf(_T("Output file name expected.\n"));
        return;
    }

    if (!g_hardimage.IsChecksum())
    {
        wprintf(_T("Cannot perform the operation: home block checksum is incorrect.\n"));
        return;
    }

    g_hardimage.SavePartitionToFile(g_nPartition, g_sFileName);
}

void DoHardUpdatePartition()
{
    if (g_nPartition < 0)
    {
        wprintf(_T("Partition number expected.\n"));
        return;
    }
    if (g_sFileName == NULL)
    {
        wprintf(_T("Input file name expected.\n"));
        return;
    }

    if (g_hardimage.IsReadOnly())
    {
        wprintf(_T("Cannot perform the operation: hard disk image file is read-only.\n"));
        return;
    }
    if (!g_hardimage.IsChecksum())
    {
        wprintf(_T("Cannot perform the operation: home block checksum is incorrect.\n"));
        return;
    }

    g_hardimage.UpdatePartitionFromFile(g_nPartition, g_sFileName);
}

void DoHardPartitionList()
{
    if (g_nPartition < 0)
    {
        wprintf(_T("Partition number expected.\n"));
        return;
    }

    if (!g_hardimage.IsChecksum())
    {
        wprintf(_T("Cannot perform the operation: home block checksum is incorrect.\n"));
        return;
    }

    if (!g_hardimage.PrepareDiskImage(g_nPartition, &g_diskimage))
    {
        wprintf(_T("Failed to prepare partition disk image.\n"));
        return;
    }

    g_diskimage.DecodeImageCatalog();
    g_diskimage.PrintCatalogDirectory();
}

void DoHardPartitionExtractFile()
{
    if (g_nPartition < 0)
    {
        wprintf(_T("Partition number expected.\n"));
        return;
    }
    if (g_sFileName == NULL)
    {
        wprintf(_T("Output file name expected.\n"));
        return;
    }

    if (!g_hardimage.IsChecksum())
    {
        wprintf(_T("Cannot perform the operation: home block checksum is incorrect.\n"));
        return;
    }

    if (!g_hardimage.PrepareDiskImage(g_nPartition, &g_diskimage))
    {
        wprintf(_T("Failed to prepare partition disk image.\n"));
        return;
    }

    g_diskimage.DecodeImageCatalog();
    g_diskimage.SaveEntryToExternalFile(g_sFileName);
}

void DoHardPartitionAddFile()
{
    if (g_nPartition < 0)
    {
        wprintf(_T("Partition number expected.\n"));
        return;
    }
    if (g_sFileName == NULL)
    {
        wprintf(_T("Input file name expected.\n"));
        return;
    }

    if (!g_hardimage.IsChecksum())
    {
        wprintf(_T("Cannot perform the operation: home block checksum is incorrect.\n"));
        return;
    }

    if (g_diskimage.IsReadOnly())
    {
        wprintf(_T("Cannot perform the operation: disk image file is read-only.\n"));
        return;
    }

    if (!g_hardimage.PrepareDiskImage(g_nPartition, &g_diskimage))
    {
        wprintf(_T("Failed to prepare partition disk image.\n"));
        return;
    }

    g_diskimage.DecodeImageCatalog();
    g_diskimage.AddFileToImage(g_sFileName);
}


//////////////////////////////////////////////////////////////////////
