// Recompiler.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "main.h"

typedef std::string string;

const size_t MAX_COMMAND_SIZE = 6;

uint8_t g_command[MAX_COMMAND_SIZE];
uint16_t g_commandaddr;


/////////////////////////////////////////////////////////////////////////////


static const char* rpnames_bcdehlsp[] = { "R1", "R2", "R3", "R6" };  // BC, DE, HL, SP
static const char* rpnames_bcdehlaf[] = { "R1", "R2", "R3", "R0" };  // BC, DE, HL, AF
static const char* rnames_cela[] = { "R1", "R2", "R3", "R0" };  // C, E, L, A
static const char* rnames_bdhhl[] = { "R1", "R2", "R3", "(R3)" };  // B, D, H, (HL)


string PatternProc_NOP()
{
	return "NOP";
}

string PatternProc_LD_BCDEHLSP_0000()
{
	const char* rpname = rpnames_bcdehlsp[(g_command[0] >> 4) & 3];
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "CLR %s", rpname);
	return buffer;
}

string PatternProc_LD_BCDEHLSP_NNNN()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	const char* rpname = rpnames_bcdehlsp[(g_command[0] >> 4) & 3];
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "MOV #L%04X, %s or MOV #%06o, %s", wparam, rpname, wparam, rpname);
	return buffer;
}

string PatternProc_LD_BCDEADDR_A()
{
	if (g_command[0] == 0x02)
		return "MOVB R0, (R1)";
	else
		return "MOVB R0, (R2)";
}

string PatternProc_ADD_HL_BCDEHLSP()
{
	const char* rpname = rpnames_bcdehlsp[(g_command[0] >> 4) & 3];
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "ADD %s, R3", rpname);
	return buffer;
}

string PatternProc_INC_CELA()
{
	const char* rpname = rnames_cela[(g_command[0] >> 4) & 3];
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "INCB %s or INC %s", rpname, rpname);
	return buffer;
}
string PatternProc_DEC_CELA()
{
	const char* rpname = rnames_cela[(g_command[0] >> 4) & 3];
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "DECB %s or DEC %s", rpname, rpname);
	return buffer;
}
string PatternProc_INC_BCDEHLSP()
{
	const char* rpname = rpnames_bcdehlsp[(g_command[0] >> 4) & 3];
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "INC %s", rpname);
	return buffer;
}
string PatternProc_DEC_BCDEHLSP()
{
	const char* rpname = rpnames_bcdehlsp[(g_command[0] >> 4) & 3];
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "DEC %s", rpname);
	return buffer;
}

string PatternProc_INC_BDHHL()
{
	if (g_command[0] == 0x34)  // inc (hl)
		return "INCB (R3)";

	char buffer[40];
	const char* rpname = rnames_bdhhl[(g_command[0] >> 4) & 3];
	snprintf(buffer, sizeof(buffer), "ADD #00400, %s", rpname);
	return buffer;
}
string PatternProc_DEC_BDHHL()
{
	if (g_command[0] == 0x34)  // dec (hl)
		return "DECB (R3)";

	char buffer[40];
	const char* rpname = rnames_bdhhl[(g_command[0] >> 4) & 3];
	snprintf(buffer, sizeof(buffer), "SUB #00400, %s", rpname);
	return buffer;
}

string PatternProc_LD_HLADDR_NN()
{
	uint8_t bparam = g_command[1];
	char buffer[40];
	if (bparam == 0)
		return "CLRB (R3)";

	snprintf(buffer, sizeof(buffer), "MOVB #%03o, (R3)", bparam);
	return buffer;
}
string PatternProc_LD_BDHHL_NN()
{
	if (g_command[0] == 0x36)
		return PatternProc_LD_HLADDR_NN();

	uint8_t bparam = g_command[1];
	char buffer[40];
	const char* rpname = rnames_bdhhl[(g_command[0] >> 4) & 3];
	if (bparam == 0)
	{
		snprintf(buffer, sizeof(buffer), "BIC #177400, %s", rpname);
		return buffer;
	}

	snprintf(buffer, sizeof(buffer), "BIC #177400, %s / BIS #%06o, %s", rpname, bparam << 8, rpname);
	return buffer;
}

