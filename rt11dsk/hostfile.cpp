/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>
#include "rt11dsk.h"
#include "diskimage.h"
#include "hostfile.h"
#include "errno.h"

CHostFile::CHostFile(const char* _host_fn)
{
    data = nullptr;
    memset(rt11_fn, 0, sizeof(rt11_fn));
    mtime_sec = 0;
    host_fn = _host_fn;
    memset(_name, 0, sizeof(_name));
}

CHostFile::~CHostFile()
{
    if (data)
        free(data);
}

// Parse sFileName as RT11 file name, 6.3 format
// Resulting filename and fileext are uppercased strings.
bool CHostFile::ParseFileName63(void)
{
    const char * p_ext = strrchr(host_fn, '.');
    const char * p_name = strrchr(host_fn, separator());
    memset(_name, ' ', sizeof(_name) - 1);
    _name[sizeof(_name) - 1] = '\0';
    if (p_ext == nullptr)
    {
        fprintf(stderr, "Wrong filename format (no ext): %s\n", host_fn);
        return false;
    }
    if (p_name == nullptr)
    {
        p_name = host_fn;
    }
    else
    {
        ++p_name;
    }

    int fn_size = p_ext - p_name;
    if (fn_size == 0 || fn_size > 6)
    {
        fprintf(stderr, "Wrong filename format (long name: %d): %s\n", fn_size, host_fn);
        return false;
    }
    size_t ext_size = strlen(p_ext + 1);
    if (ext_size == 0 || ext_size > 3)
    {
        fprintf(stderr, "Wrong filename format (long ext): %s\n", host_fn);
        return false;
    }
    for (int i = 0; i < fn_size; i++)
        _name[i] = (char)toupper(p_name[i]);
    for (int i = 0; i < ext_size; i++)
        _name[6 + i] = (char)toupper(p_ext[i + 1]);
    irad50(9, _name, rt11_fn);
    return true;
}

bool CHostFile::read(void)
{
    struct stat st;
    if (stat(host_fn, &st) != 0)
    {
        fprintf(stderr, "Failed to stat the file: %s\n", host_fn);
        return false;
    }
    // FIXME: сохранить дату файла для послед. записи в ods-1 entry
    mtime_sec = st.st_mtime; // LINUX specific, may not work on MacOS
    // Проверка, не слишком ли длинный файл для этого тома
    if (st.st_size > RT11_MAX_FILE_SIZE)
    {
        fprintf(stderr, "Failed is too big (max %d bytes): %s\n", RT11_MAX_FILE_SIZE, host_fn);
        return false;
    }
    // Проверка на файл нулевой длины
    if (st.st_size == 0)
    {
        fprintf(stderr, "Failed is empty: %s\n", host_fn);
        return false;
    }
    // Открываем помещаемый файл на чтение
    FILE* fpFile = ::fopen(host_fn, "rb");
    if (fpFile == nullptr)
    {
        fprintf(stderr, "Failed to open the file: %s\n", host_fn);
        return false;
    }

    // Определяем длину файла, с учетом округления до полного блока
    rt11_sz =  // Требуемая ширина свободного места в блоках
        (uint16_t) ((st.st_size + RT11_BLOCK_SIZE - 1) / RT11_BLOCK_SIZE);
    uint32_t dwFileSize =  // Длина файла с учетом округления до полного блока
        ((uint32_t) rt11_sz) * RT11_BLOCK_SIZE;

    // Выделяем память и считываем данные файла
    data = ::calloc(dwFileSize, 1);
    size_t lBytesRead = ::fread(data, 1, st.st_size, fpFile);
    if (lBytesRead != st.st_size)
    {
        fprintf(stderr, "Failed to read the file: %s\n", host_fn);
        //exit(-1);
        return false;
    }
    ::fclose(fpFile);

    printf("File size is %d bytes or %d blocks\n", (int)st.st_size, rt11_sz);
    return true;
}
