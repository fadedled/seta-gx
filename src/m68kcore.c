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

#include "m68kcore.h"
#include "memory.h"
#include "musashi/m68k.h"
#include "musashi/m68kcpu.h"

extern u8 * SoundRam;

//TODO: this should not be done this way.. implement directly
M68K_READ *mus_read8;
M68K_READ *mus_read16;
M68K_WRITE *mus_write8;
M68K_WRITE *mus_write16;


int musashi_Init(void)
{
	m68k_init();
	m68k_set_reset_instr_callback(m68k_pulse_reset);
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	return 0;
}

void musashi_DeInit(void)
{
	//Does nothing
}

void musashi_Reset(void)
{
	m68k_pulse_reset();
}

s32 musashi_Exec(s32 cycles)
{
	return m68k_execute(cycles);
}

void musashi_Sync(void)
{
	//Does nothing
}


u32 musashi_GetDReg(u32 n)
{
	return m68k_get_reg(NULL, M68K_REG_D0 + n);
}

u32 musashi_GetAReg(u32 n)
{
	return m68k_get_reg(NULL, M68K_REG_A0 + n);
}

u32 musashi_GetPC(void)
{
	return m68k_get_reg(NULL, M68K_REG_PC);
}

u32 musashi_GetSR(void)
{
	return m68k_get_reg(NULL, M68K_REG_SR);
}

u32 musashi_GetUSP(void)
{
	return m68k_get_reg(NULL, M68K_REG_USP);
}

u32 musashi_GetMSP(void)
{
	return m68k_get_reg(NULL, M68K_REG_MSP);
}

void musashi_SetDReg(u32 n, u32 val)
{
	m68k_set_reg(M68K_REG_D0 + n, val);
}

void musashi_SetAReg(u32 n, u32 val)
{
	m68k_set_reg(M68K_REG_A0 + n, val);
}

void musashi_SetPC(u32 val)
{
	m68k_set_reg(M68K_REG_PC, val);
}

void musashi_SetSR(u32 val)
{
	m68k_set_reg(M68K_REG_SR, val);
}

void musashi_SetUSP(u32 val)
{
	m68k_set_reg(M68K_REG_USP, val);
}

void musashi_SetMSP(u32 val)
{
	m68k_set_reg(M68K_REG_MSP, val);
}

void musashi_SetFetch(u32 low_adr, u32 high_adr, pointer fetch_addr)
{
	//Does nothing
}

void FASTCALL musashi_SetIRQ(s32 level)
{
	if (level) {
		m68k_set_irq(level);
	}
}

void FASTCALL musashi_WriteNotify(u32 address, u32 size)
{
	//Does nothing
}

void musashi_SetReadB(M68K_READ *func)
{
	mus_read8 = func;
}

void musashi_SetReadW(M68K_READ *func)
{
	mus_read16 = func;
}

void musashi_SetWriteB(M68K_WRITE *func)
{
	mus_write8 = func;
}

void musashi_SetWriteW(M68K_WRITE *func)
{
	mus_write16 = func;
}

//Implementation for musashi read/write functions
u32 m68k_read_memory_8(u32 address)
{
	return mus_read8(address);
}

u32 m68k_read_memory_16(u32 address)
{
	return mus_read16(address);
}

u32 m68k_read_memory_32(u32 address)
{
	return ((((u32)mus_read16(address)) << 16) | mus_read16(address + 2));
}

void m68k_write_memory_8(u32 address, u32 value)
{
	mus_write8(address, value);
}

void m68k_write_memory_16(u32 address, u32 value)
{
	mus_write16(address, value);
}

void m68k_write_memory_32(u32 address, u32 value)
{
	mus_write16(address,     value >> 16);
	mus_write16(address + 2, value & 0xFFFFu);
}