string PatternProc_DJNZ()
{
	uint8_t bparam = g_command[1];
	int delta = bparam & 0x80 ?  (bparam & 0x7F) - 128 + 2 : ((int)bparam) + 2;
	uint16_t jmpaddr = g_commandaddr + delta;
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "SOB R1, L%04X", jmpaddr);
	return buffer;
}
string PatternProc_JR()
{
	uint8_t bparam = g_command[1];
	int delta = bparam & 0x80 ? (bparam & 0x7F) - 128 + 2 : ((int)bparam) + 2;
	uint16_t jmpaddr = g_commandaddr + delta;
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "BR L%04X", jmpaddr);
	return buffer;
}
string PatternProc_JR_ZNZ()
{
	uint8_t bparam = g_command[1];
	int delta = bparam & 0x80 ? (bparam & 0x7F) - 128 + 2 : ((int)bparam) + 2;
	uint16_t jmpaddr = g_commandaddr + delta;
	char buffer[40];
	const char* op = g_command[0] == 0x20 ? "BNE" : "BEQ";
	snprintf(buffer, sizeof(buffer), "%s L%04X", op, jmpaddr);
	return buffer;
}
string PatternProc_JR_CNC()
{
	uint8_t bparam = g_command[1];
	int delta = bparam & 0x80 ? (bparam & 0x7F) - 128 + 2 : ((int)bparam) + 2;
	uint16_t jmpaddr = g_commandaddr + delta;
	char buffer[40];
	const char* op = g_command[0] == 0x30 ? "BHIS" : "BLO";
	snprintf(buffer, sizeof(buffer), "%s L%04X", op, jmpaddr);
	return buffer;
}

string PatternProc_ROLL_A()
{
	switch (g_command[0])
	{
	case 0x07: /* rlca */ return "ROLB R0";
	case 0x0F: /* rrca */ return "RORB R0";
	case 0x17: /* rla */  return "???";
	case 0x1F: /* rra */  return "???";
	}
	return "";
}

string PatternProc_RLA()
{
	return "ROLB R0";
}
string PatternProc_RRA()
{
	return "RORB R0";
}
string PatternProc_SRL_A()
{
	return "ASRB R0 or ASR R0";
}

string PatternProc_SCF_CCF()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	char buffer[40];
	const char* op = g_command[0] == 0x37 ? "SEC" : "CLC";
	snprintf(buffer, sizeof(buffer), "%s", op);
	return buffer;
}

string PatternProc_LD_ADDR_A()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "MOVB R0, @#L%04X", wparam);
	return buffer;
}
string PatternProc_LD_A_ADDR()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "MOVB @#L%04X, R0", wparam);
	return buffer;
}

string PatternProc_LD_A_NN()
{
	uint8_t bparam = g_command[1];
	char buffer[40];
	if (bparam == 0)
		return "CLR R0";

	snprintf(buffer, sizeof(buffer), "MOV #%03o, R0", bparam);
	return buffer;
}
string PatternProc_LD_CELA_NN()
{
	if (g_command[0] == 0x3E)
		return PatternProc_LD_A_NN();
	const char* rpname = rnames_cela[(g_command[0] >> 4) & 3];
	uint8_t bparam = g_command[1];
	char buffer[40];
	if (bparam == 0)
	{
		snprintf(buffer, sizeof(buffer), "BIC #377, %s", rpname);
		return buffer;
	}
	if (bparam == 0xFF)
	{
		snprintf(buffer, sizeof(buffer), "BIS #377, %s", rpname);
		return buffer;
	}

	snprintf(buffer, sizeof(buffer), "BIC #377, %s / BIS #%03o, %s", rpname, bparam, rpname);
	return buffer;
}

string PatternProc_LD_X_A()
{
	switch (g_command[0])
	{
	case 0x47: /* ld b,a */ return "SWAP R1 / BIC #377, R1 / BISB R0, R1 / SWAP R1";
	case 0x57: /* ld d,a */ return "SWAP R2 / BIC #377, R2 / BISB R0, R2 / SWAP R2";
	case 0x67: /* ld h,a */ return "SWAP R3 / BIC #377, R3 / BISB R0, R3 / SWAP R3";
	case 0x77: /* ld (hl),a */ return "MOVB R0, (R3)";
	case 0x4F: /* ld c,a */ return "MOV R0, R1 or BIC #377, R1 / BISB R0, R1";
	case 0x5F: /* ld e,a */ return "MOV R0, R2 or BIC #377, R2 / BISB R0, R2";
	case 0x6F: /* ld l,a */ return "MOV R0, R3 or BIC #377, R3 / BISB R0, R3";
	case 0x7F: /* ld a,a */ return "???";
	}
	return "";
}

