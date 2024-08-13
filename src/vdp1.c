/*  Copyright 2003-2005 Guillaume Duhamel
    Copyright 2004 Lawrence Sebald
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
#include "vidsoft.h"
#include "vdp1.h"
#include "debug.h"
#include "scu.h"
#include "vdp2.h"
#include "osd/osd.h"
#ifdef GEKKO
#include "yabause.h"
#endif

#define VDP1_PTE_SIZE 		VDP1_RAM_SIZE / 4096

u8 vdp1_regs[PAGE_SIZE] ATTRIBUTE_ALIGN(PAGE_SIZE);
Vdp1Cmd *vdp1cmd;
u8 *Vdp1Ram;
u8 *Vdp1FrameBuffer;
Vdp1 *Vdp1Regs;
Vdp1External_struct Vdp1External;



//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL Vdp1RamReadByte(u32 addr) {
	return T1ReadByte(Vdp1Ram, addr & 0x7FFFF);
}

//////////////////////////////////////////////////////////////////////////////

u16 FASTCALL Vdp1RamReadWord(u32 addr) {
	return T1ReadWord(Vdp1Ram, addr & 0x7FFFF);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL Vdp1RamReadLong(u32 addr) {
	return T1ReadLong(Vdp1Ram, addr & 0x7FFFF);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp1RamWriteByte(u32 addr, u8 val) {
	T1WriteByte(Vdp1Ram, (addr & 0x7FFFF), val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp1RamWriteWord(u32 addr, u16 val) {
	T1WriteWord(Vdp1Ram, addr & 0x7FFFF, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp1RamWriteLong(u32 addr, u32 val) {
	T1WriteLong(Vdp1Ram, addr & 0x7FFFF, val);
}

//////////////////////////////////////////////////////////////////////////////

u8 FASTCALL Vdp1FrameBufferReadByte(u32 addr) {
	return T1ReadByte(Vdp1FrameBuffer, addr & 0x3FFFF);
}

//////////////////////////////////////////////////////////////////////////////

u16 FASTCALL Vdp1FrameBufferReadWord(u32 addr) {
	return T1ReadWord(Vdp1FrameBuffer, addr & 0x3FFFF);
}

//////////////////////////////////////////////////////////////////////////////

u32 FASTCALL Vdp1FrameBufferReadLong(u32 addr) {
	return T1ReadLong(Vdp1FrameBuffer, addr & 0x3FFFF);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp1FrameBufferWriteByte(u32 addr, u8 val) {
	T1WriteByte(Vdp1FrameBuffer, addr & 0x3FFFF, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp1FrameBufferWriteWord(u32 addr, u16 val) {
	T1WriteWord(Vdp1FrameBuffer, addr & 0x3FFFF, val);
}

//////////////////////////////////////////////////////////////////////////////

void FASTCALL Vdp1FrameBufferWriteLong(u32 addr, u32 val) {
	T1WriteLong(Vdp1FrameBuffer, addr & 0x3FFFF, val);
}

//////////////////////////////////////////////////////////////////////////////

//DONE
int Vdp1Init(void) {
	Vdp1Regs = (Vdp1 *) vdp1_regs;
	memset(Vdp1Regs, 0, PAGE_SIZE);
	Vdp1External.status = VDP1_STATUS_IDLE;
	Vdp1External.disptoggle = 1;

	Vdp1Regs->TVMR = 0;
	Vdp1Regs->FBCR = 0;
	Vdp1Regs->PTMR = 0;
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

//DONE
void Vdp1DeInit(void) {
	//Nothing...
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
int VideoInit(int coreid) {
	// Make sure the core is freed
	VIDSoftDeInit();

	if (VIDSoftInit() != 0)
		return -1;

   // Reset resolution/priority variables
	if (Vdp2Regs){
		//XXX: this is wrong?
		VIDSoftVdp1Reset();
	}

	return 0;
}


//////////////////////////////////////////////////////////////////////////////

void VideoDeInit(void) {
	VIDSoftDeInit();
}
//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
void Vdp1Reset(void) {
	memset(Vdp1Regs, 0, sizeof(*Vdp1Regs));
	Vdp1Regs->PTMR = 0;
	Vdp1Regs->MODR = 0x1000; // VDP1 Version 1
	Vdp1Regs->ENDR = 0;
	VIDSoftVdp1Reset();

	Vdp1Regs->userclipX1 = 0;
	Vdp1Regs->userclipY1 = 0;
	Vdp1Regs->userclipX2 = 0;
	Vdp1Regs->userclipY2 = 0;
	Vdp1Regs->systemclipX1 = 0;
	Vdp1Regs->systemclipY1 = 0;
	Vdp1Regs->systemclipX2 = 0;
	Vdp1Regs->systemclipY2 = 0;

	// Safe tarminator for Radient silvergun with no bios
	T1WriteWord(Vdp1Ram, 0x40000, 0x8000);

	////XXX
	//vdp1_clock = 0;
}

//XXX: is this used?
int VideoSetSetting( int type, int value )
{
	//VIDSoftSetSettingValue(type, value);
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

//DONE
u8 FASTCALL Vdp1ReadByte(u32 addr) {
   //trying to byte-read a Vdp1 register, not possible
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
u16 FASTCALL Vdp1ReadWord(u32 addr) {
   switch(addr & 0xFF) {
      case 0x10:
        return Vdp1Regs->EDSR;
      case 0x12:
        return Vdp1Regs->LOPR;
      case 0x14:
        return Vdp1Regs->COPR;
      case 0x16: {
        u16 mode = 0x1000 | ((Vdp1Regs->PTMR & 2) << 7) | ((Vdp1Regs->FBCR & 0x1E) << 3) | (Vdp1Regs->TVMR & 0xF);
        return mode;
      }
   }
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

//DONE
u32 FASTCALL Vdp1ReadLong(u32 addr) {
   //trying to long-read a Vdp1 register
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

//DONE
void FASTCALL Vdp1WriteByte(u32 addr, UNUSED u8 val) {
   //trying to byte-write a Vdp1 register
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
void FASTCALL Vdp1WriteWord(u32 addr, u16 val) {
	switch(addr & 0xFF) {
		case 0x0:
			Vdp1Regs->TVMR = val;
		break;
		case 0x2:
		Vdp1Regs->FBCR = val;
		if ((Vdp1Regs->FBCR & 3) == 3) {
			Vdp1External.manualchange = 1;
		}
		else if ((Vdp1Regs->FBCR & 3) == 2) {
			Vdp1External.manualerase = 1;
		}
		break;
		case 0x4:
			Vdp1Regs->PTMR = val;
#if 0 //YAB_ASYNC_RENDERING
			if (val == 1){
				if ( YaGetQueueSize(vdp1_rcv_evqueue) > 0){
					yabsys.wait_line_count = -1;
					do{
						YabWaitEventQueue(vdp1_rcv_evqueue);
					} while (YaGetQueueSize(vdp1_rcv_evqueue) != 0);
				}
				Vdp1Regs->EDSR >>= 1;
				yabsys.wait_line_count = yabsys.LineCount + 50;
				yabsys.wait_line_count %= yabsys.MaxLineCount;
				if (yabsys.wait_line_count == 5) { yabsys.wait_line_count = 4; }
				YabAddEventQueue(evqueue,VDPEV_DIRECT_DRAW);
				YabThreadYield();
			}
#else
			if (val == 1) {
				Vdp1Regs->EDSR >>= 1;
				Vdp1NoDraw();
				//VIDSoftVdp1DrawEnd();
				//XXX
				//yabsys.wait_line_count = yabsys.LineCount + 50;
				//yabsys.wait_line_count %= yabsys.MaxLineCount;
			}
#endif
			break;
		case 0x6:
			Vdp1Regs->EWDR = val;
			break;
		case 0x8:
			Vdp1Regs->EWLR = val;
			break;
		case 0xA:
			Vdp1Regs->EWRR = val;
			break;
		case 0xC:
			Vdp1Regs->ENDR = val;
			Vdp1External.status = VDP1_STATUS_IDLE;
			//XXX
			//yabsys.wait_line_count = -1;
			break;
		default: break;
	}
}

//////////////////////////////////////////////////////////////////////////////

//DONE
void FASTCALL Vdp1WriteLong(u32 addr, UNUSED u32 val) {
   //trying to long-write a Vdp1 register
}

//////////////////////////////////////////////////////////////////////////////

//Builds Vram for wii textures and palettes
//XXX: Todo: Add palettes
void vdp1_BuildVram(void) {
	u32 returnAddr;
	u32 commandCounter;
	u16 command;

	u32 addr = 0;
	returnAddr = 0xFFFFFFFF;
	commandCounter = 0;

	command = T1ReadWord(Vdp1Ram, addr);

	while (!(command & 0x8000) && commandCounter < 2000) { // fix me
		// First, process the command
		if (!(command & 0x4000)) { // if (!skip)
			switch (command & 0x000F) {
			case 0: // normal sprite draw
			case 1: // scaled sprite draw
			case 2: // distorted sprite draw
			case 3: //Mirror Distorted sprite
			case 4: // polygon draw
				VidSoftTexConvert(addr);
						break;
			default: // Abort or useless
				if ((command & 0x000F) > 11) {
					return;
				}
			}
		}

		// Next, determine where to go next
		switch ((command & 0x3000) >> 12) {
		case 0: // NEXT, jump to following table
			addr += 0x20;
			break;
		case 1: // ASSIGN, jump to CMDLINK
			addr = T1ReadWord(Vdp1Ram, addr + 2) * 8;
			break;
		case 2: // CALL, call a subroutine
			if (returnAddr == 0xFFFFFFFF)
				returnAddr = addr + 0x20;

			addr = T1ReadWord(Vdp1Ram, addr + 2) * 8;
			break;
		case 3: // RETURN, return from subroutine
			if (returnAddr != 0xFFFFFFFF) {
				addr = returnAddr;
				returnAddr = 0xFFFFFFFF;
			}
			else
				addr += 0x20;
			break;
		}
		command = T1ReadWord(Vdp1Ram, addr);
		commandCounter++;
	}
}


//////////////////////////////////////////////////////////////////////////////

void Vdp1Draw(void) {
   u32 returnAddr;
   u32 commandCounter;
   u16 command;


	if (!Vdp1External.disptoggle) {
		Vdp1NoDraw();
		return;
	}

#if USE_NEW_VDP1
	SGX_Vdp1Begin();
#else
	VIDSoftVdp1DrawStart();
#endif
	//TODO: Only do this once.
	DCFlushRange(Vdp1Ram, 0x80000);
	Vdp1Regs->addr = 0;
	returnAddr = 0xFFFFFFFF;
	commandCounter = 0;

   // beginning of a frame
   // BEF <- CEF
   // CEF <- 0
   Vdp1Regs->EDSR >>= 1;
   /* this should be done after a frame change or a plot trigger */
   Vdp1Regs->COPR = 0;

   command = T1ReadWord(Vdp1Ram, Vdp1Regs->addr);

	vdp1cmd = (Vdp1Cmd*) (Vdp1Ram + Vdp1Regs->addr);
   while (!(command & 0x8000) && commandCounter < 2048) { // fix me
      // First, process the command
      if (!(command & 0x4000)) { // if (!skip)
#if USE_NEW_VDP1
			switch (command & 0x000F) {
				case 0: SGX_Vdp1DrawNormalSpr();    break;
				case 1: SGX_Vdp1DrawScaledSpr();    break;
				case 2: SGX_Vdp1DrawDistortedSpr(); break;
				case 3: SGX_Vdp1DrawDistortedSpr(); break;
				case 4: SGX_Vdp1DrawPolygon();      break;
				case 5: SGX_Vdp1DrawPolyline();     break;
				case 6: SGX_Vdp1DrawLine();         break;
				case 7: SGX_Vdp1DrawPolyline();     break;
				case 8: SGX_Vdp1UserClip();         break;
				case 9: SGX_Vdp1SysClip();          break;
				case 10: SGX_Vdp1LocalCoord();      break;
				case 11: SGX_Vdp1UserClip();        break;
				default: // Abort
					Vdp1Regs->EDSR |= 2;
					VIDSoftVdp1DrawEnd();
					Vdp1Regs->LOPR = Vdp1Regs->addr >> 3;
					Vdp1Regs->COPR = Vdp1Regs->addr >> 3;
					return;
			}
#else
         switch (command & 0x000F) {
            case 0: // normal sprite draw
				//VidSoftTexConvert(Vdp1Regs->addr);
				VIDSoftVdp1NormalSpriteDraw();
				break;
            case 1: // scaled sprite draw
				//VidSoftTexConvert(Vdp1Regs->addr);
				VIDSoftVdp1ScaledSpriteDraw();
				break;
            case 2: // distorted sprite draw
            case 3: //Mirror Distorted sprite
				//VidSoftTexConvert(Vdp1Regs->addr);
				VIDSoftVdp1DistortedSpriteDraw();
				break;
            case 4: // polygon draw
//XXX: Note
//for the actual hardware, polygons are essentially identical to distorted sprites
//the actual hardware draws using diagonal lines, which is why using half-transparent processing
//on distorted sprites and polygons is not recommended since the hardware overdraws to prevent gaps
//thus, with half-transparent processing some pixels will be processed more than once, producing moire patterns in the drawn shapes
               VIDSoftVdp1DistortedSpriteDraw();
               break;
            case 5: // polyline draw
               VIDSoftVdp1PolylineDraw();
               break;
            case 6: // line draw
               VIDSoftVdp1LineDraw();
               break;
            case 7: //Mirror Line draw
               VIDSoftVdp1PolylineDraw();
               break;
            case 8: // user clipping coordinates
               VIDSoftVdp1UserClipping();
               break;
            case 9: // system clipping coordinates
               VIDSoftVdp1SystemClipping();
               break;
            case 10: // local coordinate
               VIDSoftVdp1LocalCoordinate();
               break;
            case 11: //Mirror Local Coordinate
               VIDSoftVdp1UserClipping();
               break;
            default: // Abort
               Vdp1Regs->EDSR |= 2;
               VIDSoftVdp1DrawEnd();
               Vdp1Regs->LOPR = Vdp1Regs->addr >> 3;
               Vdp1Regs->COPR = Vdp1Regs->addr >> 3;
               return;
         }
#endif
      }

		if (Vdp1Regs->EDSR & 0x02){
			Vdp1Regs->LOPR = Vdp1Regs->addr >> 3;
			Vdp1Regs->COPR = Vdp1Regs->addr >> 3;
			return;
		}

		// Next, determine where to go next
		switch ((command & 0x3000) >> 12) {
		case 0: // NEXT, jump to following table
			Vdp1Regs->addr += 0x20;
			break;
		case 1: // ASSIGN, jump to CMDLINK
			Vdp1Regs->addr = T1ReadWord(Vdp1Ram, (Vdp1Regs->addr + 2)  & 0x7FFFF) * 8;
			if (Vdp1Regs->addr == 0) {return;}
			break;
		case 2: // CALL, call a subroutine
			if (returnAddr == 0xFFFFFFFF)
				returnAddr = Vdp1Regs->addr + 0x20;

			Vdp1Regs->addr = T1ReadWord(Vdp1Ram, (Vdp1Regs->addr + 2)  & 0x7FFFF) * 8;
			if (Vdp1Regs->addr == 0) {return;}
			break;
		case 3: // RETURN, return from subroutine
			if (returnAddr != 0xFFFFFFFF) {
				Vdp1Regs->addr = returnAddr;
				returnAddr = 0xFFFFFFFF;
			}
			else
				Vdp1Regs->addr += 0x20;
			if (Vdp1Regs->addr == 0) {return;}
			break;
		}

		command = T1ReadWord(Vdp1Ram, Vdp1Regs->addr & 0x7FFFF);
		vdp1cmd = (Vdp1Cmd*) (Vdp1Ram + Vdp1Regs->addr);
		commandCounter++;
		if (command & 0x8000) {
			Vdp1Regs->LOPR = Vdp1Regs->addr >> 3;
			Vdp1Regs->COPR = Vdp1Regs->addr >> 3;
		}
   }
	//Get vram out of the cache
	//DCFlushRange(wii_vram, 0x80000);

   // we set two bits to 1
   Vdp1Regs->EDSR |= 2;
   Vdp1Regs->COPR = Vdp1Regs->addr >> 3;
   ScuSendDrawEnd();
}

