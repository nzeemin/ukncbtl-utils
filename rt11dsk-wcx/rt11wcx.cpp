/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// rt11wcx.cpp : Total Commander packer plugin (WCX) for RT-11 floppy disk images

#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <string>
#include <vector>
#include "wcxhead.h"
#include "diskimage.h"

#define WCX_EXPORT  extern "C"

#define PROGRESS_BLOCKS     32


//////////////////////////////////////////////////////////////////////

struct CArchiveItem
{
    wchar_t     name[16];
    uint16_t    r50[3];
    uint16_t    start;
    uint16_t    length;
    int         filetime;   // DOS date/time
};

struct CArchive
{
    CDiskImage  image;
    std::wstring arcname;
    std::vector<CArchiveItem> items;
    size_t      next;       // Next item for ReadHeader
    size_t      cur;        // Item returned by the last ReadHeader
    CArchive() : next(0), cur(0) { }
};

static tProcessDataProc  g_pProcessData = nullptr;
static tProcessDataProcW g_pProcessDataW = nullptr;


//////////////////////////////////////////////////////////////////////

static std::wstring AnsiToWide(const char* str)
{
    if (str == nullptr || *str == 0)
        return std::wstring();
    int len = ::MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0);
    std::vector<wchar_t> buf(len + 1);
    ::MultiByteToWideChar(CP_ACP, 0, str, -1, &buf[0], len);
    return std::wstring(&buf[0]);
}

static void WideToAnsi(const wchar_t* str, char* buf, int bufsize)
{
    buf[0] = 0;
    ::WideCharToMultiByte(CP_ACP, 0, str, -1, buf, bufsize, nullptr, nullptr);
    buf[bufsize - 1] = 0;
}

// RT-11 packed date to DOS date/time; years before 1980 are not representable and replaced with 01-01-1980
static int DatepacToDosTime(uint16_t datepac)
{
    int year = (datepac & 0x1F) + ((datepac >> 14) & 3) * 32 + 1972;
    int day = (datepac >> 5) & 0x1F;
    int month = (datepac >> 10) & 0xF;
    if (month < 1 || month > 12 || day < 1 || year < 1980)
        return (0 << 25) | (1 << 21) | (1 << 16);
    return ((year - 1980) << 25) | (month << 21) | (day << 16);
}

static time_t FileTimeToTimeT(const FILETIME& ft)
{
    ULARGE_INTEGER u;
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    const ULONGLONG epoch = 116444736000000000ULL;  // 1970-01-01 in FILETIME units
    if (u.QuadPart < epoch)
        return 0;
    return (time_t)((u.QuadPart - epoch) / 10000000ULL);
}

// Callback for the progress dialog; returns false if the user has aborted the operation
static bool ReportProgress(const wchar_t* name, int bytes)
{
    if (g_pProcessDataW != nullptr)
        return g_pProcessDataW((WCHAR*)name, bytes) != 0;
    if (g_pProcessData != nullptr)
    {
        char buf[MAX_PATH];
        WideToAnsi(name, buf, MAX_PATH);
        return g_pProcessData(buf, bytes) != 0;
    }
    return true;
}

static int DiskResultToError(EDiskResult result)
{
    switch (result)
    {
    case DISK_OK:           return 0;
    case DISK_ERR_OPEN:     return E_EOPEN;
    case DISK_ERR_READ:     return E_EREAD;
    case DISK_ERR_WRITE:    return E_EWRITE;
    case DISK_ERR_FORMAT:   return E_UNKNOWN_FORMAT;
    case DISK_ERR_NOT_FOUND:return E_NO_FILES;
    case DISK_ERR_NO_SPACE: return E_EWRITE;
    case DISK_ERR_CATALOG_FULL: return E_EWRITE;
    case DISK_ERR_READONLY: return E_ECREATE;
    case DISK_ERR_BAD_NAME: return E_NOT_SUPPORTED;
    case DISK_ERR_TOO_BIG:  return E_NOT_SUPPORTED;
    }
    return E_BAD_ARCHIVE;
}