string PatternProc_LD_A_X()
{
	switch (g_command[0])
	{
	case 0x78: /* ld a,b */ return "SWAP R1 / MOVB R1, R0 / SWAP R1";
	case 0x79: /* ld a,c */ return "MOVB R1, R0";
	case 0x7A: /* ld a,d */ return "SWAP R2 / MOVB R2, R0 / SWAP R2";
	case 0x7B: /* ld a,e */ return "MOVB R2, R0";
	case 0x7C: /* ld a,h */ return "SWAP R3 / MOVB R3, R0 / SWAP R3";
	case 0x7D: /* ld a,l */ return "MOVB R3, R0";
	case 0x7E: /* ld a,(hl) */ return "MOVB (R3), R0";
	case 0x7F: /* ld a,a */ return "???";
	}
	return "";
}

string PatternProc_LD_HLADDR_X()
{
	switch (g_command[0])
	{
	case 0x70: /* ld (hl),b */ return "???";
	case 0x71: /* ld (hl),c */ return "MOVB R1, (R3)";
	case 0x72: /* ld (hl),d */ return "???";
	case 0x73: /* ld (hl),e */ return "MOVB R2, (R3)";
	case 0x74: /* ld (hl),h */ return "???";
	case 0x75: /* ld (hl),l */ return "MOVB R3, (R3)";
	case 0x76: /* halt */ return "HALT";
	case 0x77: /* ld (hl),a */ return "MOVB R0, (R3)";
	}
	return "";
}

string PatternProc_LD_A_HLADDR()
{
	return "MOVB (R3), R0";
}

string PatternProc_INCDEC_HLADDR()
{
	return (g_command[0] == 0x34) ? "INC (R3)" : "DEC (R3)";
}

string PatternProc_LD_ADDR_HL()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	char buffer[50];
	snprintf(buffer, sizeof(buffer), "MOVB R3, @#%04X / SWAB R3 / MOVB R3, @#%04X / SWAB R3", wparam, wparam + 1);
	return buffer;
}
string PatternProc_LD_HL_ADDR()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	char buffer[50];
	snprintf(buffer, sizeof(buffer), "CLR R3 / BISB @#%04X, R3 / SWAB R3 / BISB @#%04X, R3 / SWAB R3", wparam, wparam + 1);
	return buffer;
}

string PatternProc_ADD_A_A()
{
	return "ASL R0 or ASLB R0";
}
string PatternProc_ADD_A_NN()
{
	char buffer[40];
	uint8_t bparam = g_command[1];
	snprintf(buffer, sizeof(buffer), "ADD #%03o, R0", bparam);
	return buffer;
}
string PatternProc_ADD_A_HLADDR()
{
	return "MOVB (R3), R5 / ADD R5, R0";
}
string PatternProc_SUB_A_NN()
{
	char buffer[40];
	uint8_t bparam = g_command[1];
	snprintf(buffer, sizeof(buffer), "SUB #%03o, R0", bparam);
	return buffer;
}

string PatternProc_ANDOR_A_HLADDR()
{
	char buffer[40];
	if (g_command[0] == 0xA6)  // AND
	{
		snprintf(buffer, sizeof(buffer), "??? (R3), R0"); //TODO
	}
	else  // OR
	{
		snprintf(buffer, sizeof(buffer), "BISB (R3), R0");
	}
	return buffer;
}
string PatternProc_ANDOR_A_NN()
{
	uint8_t bparam = g_command[1];
	char buffer[40];
	if (g_command[0] == 0xE6)  // AND
	{
		snprintf(buffer, sizeof(buffer), "BICB #%03o, R0", ~bparam & 0xFF); //TODO
	}
	else  // OR
	{
		snprintf(buffer, sizeof(buffer), "BISB #%03o, R0", bparam);
	}
	return buffer;
}

string PatternProc_XOR_A()
{
	return "CLR R0";
}
string PatternProc_OR_A()
{
	return "TST R0 or TSTB R0";
}

string PatternProc_ADD_X()
{
	switch (g_command[0])
	{
	case 0x80: /* add b */ return "???";
	case 0x81: /* add c */ return "ADD R1 or CLR R5 / MOVB R1, R5 / ADD R5, R0";
	case 0x82: /* add d */ return "???";
	case 0x83: /* add e */ return "ADD R2 or CLR R5 / MOVB R2, R5 / ADD R5, R0";
	case 0x84: /* add h */ return "???";
	case 0x85: /* add l */ return "ADD R3 or CLR R5 / MOVB R3, R5 / ADD R5, R0";
	case 0x86: /* add (hl) */ return "???";
	case 0x87: /* add a */ return "CLR R0";
	}
	return "";
}

