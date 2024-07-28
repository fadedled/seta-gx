/*  Copyright 2005 Guillaume Duhamel
    Copyright 2005-2006 Theo Berkau

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

//#ifdef PSP  // see FIXME in T1MemoryInit()

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/stat.h>
#include <ctype.h>

#include "memory.h"
#include "coffelf.h"
#include "cs0.h"
#include "cs1.h"
#include "cs2.h"
#include "debug.h"
#include "error.h"
#include "sh2core.h"
#include "scsp.h"
#include "scu.h"
#include "smpc.h"
#include "vdp1.h"
#include "vdp2.h"
#include "yabause.h"
#include "yui.h"
#include "sh2/sh2.h"

#include "vidsoft.h"

//////////////////////////////////////////////////////////////////////////////

WriteFunc8 mem_write8_arr[0x100];
WriteFunc16 mem_write16_arr[0x100];
WriteFunc32 mem_write32_arr[0x100];

ReadFunc8 mem_read8_arr[0x100];
ReadFunc16 mem_read16_arr[0x100];
ReadFunc32 mem_read32_arr[0x100];


//Mask for repeating memorymap sections
u32 read_mask[0x800];
u32 write_mask[0x800];

//Will we need more?
#if 0
u32 read8_mask[0x800];
u32 read16_mask[0x800];
u32 read32_mask[0x800];
u32 write8_mask[0x800];
u32 write16_mask[0x800];
u32 write32_mask[0x800];
#endif

u8 *wram;
u8 *bios_rom;
u8 *bup_ram;

/* This flag is set to 1 on every write to backup RAM.  Ports can freely
 * check or clear this flag to determine when backup RAM has been written,
 * e.g. for implementing autosave of backup RAM. */
u8 bup_ram_written;

//////////////////////////////////////////////////////////////////////////////

u8 * T1MemoryInit(u32 size)
{
//#ifdef PSP  // FIXME: could be ported to all arches, but requires stdint.h
              //        for uintptr_t
#if defined(PSP) || defined(GEKKO)
   u8 * base;
   u8 * mem;

   if ((base = calloc((size * sizeof(u8)) + sizeof(u8 *) + 64, 1)) == NULL)
      return NULL;

   mem = base + sizeof(u8 *);
   mem = mem + (64 - ((uintptr_t) mem & 63));
   *(u8 **)(mem - sizeof(u8 *)) = base; // Save base pointer below memory block

   return mem;
#else
   return calloc(size, sizeof(u8));
#endif
}

//////////////////////////////////////////////////////////////////////////////

void T1MemoryDeInit(u8 * mem)
{
//#ifdef PSP
#if defined(PSP) || defined(GEKKO)
   if (mem)
      free(*(u8 **)(mem - sizeof(u8 *)));
#else
#ifdef GEKKO
   if (mem) //avoid crash
#endif
   free(mem);
#endif
}

//////////////////////////////////////////////////////////////////////////////

T3Memory * T3MemoryInit(u32 size)
{
   T3Memory * mem;

   if ((mem = (T3Memory *) calloc(1, sizeof(T3Memory))) == NULL)
      return NULL;

   if ((mem->base_mem = (u8 *) calloc(size, sizeof(u8))) == NULL)
      return NULL;

   mem->mem = mem->base_mem + size;

   return mem;
}

//////////////////////////////////////////////////////////////////////////////

void T3MemoryDeInit(T3Memory * mem)
{
#ifdef GEKKO
   if(mem->base_mem)
#endif
   free(mem->base_mem);
#ifdef GEKKO
   if(mem)
#endif
   free(mem);
}

//////////////////////////////////////////////////////////////////////////////

