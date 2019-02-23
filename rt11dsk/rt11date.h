/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#ifndef _RT11DATE_H
#define _RT11DATE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>
#include <string.h>

#pragma pack(push,1)
    union rt11date
    {
        uint16_t    pac;
        struct  __i
        {
            uint8_t year: 5;
            uint8_t day: 5;
            uint8_t mon: 4;
            uint8_t age: 2;
        } i;
    };
#pragma pack(pop)

    uint16_t clock2rt11date(const time_t clock);
    void rt11date_str(uint16_t date, char* buf, size_t sz);

#ifdef __cplusplus
}
#endif
#endif