// Case-insensitive wildcard match with '*' and '?'
static bool WildcardMatch(const wchar_t* pattern, const wchar_t* text)
{
    for (; *pattern != 0; pattern++)
    {
        if (*pattern == L'*')
        {
            while (pattern[1] == L'*') pattern++;
            for (const wchar_t* p = text; ; p++)
            {
                if (WildcardMatch(pattern + 1, p)) return true;
                if (*p == 0) return false;
            }
        }
        if (*text == 0)
            return false;
        if (*pattern != L'?' && towupper(*pattern) != towupper(*text))
            return false;
        text++;
    }
    return *text == 0;
}

static void BuildItemList(CArchive* pArchive)
{
    const CVolumeInformation& info = pArchive->image.GetVolumeInfo();
    for (size_t s = 0; s < info.segments.size(); s++)
    {
        const std::vector<CVolumeCatalogEntry>& entries = info.segments[s].entries;
        for (size_t i = 0; i < entries.size(); i++)
        {
            const CVolumeCatalogEntry& entry = entries[i];
            if (!entry.IsFile()) continue;

            CArchiveItem item;
            char name[12];
            entry.GetName(name);
            for (int k = 0; k < 12; k++)
                item.name[k] = (wchar_t)(unsigned char)name[k];
            item.name[12] = 0;
            memcpy(item.r50, entry.namerad50, sizeof(item.r50));
            item.start = entry.start;
            item.length = entry.length;
            item.filetime = DatepacToDosTime(entry.datepac);
            pArchive->items.push_back(item);
        }
    }
}

static HANDLE OpenArchiveCommon(const wchar_t* fileName, int* pResult)
{
    CArchive* pArchive = new CArchive();
    if (!pArchive->image.Attach(fileName))
    {
        *pResult = DiskResultToError(pArchive->image.GetLastResult());
        delete pArchive;
        return nullptr;
    }
    pArchive->arcname = fileName;
    BuildItemList(pArchive);
    *pResult = 0;
    return (HANDLE)pArchive;
}

static int ExtractItem(CArchive* pArchive, const CArchiveItem& item, const wchar_t* destPath)
{
    HANDLE hFile = ::CreateFileW(destPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return E_ECREATE;

    int result = 0;
    int pending = 0;    // Bytes written since the last progress report
    for (int b = 0; b < item.length; b++)
    {
        const uint8_t* pBlock = pArchive->image.GetBlock(item.start + b);
        if (pBlock == nullptr)
        {
            result = E_BAD_DATA;
            break;
        }
        DWORD written = 0;
        if (!::WriteFile(hFile, pBlock, RT11_BLOCK_SIZE, &written, nullptr) || written != RT11_BLOCK_SIZE)
        {
            result = E_EWRITE;
            break;
        }
        pending += RT11_BLOCK_SIZE;
        if ((b + 1) % PROGRESS_BLOCKS == 0 || b + 1 == item.length)
        {
            bool okContinue = ReportProgress(destPath, pending);
            pending = 0;
            if (!okContinue)
            {
                result = E_EABORTED;
                break;
            }
        }
    }
    ::CloseHandle(hFile);
    if (result != 0)
        ::DeleteFileW(destPath);
    return result;
}

static bool ReadHostFile(const wchar_t* path, std::vector<uint8_t>& data, time_t* pMtime, int* pError)
{
    HANDLE hFile = ::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        *pError = E_EOPEN;
        return false;
    }
    LARGE_INTEGER size;
    FILETIME ft;
    if (!::GetFileSizeEx(hFile, &size) || !::GetFileTime(hFile, nullptr, nullptr, &ft))
    {
        ::CloseHandle(hFile);
        *pError = E_EREAD;
        return false;
    }
    if (size.QuadPart > (LONGLONG)RT11_BLOCK_SIZE * RT11_MAX_FILE_BLOCKS)
    {
        ::CloseHandle(hFile);
        *pError = E_NOT_SUPPORTED;
        return false;
    }
    data.resize((size_t)size.QuadPart);
    DWORD read = 0;
    BOOL okRead = TRUE;
    if (size.QuadPart > 0)
        okRead = ::ReadFile(hFile, &data[0], (DWORD)size.QuadPart, &read, nullptr);
    ::CloseHandle(hFile);
    if (!okRead || read != (DWORD)size.QuadPart)
    {
        *pError = E_EREAD;
        return false;
    }
    *pMtime = FileTimeToTimeT(ft);
    return true;
}

