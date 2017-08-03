/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

/// \file Disasm.cpp
/// \brief Disassembler for KM1801VM2 processor
/// \details See defines in header file Emubase.h

#include "stdafx.h"
#include "Defines.h"


// Формат отображения режимов адресации
const LPCSTR ADDRESS_MODE_FORMAT[] =
{
    ("%s"), ("(%s)"), ("(%s)+"), ("@(%s)+"), ("-(%s)"), ("@-(%s)"), ("%06o(%s)"), ("@%06o(%s)")
};
// Формат отображения режимов адресации для регистра PC
const LPCSTR ADDRESS_MODE_PC_FORMAT[] =
{
    ("PC"), ("(PC)"), ("#%06o"), ("@#%06o"), ("-(PC)"), ("@-(PC)"), ("%06o"), ("@%06o")
};

//   strSrc - at least 24 characters
uint16_t ConvertSrcToString(uint16_t instr, uint16_t addr, char* strSrc, uint16_t code)
{
    uint8_t reg = GetDigit(instr, 2);
    uint8_t param = GetDigit(instr, 3);

    LPCSTR pszReg = REGISTER_NAME[reg];

    if (reg != 7)
    {
        LPCSTR format = ADDRESS_MODE_FORMAT[param];

        if (param == 6 || param == 7)
        {
            uint16_t word = code;  //TODO: pMemory
            _snprintf(strSrc, 24, format, word, pszReg);
            return 1;
        }
        else
            _snprintf(strSrc, 24, format, pszReg);
    }
    else
    {
        LPCSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3)
        {
            uint16_t word = code;  //TODO: pMemory
            _snprintf(strSrc, 24, format, word);
            return 1;
        }
        else if (param == 6 || param == 7)
        {
            uint16_t word = code;  //TODO: pMemory
            _snprintf(strSrc, 24, format, (uint16_t)(addr + word + 2));
            return 1;
        }
        else
            _snprintf(strSrc, 24, format, pszReg);
    }

    return 0;
}

//   strDst - at least 24 characters
uint16_t ConvertDstToString (uint16_t instr, uint16_t addr, char* strDst, uint16_t code)
{
    uint8_t reg = GetDigit(instr, 0);
    uint8_t param = GetDigit(instr, 1);

    LPCSTR pszReg = REGISTER_NAME[reg];

    if (reg != 7)
    {
        LPCSTR format = ADDRESS_MODE_FORMAT[param];

        if (param == 6 || param == 7)
        {
            _snprintf(strDst, 24, format, code, pszReg);
            return 1;
        }
        else
            _snprintf(strDst, 24, format, pszReg);
    }
    else
    {
        LPCSTR format = ADDRESS_MODE_PC_FORMAT[param];

        if (param == 2 || param == 3)
        {
            _snprintf(strDst, 24, format, code);
            return 1;
        }
        else if (param == 6 || param == 7)
        {
            _snprintf(strDst, 24, format, (uint16_t)(addr + code + 2));
            return 1;
        }
        else
            _snprintf(strDst, 24, format, pszReg);
    }

    return 0;
}