string PatternProc_SUB_X()
{
	switch (g_command[0])
	{
	case 0x90: /* sub b */ return "???";
	case 0x91: /* sub c */ return "SUB R1 or CLR R5 / MOVB R1, R5 / SUB R5, R0";
	case 0x92: /* sub d */ return "???";
	case 0x93: /* sub e */ return "SUB R2 or CLR R5 / MOVB R2, R5 / SUB R5, R0";
	case 0x94: /* sub h */ return "???";
	case 0x95: /* sub l */ return "SUB R3 or CLR R5 / MOVB R3, R5 / SUB R5, R0";
	case 0x96: /* sub (hl) */ return "???";
	case 0x97: /* sub a */ return "CLR R0";
	}
	return "";
}

string PatternProc_JP()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "JMP L%04X", wparam);
	return buffer;
}
string PatternProc_JP_CC()
{
	const char* op;
	if ((g_command[0] & 0x0F) == 0x02)  // jp nz, jp nc, jp po, jp p
	{
		static const char* ops[] = { "BEQ", "BLO", "BVC", "BHI" };
		op = ops[(g_command[0] & 0x30) >> 4];
	}
	else
	{
		static const char* ops[] = { "BNE", "BHIS", "BVS", "BLOS" };
		op = ops[(g_command[0] & 0x30) >> 4];
	}
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "%s L%04X / JMP L%04X", op, g_commandaddr + 3, wparam);
	return buffer;
}
string PatternProc_RET_CC()
{
	const char* op;
	if ((g_command[0] & 0x0F) == 0)  // ret nz, ret nc, ret po, ret p
	{
		static const char* ops[] = { "BEQ", "BLO", "BVC", "BHI" };
		op = ops[(g_command[0] & 0x30) >> 4];
	}
	else  // ret z, ret c, ret pe, ret m
	{
		static const char* ops[] = { "BNE", "BHIS", "BVS", "BLOS" };
		op = ops[(g_command[0] & 0x30) >> 4];
	}
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "%s L%04X / RETURN", op, g_commandaddr + 1);
	return buffer;
}
string PatternProc_CALL_CC()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	const char* op;
	if ((g_command[0] & 0x0F) == 4)  // call nz, call nc, call po, call p
	{
		static const char* ops[] = { "BEQ", "BLO", "BVC", "BHI" };
		op = ops[(g_command[0] & 0x30) >> 4];
	}
	else  // call z, call c, call pe, call m
	{
		static const char* ops[] = { "BNE", "BHIS", "BVS", "BLOS" };
		op = ops[(g_command[0] & 0x30) >> 4];
	}
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "%s L%04X / CALL L%04X", op, g_commandaddr + 3, wparam);
	return buffer;
}

string PatternProc_POP_BCDEHLAF()
{
	const char* rpname = rpnames_bcdehlaf[(g_command[0] >> 4) & 3];
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "POP %s", rpname);
	return buffer;
}
string PatternProc_PUSH_BCDEHLAF()
{
	const char* rpname = rpnames_bcdehlaf[(g_command[0] >> 4) & 3];
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "PUSH %s", rpname);
	return buffer;
}

string PatternProc_RET()
{
	return "RETURN";
}

string PatternProc_CALL()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "CALL L%04X", wparam);
	return buffer;
}

string PatternProc_CP_XX()
{
	uint8_t bparam = g_command[1];
	char buffer[40];
	//snprintf(buffer, sizeof(buffer), "CMPB R0, #%d.", bparam);
	snprintf(buffer, sizeof(buffer), "CMPB R0, #%03o", bparam);
	return buffer;
}

string PatternProc_BIT_X_A()
{
	int bitno = ((g_command[1] >> 3) & 0x7);
	int mask = 1 << bitno;
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "BIT #%03o, R0", mask);
	return buffer;
}

string PatternProc_NEG()
{
	return "NEGB R0 or NEG R0";
}

string PatternProc_EX_DE_HL()
{
	return "XOR R3, R2 / XOR R2, R3 / XOR R3, R2";
}

