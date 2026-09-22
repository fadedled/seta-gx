/*  Copyright 2003-2005 Guillaume Duhamel
    Copyright 2004-2006 Theo Berkau
    Copyright 2006      Anders Montonen

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

#ifndef YABAUSE_H
#define YABAUSE_H

#include "core.h"


enum CoreTask {
	SX_VBLANK_IN,
	SX_VBLANK_OUT,
	SX_HBLANK_IN,
	SX_TIMER0,
	SX_TIMER1,
	SX_DPS_END,
	SX_SOUND_REQUEST,
	SX_SYSTEM_MANAGER,
	SX_PAD_INTERRUPT,
	SX_LEVEL2_DMA_END,
	SX_LEVEL1_DMA_END,
	SX_LEVEL0_DMA_END,
	SX_DMA_ILLEGAL,
	SX_SPRITE_DRAW_END,
	SX_EXT_IRQ_00,
	SX_EXT_IRQ_01,
	SX_EXT_IRQ_02,
	SX_EXT_IRQ_03,
	SX_EXT_IRQ_04,
	SX_EXT_IRQ_05,
	SX_EXT_IRQ_06,
	SX_EXT_IRQ_07,
	SX_EXT_IRQ_08,
	SX_EXT_IRQ_09,
	SX_EXT_IRQ_0A,
	SX_EXT_IRQ_0B,
	SX_EXT_IRQ_0C,
	SX_EXT_IRQ_0D,
	SX_EXT_IRQ_0E,
	SX_EXT_IRQ_0F,
	SX_TASK_MAX
};


typedef struct
{
   int percoretype;
   int sh2coretype;
   int vidcoretype;
#ifdef SCSP_PLUGIN
   int scspcoretype;
#endif
   int sndcoretype;
   int m68kcoretype;
   int cdcoretype;
   int carttype;
   u8 regionid;
   const char *biospath;
   const char *cdfilename;
   const char *mpegpath;
   const char *cartpath;
   const char *netlinksetting;
   int videoformattype;
   int frameskip;
   int clocksync;  // 1 = sync internal clock to emulation, 0 = realtime clock
   u32 basetime;   // Initial time in clocksync mode (0 = start w/ system time)
   int usethreads;
} yabauseinit_struct;

#define CLKTYPE_26MHZ           0
#define CLKTYPE_28MHZ           1

#define VIDEOFORMATTYPE_NTSC    0
#define VIDEOFORMATTYPE_PAL     1


void YabauseChangeTiming(int freqtype);
int YabauseInit(yabauseinit_struct *init);
void YabauseDeInit(void);
void YabauseSetDecilineMode(int on);
void YabauseResetNoLoad(void);
void YabauseReset(void);
void YabauseResetButton(void);
int YabauseExec(void);
void YabauseStartSlave(void);
void YabauseStopSlave(void);
u64 YabauseGetTicks(void);
void YabauseSetVideoFormat(int type);
void YabauseSpeedySetup(void);
int YabauseQuickLoadGame(void);

#define YABSYS_TIMING_BITS  20
#define YABSYS_TIMING_MASK  ((1 << YABSYS_TIMING_BITS) - 1)

#define SYS_FLAGS_SHOW_FPS 			0x2	/* Shows FPS on screen */
#define SYS_FLAGS_SHOW_DEBUG 		0x4	/* Shows debug info on screen */

typedef struct {
	int DecilineMode;
	int DecilineCount;
	int LineCount;
	int VBlankLineCount;
	int MaxLineCount;
	u32 DecilineStop;  // Fixed point
	u32 SH2CycleFrac;  // Fixed point
	u32 DecilineUsec;  // Fixed point
	u32 UsecFrac;      // Fixed point
	u32 flags;         /* System flags */
	int CurSH2FreqType;
	int IsPal;
	u8 UseThreads;
	u8 IsSSH2Running;
	u64 OneFrameTime;
	u64 tickfreq;
	int usequickload;
} yabsys_struct;

extern yabsys_struct yabsys;
extern int lagframecounter;
extern int LagFrameFlag;
extern int framelength;
extern int framecounter;

int YabauseEmulate(void);


//New core stuff
u32   core_Exec(void);
char* core_SetAutoload(char* path, char* filename);
void  core_RegisterReset(void);
void  core_RegisterExit(void);
u32   core_IsRunning(void);


#endif