static u8 FASTCALL UnhandledMemoryReadByte(USED_IF_DEBUG u32 addr)
{
	char str_mem[64];
	sprintf(str_mem, "Unhandled byte read %08x %08x", addr);
	osd_MsgAdd(300, 300, 0xFFFF00FF, str_mem);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

static u16 FASTCALL UnhandledMemoryReadWord(USED_IF_DEBUG u32 addr)
{
	char str_mem[64];
	sprintf(str_mem, "Unhandled word read %08x %08x", addr);
	osd_MsgAdd(300, 300, 0xFFFF00FF, str_mem);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL UnhandledMemoryReadLong(USED_IF_DEBUG u32 addr)
{
	char str_mem[64];
	sprintf(str_mem, "Unhandled long read %08x %08x", addr);
	osd_MsgAdd(300, 300, 0xFFFF00FF, str_mem);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL UnhandledMemoryWriteByte(USED_IF_DEBUG u32 addr, UNUSED u8 val)
{
   LOG("Unhandled byte write %08X\n", (unsigned int)addr);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL UnhandledMemoryWriteWord(USED_IF_DEBUG u32 addr, UNUSED u16 val)
{
   LOG("Unhandled word write %08X\n", (unsigned int)addr);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL UnhandledMemoryWriteLong(USED_IF_DEBUG u32 addr, UNUSED u32 val)
{
	LOG("Unhandled long write %08X\n", (unsigned int)addr);
}

//////////////////////////////////////////////////////////////////////////////

u8  FASTCALL bios_Read8(u32 addr)
{
	return bios_rom[addr & (BIOS_SIZE - 1)];
}


u16 FASTCALL bios_Read16(u32 addr)
{
	return *((u16*) (bios_rom + (addr & (BIOS_SIZE - 1))));
}


u32 FASTCALL bios_Read32(u32 addr)
{
	return *((u32*) (bios_rom + (addr & (BIOS_SIZE - 1))));
}

//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL bup_Read8(u32 addr)
{
	return bup_ram[addr & (BACKUP_RAM_SIZE - 1)];
}

void FASTCALL bup_Write8(u32 addr, u8 val)
{
	bup_ram[(addr & (BACKUP_RAM_SIZE - 1)) | 1] = val;
	bup_ram_written = 1;
}

//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL wram_Read8(u32 addr)
{
	addr = (addr | ((addr >> 6) & HIGH_WRAM_SIZE)) & (WRAM_SIZE - 1);
	return wram[addr];
}

u16 FASTCALL wram_Read16(u32 addr)
{
	addr = (addr | ((addr >> 6) & HIGH_WRAM_SIZE)) & (WRAM_SIZE - 1);
	return *((u16*) (wram + addr));
}

u32 FASTCALL wram_Read32(u32 addr)
{
	addr = (addr | ((addr >> 6) & HIGH_WRAM_SIZE)) & (WRAM_SIZE - 1);
	return *((u32*) (wram + addr));
}

void FASTCALL wram_Write8(u32 addr, u8 val)
{
	addr = (addr | ((addr >> 6) & HIGH_WRAM_SIZE)) & (WRAM_SIZE - 1);
	wram[addr] = val;
}

void FASTCALL wram_Write16(u32 addr, u16 val)
{
	addr = (addr | ((addr >> 6) & HIGH_WRAM_SIZE)) & (WRAM_SIZE - 1);
	*((u16*) (wram + addr)) = val;
}

void FASTCALL wram_Write32(u32 addr, u32 val)
{
	addr = (addr | ((addr >> 6) & HIGH_WRAM_SIZE)) & (WRAM_SIZE - 1);
	*((u32*) (wram + addr)) = val;
}


//////////////////////////////////////////////////////////////////////////////


static u8 FASTCALL discr_Vdp2ScuRead8(u32 addr)
{
	if ((addr & 0xFFFF0000) == 0x05FE0000) {
		return ScuReadByte(addr);
	}
	return Vdp2ReadByte(addr);
}

static u16 FASTCALL discr_Vdp2ScuRead16(u32 addr)
{
	if ((addr & 0xFFFF0000) == 0x05FE0000) {
		return ScuReadWord(addr);
	}
	return Vdp2ReadWord(addr);
}

static u32 FASTCALL discr_Vdp2ScuRead32(u32 addr)
{
	if ((addr & 0xFFFF0000) == 0x05FE0000) {
		return ScuReadLong(addr);
	}
	return Vdp2ReadLong(addr);
}

static void FASTCALL discr_Vdp2ScuWrite8(u32 addr, u8 val)
{
	if ((addr & 0xFFFF0000) == 0x05FE0000) {
		ScuWriteByte(addr, val);
	} else {
		Vdp2WriteByte(addr, val);
	}
}

static void FASTCALL discr_Vdp2ScuWrite16(u32 addr, u16 val)
{
	if ((addr & 0xFFFF0000) == 0x05FE0000) {
		ScuWriteWord(addr, val);
	} else {
		Vdp2WriteWord(addr, val);
	}
}

static void FASTCALL discr_Vdp2ScuWrite32(u32 addr, u32 val)
{
	if ((addr & 0xFFFF0000) == 0x05FE0000) {
		ScuWriteLong(addr, val);
		return;
	} else {
		Vdp2WriteLong(addr, val);
	}
}

//////////////////////////////////////////////////////////////////////////////

//XXX: optimize this

#define clock_shift 	1

inline u32 getMemClock(u32 addr) {

  addr = addr & 0xDFFFFFFF;

  // CPU bus 1
  if (addr >= 0x000000 && addr < 0x00300000) { //LO and other
    return 22 >> clock_shift;
  }

  // A bus
  else if (addr >= 0x02000000 && addr < 0x05800000) {
    return 80 >> clock_shift;
  }

  // B bus
  else if (addr >= 0x05A00000 && addr < 0x05E80000) { //Wrong
    return 80 >> clock_shift;
  }
  else if (addr >= 0x05f00000 && addr < 0x06100000) { //HI WRAM
    return (16 >> clock_shift);
  }
  return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void mem_MaskFill(u32 start, u32 end, u32 mask, u32 r, u32 w)
{
	u32 r_mask = (mask | start) & (r ? 0xFFFFFFFF : 0x0);
	u32 w_mask = (mask | start) & (w ? 0xFFFFFFFF : 0x0);
	start >>= 16;
	end >>= 16;
	u32 i = start;
	for (; i < end; ++i) {
		read_mask[i] = r_mask;
		write_mask[i] = w_mask;
	}
	read_mask[i] = r_mask;
	write_mask[i] = w_mask;
}



static void FillMemoryArea(unsigned short start, unsigned short end,
                           ReadFunc8 r8func, ReadFunc16 r16func,
                           ReadFunc32 r32func, WriteFunc8 w8func,
                           WriteFunc16 w16func, WriteFunc32 w32func)
{
   int i;

   for (i=start; i < end; i++)
   {
      mem_read8_arr[i] = r8func;
      mem_read16_arr[i] = r16func;
      mem_read32_arr[i] = r32func;
      mem_write8_arr[i] = w8func;
      mem_write16_arr[i] = w16func;
      mem_write32_arr[i] = w32func;
   }
}

//////////////////////////////////////////////////////////////////////////////
//TODO: move this to mem_init and use a mem_Reset
void mem_allocate(void)
{
	//Should be in MEM1
	wram = (u8*) memalign(32, 0x200000);
	Vdp1FrameBuffer = (u8*) memalign(32, 0x80000);
	Vdp1Ram = (u8*) memalign(32, 0x80000);
	wii_vram = (u8*) memalign(32, 0x80000);	//Should be done in another place


	//Should be in MEM2
	SoundRam = (u8*) memalign(32, 0x80000);
	bios_rom = (u8*) memalign(32, 0x80000);
	display_fb = (u32*) memalign(32, 0x58000);	//Should be done in another place
	bup_ram = (u8*) memalign(32, 0x10000);
}


void mem_Init(void)
{
	memset(wram, 0x0, WRAM_SIZE);
	memset(bios_rom, 0x0, BIOS_SIZE);
	memset(bup_ram, 0x0, BACKUP_RAM_SIZE);
}

void mem_Deinit(void)
{

}

void MappedMemoryInit()
{
	//Number of pages: 2 + 4 +1 + 1 + 128 + 128 + 1 + 128 + 1 + 1 + 1 + 4
	//Now we would have 545 pages used
	//VM_Init(1024*1024, 256*1024);	//DFUK
	//VM_BATSet(0, general_ram, 0x00000000u, BL_ENC_4M);			//LOW_RAM_BASE
	//lightrec_mmap(MINIT_BASE, 0x01000000u, PAGE_SIZE);	//MINIT (4 bytes)
	//lightrec_mmap(SINIT_BASE, 0x01800000u, PAGE_SIZE);	//SINIT (4 bytes)
	//XXX: This fakes a non conected cartridge
	//lightrec_mmap(CS2_REG_BASE, 0x05800000u, PAGE_SIZE);	//CS2 (CD Regs) (64 bytes)
	//VM_BATSet(2, AUDIO_RAM_BASE, 0x05A00000u, BL_ENC_512K);	//AudioRAM	(512 KiB) BAT
	//lightrec_mmap(SCSP_REG_BASE, 0x05B00000u, PAGE_SIZE);	//SCSP Regs (dunno)
	//lightrec_mmap(Vdp1Ram, 0x05C00000u, VDP1_RAM_SIZE);	//VDP1 VRAM (512 KiB)
	//lightrec_mmap(Vdp1FrameBuffer, 0x05C80000u, VDP1_FB_SIZE);	//VDP1 FrameBuffer (512 KiB)
	//lightrec_mmap(VDP1_REG_BASE, 0x05D00000u, PAGE_SIZE);	//VDP1 Regs (24 bytes)
	//lightrec_mmap(memory, 0x00000000u, 0x200000);	//VDP2 VRAM (512 KiB)
	//lightrec_mmap(VDP2_CRAM_BASE, 0x05F00000u, PAGE_SIZE);	//VDP2 CRAM (4 KiB)
	//lightrec_mmap(VDP2_REG_BASE, 0x05F80000u, PAGE_SIZE);	//VDP2 Regs (288 bytes)
	//lightrec_mmap(SCU_REG_BASE, 0x05FE0000u, PAGE_SIZE);	//SCU Regs (208 bytes)
	//VM_BATSet(1, HIGH_RAM_BASE, 0x06000000u, BL_ENC_1M);	//HIGH_RAM_BASE


	mem_MaskFill(0x00000000, 0x07FFFFFF, 0, 0, 0); // Init all

	mem_MaskFill(0x00000000, 0x000FFFFF, 0x7FFFF, 1, 1); //BIOS Rom
	mem_MaskFill(0x00100000, 0x0017FFFF,    0x7F, 1, 1); //SMPC Regs
	mem_MaskFill(0x00180000, 0x001FFFFF,  0xFFFF, 1, 1); //BUP RAM
	mem_MaskFill(0x00200000, 0x002FFFFF, 0xFFFFF, 1, 1); //Low RAM
	mem_MaskFill(0x01000000, 0x017FFFFF,     0x0, 1, 1); //MINIT
	mem_MaskFill(0x01800000, 0x01FFFFFF,     0x0, 1, 1); //SINIT
	mem_MaskFill(0x02000000, 0x03FFFFFF,     0x0, 1, 1); //CS0 (can change)
	mem_MaskFill(0x04000000, 0x04FFFFFF,     0x0, 1, 1); //CS1 (can change)
	mem_MaskFill(0x05800000, 0x058FFFFF,    0x3F, 1, 1); //CS2
	mem_MaskFill(0x05A00000, 0x05AFFFFF, 0x7FFFF, 1, 1); //AUDIO RAM (can change)
	mem_MaskFill(0x05B00000, 0x05BFFFFF,   0xFFF, 1, 1); //SCSP Regs
	mem_MaskFill(0x05C00000, 0x05C7FFFF, 0x7FFFF, 1, 1); //VDP1 VRAM
	mem_MaskFill(0x05C80000, 0x05CFFFFF, 0x3FFFF, 1, 1); //VDP1 FB
	mem_MaskFill(0x05D00000, 0x05D7FFFF,    0x1F, 1, 1); //VDP1 Regs
	mem_MaskFill(0x05E00000, 0x05EFFFFF, 0x7FFFF, 1, 1); //VDP2 VRAM
	mem_MaskFill(0x05F00000, 0x05F7FFFF,   0xFFF, 1, 1); //VDP2 CRAM
	mem_MaskFill(0x05F80000, 0x05FBFFFF,   0x1FF, 1, 1); //VDP2 Regs
	mem_MaskFill(0x05FE0000, 0x05FEFFFF,    0xFF, 1, 1); //SCU Regs
	mem_MaskFill(0x06000000, 0x07FFFFFF, 0xFFFFF, 1, 1); //High RAM

   // Initialize everyting to unhandled to begin with
   FillMemoryArea(0x00, 0xFF, &UnhandledMemoryReadByte,
                                &UnhandledMemoryReadWord,
                                &UnhandledMemoryReadLong,
                                &UnhandledMemoryWriteByte,
                                &UnhandledMemoryWriteWord,
                                &UnhandledMemoryWriteLong);

   // Fill the rest
   //Bios Rom
   FillMemoryArea(0x00, 0x02, &bios_Read8,
                                &bios_Read16,
                                &bios_Read32,
                                &UnhandledMemoryWriteByte,		//XXX: This should be read only
                                &UnhandledMemoryWriteWord,		//XXX: This should be read only
                                &UnhandledMemoryWriteLong);		//XXX: This should be read only
   //SMPC Regs
   FillMemoryArea(0x02, 0x03, &SmpcReadByte,
                                &SmpcReadWord,
                                &SmpcReadLong,
                                &SmpcWriteByte,
                                &SmpcWriteWord,
                                &SmpcWriteLong);
   //Backup Ram
   FillMemoryArea(0x03, 0x04, &bup_Read8,
                                &UnhandledMemoryReadWord,
                                &UnhandledMemoryReadLong,
                                &bup_Write8,
                                &UnhandledMemoryWriteWord,
                                &UnhandledMemoryWriteLong);
   //Work RAM Low
   FillMemoryArea(0x04, 0x06, &wram_Read8,
                                &wram_Read16,
                                &wram_Read32,
                                &wram_Write8,
                                &wram_Write16,
                                &wram_Write32);
   //Master SH2 Init
   FillMemoryArea(0x20, 0x30, &UnhandledMemoryReadByte,
                                &UnhandledMemoryReadWord,
                                &UnhandledMemoryReadLong,
                                &UnhandledMemoryWriteByte,
                                &SSH2InputCaptureWriteWord,
                                &UnhandledMemoryWriteLong);
   //Slave SH2 Init
   FillMemoryArea(0x30, 0x40, &UnhandledMemoryReadByte,
                                &UnhandledMemoryReadWord,
                                &UnhandledMemoryReadLong,
                                &UnhandledMemoryWriteByte,
                                &MSH2InputCaptureWriteWord,
                                &UnhandledMemoryWriteLong);
   //CS0
   FillMemoryArea(0x40, 0x80, CartridgeArea->Cs0ReadByte,
								CartridgeArea->Cs0ReadWord,
								CartridgeArea->Cs0ReadLong,
								CartridgeArea->Cs0WriteByte,
								CartridgeArea->Cs0WriteWord,
								CartridgeArea->Cs0WriteLong);
   //CS1
	FillMemoryArea(0x80, 0xA0, &Cs1ReadByte,
								&Cs1ReadWord,
								&Cs1ReadLong,
								&Cs1WriteByte,
								&Cs1WriteWord,
								&Cs1WriteLong);
   //CS2 (CD-ROM Regs)
   FillMemoryArea(0xB0, 0xB2, &Cs2ReadByte,
                                &Cs2ReadWord,
                                &Cs2ReadLong,
                                &Cs2WriteByte,
                                &Cs2WriteWord,
                                &Cs2WriteLong);
#ifndef SCSP_PLUGIN
   FillMemoryArea(0xB4, 0xB6, &SoundRamReadByte,
                                &SoundRamReadWord,
                                &SoundRamReadLong,
                                &SoundRamWriteByte,
                                &SoundRamWriteWord,
                                &SoundRamWriteLong);
#else
   FillMemoryArea(0xB4, 0xB6, SCSCore->SoundRamReadByte,
                                SCSCore->SoundRamReadWord,
                                SCSCore->SoundRamReadLong,
                                SCSCore->SoundRamWriteByte,
                                SCSCore->SoundRamWriteWord,
                                SCSCore->SoundRamWriteLong);
#endif
#ifndef SCSP_PLUGIN
   FillMemoryArea(0xB6, 0xB8, &scsp_r_b,
                                &scsp_r_w,
                                &scsp_r_d,
                                &scsp_w_b,
                                &scsp_w_w,
                                &scsp_w_d);
#else
   FillMemoryArea(0xB6, 0xB8, SCSCore->ReadByte,
                                SCSCore->ReadWord,
                                SCSCore->ReadLong,
                                SCSCore->WriteByte,
                                SCSCore->WriteWord,
                                SCSCore->WriteLong);
#endif
	//VDP1 VRAM
   FillMemoryArea(0xB8, 0xB9, &Vdp1RamReadByte,
                                &Vdp1RamReadWord,
                                &Vdp1RamReadLong,
                                &Vdp1RamWriteByte,
                                &Vdp1RamWriteWord,
                                &Vdp1RamWriteLong);
   //VDP1 Framebuffer
   FillMemoryArea(0xB9, 0xBA, &Vdp1FrameBufferReadByte,
                                &Vdp1FrameBufferReadWord,
                                &Vdp1FrameBufferReadLong,
                                &Vdp1FrameBufferWriteByte,
                                &Vdp1FrameBufferWriteWord,
                                &Vdp1FrameBufferWriteLong);
   FillMemoryArea(0xBA, 0xBC, &Vdp1ReadByte,
                                &Vdp1ReadWord,
                                &Vdp1ReadLong,
                                &Vdp1WriteByte,
                                &Vdp1WriteWord,
                                &Vdp1WriteLong);
   FillMemoryArea(0xBC, 0xBE, &Vdp2RamReadByte,
                                &Vdp2RamReadWord,
                                &Vdp2RamReadLong,
                                &Vdp2RamWriteByte,
                                &Vdp2RamWriteWord,
                                &Vdp2RamWriteLong);
   FillMemoryArea(0xBE, 0xBF, &Vdp2ColorRamReadByte,
                                &Vdp2ColorRamReadWord,
                                &Vdp2ColorRamReadLong,
                                &Vdp2ColorRamWriteByte,
                                &Vdp2ColorRamWriteWord,
                                &Vdp2ColorRamWriteLong);
   FillMemoryArea(0xBF, 0xC0, &discr_Vdp2ScuRead8,
                                &discr_Vdp2ScuRead16,
                                &discr_Vdp2ScuRead32,
                                &discr_Vdp2ScuWrite8,
                                &discr_Vdp2ScuWrite16,
                                &discr_Vdp2ScuWrite32);
    FillMemoryArea(0xC0, 0xFF, &wram_Read8,
                                &wram_Read16,
                                &wram_Read32,
                                &wram_Write8,
                                &wram_Write16,
                                &wram_Write32);
}


//////////////////////////////////////////////////////////////////////////////


//XXX: Beware the commented lines, they could be important
//XXX: Very important, must mask the sectors repeating to have an acurate real memory map

u8 FASTCALL mem_Read8(u32 addr)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			addr &= 0x0FFFFFFF;
			return mem_read8_arr[(addr >> 19) & 0xFF](addr);
		case 0x3:	//Adress Array, read/write space
			return 0;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			return DataArrayReadByte(addr);
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				return OnchipReadByte(addr & 0x1FF);
			//}
	}
	return UnhandledMemoryReadByte(addr);
}


u16 FASTCALL mem_Read16(u32 addr)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			addr &= 0x0FFFFFFF;
			return mem_read16_arr[(addr >> 19) & 0xFF](addr);
		case 0x3:	//Adress Array, read/write space
			return 0;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			return DataArrayReadWord(addr);
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				return OnchipReadWord(addr & 0x1FF);
			//}
	}
	return UnhandledMemoryReadWord(addr);
}


u32 FASTCALL mem_Read32(u32 addr)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			addr &= 0x0FFFFFFF;
			return mem_read32_arr[(addr >> 19) & 0xFF](addr);
		case 0x3:	//Adress Array, read/write space
			return AddressArrayReadLong(addr);
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			return DataArrayReadLong(addr);
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				return OnchipReadLong(addr & 0x1FF);
			//}
	}
	return UnhandledMemoryReadLong(addr);
}


