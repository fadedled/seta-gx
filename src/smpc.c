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

#include <stdlib.h>
#include <time.h>
#include "smpc.h"
#include <assert.h>
#include "cs2.h"
#include "debug.h"
#include "peripheral.h"
#include "scsp.h"
#include "scu.h"
#include "sh2core.h"
#include "vdp1.h"
#include "vdp2.h"
#include "yabause.h"


SmpcInternal * SmpcInternalVars;
int intback_wait_for_line = 0;
#if 0
u8 bustmp = 0;
#endif

#ifdef GEKKO
int smpcperipheraltiming = 1000;
int smpcothertiming = 1050;
#endif

//////////////////////////////////////////////////////////////////////////////

int SmpcInit(u8 regionid, int clocksync, u32 basetime)
{
   if ((SmpcInternalVars = (SmpcInternal *) calloc(1, sizeof(SmpcInternal))) == NULL)
      return -1;

   SmpcInternalVars->regionsetting = regionid;
   SmpcInternalVars->regionid = regionid;
   SmpcInternalVars->clocksync = clocksync;
   SmpcInternalVars->basetime = basetime ? basetime : time(NULL);

   return 0;
}


int SmpcSetClockSync(int clocksync, u32 basetime) {
	//XXX: this check is unnecesary
	if (SmpcInternalVars == NULL) {
		return -1;
	}
	SmpcInternalVars->clocksync = clocksync;
	SmpcInternalVars->basetime = basetime ? basetime : time(NULL);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SmpcDeInit(void) {

   if (SmpcInternalVars)
      free(SmpcInternalVars);
   SmpcInternalVars = NULL;
}

//////////////////////////////////////////////////////////////////////////////

void SmpcRecheckRegion(void) {
   if (SmpcInternalVars == NULL)
      return;

   if (SmpcInternalVars->regionsetting == REGION_AUTODETECT)
   {
      // Time to autodetect the region using the cd block
      SmpcInternalVars->regionid = Cs2GetRegionID();

      // Since we couldn't detect the region from the CD, let's assume
      // it's japanese
      if (SmpcInternalVars->regionid == 0)
         SmpcInternalVars->regionid = 1;
   }
   else
      Cs2GetIP(0);
}

//////////////////////////////////////////////////////////////////////////////

void SmpcReset(void) {
	memset(SMPC_REG_BASE, 0, 0x80);
   memset((void *)SmpcInternalVars->SMEM, 0, 4);

   SmpcRecheckRegion();

   SmpcInternalVars->dotsel = 0;
   SmpcInternalVars->mshnmi = 0;
   SmpcInternalVars->sysres = 0;
   SmpcInternalVars->sndres = 0;
   SmpcInternalVars->cdres = 0;
   SmpcInternalVars->resd = 1;
   SmpcInternalVars->ste = 0;
   SmpcInternalVars->resb = 0;

   SmpcInternalVars->intback=0;
   SmpcInternalVars->intbackIreg0=0;
   SmpcInternalVars->firstPeri=0;

   SmpcInternalVars->timing=0;

   //memset((void *)&SmpcInternalVars->port1, 0, sizeof(PortData_struct));
   //memset((void *)&SmpcInternalVars->port2, 0, sizeof(PortData_struct));
}

//////////////////////////////////////////////////////////////////////////////

void SmpcCKCHG(u32 clk_type) {
   // Reset VDP1, VDP2, SCU, and SCSP
   Vdp1Reset();
   Vdp2Reset();
   ScuReset();
   ScspReset();

   // Clear VDP1/VDP2 ram
   YabauseStopSlave();

   // change clock
   YabauseChangeTiming(clk_type);

   // Set DOTSEL
   SmpcInternalVars->dotsel = clk_type;

   // Send NMI
   SH2NMI(MSH2);
}

int totalseconds;
int noon= 43200;

//////////////////////////////////////////////////////////////////////////////

static void SmpcINTBACKStatus(void) {
	// return time, cartidge, zone, etc. data
	int i;
	struct tm times;
	u8 year[4];
	time_t tmp;

	SMPC_REG_OREG(0) = 0x80 | (SmpcInternalVars->resd << 6);   // goto normal startup
	//SMPC_REG_OREG[0] = 0x0 | (SmpcInternalVars->resd << 6);  // goto setclock/setlanguage screen

	// write time data in OREG1-7
	if (SmpcInternalVars->clocksync) {
		//XXX: update to new frame_count
		//tmp = SmpcInternalVars->basetime + ((u64)yabsys.frame_count * 1001 / 60000);
		tmp = SmpcInternalVars->basetime + ((u64)framecounter * 1001 / 60000);
	} else {
		tmp = time(NULL);
	}
	//WII clock
	localtime_r(&tmp, &times);

	year[0] = (1900 + times.tm_year) / 1000;
	year[1] = ((1900 + times.tm_year) % 1000) / 100;
	year[2] = (((1900 + times.tm_year) % 1000) % 100) / 10;
	year[3] = (((1900 + times.tm_year) % 1000) % 100) % 10;
	SMPC_REG_OREG(1) = (year[0] << 4) | year[1];
	SMPC_REG_OREG(2) = (year[2] << 4) | year[3];
	SMPC_REG_OREG(3) = (times.tm_wday << 4) | (times.tm_mon + 1);
	SMPC_REG_OREG(4) = ((times.tm_mday / 10) << 4) | (times.tm_mday % 10);
	SMPC_REG_OREG(5) = ((times.tm_hour / 10) << 4) | (times.tm_hour % 10);
	SMPC_REG_OREG(6) = ((times.tm_min / 10) << 4) | (times.tm_min % 10);
	SMPC_REG_OREG(7) = ((times.tm_sec / 10) << 4) | (times.tm_sec % 10);


	// write cartidge data in OREG8
	SMPC_REG_OREG(8) = 0; // FIXME : random value

	// write zone data in OREG9 bits 0-7
	// 1 -> japan
	// 2 -> asia/ntsc
	// 4 -> north america
	// 5 -> central/south america/ntsc
	// 6 -> corea
	// A -> asia/pal
	// C -> europe + others/pal
	// D -> central/south america/pal
	SMPC_REG_OREG(9) = SmpcInternalVars->regionid;

	// system state, first part in OREG10, bits 0-7
	// bit | value  | comment
	// ---------------------------
	// 7   | 0      |
	// 6   | DOTSEL |
	// 5   | 1      |
	// 4   | 1      |
	// 3   | MSHNMI |
	// 2   | 1      |
	// 1   | SYSRES |
	// 0   | SNDRES |
	SMPC_REG_OREG(10) = 0x34|(SmpcInternalVars->dotsel<<6)|(SmpcInternalVars->mshnmi<<3)|(SmpcInternalVars->sysres<<1)|SmpcInternalVars->sndres;

	// system state, second part in OREG11, bit 6
	// bit 6 -> CDRES
	SMPC_REG_OREG(11) = SmpcInternalVars->cdres << 6; // FIXME

	// SMEM
	for(i = 0;i < 4;i++)
		SMPC_REG_OREG(12+i) = SmpcInternalVars->SMEM[i];

	SMPC_REG_OREG(31) = 0x10; // set to intback command
}

//////////////////////////////////////////////////////////////////////////////

static void SmpcINTBACKPeripheral(void) {

  if (SmpcInternalVars->firstPeri)
    SMPC_REG_SR = 0xC0 | (SMPC_REG_IREG(1) >> 4);
  else
    SMPC_REG_SR = 0x80 | (SMPC_REG_IREG(1) >> 4);

  SmpcInternalVars->firstPeri = 0;

  /* Port Status:
  0x04 - Sega-tap is connected
  0x16 - Multi-tap is connected
  0x21-0x2F - Clock serial peripheral is connected
  0xF0 - Not Connected or Unknown Device
  0xF1 - Peripheral is directly connected */

  /* PeripheralID:
  0x02 - Digital Device Standard Format
  0x13 - Racing Device Standard Format
  0x15 - Analog Device Standard Format
  0x23 - Pointing Device Standard Format
  0x23 - Shooting Device Standard Format
  0x34 - Keyboard Device Standard Format
  0xE1 - Mega Drive 3-Button Pad
  0xE2 - Mega Drive 6-Button Pad
  0xE3 - Saturn Mouse
  0xFF - Not Connected */

  /* Special Notes(for potential future uses):

  If a peripheral is disconnected from a port, you only return 1 byte for
  that port(which is the port status 0xF0), at the next OREG you then return
  the port status of the next port.

  e.g. If Port 1 has nothing connected, and Port 2 has a controller
       connected:

  OREG0 = 0xF0
  OREG1 = 0xF1
  OREG2 = 0x02
  etc.
  */

	//u32 data_sent = (per_data.data_sent ? 0 : per_data.data_size);
	for (u32 i = 0; i < per_data.data_size; ++i) {
		SMPC_REG_OREG(i) = per_data.data[i];
	}
	//per_data.data_sent = 1;
	LagFrameFlag = 0; 	//???

/*
  Use this as a reference for implementing other peripherals
  // Port 1
  SMPC_REG_OREG[0] = 0xF1; //Port Status(Directly Connected)
  SMPC_REG_OREG[1] = 0xE3; //PeripheralID(Shuttle Mouse)
  SMPC_REG_OREG[2] = 0x00; //First Data
  SMPC_REG_OREG[3] = 0x00; //Second Data
  SMPC_REG_OREG[4] = 0x00; //Third Data

  // Port 2
  SMPC_REG_OREG[5] = 0xF0; //Port Status(Not Connected)
*/
}

//////////////////////////////////////////////////////////////////////////////

static void SmpcINTBACK(void) {
	SMPC_REG_SF = 1;
	if (SmpcInternalVars->intback) {
		SmpcINTBACKPeripheral();
		ScuSendSystemManager();
		return;
	}

	//we think rayman sets 0x40 so that it breaks the intback command immediately when it blocks,
	//rather than having to set 0x40 in response to an interrupt
	if ((SmpcInternalVars->intbackIreg0 = (SMPC_REG_IREG(0) & 1))) {
		// Return non-peripheral data
		SmpcInternalVars->firstPeri = 1;
		SmpcInternalVars->intback = (SMPC_REG_IREG(1) & 0x8) >> 3; // does the program want peripheral data too?
		SmpcINTBACKStatus();
		SMPC_REG_SR = 0x4F | (SmpcInternalVars->intback << 5); // the low nibble is undefined(or 0xF)
		ScuSendSystemManager();
		return;
	}
	if (SMPC_REG_IREG(1) & 0x8) {
		SmpcInternalVars->firstPeri = 1;
		SmpcInternalVars->intback = 1;
		SMPC_REG_SR = 0x40;
		SmpcINTBACKPeripheral();
		SMPC_REG_OREG(31) = 0x10; // may need to be changed
		ScuSendSystemManager();
		return;
	}
}

//////////////////////////////////////////////////////////////////////////////

void SmpcINTBACKEnd(void) {
	SmpcInternalVars->intback = 0;
}

//////////////////////////////////////////////////////////////////////////////

//XXX: unused
#if 0
static void SmpcSETSMEM(void) {
	SmpcInternalVars->SMEM[0] = SMPC_REG_IREG(0);
	SmpcInternalVars->SMEM[1] = SMPC_REG_IREG(1);
	SmpcInternalVars->SMEM[2] = SMPC_REG_IREG(2);
	SmpcInternalVars->SMEM[3] = SMPC_REG_IREG(3);

	SMPC_REG_OREG(31) = 0x17;
}
#endif

//////////////////////////////////////////////////////////////////////////////

void SmpcResetButton(void) {
	// If RESD isn't set, send an NMI request to the MSH2.
	if (SmpcInternalVars->resd)
		return;

	SH2SendInterrupt(MSH2, 0xB, 16);
}

//////////////////////////////////////////////////////////////////////////////

void SmpcExec(s32 t) {
   if (SmpcInternalVars->timing > 0) {
//XXX: new implementation
#if 0
	  if (intback_wait_for_line)
      {
         if (yabsys.LineCount == 207)
         {
            SmpcInternalVars->timing = -1;
            intback_wait_for_line = 0;
         }
      }
#endif
      SmpcInternalVars->timing -= t;
      if (SmpcInternalVars->timing <= 0) {
         switch(SMPC_REG_COMREG) {
            case 0x0:
               //SMPCLOG("smpc\t: MSHON not implemented\n");
               break;
            case 0x2: //SSHON
               YabauseStartSlave();
               break;
            case 0x3: //SSHOFF
               YabauseStopSlave();
               break;
            case 0x6: //SNDON
               	M68KStart();
				SMPC_REG_OREG(31) = 0x6;
               break;
            case 0x7: //SNDOFF
               	M68KStop();
				SMPC_REG_OREG(31) = 0x7;
               break;
            case 0x8:
            case 0x9:
            case 0xA:
            case 0xB:
            case 0xC:
            case 0xD:
				break;
            case 0xE: //CKCHG352
               SmpcCKCHG(CLKTYPE_28MHZ);
               break;
            case 0xF: //CKCHG320
               SmpcCKCHG(CLKTYPE_26MHZ);
               break;
            case 0x10: //INTBACK
               SmpcINTBACK();
               break;
            case 0x17: //SETSMEM
               	SmpcInternalVars->SMEM[0] = SMPC_REG_IREG(0);
				SmpcInternalVars->SMEM[1] = SMPC_REG_IREG(1);
				SmpcInternalVars->SMEM[2] = SMPC_REG_IREG(2);
				SmpcInternalVars->SMEM[3] = SMPC_REG_IREG(3);
				SMPC_REG_OREG(31) = 0x17;
               break;
            case 0x18: //NMIREQ
               	SH2SendInterrupt(MSH2, 0xB, 16);
				SMPC_REG_OREG(31) = 0x18;
				break;
            case 0x19: //RESENAB
               	SmpcInternalVars->resd = 0;
				SMPC_REG_OREG(31) = 0x19;
               break;
            case 0x1A: //RESDISA
               	SmpcInternalVars->resd = 1;
				SMPC_REG_OREG(31) = 0x1A;
               break;
            default:
               //SMPCLOG("smpc\t: Command %02X not implemented\n", SMPC_REG_COMREG);
               break;
         }

         SMPC_REG_SF = 0;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL SmpcReadByte(u32 addr) {
	addr &= 0x7F;
#if 0
	if (addr == 0x063) {
		bustmp &= ~0x01;
		bustmp |= SMPC_REG_SF;
		return bustmp;
	}
#endif
	return SMPC_REG_BASE[addr];
}

//////////////////////////////////////////////////////////////////////////////

u16 FASTCALL SmpcReadWord(USED_IF_SMPC_DEBUG u32 addr) {
	//Byte access only
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL SmpcReadLong(USED_IF_SMPC_DEBUG u32 addr) {
	//Byte access only
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

//XXX: NEEDS UPDATE
static void SmpcSetTiming(void) {
   switch(SMPC_REG_COMREG) {
      case 0x0:
         //SMPCLOG("smpc\t: MSHON not implemented\n");
         SmpcInternalVars->timing = 1;
         return;
      case 0x8:
         //SMPCLOG("smpc\t: CDON not implemented\n");
         SmpcInternalVars->timing = 1;
         return;
      case 0x9:
         //SMPCLOG("smpc\t: CDOFF not implemented\n");
         SmpcInternalVars->timing = 1;
         return;
      case 0xD:
      case 0xE:
      case 0xF:
#ifndef GEKKO
         SmpcInternalVars->timing = 1; // this has to be tested on a real saturn
#else
         SmpcInternalVars->timing = smpcothertiming;
#endif
         return;
      case 0x10:
         if (SmpcInternalVars->intback)
            SmpcInternalVars->timing = 20; // this will need to be verified
         else {
            // Calculate timing based on what data is being retrieved

            SmpcInternalVars->timing = 1;

            // If retrieving non-peripheral data, add 0.2 milliseconds
            if (SMPC_REG_IREG(0) == 0x01)
               SmpcInternalVars->timing += 2;

            // If retrieving peripheral data, add 15 milliseconds
            if (SMPC_REG_IREG(1) & 0x8)
#ifndef GEKKO
               SmpcInternalVars->timing += 16000; // Strangely enough, this works better
                                               // too long for wii
#else
               SmpcInternalVars->timing += smpcperipheraltiming;
#endif
//               SmpcInternalVars->timing += 150;
         }
         return;
      case 0x17:
         SmpcInternalVars->timing = 1;
         return;
      case 0x2:
         SmpcInternalVars->timing = 1;
         return;
      case 0x3:
         SmpcInternalVars->timing = 1;
         return;
      case 0x6:
      case 0x7:
      case 0x18:
      case 0x19:
      case 0x1A:
         SmpcInternalVars->timing = 1;
         return;
      default:
         //SMPCLOG("smpc\t: unimplemented command: %02X\n", SMPC_REG_COMREG);
         SMPC_REG_SF = 0;
         break;
   }
}

//////////////////////////////////////////////////////////////////////////////

//XXX: NEEDS UPDATE
void FASTCALL SmpcWriteByte(u32 addr, u8 val) {
	addr &= 0x7F;
	SMPC_REG_BASE[addr] = val;

	switch(addr) {
		case 0x01: // Maybe an INTBACK continue/break request
			if (SmpcInternalVars->intback) {
				if (SMPC_REG_IREG(0) & 0x40) {
					// Break
					SmpcInternalVars->intback = 0;
					SMPC_REG_SR &= 0x0F;
					break;
				}
				else if (SMPC_REG_IREG(0) & 0x80) {
					// Continue
					SMPC_REG_COMREG = 0x10;
					SmpcSetTiming();
					SMPC_REG_SF = 1;
				}
			} return;
		case 0x1F:
			SmpcSetTiming();
			return;
		case 0x63:
			SMPC_REG_SF &= 0x1;
			return;
		//XXX: Copy for other port
		case 0x75:
			// FIX ME (should support other peripherals)
			//SMPC_REG_DDR1 & 0x7F;
			switch (SMPC_REG_DDR1 & 0x7F) { // Which Control Method do we use?
				case 0x40:
					SMPCLOG("smpc\t: Peripheral TH Control Method not implemented\n");
					break;
				case 0x60:

					switch ((val >> 5) & 0x3) {
						//XXX: use actual values from peripheral.c
						case 0: val = (val & 0x80) | 0x10 | ((per_data.data[3] >> 4) & 0xF);	break;
						case 1: val = (val & 0x80) | 0x10 | ((per_data.data[2] >> 4) & 0xF);	break;
						case 2: val = (val & 0x80) | 0x10 | (per_data.data[2] & 0xF);			break;
						case 3: val = (val & 0x80) | 0x14 | (per_data.data[3] & 0xF);			break;
					}
					SMPC_REG_PDR1 = val;
					break;
				default:
					SMPCLOG("smpc\t: Peripheral Unknown Control Method not implemented\n");
					break;
			} return;
      default:
         return;
   }
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL SmpcWriteWord(USED_IF_SMPC_DEBUG u32 addr, UNUSED u16 val)
{
	//Byte access only
}


void FASTCALL SmpcWriteLong(USED_IF_SMPC_DEBUG u32 addr, UNUSED u32 val)
{
	//Byte access only
}

//////////////////////////////////////////////////////////////////////////////

