/*  Copyright 2007 Guillaume Duhamel

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

#ifndef M68KCORE_H
#define M68KCORE_H

#include "core.h"

#define M68KCORE_DEFAULT -1
#define M68KCORE_DUMMY    0
#define M68KCORE_C68K     1
#define M68KCORE_Q68      2

typedef u32 FASTCALL M68K_READ(const u32 adr);
typedef void FASTCALL M68K_WRITE(const u32 adr, u32 data);

typedef struct {
	int id;
	const char *Name;

	int (*Init)(void);
	void (*DeInit)(void);
	void (*Reset)(void);

	s32 FASTCALL (*Exec)(s32 cycle);
        void (*Sync)(void);

	u32 (*GetDReg)(u32 num);
	u32 (*GetAReg)(u32 num);
	u32 (*GetPC)(void);
	u32 (*GetSR)(void);
	u32 (*GetUSP)(void);
	u32 (*GetMSP)(void);

	void (*SetDReg)(u32 num, u32 val);
	void (*SetAReg)(u32 num, u32 val);
	void (*SetPC)(u32 val);
	void (*SetSR)(u32 val);
	void (*SetUSP)(u32 val);
	void (*SetMSP)(u32 val);

	void (*SetFetch)(u32 low_adr, u32 high_adr, pointer fetch_adr);
	void FASTCALL (*SetIRQ)(s32 level);
	void FASTCALL (*WriteNotify)(u32 address, u32 size);

	void (*SetReadB)(M68K_READ *Func);
	void (*SetReadW)(M68K_READ *Func);
	void (*SetWriteB)(M68K_WRITE *Func);
	void (*SetWriteW)(M68K_WRITE *Func);
} M68K_struct;


int musashi_Init(void);
void musashi_DeInit(void);
void musashi_Reset(void);
s32 musashi_Exec(s32 cycles);
void musashi_Sync(void);
u32 musashi_GetDReg(u32 n);
u32 musashi_GetAReg(u32 n);
u32 musashi_GetPC(void);
u32 musashi_GetSR(void);
u32 musashi_GetUSP(void);
u32 musashi_GetMSP(void);
void musashi_SetDReg(u32 n, u32 val);
void musashi_SetAReg(u32 n, u32 val);
void musashi_SetPC(u32 val);
void musashi_SetSR(u32 val);
void musashi_SetUSP(u32 val);
void musashi_SetMSP(u32 val);
void musashi_SetFetch(u32 low_adr, u32 high_adr, pointer fetch_addr);
void FASTCALL musashi_SetIRQ(s32 level);
void FASTCALL musashi_WriteNotify(u32 address, u32 size);
void musashi_SetReadB(M68K_READ *func);
void musashi_SetReadW(M68K_READ *func);
void musashi_SetWriteB(M68K_WRITE *func);
void musashi_SetWriteW(M68K_WRITE *func);
void musashi_SaveState(FILE *fp);
void musashi_LoadState(FILE *fp);
//Implementation for musashi read/write functions
u32 m68k_read_memory_8(u32 address);
u32 m68k_read_memory_16(u32 address);
u32 m68k_read_memory_32(u32 address);
void m68k_write_memory_8(u32 address, u32 value);
void m68k_write_memory_16(u32 address, u32 value);
void m68k_write_memory_32(u32 address, u32 value);

extern M68K_struct * M68K;

int M68KInit(int coreid);

extern M68K_struct M68KDummy;
extern M68K_struct M68KMusashi;

#endif
