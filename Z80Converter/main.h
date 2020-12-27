#pragma once

#include <stdio.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <array>


// Z80 disassmebler, see dasmz80.cpp

int DAsm(char *S, unsigned char *A);


// Globals

extern std::array<uint8_t, 65536> g_memdmp;


// Recompiler

std::string recomp(uint16_t addr);

