/*  Copyright 2006 Theo Berkau

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

#ifndef VIDSOFT_H
#define VIDSOFT_H

#include <gccore.h>
#include "memory.h"
#include "sgx/sgx.h"

#define VIDCORE_SOFT   2
#ifdef GEKKO
#define VIDCORE_SOFTOLD   3
#endif

extern u32 *dispbuffer;
extern u32 tex_dirty[0x200];

int VIDSoftInit(void);
void VIDSoftDeInit(void);
int VIDSoftVdp1Reset(void);
void VIDSoftVdp1DrawStart(void);
void VIDSoftVdp1DrawEnd(void);
void VIDSoftVdp1NormalSpriteDraw(void);
void VIDSoftVdp1ScaledSpriteDraw(void);
void VIDSoftVdp1DistortedSpriteDraw(void);
void VIDSoftVdp1PolylineDraw(void);
void VIDSoftVdp1LineDraw(void);
void VIDSoftVdp1UserClipping(void);
void VIDSoftVdp1SystemClipping(void);
void VIDSoftVdp1LocalCoordinate(void);
int VIDSoftVdp2Reset(void);
void VIDSoftVdp2DrawStart(void);
void VIDSoftVdp2DrawEnd(void);
void VIDSoftVdp2DrawScreens(void);
void VIDSoftVdp2SetResolution(u16 TVMD);
//FASTCALL
void VIDSoftOnScreenDebugMessage(char *string, ...);
void VIDSoftVdp1SwapFrameBuffer(void);
void VIDSoftVdp1EraseFrameBuffer(void);


void VidSoftTexConvert(u32 ram_addr);

void vid_Vdp1PolygonDraw(void);
//external use
void VIDSoftVdp2DrawScreen(int screen);

#endif
