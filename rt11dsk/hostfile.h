/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#ifndef _HOSTFILE_H
#define _HOSTFILE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <time.h>

    inline char separator()
    {
#if defined _WIN32 || defined __CYGWIN__
        return '\\';
#else
        return '/';
#endif
    }

#define RT11_BLOCK_SIZE     512
#define RT11_MAX_FILE_SIZE  (RT11_BLOCK_SIZE * 0xFFFF)

    struct CHostFile
    {
    public:
        const char* host_fn;
        void*       data;
        clock_t     mtime_sec;
        uint16_t    rt11_fn[3]; // radix-50
        uint16_t    rt11_sz; // file size in blocks
        char        _name[10]; // 6(name) + 3(ext) + 1\0

        CHostFile(const char* _host_fn);
        ~CHostFile();
        bool ParseFileName63(void);
        bool read(void);

        inline char* name(void) { return _name; };
        inline char* ext(void) { return _name + 6; };
    };

#ifdef __cplusplus
}
#endif
#endif