// Split a double-null terminated list
static void SplitList(const wchar_t* list, std::vector<std::wstring>& result)
{
    while (*list != 0)
    {
        size_t len = wcslen(list);
        result.push_back(std::wstring(list, len));
        list += len + 1;
    }
}


//////////////////////////////////////////////////////////////////////
// Plugin interface: reading

WCX_EXPORT HANDLE __stdcall OpenArchiveW(tOpenArchiveDataW* ArchiveData)
{
    return OpenArchiveCommon(ArchiveData->ArcName, &ArchiveData->OpenResult);
}

WCX_EXPORT HANDLE __stdcall OpenArchive(tOpenArchiveData* ArchiveData)
{
    return OpenArchiveCommon(AnsiToWide(ArchiveData->ArcName).c_str(), &ArchiveData->OpenResult);
}

WCX_EXPORT int __stdcall ReadHeaderExW(HANDLE hArcData, tHeaderDataExW* HeaderData)
{
    CArchive* pArchive = (CArchive*)hArcData;
    if (pArchive->next >= pArchive->items.size())
        return E_END_ARCHIVE;
    pArchive->cur = pArchive->next++;
    const CArchiveItem& item = pArchive->items[pArchive->cur];

    memset(HeaderData, 0, sizeof(*HeaderData));
    wcsncpy(HeaderData->ArcName, pArchive->arcname.c_str(), 1023);
    wcsncpy(HeaderData->FileName, item.name, 1023);
    HeaderData->PackSize = HeaderData->UnpSize = (unsigned int)item.length * RT11_BLOCK_SIZE;
    HeaderData->FileTime = item.filetime;
    HeaderData->FileAttr = 0;
    return 0;
}

WCX_EXPORT int __stdcall ReadHeader(HANDLE hArcData, tHeaderData* HeaderData)
{
    CArchive* pArchive = (CArchive*)hArcData;
    if (pArchive->next >= pArchive->items.size())
        return E_END_ARCHIVE;
    pArchive->cur = pArchive->next++;
    const CArchiveItem& item = pArchive->items[pArchive->cur];

    memset(HeaderData, 0, sizeof(*HeaderData));
    WideToAnsi(pArchive->arcname.c_str(), HeaderData->ArcName, sizeof(HeaderData->ArcName));
    WideToAnsi(item.name, HeaderData->FileName, sizeof(HeaderData->FileName));
    HeaderData->PackSize = HeaderData->UnpSize = (int)item.length * RT11_BLOCK_SIZE;
    HeaderData->FileTime = item.filetime;
    HeaderData->FileAttr = 0;
    return 0;
}

WCX_EXPORT int __stdcall ProcessFileW(HANDLE hArcData, int Operation, WCHAR* DestPath, WCHAR* DestName)
{
    CArchive* pArchive = (CArchive*)hArcData;
    if (Operation == PK_SKIP)
        return 0;
    if (pArchive->cur >= pArchive->items.size())
        return E_END_ARCHIVE;
    const CArchiveItem& item = pArchive->items[pArchive->cur];

    if (Operation == PK_TEST)
    {
        for (int b = 0; b < item.length; b++)
            if (pArchive->image.GetBlock(item.start + b) == nullptr)
                return E_BAD_DATA;
        return 0;
    }

    std::wstring path;
    if (DestPath != nullptr) path = DestPath;
    if (DestName != nullptr) path += DestName;
    return ExtractItem(pArchive, item, path.c_str());
}

