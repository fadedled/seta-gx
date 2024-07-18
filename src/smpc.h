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

#ifndef __SMPC_H__
#define __SMPC_H__

#include "memory.h"

#define REGION_AUTODETECT               0
#define REGION_JAPAN                    1
#define REGION_ASIANTSC                 2
#define REGION_NORTHAMERICA             4
#define REGION_CENTRALSOUTHAMERICANTSC  5
#define REGION_KOREA                    6
#define REGION_ASIAPAL                  10
#define REGION_EUROPE                   12
#define REGION_CENTRALSOUTHAMERICAPAL   13



#define SMPC_REG_IREG(x)	(*(SMPC_REG_BASE + 0x01 + ((x) << 1)))
#define SMPC_REG_COMREG		(*(SMPC_REG_BASE + 0x1F))
#define SMPC_REG_OREG(x)	(*(SMPC_REG_BASE + 0x21 + ((x) << 1)))
#define SMPC_REG_SR			(*(SMPC_REG_BASE + 0x61))
#define SMPC_REG_SF			(*(SMPC_REG_BASE + 0x63))
#define SMPC_REG_PDR1		(*(SMPC_REG_BASE + 0x75))
#define SMPC_REG_PDR2		(*(SMPC_REG_BASE + 0x77))
#define SMPC_REG_DDR1		(*(SMPC_REG_BASE + 0x79))
#define SMPC_REG_DDR2		(*(SMPC_REG_BASE + 0x7B))
#define SMPC_REG_IOSEL		(*(SMPC_REG_BASE + 0x7D))
#define SMPC_REG_EXLE		(*(SMPC_REG_BASE + 0x7F))


typedef struct
{
	int offset;
	int size;
	u8 data[256];
} PortData_struct;

typedef struct {
	u8 dotsel; // 0 -> 320 | 1 -> 352
	u8 mshnmi;
	u8 sndres;
	u8 cdres;
	u8 sysres;
	u8 resb;
	u8 ste;
	u8 resd;
	u8 intback;
	u8 intbackIreg0;
	u8 firstPeri;
	u8 regionid;
	u8 regionsetting;
	u8 SMEM[4];
	s32 timing;
	//PortData_struct port1;
	//PortData_struct port2;
	u8 clocksync;
	u32 basetime;  // Safe until early 2106.  After that you're on your own (:
} SmpcInternal;

extern SmpcInternal * SmpcInternalVars;

int SmpcInit(u8 regionid, int clocksync, u32 basetime);
void SmpcDeInit(void);
void SmpcRecheckRegion(void);
void SmpcReset(void);
void SmpcResetButton(void);
void SmpcExec(s32 t);
void SmpcINTBACKEnd(void);
void SmpcCKCHG(u32 clk_type);

u8 FASTCALL		SmpcReadByte(u32);
u16 FASTCALL	SmpcReadWord(u32);
u32 FASTCALL	SmpcReadLong(u32);
void FASTCALL	SmpcWriteByte(u32, u8);
void FASTCALL	SmpcWriteWord(u32, u16);
void FASTCALL	SmpcWriteLong(u32, u32);

int SmpcSetClockSync(int clocksync, u32 basetime);

#endif /*__SMPC_H__*/