string PatternProc_LDI()
{
	return "MOVB (R3)+, (R2)+ / DEC R1";
}
string PatternProc_LDIR()
{
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "MOVB (R3)+, (R2)+ / SOB R1, L%04X", g_commandaddr);
	return buffer;
}

string PatternProc_LD_IXIY_NNNN()
{
	uint16_t wparam = g_command[1] + g_command[2] * 256;
	char buffer[40];
	snprintf(buffer, sizeof(buffer), "MOV #%06o, R5 or MOV #L%04X, R5", wparam, wparam);
	return buffer;
}
string PatternProc_INC_IXIY()
{
	return "INC R5";
}
string PatternProc_DEC_IXIY()
{
	return "DEC R5";
}
string PatternProc_POP_IXIY()
{
	return "POP R5";
}
string PatternProc_PUSH_IXIY()
{
	return "PUSH R5";
}


/////////////////////////////////////////////////////////////////////////////


typedef std::string(*PatternProc)();

struct Pattern
{
	int length;
	uint8_t bytes[MAX_COMMAND_SIZE];
	uint8_t masks[MAX_COMMAND_SIZE];
	PatternProc proc;
};

Pattern g_patterns[] =
{
	// Special cases
	{ 3, { 0x01, 0, 0 }, { 0xCF, 0xFF, 0xFF }, PatternProc_LD_BCDEHLSP_0000 },

	// Main table
	{ 1, { 0x00 }, { 0xFF }, PatternProc_NOP },
	{ 1, { 0x01 }, { 0xCF }, PatternProc_LD_BCDEHLSP_NNNN },
	{ 1, { 0x02 }, { 0xEF }, PatternProc_LD_BCDEADDR_A },
	{ 1, { 0x03 }, { 0xCF }, PatternProc_INC_BCDEHLSP },
	{ 1, { 0x04 }, { 0xCF }, PatternProc_INC_BDHHL },
	{ 1, { 0x05 }, { 0xCF }, PatternProc_DEC_BDHHL },
	{ 1, { 0x06 }, { 0xCF }, PatternProc_LD_BDHHL_NN },
	{ 1, { 0x07 }, { 0xE7 }, PatternProc_ROLL_A },
	{ 1, { 0x09 }, { 0xCF }, PatternProc_ADD_HL_BCDEHLSP },
	{ 1, { 0x0B }, { 0xCF }, PatternProc_DEC_BCDEHLSP },
	{ 1, { 0x0C }, { 0xCF }, PatternProc_INC_CELA },
	{ 1, { 0x0D }, { 0xCF }, PatternProc_DEC_CELA },
	{ 1, { 0x0E }, { 0xCF }, PatternProc_LD_CELA_NN },
	{ 1, { 0x10 }, { 0xFF }, PatternProc_DJNZ },
	{ 1, { 0x18 }, { 0xFF }, PatternProc_JR },
	{ 1, { 0x17 }, { 0xFF }, PatternProc_RLA },
	{ 1, { 0x1F }, { 0xFF }, PatternProc_RRA },
	{ 1, { 0x20 }, { 0xF7 }, PatternProc_JR_ZNZ },
	{ 1, { 0x22 }, { 0xFF }, PatternProc_LD_ADDR_HL },
	{ 1, { 0x2A }, { 0xFF }, PatternProc_LD_HL_ADDR },
	{ 1, { 0x30 }, { 0xF7 }, PatternProc_JR_CNC },
	{ 1, { 0x32 }, { 0xFF }, PatternProc_LD_ADDR_A },
	{ 1, { 0x34 }, { 0xFE }, PatternProc_INCDEC_HLADDR },
	{ 1, { 0x37 }, { 0xF7 }, PatternProc_SCF_CCF },
	{ 1, { 0x3A }, { 0xFF }, PatternProc_LD_A_ADDR },
	{ 1, { 0x3E }, { 0xFF }, PatternProc_LD_A_NN },
	{ 1, { 0x47 }, { 0xC7 }, PatternProc_LD_X_A },
	{ 1, { 0x70 }, { 0xF8 }, PatternProc_LD_HLADDR_X },
	{ 1, { 0x78 }, { 0xF8 }, PatternProc_LD_A_X },
	{ 1, { 0x7E }, { 0xFF }, PatternProc_LD_A_HLADDR },
	{ 1, { 0x86 }, { 0xFF }, PatternProc_ADD_A_HLADDR },
	{ 1, { 0x87 }, { 0xFF }, PatternProc_ADD_A_A },
	{ 1, { 0x80 }, { 0xF8 }, PatternProc_ADD_X },
	{ 1, { 0x90 }, { 0xF8 }, PatternProc_SUB_X },
	{ 1, { 0xA6 }, { 0xEF }, PatternProc_ANDOR_A_HLADDR },
	{ 1, { 0xAF }, { 0xFF }, PatternProc_XOR_A },
	{ 1, { 0xB7 }, { 0xFF }, PatternProc_OR_A },
	{ 1, { 0xC0 }, { 0xC7 }, PatternProc_RET_CC },
	{ 1, { 0xC1 }, { 0xCF }, PatternProc_POP_BCDEHLAF },
	{ 1, { 0xC2 }, { 0xC7 }, PatternProc_JP_CC },
	{ 1, { 0xC3 }, { 0xFF }, PatternProc_JP },
	{ 1, { 0xC4 }, { 0xC7 }, PatternProc_CALL_CC },
	{ 1, { 0xC5 }, { 0xCF }, PatternProc_PUSH_BCDEHLAF },
	{ 1, { 0xC6 }, { 0xFF }, PatternProc_ADD_A_NN },
	{ 1, { 0xC9 }, { 0xFF }, PatternProc_RET },
	{ 1, { 0xCD }, { 0xFF }, PatternProc_CALL },
	{ 1, { 0xD6 }, { 0xFF }, PatternProc_SUB_A_NN },
	{ 1, { 0xE6 }, { 0xEF }, PatternProc_ANDOR_A_NN },
	{ 1, { 0xEB }, { 0xEF }, PatternProc_EX_DE_HL },
	{ 1, { 0xFE }, { 0xFF }, PatternProc_CP_XX },

	// CB table
	{ 2, { 0xCB, 0x3F }, { 0xFF, 0xFF }, PatternProc_SRL_A },
	{ 2, { 0xCB, 0x47 }, { 0xFF, 0xC7 }, PatternProc_BIT_X_A },

	// ED table
	{ 2, { 0xED, 0x44 }, { 0xFF, 0xFF }, PatternProc_NEG },
	{ 2, { 0xED, 0xA0 }, { 0xFF, 0xFF }, PatternProc_LDI },
	{ 2, { 0xED, 0xB0 }, { 0xFF, 0xFF }, PatternProc_LDIR },

	// DD/FD table
	{ 2, { 0xDD, 0x21 }, { 0xDF, 0xFF }, PatternProc_LD_IXIY_NNNN },
	{ 2, { 0xDD, 0x23 }, { 0xDF, 0xFF }, PatternProc_INC_IXIY },
	{ 2, { 0xDD, 0x2B }, { 0xDF, 0xFF }, PatternProc_DEC_IXIY },
	{ 2, { 0xDD, 0xE1 }, { 0xDF, 0xFF }, PatternProc_POP_IXIY },
	{ 2, { 0xDD, 0xE5 }, { 0xDF, 0xFF }, PatternProc_PUSH_IXIY },

};
int NUMBER_OF_PATTERNS = sizeof(g_patterns) / sizeof(g_patterns[0]);


