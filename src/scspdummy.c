/*  Copyright 2004 Stephane Dallongeville
    Copyright 2004-2007 Theo Berkau
    Copyright 2006 Guillaume Duhamel

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

#include "scsp.h"

//////////////////////////////////////////////////////////////////////////////
// Dummy SCSP Interface
//////////////////////////////////////////////////////////////////////////////

static int SCSDummyInit(int coreid, void (*interrupt_handler)(void));
static void SCSDummyDeInit(void);
static void SCSDummyReset(void);
static int SCSDummyChangeVideoFormat(int type);
static void SCSDummyExec(int decilines);
static void SCSDummyMuteAudio(int flags);
static void SCSDummyUnMuteAudio(int flags);
static void SCSDummySetVolume(int volume);
static void SCSDummyM68KStart(void);
static void SCSDummyM68KStop(void);
static u8 FASTCALL SCSDummySoundRamReadByte(u32 addr);
static void FASTCALL SCSDummySoundRamWriteByte(u32 addr, u8 val);
static u16 FASTCALL SCSDummySoundRamReadWord(u32 addr);
static void FASTCALL SCSDummySoundRamWriteWord(u32 addr, u16 val);
static u32 FASTCALL SCSDummySoundRamReadLong(u32 addr);
static void FASTCALL SCSDummySoundRamWriteLong(u32 addr, u32 val);
static void SCSDummyScspReceiveCDDA(const u8 *sector);
static int SCSDummySoundSaveState(FILE *fp);
static int SCSDummySoundLoadState(FILE *fp, int version, int size);
static u32 FASTCALL SCSDummyM68KReadWord(const u32 adr);
static u8 FASTCALL SCSDummyReadByte(u32 address);
static u16 FASTCALL SCSDummyReadWord(u32 address);
static u32 FASTCALL SCSDummyReadLong(u32 address);
static void FASTCALL SCSDummyWriteByte(u32 address, u8 data);
static void FASTCALL SCSDummyWriteWord(u32 address, u16 data);
static void FASTCALL SCSDummyWriteLong(u32 address, u32 data);
static M68KBreakpointInfo *SCSDummyM68KGetBreakpointList(void);

SCSPInterface_struct SCSDummy = {
SCSCORE_DUMMY,
"Dummy SCSP Interface",
SCSDummyInit,
SCSDummyDeInit,
SCSDummyReset,
SCSDummyChangeVideoFormat,
SCSDummyExec,
SCSDummyMuteAudio,
SCSDummyUnMuteAudio,
SCSDummySetVolume,
SCSDummyM68KStart,
SCSDummyM68KStop,
SCSDummySoundRamReadByte,
SCSDummySoundRamWriteByte,
SCSDummySoundRamReadWord,
SCSDummySoundRamWriteWord,
SCSDummySoundRamReadLong,
SCSDummySoundRamWriteLong,
SCSDummyScspReceiveCDDA,
SCSDummySoundSaveState,
SCSDummySoundLoadState,
SCSDummyM68KReadWord,
SCSDummyReadByte,
SCSDummyReadWord,
SCSDummyReadLong,
SCSDummyWriteByte,
SCSDummyWriteWord,
SCSDummyWriteLong,
SCSDummyM68KGetBreakpointList
};

//////////////////////////////////////////////////////////////////////////////

static int SCSDummyInit(int coreid, void (*interrupt_handler)(void))
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void SCSDummyDeInit(void)
{
}

//////////////////////////////////////////////////////////////////////////////

static void SCSDummyReset(void)
{
}

//////////////////////////////////////////////////////////////////////////////

static int SCSDummyChangeVideoFormat(int type)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void SCSDummyExec(int decilines)
{
}

//////////////////////////////////////////////////////////////////////////////

static void SCSDummyMuteAudio(int flags)
{
}

//////////////////////////////////////////////////////////////////////////////

static void SCSDummyUnMuteAudio(int flags)
{
}

//////////////////////////////////////////////////////////////////////////////

static void SCSDummySetVolume(UNUSED int volume)
{
}

//////////////////////////////////////////////////////////////////////////////

static void SCSDummyM68KStart(void)
{
}

//////////////////////////////////////////////////////////////////////////////

static void SCSDummyM68KStop(void)
{
}

//////////////////////////////////////////////////////////////////////////////

static u8 FASTCALL SCSDummySoundRamReadByte(u32 addr)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SCSDummySoundRamWriteByte(u32 addr, u8 val)
{
}

//////////////////////////////////////////////////////////////////////////////

static u16 FASTCALL SCSDummySoundRamReadWord(u32 addr)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SCSDummySoundRamWriteWord(u32 addr, u16 val)
{
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL SCSDummySoundRamReadLong(u32 addr)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SCSDummySoundRamWriteLong(u32 addr, u32 val)
{
}

//////////////////////////////////////////////////////////////////////////////

static void SCSDummyScspReceiveCDDA(const u8 *sector)
{
}

//////////////////////////////////////////////////////////////////////////////

static int SCSDummySoundSaveState(FILE *fp)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int SCSDummySoundLoadState(FILE *fp, int version, int size)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL SCSDummyM68KReadWord(const u32 adr)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static u8 FASTCALL SCSDummyReadByte(u32 address)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static u16 FASTCALL SCSDummyReadWord(u32 address)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL SCSDummyReadLong(u32 address)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SCSDummyWriteByte(u32 address, u8 data)
{
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SCSDummyWriteWord(u32 address, u16 data)
{
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SCSDummyWriteLong(u32 address, u32 data)
{
}

//////////////////////////////////////////////////////////////////////////////

static M68KBreakpointInfo *SCSDummyM68KGetBreakpointList(void)
{
   M68KBreakpointInfo *m68k_breakpoint_dummy;

   return m68k_breakpoint_dummy;
}

//////////////////////////////////////////////////////////////////////////////
