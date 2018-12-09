/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

#pragma once

#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <memory.h>
#include <string.h>
#include <cstring>

#ifdef __GNUC__
#define _stricmp    strcasecmp
#define _strnicmp   strnicmp
#endif


//////////////////////////////////////////////////////////////////////
// RADIX50 convertion rotines headers, see rad50.cpp

void r50asc(int cnt, uint16_t* r50, char str[]);
void irad50(int cnt, char str[], uint16_t r50[]);
void rtDateStr(uint16_t date, char* str);


//////////////////////////////////////////////////////////////////////