//////////////////////////////////////////////////////////////////////////////

void Vdp1NoDraw(void) {
   u32 returnAddr;
   u32 commandCounter;
   u16 command;


   Vdp1Regs->addr = 0;
   returnAddr = 0xFFFFFFFF;
   commandCounter = 0;

   // beginning of a frame (ST-013-R3-061694 page 53)
   // BEF <- CEF
   // CEF <- 0
   Vdp1Regs->EDSR >>= 1;
   /* this should be done after a frame change or a plot trigger */
   Vdp1Regs->COPR = 0;

   command = T1ReadWord(Vdp1Ram, Vdp1Regs->addr);

   while (!(command & 0x8000) && commandCounter < 2000) { // fix me
      // First, process the command
      if (!(command & 0x4000)) { // if (!skip)
         switch (command & 0x000F) {
            case 0: // normal sprite draw
            case 1: // scaled sprite draw
            case 2: // distorted sprite draw
            case 3: // distorted sprite draw
            case 4: // polygon draw
            case 5: // polyline draw
            case 6: // line draw
            case 7:
               break;
            case 8: // user clipping coordinates
               VIDSoftVdp1UserClipping();
               break;
            case 9: // system clipping coordinates
               VIDSoftVdp1SystemClipping();
               break;
            case 10: // local coordinate
               VIDSoftVdp1LocalCoordinate();
               break;
            case 11: // undocumented mirror
               VIDSoftVdp1UserClipping();
               break;
            default: // Abort
               Vdp1Regs->EDSR |= 2;
               VIDSoftVdp1DrawEnd();
               Vdp1Regs->LOPR = Vdp1Regs->addr >> 3;
               Vdp1Regs->COPR = Vdp1Regs->addr >> 3;
               return;
         }
      }

		// Next, determine where to go next
		switch ((command & 0x3000) >> 12) {
		case 0: // NEXT, jump to following table
			Vdp1Regs->addr += 0x20;
			break;
		case 1: // ASSIGN, jump to CMDLINK
			Vdp1Regs->addr = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 2) * 8;
			break;
		case 2: // CALL, call a subroutine
			if (returnAddr == 0xFFFFFFFF)
				returnAddr = Vdp1Regs->addr + 0x20;

			Vdp1Regs->addr = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 2) * 8;
			break;
		case 3: // RETURN, return from subroutine
			if (returnAddr != 0xFFFFFFFF) {
				Vdp1Regs->addr = returnAddr;
				returnAddr = 0xFFFFFFFF;
			}
			else
				Vdp1Regs->addr += 0x20;
			break;
		}

      command = T1ReadWord(Vdp1Ram, Vdp1Regs->addr);
      commandCounter++;
   }

   // we set two bits to 1
   Vdp1Regs->EDSR |= 2;
   ScuSendDrawEnd();
}