void FASTCALL mem_Write8(u32 addr, u8 val)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			addr &= 0x0FFFFFFF;
			mem_write8_arr[(addr >> 19) & 0xFF](addr, val); return;
		case 0x3:	//Adress Array, read/write space
			break;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			DataArrayWriteByte(addr, val); return;
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				OnchipWriteByte(addr & 0x1FF, val);
				return;
			//}
	}
	UnhandledMemoryWriteByte(addr, val);
}


void FASTCALL mem_Write16(u32 addr, u16 val)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			addr &= 0x0FFFFFFF;
			mem_write16_arr[(addr >> 19) & 0xFF](addr, val); return;
		case 0x3:	//Adress Array, read/write space
			break;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			DataArrayWriteWord(addr, val); return;
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				OnchipWriteWord(addr & 0x1FF, val);
				return;
			//}
	}
	UnhandledMemoryWriteWord(addr, val);
}


void FASTCALL mem_Write32(u32 addr, u32 val)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//Dunno
			addr &= 0x0FFFFFFF;
			mem_write32_arr[(addr >> 19) & 0xFF](addr, val); return;
		case 0x3:	//Adress Array, read/write space
			AddressArrayWriteLong(addr, val); return;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			DataArrayWriteLong(addr, val); return;
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				OnchipWriteLong(addr & 0x1FF, val);
				return;
			//}
	}
	UnhandledMemoryWriteLong(addr, val);
}


