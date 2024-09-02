/*  Copyright 2004-2005 Theo Berkau
    Copyright 2006 Ex-Cyber
    Copyright 2005 Guillaume Duhamel

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <stdlib.h>
#include "cs0.h"
#include "error.h"
#ifdef GEKKO
#include "cs2.h"
static char rom16mname[512];
#endif

cartridge_struct *CartridgeArea;

//////////////////////////////////////////////////////////////////////////////
// Dummy/No Cart Functions
//////////////////////////////////////////////////////////////////////////////

static u8 DummyCs0ReadByte(UNUSED u32 addr)
{
	return 0xFF;
}


static u16 DummyCs0ReadWord(UNUSED u32 addr)
{
	return 0xFFFF;
}


static u32 DummyCs0ReadLong(UNUSED u32 addr)
{
	return 0xFFFFFFFF;
}


static void DummyCs0WriteByte(UNUSED u32 addr, UNUSED u8 val)
{
}


static void DummyCs0WriteWord(UNUSED u32 addr, UNUSED u16 val)
{
}


static void DummyCs0WriteLong(UNUSED u32 addr, UNUSED u32 val)
{
}


static u8 DummyCs2ReadByte(UNUSED u32 addr)
{
	return 0xFF;
}


static u16 DummyCs2ReadWord(UNUSED u32 addr)
{
	return 0xFFFF;
}


static u32 DummyCs2ReadLong(UNUSED u32 addr)
{
	return 0xFFFFFFFF;
}


static void DummyCs2WriteByte(UNUSED u32 addr, UNUSED u8 val)
{
}


static void DummyCs2WriteWord(UNUSED u32 addr, UNUSED u16 val)
{
}


static void DummyCs2WriteLong(UNUSED u32 addr, UNUSED u32 val)
{
}

//////////////////////////////////////////////////////////////////////////////
// 8 Mbit Dram
//////////////////////////////////////////////////////////////////////////////

static u8 DRAM8MBITCs0ReadByte(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		return T1ReadByte(CartridgeArea->dram, addr);
	}
	return 0xFF;
}

//////////////////////////////////////////////////////////////////////////////

static u16 DRAM8MBITCs0ReadWord(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		return T1ReadWord(CartridgeArea->dram, addr);
	}
	return 0xFFFF;
}

//////////////////////////////////////////////////////////////////////////////

static u32 DRAM8MBITCs0ReadLong(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		return T1ReadLong(CartridgeArea->dram, addr);
	}
	return 0xFFFFFFFF;
}

//////////////////////////////////////////////////////////////////////////////

static void DRAM8MBITCs0WriteByte(u32 addr, u8 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		T1WriteByte(CartridgeArea->dram, addr, val);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void DRAM8MBITCs0WriteWord(u32 addr, u16 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		T1WriteWord(CartridgeArea->dram, addr, val);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void DRAM8MBITCs0WriteLong(u32 addr, u32 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		T1WriteLong(CartridgeArea->dram, addr, val);
	}
}

//////////////////////////////////////////////////////////////////////////////
// 32 Mbit Dram
//////////////////////////////////////////////////////////////////////////////

static u8 DRAM32MBITCs0ReadByte(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		return T1ReadByte(CartridgeArea->dram, addr & 0x3FFFFF);
	}
	return 0xFF;
}

//////////////////////////////////////////////////////////////////////////////

static u16 DRAM32MBITCs0ReadWord(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		return T1ReadWord(CartridgeArea->dram, addr & 0x3FFFFF);
	}
	return 0xFFFF;
}

//////////////////////////////////////////////////////////////////////////////

static u32 DRAM32MBITCs0ReadLong(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		return T1ReadLong(CartridgeArea->dram, addr & 0x3FFFFF);
	}
	return 0xFFFFFFFF;
}

//////////////////////////////////////////////////////////////////////////////

static void DRAM32MBITCs0WriteByte(u32 addr, u8 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		return T1WriteByte(CartridgeArea->dram, addr & 0x3FFFFF, val);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void DRAM32MBITCs0WriteWord(u32 addr, u16 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		return T1WriteWord(CartridgeArea->dram, addr & 0x3FFFFF, val);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void DRAM32MBITCs0WriteLong(u32 addr, u32 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		return T1WriteLong(CartridgeArea->dram, addr & 0x3FFFFF, val);
	}
}

//////////////////////////////////////////////////////////////////////////////
// 16 Mbit Rom
//////////////////////////////////////////////////////////////////////////////

static u8 ROM16MBITCs0ReadByte(u32 addr)
{
	return T1ReadByte(CartridgeArea->rom, addr & 0x1FFFFF);
}

//////////////////////////////////////////////////////////////////////////////

static u16 ROM16MBITCs0ReadWord(u32 addr)
{
	return T1ReadWord(CartridgeArea->rom, addr & 0x1FFFFF);
}

//////////////////////////////////////////////////////////////////////////////

static u32 ROM16MBITCs0ReadLong(u32 addr)
{
	return T1ReadLong(CartridgeArea->rom, addr & 0x1FFFFF);
}

//////////////////////////////////////////////////////////////////////////////

static void ROM16MBITCs0WriteByte(u32 addr, u8 val)
{
	T1WriteByte(CartridgeArea->rom, addr & 0x1FFFFF, val);
}

//////////////////////////////////////////////////////////////////////////////

static void ROM16MBITCs0WriteWord(u32 addr, u16 val)
{
	T1WriteWord(CartridgeArea->rom, addr & 0x1FFFFF, val);
}

//////////////////////////////////////////////////////////////////////////////

static void ROM16MBITCs0WriteLong(u32 addr, u32 val)
{
	T1WriteLong(CartridgeArea->rom, addr & 0x1FFFFF, val);
}

//////////////////////////////////////////////////////////////////////////////
// General Cart functions
//////////////////////////////////////////////////////////////////////////////

int CartInit(const char * filename, int type)
{
	if ((CartridgeArea = (cartridge_struct *)calloc(1, sizeof(cartridge_struct))) == NULL)
	  return -1;

	CartridgeArea->carttype = type;
#ifndef GEKKO
	CartridgeArea->filename = filename;
#else
	if (type == CART_ROM16MBIT) {
		strcpy(rom16mname, filename);
		if(strlen(cdip->itemnum)!=0) {
			strcat(rom16mname, cdip->itemnum);
			strcat(rom16mname, "_");
		}
		strcat(rom16mname, "ROM16M.bin");
		filename = rom16mname;
		CartridgeArea->filename = filename;
	} else {
		CartridgeArea->filename = filename;
	}
#endif

	// Setup default mappings
	CartridgeArea->Cs0ReadByte = &DummyCs0ReadByte;
	CartridgeArea->Cs0ReadWord = &DummyCs0ReadWord;
	CartridgeArea->Cs0ReadLong = &DummyCs0ReadLong;
	CartridgeArea->Cs0WriteByte = &DummyCs0WriteByte;
	CartridgeArea->Cs0WriteWord = &DummyCs0WriteWord;
	CartridgeArea->Cs0WriteLong = &DummyCs0WriteLong;

	CartridgeArea->Cs2ReadByte = &DummyCs2ReadByte;
	CartridgeArea->Cs2ReadWord = &DummyCs2ReadWord;
	CartridgeArea->Cs2ReadLong = &DummyCs2ReadLong;
	CartridgeArea->Cs2WriteByte = &DummyCs2WriteByte;
	CartridgeArea->Cs2WriteWord = &DummyCs2WriteWord;
	CartridgeArea->Cs2WriteLong = &DummyCs2WriteLong;


	switch(type) {
		case CART_DRAM8MBIT: { // 8-Mbit Dram Cart
			if ((CartridgeArea->dram = T1MemoryInit(0x100000)) == NULL)
				return -1;

			CartridgeArea->cartid = 0x5A;

			// Setup Functions
			CartridgeArea->Cs0ReadByte = &DRAM8MBITCs0ReadByte;
			CartridgeArea->Cs0ReadWord = &DRAM8MBITCs0ReadWord;
			CartridgeArea->Cs0ReadLong = &DRAM8MBITCs0ReadLong;
			CartridgeArea->Cs0WriteByte = &DRAM8MBITCs0WriteByte;
			CartridgeArea->Cs0WriteWord = &DRAM8MBITCs0WriteWord;
			CartridgeArea->Cs0WriteLong = &DRAM8MBITCs0WriteLong;
		} break;
		case CART_DRAM32MBIT: { // 32-Mbit Dram Cart
			if ((CartridgeArea->dram = T1MemoryInit(0x400000)) == NULL)
				return -1;

			CartridgeArea->cartid = 0x5C;

			// Setup Functions
			CartridgeArea->Cs0ReadByte = &DRAM32MBITCs0ReadByte;
			CartridgeArea->Cs0ReadWord = &DRAM32MBITCs0ReadWord;
			CartridgeArea->Cs0ReadLong = &DRAM32MBITCs0ReadLong;
			CartridgeArea->Cs0WriteByte = &DRAM32MBITCs0WriteByte;
			CartridgeArea->Cs0WriteWord = &DRAM32MBITCs0WriteWord;
			CartridgeArea->Cs0WriteLong = &DRAM32MBITCs0WriteLong;
		} break;
		case CART_ROM16MBIT: { // 16-Mbit Rom Cart
			if ((CartridgeArea->rom = T1MemoryInit(0x200000)) == NULL)
				return -1;

			CartridgeArea->cartid = 0xFF; // I have no idea what the real id is

			// Load Rom to memory
			if (T123Load(CartridgeArea->rom, 0x200000, 1, filename) != 0)
				return -1;

			// Setup Functions
			CartridgeArea->Cs0ReadByte = &ROM16MBITCs0ReadByte;
			CartridgeArea->Cs0ReadWord = &ROM16MBITCs0ReadWord;
			CartridgeArea->Cs0ReadLong = &ROM16MBITCs0ReadLong;
			CartridgeArea->Cs0WriteByte = &ROM16MBITCs0WriteByte;
			CartridgeArea->Cs0WriteWord = &ROM16MBITCs0WriteWord;
			CartridgeArea->Cs0WriteLong = &ROM16MBITCs0WriteLong;
		} break;
		default: {// No Cart
			CartridgeArea->cartid = 0xFF;
		} break;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void CartDeInit(void)
{
	if (CartridgeArea) {
		if (CartridgeArea->rom) {
			T1MemoryDeInit(CartridgeArea->rom);
		}

		if (CartridgeArea->dram) {
			T1MemoryDeInit(CartridgeArea->dram);
		}
		free(CartridgeArea);
	}
	CartridgeArea = NULL;
}