WCX_EXPORT int __stdcall ProcessFile(HANDLE hArcData, int Operation, char* DestPath, char* DestName)
{
    std::wstring path = AnsiToWide(DestPath);
    std::wstring name = AnsiToWide(DestName);
    return ProcessFileW(hArcData, Operation, DestPath != nullptr ? (WCHAR*)path.c_str() : nullptr,
                        DestName != nullptr ? (WCHAR*)name.c_str() : nullptr);
}

WCX_EXPORT int __stdcall CloseArchive(HANDLE hArcData)
{
    delete (CArchive*)hArcData;
    return 0;
}

WCX_EXPORT void __stdcall SetChangeVolProc(HANDLE, tChangeVolProc)
{
}

WCX_EXPORT void __stdcall SetChangeVolProcW(HANDLE, tChangeVolProcW)
{
}

WCX_EXPORT void __stdcall SetProcessDataProc(HANDLE, tProcessDataProc pProcessDataProc)
{
    g_pProcessData = pProcessDataProc;
}

WCX_EXPORT void __stdcall SetProcessDataProcW(HANDLE, tProcessDataProcW pProcessDataProc)
{
    g_pProcessDataW = pProcessDataProc;
}


//////////////////////////////////////////////////////////////////////
// Plugin interface: detection and capabilities

WCX_EXPORT int __stdcall GetPackerCaps()
{
    return PK_CAPS_NEW | PK_CAPS_MULTIPLE | PK_CAPS_MODIFY | PK_CAPS_DELETE | PK_CAPS_BY_CONTENT;
}

WCX_EXPORT BOOL __stdcall CanYouHandleThisFileW(WCHAR* FileName)
{
    CDiskImage image;
    return image.Attach(FileName) ? TRUE : FALSE;
}

WCX_EXPORT BOOL __stdcall CanYouHandleThisFile(char* FileName)
{
    return CanYouHandleThisFileW((WCHAR*)AnsiToWide(FileName).c_str());
}


//////////////////////////////////////////////////////////////////////
// Plugin interface: modification

WCX_EXPORT int __stdcall PackFilesW(WCHAR* PackedFile, WCHAR* SubPath, WCHAR* SrcPath, WCHAR* AddList, int Flags)
{
    if (SubPath != nullptr && *SubPath != 0)  // RT-11 volume has no subdirectories
        return E_NOT_SUPPORTED;

    CDiskImage image;
    if (!image.Attach(PackedFile))
        return DiskResultToError(image.GetLastResult());
    if (image.IsReadOnly())
        return E_ECREATE;

    std::wstring srcpath = SrcPath != nullptr ? SrcPath : L"";
    if (!srcpath.empty() && srcpath[srcpath.size() - 1] != L'\\')
        srcpath += L'\\';

    std::vector<std::wstring> names;
    SplitList(AddList, names);

    int result = 0;
    std::vector<std::wstring> moved;
    for (size_t i = 0; i < names.size() && result == 0; i++)
    {
        const std::wstring& name = names[i];
        if (name.empty() || name[name.size() - 1] == L'\\')  // Skip directories
            continue;

        uint16_t r50[3];
        if (!Rt11ParseFileName(name.c_str(), r50))  // Not a 6.3 name
        {
            result = E_NOT_SUPPORTED;
            break;
        }

        std::wstring path = srcpath + name;
        std::vector<uint8_t> data;
        time_t mtime = 0;
        if (!ReadHostFile(path.c_str(), data, &mtime, &result))
            break;

        EDiskResult diskresult = image.AddFile(r50, data.empty() ? nullptr : &data[0], data.size(), mtime);
        if (diskresult != DISK_OK)
        {
            result = DiskResultToError(diskresult);
            break;
        }
        moved.push_back(path);

        if (!ReportProgress(path.c_str(), (int)data.size()))
            result = E_EABORTED;
    }

    // Save whatever was added, even on error or abort
    EDiskResult flushresult = image.Flush();
    if (flushresult != DISK_OK)
        return DiskResultToError(flushresult);

    if (result == 0 && (Flags & PK_PACK_MOVE_FILES))
        for (size_t i = 0; i < moved.size(); i++)
            ::DeleteFileW(moved[i].c_str());
    return result;
}