//////////////////////////////////////////////////////////////////////////////

int LoadBios(const char *filename)
{
	return T123Load(bios_rom, BIOS_SIZE, 2, filename);
}

//////////////////////////////////////////////////////////////////////////////

int LoadBackupRam(const char *filename)
{
	return T123Load(bup_ram, BACKUP_RAM_SIZE, 1, filename);
}

//////////////////////////////////////////////////////////////////////////////

void FormatBackupRam(void *mem, u32 size)
{
   int i, i2;
   u32 i3;
   u8 header[32] = {
      0xFF, 'B', 0xFF, 'a', 0xFF, 'c', 0xFF, 'k',
      0xFF, 'U', 0xFF, 'p', 0xFF, 'R', 0xFF, 'a',
      0xFF, 'm', 0xFF, ' ', 0xFF, 'F', 0xFF, 'o',
      0xFF, 'r', 0xFF, 'm', 0xFF, 'a', 0xFF, 't'
   };

   // Fill in header
   for(i2 = 0; i2 < 4; i2++)
      for(i = 0; i < 32; i++)
         T1WriteByte(mem, (i2 * 32) + i, header[i]);

   // Clear the rest
   for(i3 = 0x80; i3 < size; i3+=2)
   {
      T1WriteByte(mem, i3, 0xFF);
      T1WriteByte(mem, i3+1, 0x00);
   }
}