//////////////////////////////////////////////////////////////////////////////

//XXX: Make this better
//HALF-DONE
void FASTCALL Vdp1ReadCommand(vdp1cmd_struct *cmd, u32 addr) {
	cmd->CMDCTRL = T1ReadWord(Vdp1Ram, addr);
	cmd->CMDLINK = T1ReadWord(Vdp1Ram, addr + 0x2);
	cmd->CMDPMOD = T1ReadWord(Vdp1Ram, addr + 0x4);
	cmd->CMDCOLR = T1ReadWord(Vdp1Ram, addr + 0x6);
	cmd->CMDSRCA = T1ReadWord(Vdp1Ram, addr + 0x8);
	cmd->CMDSIZE = T1ReadWord(Vdp1Ram, addr + 0xA);
	cmd->CMDXA = T1ReadWord(Vdp1Ram, addr + 0xC);
	cmd->CMDYA = T1ReadWord(Vdp1Ram, addr + 0xE);
	cmd->CMDXB = T1ReadWord(Vdp1Ram, addr + 0x10);
	cmd->CMDYB = T1ReadWord(Vdp1Ram, addr + 0x12);
	cmd->CMDXC = T1ReadWord(Vdp1Ram, addr + 0x14);
	cmd->CMDYC = T1ReadWord(Vdp1Ram, addr + 0x16);
	cmd->CMDXD = T1ReadWord(Vdp1Ram, addr + 0x18);
	cmd->CMDYD = T1ReadWord(Vdp1Ram, addr + 0x1A);
	cmd->CMDGRDA = T1ReadWord(Vdp1Ram, addr + 0x1C);
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static INLINE int CheckEndcode(int dot, int endcode, int *code)
{
   if (dot == endcode)
   {
      ++code[0];
      if (code[0] == 2)
      {
         code[0] = 0;
         return 2;
      }
      return 1;
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static INLINE int DoEndcode(int count, u32 *charAddr, u32 **textdata, int width, int xoff, int oddpixel, int pixelsize)
{
   if (count > 1)
   {
      float divisor = (float)(8 / pixelsize);

      if(divisor != 0)
         charAddr[0] += (int)((float)(width - xoff + oddpixel) / divisor);
      memset(textdata[0], 0, sizeof(u32) * (width - xoff));
      textdata[0] += (width - xoff);
      return 1;
   }
   else
      *textdata[0]++ = 0;

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void ToggleVDP1(void)
{
   Vdp1External.disptoggle ^= 1;
}