WCX_EXPORT int __stdcall PackFiles(char* PackedFile, char* SubPath, char* SrcPath, char* AddList, int Flags)
{
    std::wstring packed = AnsiToWide(PackedFile);
    std::wstring subpath = AnsiToWide(SubPath);
    std::wstring srcpath = AnsiToWide(SrcPath);
    std::vector<wchar_t> list;  // Convert the double-null terminated list
    for (const char* p = AddList; *p != 0; p += strlen(p) + 1)
    {
        std::wstring item = AnsiToWide(p);
        list.insert(list.end(), item.begin(), item.end());
        list.push_back(0);
    }
    list.push_back(0);
    return PackFilesW((WCHAR*)packed.c_str(), (WCHAR*)subpath.c_str(), (WCHAR*)srcpath.c_str(), &list[0], Flags);
}

WCX_EXPORT int __stdcall DeleteFilesW(WCHAR* PackedFile, WCHAR* DeleteList)
{
    CDiskImage image;
    if (!image.Attach(PackedFile))
        return DiskResultToError(image.GetLastResult());
    if (image.IsReadOnly())
        return E_ECREATE;

    std::vector<std::wstring> names;
    SplitList(DeleteList, names);

    // Collect the files to delete first, as the delete changes the catalog
    std::vector<uint16_t> targets;  // 3 words per file
    {
        const CVolumeInformation& info = image.GetVolumeInfo();
        for (size_t s = 0; s < info.segments.size(); s++)
        {
            const std::vector<CVolumeCatalogEntry>& entries = info.segments[s].entries;
            for (size_t e = 0; e < entries.size(); e++)
            {
                if (!entries[e].IsFile()) continue;
                char name[12];
                entries[e].GetName(name);
                std::wstring wname = AnsiToWide(name);
                for (size_t i = 0; i < names.size(); i++)
                {
                    const std::wstring& pattern = names[i];
                    bool match = (pattern == L"*.*") || WildcardMatch(pattern.c_str(), wname.c_str());
                    if (!match) continue;
                    targets.insert(targets.end(), entries[e].namerad50, entries[e].namerad50 + 3);
                    break;
                }
            }
        }
    }

    int result = 0;
    for (size_t i = 0; i + 3 <= targets.size() && result == 0; i += 3)
    {
        EDiskResult diskresult = image.RemoveFile(&targets[i]);
        if (diskresult != DISK_OK)
            result = DiskResultToError(diskresult);
    }

    EDiskResult flushresult = image.Flush();
    if (flushresult != DISK_OK)
        return DiskResultToError(flushresult);
    return result;
}

WCX_EXPORT int __stdcall DeleteFiles(char* PackedFile, char* DeleteList)
{
    std::wstring packed = AnsiToWide(PackedFile);
    std::vector<wchar_t> list;
    for (const char* p = DeleteList; *p != 0; p += strlen(p) + 1)
    {
        std::wstring item = AnsiToWide(p);
        list.insert(list.end(), item.begin(), item.end());
        list.push_back(0);
    }
    list.push_back(0);
    return DeleteFilesW((WCHAR*)packed.c_str(), &list[0]);
}


//////////////////////////////////////////////////////////////////////
