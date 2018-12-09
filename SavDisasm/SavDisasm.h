/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// SavDisasm.h

#pragma once

#ifdef _MSC_VER
#  define _CRT_SECURE_NO_WARNINGS
#endif

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

#ifdef __GNUC__
#  define _snprintf  snprintf
#endif


//////////////////////////////////////////////////////////////////////
// Disassembler

/// \brief Disassemble one instruction of KM1801VM2 processor
/// \param[in]  pMemory Memory image (we read only words of the instruction)
/// \param[in]  addr    Address of the instruction
/// \param[out] sInstr  Instruction mnemonics buffer - at least 8 characters
/// \param[out] sArg    Instruction arguments buffer - at least 32 characters
/// \return  Number of words in the instruction
uint16_t DisassembleInstruction(uint16_t* pMemory, uint16_t addr, char* sInstr, char* sArg);


//////////////////////////////////////////////////////////////////////
