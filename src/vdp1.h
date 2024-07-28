/*  Copyright 2003-2005 Guillaume Duhamel
    Copyright 2004-2006 Theo Berkau

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

#ifndef VDP1_H
#define VDP1_H

#include "memory.h"

//#define VDP1R_TVMR


typedef struct {
   u16 TVMR;
   u16 FBCR;
   u16 PTMR;
   u16 EWDR;
   u16 EWLR;
   u16 EWRR;
   u16 ENDR;
   u16 EDSR;
   u16 LOPR;
   u16 COPR;
   u16 MODR;

   u32 addr;
   int disptoggle_dont_use_me; // not used anymore, see Vdp1External_struct

   s16 localX;
   s16 localY;

   u16 systemclipX1;
   u16 systemclipY1;
   u16 systemclipX2;
   u16 systemclipY2;

   u16 userclipX1;
   u16 userclipY1;
   u16 userclipX2;
   u16 userclipY2;
} Vdp1;


u8 FASTCALL	Vdp1RamReadByte(u32);
u16 FASTCALL	Vdp1RamReadWord(u32);
u32 FASTCALL	Vdp1RamReadLong(u32);
void FASTCALL	Vdp1RamWriteByte(u32, u8);
void FASTCALL	Vdp1RamWriteWord(u32, u16);
void FASTCALL	Vdp1RamWriteLong(u32, u32);
u8 FASTCALL Vdp1FrameBufferReadByte(u32);
u16 FASTCALL Vdp1FrameBufferReadWord(u32);
u32 FASTCALL Vdp1FrameBufferReadLong(u32);
void FASTCALL Vdp1FrameBufferWriteByte(u32, u8);
void FASTCALL Vdp1FrameBufferWriteWord(u32, u16);
void FASTCALL Vdp1FrameBufferWriteLong(u32, u32);

void Vdp1DrawCommands(u8 * ram, Vdp1 * regs, u8* back_framebuffer);
void Vdp1FakeDrawCommands(u8 * ram, Vdp1 * regs);

#define VDP1_RAM_SIZE		0x80000
#define VDP1_FB_SIZE		0x80000
#define VDP1_REGS_SIZE		0x1000		//Most bytes go unused, must be min pagesize


extern u8 *Vdp1Ram;
extern u8 *vdp1_ram_al[4];
extern u8 *Vdp1FrameBuffer;
extern Vdp1 * Vdp1Regs;

enum VDP1STATUS {
  VDP1_STATUS_IDLE = 0,
  VDP1_STATUS_RUNNING
};

// struct for Vdp1 part that shouldn't be saved
typedef struct {
   int disptoggle;
   int manualerase;
   int manualchange;
   int vbalnk_erase;
   int frame_change_plot;
   int swap_frame_buffer;
   int current_frame;
   int status;
} Vdp1External_struct;

extern Vdp1External_struct Vdp1External;

typedef struct
{
   u16 CMDCTRL;
   u16 CMDLINK;
   u16 CMDPMOD;
   u16 CMDCOLR;
   u16 CMDSRCA;
   u16 CMDSIZE;
   s16 CMDXA;
   s16 CMDYA;
   s16 CMDXB;
   s16 CMDYB;
   s16 CMDXC;
   s16 CMDYC;
   s16 CMDXD;
   s16 CMDYD;
   u16 CMDGRDA;
} vdp1cmd_struct;

int Vdp1Init(void);
void Vdp1DeInit(void);
int VideoInit(int coreid);
void VideoDeInit(void);
void Vdp1Reset(void);

u8 FASTCALL	Vdp1ReadByte(u32);
u16 FASTCALL	Vdp1ReadWord(u32);
u32 FASTCALL	Vdp1ReadLong(u32);
void FASTCALL	Vdp1WriteByte(u32, u8);
void FASTCALL	Vdp1WriteWord(u32, u16);
void FASTCALL	Vdp1WriteLong(u32, u32);

void Vdp1Draw(void);
void Vdp1NoDraw(void);
void FASTCALL Vdp1ReadCommand(vdp1cmd_struct *cmd, u32 addr);
void ToggleVDP1(void);

#endif
