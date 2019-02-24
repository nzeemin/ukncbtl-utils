/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>
#include "rt11date.h"

uint16_t clock2rt11date(const time_t clock)
{
    union rt11date  r = {0};
    uint16_t    year;
    struct tm   *v = ::localtime(&clock);
    year = v->tm_year + 1900 - 1972;
    r.i.year = year % 32;
    r.i.mon = v->tm_mon + 1;
    r.i.day = v->tm_mday;
    r.i.age = year / 32;
    // printf("!date2rt11: a.m.d.y=%d.%d.%d.%d > 0x%04x\n", r.i.age, r.i.mon, r.i.day, r.i.year, r.pac);
    return r.pac;
}


/* */
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

void rt11date_str(uint16_t date, char* str, size_t sz)
{
    static const char* months[] =
    { "???", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    int year  = (date & 0x1F);
    int day   = (date >> 5)  & 0x1F;
    int month = (date >> 10) & 0xF;
    int age = (date >> 14) & 0x3;
    year += age * 32;

    if (month < 1 || month > 12)
        strncpy(str, "  -BAD-  ", sz);
    else
        // sprintf(str, "[%d] %02d-%3s-%02d", age, day, months[month], year+1972);
        ::snprintf(str, sz, "%02d-%3s-%02d", day, months[month], year >= 32 ? year + 1972 - 2000 : year + 72);
}
