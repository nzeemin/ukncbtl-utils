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
    int year = 0, mon = 1, day = 1;
    struct tm *v = ::localtime(&clock);
    if (v != nullptr)
    {
        year = v->tm_year + 1900 - 1972;
        mon = v->tm_mon + 1;
        day = v->tm_mday;
    }
    if (year < 0) year = 0;
    if (year > 127) year = 127;
    // Bit fields are not used here: MSVC does not pack them as expected
    return (uint16_t) ((year % 32) | (day << 5) | (mon << 10) | ((year / 32) << 14));
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
        strncpy(str, "   -BAD-   ", sz);
    else
        ::snprintf(str, sz, "%02d-%3s-%02d", day, months[month], year + 1972);
}