// Disassemble one instruction
//   pMemory - memory image (we read only words of the instruction)
//   sInstr  - instruction mnemonics buffer - at least 8 characters
//   sArg    - instruction arguments buffer - at least 32 characters
//   Return value: number of words in the instruction
uint16_t DisassembleInstruction(uint16_t* pMemory, uint16_t addr, char* strInstr, char* strArg)
{
    *strInstr = 0;
    *strArg = 0;

    uint16_t instr = *pMemory;

    uint16_t length = 1;
    LPCSTR strReg = NULL;
    char strSrc[24];
    char strDst[24];
    bool okByte;

    // No fields
    switch (instr)
    {
    case PI_HALT:   strcpy(strInstr, ("HALT"));   return 1;
    case PI_WAIT:   strcpy(strInstr, ("WAIT"));   return 1;
    case PI_RTI:    strcpy(strInstr, ("RTI"));    return 1;
    case PI_BPT:    strcpy(strInstr, ("BPT"));    return 1;
    case PI_IOT:    strcpy(strInstr, ("IOT"));    return 1;
    case PI_RESET:  strcpy(strInstr, ("RESET"));  return 1;
    case PI_RTT:    strcpy(strInstr, ("RTT"));    return 1;
    case PI_NOP:    strcpy(strInstr, ("NOP"));    return 1;
    case PI_CLC:    strcpy(strInstr, ("CLC"));    return 1;
    case PI_CLV:    strcpy(strInstr, ("CLV"));    return 1;
    case PI_CLVC:   strcpy(strInstr, ("CLVC"));   return 1;
    case PI_CLZ:    strcpy(strInstr, ("CLZ"));    return 1;
    case PI_CLZC:   strcpy(strInstr, ("CLZC"));   return 1;
    case PI_CLZV:   strcpy(strInstr, ("CLZV"));   return 1;
    case PI_CLZVC:  strcpy(strInstr, ("CLZVC"));  return 1;
    case PI_CLN:    strcpy(strInstr, ("CLN"));    return 1;
    case PI_CLNC:   strcpy(strInstr, ("CLNC"));   return 1;
    case PI_CLNV:   strcpy(strInstr, ("CLNV"));   return 1;
    case PI_CLNVC:  strcpy(strInstr, ("CLNVC"));  return 1;
    case PI_CLNZ:   strcpy(strInstr, ("CLNZ"));   return 1;
    case PI_CLNZC:  strcpy(strInstr, ("CLNZC"));  return 1;
    case PI_CLNZV:  strcpy(strInstr, ("CLNZV"));  return 1;
    case PI_CCC:    strcpy(strInstr, ("CCC"));    return 1;
    case PI_NOP260: strcpy(strInstr, ("NOP260")); return 1;
    case PI_SEC:    strcpy(strInstr, ("SEC"));    return 1;
    case PI_SEV:    strcpy(strInstr, ("SEV"));    return 1;
    case PI_SEVC:   strcpy(strInstr, ("SEVC"));   return 1;
    case PI_SEZ:    strcpy(strInstr, ("SEZ"));    return 1;
    case PI_SEZC:   strcpy(strInstr, ("SEZC"));   return 1;
    case PI_SEZV:   strcpy(strInstr, ("SEZV"));   return 1;
    case PI_SEZVC:  strcpy(strInstr, ("SEZVC"));  return 1;
    case PI_SEN:    strcpy(strInstr, ("SEN"));    return 1;
    case PI_SENC:   strcpy(strInstr, ("SENC"));   return 1;
    case PI_SENV:   strcpy(strInstr, ("SENV"));   return 1;
    case PI_SENVC:  strcpy(strInstr, ("SENVC"));  return 1;
    case PI_SENZ:   strcpy(strInstr, ("SENZ"));   return 1;
    case PI_SENZC:  strcpy(strInstr, ("SENZC"));  return 1;
    case PI_SENZV:  strcpy(strInstr, ("SENZV"));  return 1;
    case PI_SCC:    strcpy(strInstr, ("SCC"));    return 1;

        // Спецкоманды режима HALT ВМ2
    case PI_START:  strcpy(strInstr, ("START"));  return 1;
    case PI_STEP:   strcpy(strInstr, ("STEP"));   return 1;
    case PI_RSEL:   strcpy(strInstr, ("RSEL"));   return 1;
    case PI_MFUS:   strcpy(strInstr, ("MFUS"));   return 1;
    case PI_RCPC:   strcpy(strInstr, ("RCPC"));   return 1;
    case PI_RCPS:   strcpy(strInstr, ("RCPS"));   return 1;
    case PI_MTUS:   strcpy(strInstr, ("MTUS"));   return 1;
    case PI_WCPC:   strcpy(strInstr, ("WCPC"));   return 1;
    case PI_WCPS:   strcpy(strInstr, ("WCPS"));   return 1;
    }

    // One field
    if ((instr & ~(uint16_t)7) == PI_RTS)
    {
        if (GetDigit(instr, 0) == 7)
        {
            strcpy(strInstr, ("RETURN"));
        }
        else
        {
            strcpy(strInstr, ("RTS"));
            strcpy(strArg, REGISTER_NAME[GetDigit(instr, 0)]);
        }
        return 1;
    }

    // Two fields
    length += ConvertDstToString(instr, addr + 2, strDst, pMemory[1]);

    switch (instr & ~(uint16_t)077)
    {
    case PI_JMP:    strcpy(strInstr, ("JMP"));   strcpy(strArg, strDst);  return length;
    case PI_SWAB:   strcpy(strInstr, ("SWAB"));  strcpy(strArg, strDst);  return length;
    case PI_MARK:   strcpy(strInstr, ("MARK"));  strcpy(strArg, strDst);  return length;
    case PI_SXT:    strcpy(strInstr, ("SXT"));   strcpy(strArg, strDst);  return length;
    case PI_MTPS:   strcpy(strInstr, ("MTPS"));  strcpy(strArg, strDst);  return length;
    case PI_MFPS:   strcpy(strInstr, ("MFPS"));  strcpy(strArg, strDst);  return length;
    }

    okByte = (instr & 0100000) != 0;

    switch (instr & ~(uint16_t)0100077)
    {
    case PI_CLR:  strcpy(strInstr, okByte ? ("CLRB") : ("CLR"));  strcpy(strArg, strDst);  return length;
    case PI_COM:  strcpy(strInstr, okByte ? ("COMB") : ("COM"));  strcpy(strArg, strDst);  return length;
    case PI_INC:  strcpy(strInstr, okByte ? ("INCB") : ("INC"));  strcpy(strArg, strDst);  return length;
    case PI_DEC:  strcpy(strInstr, okByte ? ("DECB") : ("DEC"));  strcpy(strArg, strDst);  return length;
    case PI_NEG:  strcpy(strInstr, okByte ? ("NEGB") : ("NEG"));  strcpy(strArg, strDst);  return length;
    case PI_ADC:  strcpy(strInstr, okByte ? ("ADCB") : ("ADC"));  strcpy(strArg, strDst);  return length;
    case PI_SBC:  strcpy(strInstr, okByte ? ("SBCB") : ("SBC"));  strcpy(strArg, strDst);  return length;
    case PI_TST:  strcpy(strInstr, okByte ? ("TSTB") : ("TST"));  strcpy(strArg, strDst);  return length;
    case PI_ROR:  strcpy(strInstr, okByte ? ("RORB") : ("ROR"));  strcpy(strArg, strDst);  return length;
    case PI_ROL:  strcpy(strInstr, okByte ? ("ROLB") : ("ROL"));  strcpy(strArg, strDst);  return length;
    case PI_ASR:  strcpy(strInstr, okByte ? ("ASRB") : ("ASR"));  strcpy(strArg, strDst);  return length;
    case PI_ASL:  strcpy(strInstr, okByte ? ("ASLB") : ("ASL"));  strcpy(strArg, strDst);  return length;
    }

    length = 1;
    _snprintf(strDst, 24, ("%06o"), addr + ((short)(char)(uint8_t)(instr & 0xff) * 2) + 2);

    // Branchs & interrupts
    switch (instr & ~(uint16_t)0377)
    {
    case PI_BR:   strcpy(strInstr, ("BR"));   strcpy(strArg, strDst);  return 1;
    case PI_BNE:  strcpy(strInstr, ("BNE"));  strcpy(strArg, strDst);  return 1;
    case PI_BEQ:  strcpy(strInstr, ("BEQ"));  strcpy(strArg, strDst);  return 1;
    case PI_BGE:  strcpy(strInstr, ("BGE"));  strcpy(strArg, strDst);  return 1;
    case PI_BLT:  strcpy(strInstr, ("BLT"));  strcpy(strArg, strDst);  return 1;
    case PI_BGT:  strcpy(strInstr, ("BGT"));  strcpy(strArg, strDst);  return 1;
    case PI_BLE:  strcpy(strInstr, ("BLE"));  strcpy(strArg, strDst);  return 1;
    case PI_BPL:  strcpy(strInstr, ("BPL"));  strcpy(strArg, strDst);  return 1;
    case PI_BMI:  strcpy(strInstr, ("BMI"));  strcpy(strArg, strDst);  return 1;
    case PI_BHI:  strcpy(strInstr, ("BHI"));  strcpy(strArg, strDst);  return 1;
    case PI_BLOS:  strcpy(strInstr, ("BLOS"));  strcpy(strArg, strDst);  return 1;
    case PI_BVC:  strcpy(strInstr, ("BVC"));  strcpy(strArg, strDst);  return 1;
    case PI_BVS:  strcpy(strInstr, ("BVS"));  strcpy(strArg, strDst);  return 1;
    case PI_BHIS:  strcpy(strInstr, ("BHIS"));  strcpy(strArg, strDst);  return 1;
    case PI_BLO:  strcpy(strInstr, ("BLO"));  strcpy(strArg, strDst);  return 1;
    }

    _snprintf(strDst, 24, ("%06o"), (uint8_t)(instr & 0xff));

    switch (instr & ~(uint16_t)0377)
    {
    case PI_EMT:   strcpy(strInstr, ("EMT"));   strcpy(strArg, strDst);  return 1;
    case PI_TRAP:  strcpy(strInstr, ("TRAP"));  strcpy(strArg, strDst);  return 1;
    }

    // Three fields
    switch (instr & ~(uint16_t)0777)
    {
    case PI_JSR:
        if (GetDigit(instr, 2) == 7)
        {
            strcpy(strInstr, ("CALL"));
            length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
            _snprintf(strArg, 32, ("%s"), strDst);  // strArg = strDst;
        }
        else
        {
            strcpy(strInstr, ("JSR"));
            strReg = REGISTER_NAME[GetDigit(instr, 2)];
            length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
            _snprintf(strArg, 32, ("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        }
        return length;
    case PI_MUL:
        strcpy(strInstr, ("MUL"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snprintf(strArg, 32, ("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
    case PI_DIV:
        strcpy(strInstr, ("DIV"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snprintf(strArg, 32, ("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
    case PI_ASH:
        strcpy(strInstr, ("ASH"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snprintf(strArg, 32, ("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
    case PI_ASHC:
        strcpy(strInstr, ("ASHC"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snprintf(strArg, 32, ("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
    case PI_XOR:
        strcpy(strInstr, ("XOR"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        length += ConvertDstToString (instr, addr + 2, strDst, pMemory[1]);
        _snprintf(strArg, 32, ("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return length;
    case PI_SOB:
        strcpy(strInstr, ("SOB"));
        strReg = REGISTER_NAME[GetDigit(instr, 2)];
        _snprintf(strDst, 24, ("%06o"), addr - (GetDigit(instr, 1) * 8 + GetDigit(instr, 0)) * 2 + 2);
        _snprintf(strArg, 32, ("%s, %s"), strReg, strDst);  // strArg = strReg + ", " + strDst;
        return 1;
    }

    // Four fields

    okByte = (instr & 0100000) != 0;

    length += ConvertSrcToString(instr, addr + 2, strSrc, pMemory[1]);
    length += ConvertDstToString(instr, (uint16_t)(addr + 2 + (length - 1) * 2), strDst, pMemory[length]);

    switch (instr & ~(uint16_t)0107777)
    {
    case PI_MOV:
        strcpy(strInstr, okByte ? ("MOVB") : ("MOV"));
        _snprintf(strArg, 32, ("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_CMP:
        strcpy(strInstr, okByte ? ("CMPB") : ("CMP"));
        _snprintf(strArg, 32, ("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIT:
        strcpy(strInstr, okByte ? ("BITB") : ("BIT"));
        _snprintf(strArg, 32, ("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIC:
        strcpy(strInstr, okByte ? ("BICB") : ("BIC"));
        _snprintf(strArg, 32, ("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_BIS:
        strcpy(strInstr, okByte ? ("BISB") : ("BIS"));
        _snprintf(strArg, 32, ("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    }

    switch (instr & ~(uint16_t)0007777)
    {
    case PI_ADD:
        strcpy(strInstr, ("ADD"));
        _snprintf(strArg, 32, ("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    case PI_SUB:
        strcpy(strInstr, ("SUB"));
        _snprintf(strArg, 32, ("%s, %s"), strSrc, strDst);  // strArg = strSrc + ", " + strDst;
        return length;
    }

    strcpy(strInstr, ("unknown"));
    _snprintf(strArg, 32, ("%06o"), instr);
    return 1;
}
