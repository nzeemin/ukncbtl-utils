// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define _CRT_SECURE_NO_WARNINGS

// Windows Header Files:
//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
//#include <windows.h>

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <ctype.h>

typedef const char * LPCSTR;

// Define C99 stdint.h types for Visual Studio
#ifdef _MSC_VER
   typedef unsigned __int8   uint8_t;
   typedef unsigned __int16  uint16_t;
   typedef unsigned __int32  uint32_t;
   typedef unsigned __int64  uint64_t;

#  define false   0
#  define true    1
#  define bool int
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#include "SavDisasm.h"