string recomp(uint16_t addr)
{
	g_commandaddr = addr;

	// Read N bytes from g_memdmp to buffer
	for (int i = 0; i < MAX_COMMAND_SIZE; i++)
	{
		uint8_t val = 0;
		if (addr + i < 65536)
			val = g_memdmp[addr + i];
		g_command[i] = val;
	}

	// Try to find a pattern
	int pnmatch = -1;
	for (int pn = 0; pn < NUMBER_OF_PATTERNS; pn++)
	{
		Pattern& p = g_patterns[pn];

		//TODO: assert p.length <= MAX_COMMAND_SIZE

		bool match = true;
		for (int i = 0; i < p.length; i++)
		{
			uint8_t b = p.bytes[i];
			uint8_t m = p.masks[i];
			uint8_t v = g_command[i];
			if ((v & m) != b)
			{
				match = false;
				break;
			}
		}

		if (match)
		{
			pnmatch = pn;
			break;
		}
	}

	if (pnmatch < 0)
	{
		return "";  // No matching pattern found for command
	}

	Pattern& pattern = g_patterns[pnmatch];
	if (pattern.proc == nullptr)
	{
		return "";  // Pattern procedure not defined
	}

	PatternProc proc = pattern.proc;
	string result = proc();

	return result;
}


/////////////////////////////////////////////////////////////////////////////
