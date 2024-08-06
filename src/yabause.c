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

#include <sys/types.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <string.h>
#include "yabause.h"
#include "cs0.h"
#include "cs2.h"
#include "debug.h"
#include "error.h"
#include "memory.h"
#include "m68kcore.h"
#include "peripheral.h"
#include "scsp.h"
#include "scu.h"
#include "sh2core.h"
#include "osd/osd.h"
#include "smpc.h"
#include "vdp2.h"
#include "vidsoft.h"
#include "yui.h"
#include "bios.h"


#ifdef HAVE_LIBSDL
 #if defined(__APPLE__) || defined(GEKKO)
  #include <SDL/SDL.h>
 #else
  #include "SDL.h"
 #endif
#endif
#if defined(_MSC_VER) || !defined(HAVE_SYS_TIME_H)
#include <time.h>
#else
#include <sys/time.h>
#endif
#ifdef _arch_dreamcast
#include <arch/timer.h>
#endif
#ifdef GEKKO
#include <ogc/lwp_watchdog.h>
#include <string.h>
extern char saves_dir[512];
extern int eachbackupramon;
extern char prev_itemnum[512];
int declinenum = 15; //10 in original yabause
int dividenumclock = 1; //1 in original yabause
#endif

#ifdef SYS_PROFILE_H
 #include SYS_PROFILE_H
#else
 #define DONT_PROFILE
 #include "profile.h"
#endif

//////////////////////////////////////////////////////////////////////////////

//Dynarec sh2

yabsys_struct yabsys;
const char *bupfilename = NULL;
u64 tickfreq;

int lagframecounter;
int LagFrameFlag;
int framelength=16;
int framecounter;

//////////////////////////////////////////////////////////////////////////////

#ifndef NO_CLI
void print_usage(const char *program_name) {
   printf("Yabause v" VERSION "\n");
   printf("\n"
          "Purpose:\n"
          "  This program is intended to be a Sega Saturn emulator\n"
          "\n"
          "Usage: %s [OPTIONS]...\n", program_name);
   printf("   -h         --help                 Print help and exit\n");
   printf("   -b STRING  --bios=STRING          bios file\n");
   printf("   -i STRING  --iso=STRING           iso/cue file\n");
   printf("   -c STRING  --cdrom=STRING         cdrom path\n");
   printf("   -ns        --nosound              turn sound off\n");
   printf("   -a         --autostart            autostart emulation\n");
   printf("   -f         --fullscreen           start in fullscreen mode\n");
}
#endif

//////////////////////////////////////////////////////////////////////////////

void YabauseChangeTiming(int freqtype) {
   // Setup all the variables related to timing

   const double freq_base = yabsys.IsPal ? 28437500.0
      : (39375000.0 / 11.0) * 8.0;  // i.e. 8 * 3.579545... = 28.636363... MHz
   const double freq_mult = (freqtype == CLKTYPE_26MHZ) ? 15.0/16.0 : 1.0;
#ifndef GEKKO
   const double freq_shifted = (freq_base * freq_mult) * (1 << YABSYS_TIMING_BITS);
#else
   const double freq_shifted = (freq_base * freq_mult / (double)dividenumclock) * (1 << YABSYS_TIMING_BITS);
#endif
   const double usec_shifted = 1.0e6 * (1 << YABSYS_TIMING_BITS);
#ifndef GEKKO
   const double deciline_time = yabsys.IsPal ? 1.0 /  50        / 313 / 10
                                             : 1.0 / (60/1.001) / 263 / 10;
#else
   const double deciline_time = yabsys.IsPal ? 1.0 / 50.0 / 313.0 / (double)declinenum
                                             : 1.0 / 60.0 / 263.0 / (double)declinenum;
#endif

   yabsys.DecilineCount = 0;
   yabsys.LineCount = 0;
   yabsys.CurSH2FreqType = freqtype;
   yabsys.DecilineStop = (u32) (freq_shifted * deciline_time + 0.5);
   yabsys.SH2CycleFrac = 0;
   yabsys.DecilineUsec = (u32) (usec_shifted * deciline_time + 0.5);
   yabsys.UsecFrac = 0;
}

//////////////////////////////////////////////////////////////////////////////


//XXX: change errors to outputs on OSD
int	YabauseInit(yabauseinit_struct *init)
{
	// Need to set this first, so init routines see it
	yabsys.UseThreads = init->usethreads;

	// Initialize both cpu's
	if (SH2Init(init->sh2coretype) != 0) {
		//YabSetError(YAB_ERR_CANNOTINIT, _("SH2"));
		return -1;
	}

#ifdef GEKKO
   if(!eachbackupramon)
   {
#endif
   if (LoadBackupRam(init->buppath) != 0)
      FormatBackupRam(bup_ram, 0x10000);

   bup_ram_written = 0;

   bupfilename = init->buppath;
#ifdef GEKKO
   }
#endif

#ifndef GEKKO
   if (CartInit(init->cartpath, init->carttype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, "Cartridge");
      return -1;
   }
#endif

#ifdef SCSP_PLUGIN
   if (ScspChangeCore(init->scspcoretype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, "SCSP");
      return -1;
   }
#endif

#ifndef GEKKO
   MappedMemoryInit();
#endif

   if (VideoInit(init->vidcoretype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, "Video");
      return -1;
   }


   if (Cs2Init(init->carttype, init->cdcoretype, init->cdpath, init->mpegpath, NULL, NULL) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, "CS2");
      return -1;
   }

#ifdef GEKKO
   Cs2GetRegionID(); //for getting itemnum

   if (CartInit(init->cartpath, init->carttype) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, "Cartridge");
      return -1;
   }

   MappedMemoryInit();

   if(eachbackupramon)
   {
      Cs2GetRegionID(); //for getting itemnum
      static char buppath[512];
      strcpy(buppath, saves_dir);
      strcat(buppath, "/");
      if(strlen(cdip->itemnum)!=0)
      {
         strcat(buppath, cdip->itemnum);
      }
      else
      {
         if(strlen(prev_itemnum)!=0)
            strcat(buppath, prev_itemnum);
         else
            strcat(buppath, "bkram");
      }
      strcat(buppath, ".bin");
      init->buppath = buppath;
      if (LoadBackupRam(init->buppath) != 0)
         FormatBackupRam(bup_ram, 0x10000);

      bup_ram_written = 0;

      bupfilename = init->buppath;
   }
#endif

   if (ScuInit() != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, "SCU");
      return -1;
   }

#ifdef SCSP_PLUGIN
   if (SCSCore->Init(init->sndcoretype, ScuSendSoundRequest) != 0)
#else
   if (ScspInit(init->sndcoretype) != 0)
#endif
   {
      YabSetError(YAB_ERR_CANNOTINIT, "SCSP/M68K");
      return -1;
   }

   if (Vdp1Init() != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, "VDP1");
      return -1;
   }

   if (Vdp2Init() != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, "VDP2");
      return -1;
   }

   if (SmpcInit(init->regionid, init->clocksync, init->basetime) != 0)
   {
      YabSetError(YAB_ERR_CANNOTINIT, "SMPC");
      return -1;
   }

   YabauseSetVideoFormat(init->videoformattype);
   YabauseChangeTiming(CLKTYPE_26MHZ);
   yabsys.DecilineMode = 1;


   if (init->biospath != NULL && strlen(init->biospath))
   {
      if (LoadBios(init->biospath) != 0)
      {
         YabSetError(YAB_ERR_FILENOTFOUND, (void *)init->biospath);
         return -2;
      }
      yabsys.emulatebios = 0;
   }
   else
      yabsys.emulatebios = 1;

   yabsys.usequickload = 0;

   #if defined(SH2_DYNAREC)
   if(SH2Core->id==2) {
     sh2_dynarec_init();
   }
   #endif


   YabauseResetNoLoad();

   if (yabsys.usequickload || yabsys.emulatebios)
   {
      if (YabauseQuickLoadGame() != 0)
      {
         if (yabsys.emulatebios)
         {
            YabSetError(YAB_ERR_CANNOTINIT, "Game");
            return -2;
         }
         else
            YabauseResetNoLoad();
      }
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void YabauseDeInit(void) {
	SH2DeInit();

	if (T123Save(bup_ram, 0x10000, 1, bupfilename) != 0)
         YabSetError(YAB_ERR_FILEWRITE, (void *)bupfilename);

   CartDeInit();
   Cs2DeInit();
   ScuDeInit();
   ScspDeInit();
   Vdp1DeInit();
   Vdp2DeInit();
   SmpcDeInit();
   VideoDeInit();
}

//////////////////////////////////////////////////////////////////////////////

void YabauseSetDecilineMode(int on) {
   yabsys.DecilineMode = (on != 0);
}

//////////////////////////////////////////////////////////////////////////////

void YabauseResetNoLoad(void) {
	SH2Reset(MSH2);

   YabauseStopSlave();
   memset(wram, 0, 0x200000);

   // Reset CS0 area here
   // Reset CS1 area here
   Cs2Reset();
   ScuReset();
   ScspReset();
   Vdp1Reset();
   Vdp2Reset();
   SmpcReset();

   SH2PowerOn(MSH2);
}

//////////////////////////////////////////////////////////////////////////////

void YabauseReset(void) {
   YabauseResetNoLoad();

   if (yabsys.usequickload || yabsys.emulatebios)
   {
      if (YabauseQuickLoadGame() != 0)
      {
         if (yabsys.emulatebios)
            YabSetError(YAB_ERR_CANNOTINIT, "Game");
         else
            YabauseResetNoLoad();
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

void YabauseResetButton(void) {
   // This basically emulates the reset button behaviour of the saturn. This
   // is the better way of reseting the system since some operations (like
   // backup ram access) shouldn't be interrupted and this allows for that.

   SmpcResetButton();
}

//////////////////////////////////////////////////////////////////////////////

int YabauseExec(void)
{
	ScspUnMuteAudio(SCSP_MUTE_SYSTEM);
	YabauseEmulate();

	return 0;
}

//////////////////////////////////////////////////////////////////////////////
#ifndef SCSP_PLUGIN
#ifndef USE_SCSP2
int saved_centicycles;
#endif
#else
int saved_centicycles;
#endif

int YabauseEmulate(void) {
   int oneframeexec = 0;

   const u32 cyclesinc =
      yabsys.DecilineMode ? yabsys.DecilineStop : yabsys.DecilineStop * 10;
   const u32 usecinc =
      yabsys.DecilineMode ? yabsys.DecilineUsec : yabsys.DecilineUsec * 10;
#ifndef SCSP_PLUGIN
#ifndef USE_SCSP2
   u32 m68kcycles = 0;       // Integral M68k cycles per call
   u32 m68kcenticycles = 0;  // 1/100 M68k cycles per call

   if (yabsys.IsPal)
   {
#ifndef GEKKO
      /* 11.2896MHz / 50Hz / 313 lines / 10 calls/line = 72.20 cycles/call */
      m68kcycles = yabsys.DecilineMode ? 72 : 722;
      m68kcenticycles = yabsys.DecilineMode ? 20 : 0;
#else
      /* 11.2896MHz / 50Hz / 313 lines / declinenum calls/line = 721.4 cycles/ declinenum call */
      m68kcycles = yabsys.DecilineMode ? (7214/declinenum)/10 : 716;
      m68kcenticycles = yabsys.DecilineMode ? 7214-((7214/declinenum)/10)*100 : 20;
#endif
   }
   else
   {
#ifndef GEKKO
      /* 11.2896MHz / 60Hz / 263 lines / 10 calls/line = 71.62 cycles/call */
      m68kcycles = yabsys.DecilineMode ? 71 : 716;
      m68kcenticycles = yabsys.DecilineMode ? 62 : 20;
#else
      /* 11.2896MHz / 60Hz / 263 lines / declinenum calls/line = 715.4 cycles/ declinenum call */
      m68kcycles = yabsys.DecilineMode ? (7154/declinenum)/10 : 716;
      m68kcenticycles = yabsys.DecilineMode ? 7154-((7154/declinenum)/10)*100 : 20;
#endif
   }
#endif
#else // SCSP_PLUGIN
   unsigned int m68kcycles = 0;       // Integral M68k cycles per call
   unsigned int m68kcenticycles = 0;  // 1/100 M68k cycles per call

 if(SCSCore->id == SCSCORE_SCSP1)
 {
   if (yabsys.IsPal)
   {
#ifndef GEKKO
      /* 11.2896MHz / 50Hz / 313 lines / 10 calls/line = 72.20 cycles/call */
      m68kcycles = yabsys.DecilineMode ? 72 : 722;
      m68kcenticycles = yabsys.DecilineMode ? 20 : 0;
#else
      /* 11.2896MHz / 50Hz / 313 lines / declinenum calls/line = 721.4 cycles/ declinenum call */
      m68kcycles = yabsys.DecilineMode ? (7214/declinenum)/10 : 716;
      m68kcenticycles = yabsys.DecilineMode ? 7214-((7214/declinenum)/10)*100 : 20;
#endif
   }
   else
   {
#ifndef GEKKO
      /* 11.2896MHz / 60Hz / 263 lines / 10 calls/line = 71.62 cycles/call */
      m68kcycles = yabsys.DecilineMode ? 71 : 716;
      m68kcenticycles = yabsys.DecilineMode ? 62 : 20;
#else
      /* 11.2896MHz / 60Hz / 263 lines / declinenum calls/line = 715.4 cycles/ declinenum call */
      m68kcycles = yabsys.DecilineMode ? (7154/declinenum)/10 : 716;
      m68kcenticycles = yabsys.DecilineMode ? 7154-((7154/declinenum)/10)*100 : 20;
#endif
   }
 }
#endif

   	lagframecounter += (LagFrameFlag == 1);
	framecounter++;
	LagFrameFlag = 1;

   #if defined(SH2_DYNAREC)
   if(SH2Core->id==2) {
     if (yabsys.IsPal)
       YabauseDynarecOneFrameExec(722,0); // m68kcycles,m68kcenticycles
     else
       YabauseDynarecOneFrameExec(716,20);
     return 0;
   }
   #endif
	char str[128] = {0};
	u64 prof_cycles[8] = {0};
	u64 cycles_start;
	while (!oneframeexec) {
      PROFILE_START("Total Emulation");

      if (yabsys.DecilineMode) {
         // Since we run the SCU with half the number of cycles we send
         // to SH2Exec(), we always compute an even number of cycles here
         // and leave any odd remainder in SH2CycleFrac.
         yabsys.SH2CycleFrac += cyclesinc;
         u32 sh2cycles = (yabsys.SH2CycleFrac >> (YABSYS_TIMING_BITS + 1)) << 1;
         yabsys.SH2CycleFrac &= ((YABSYS_TIMING_MASK << 1) | 1);

         PROFILE_START("MSH2");
         cycles_start = gettime();
         SH2Exec(MSH2, sh2cycles);
         PROFILE_STOP("MSH2");
         prof_cycles[0] += gettime() - cycles_start;
		 osd_CyclesSet(0, prof_cycles[0]);

         PROFILE_START("SSH2");
         cycles_start = gettime();
         if (yabsys.IsSSH2Running)
            SH2Exec(SSH2, sh2cycles);
         PROFILE_STOP("SSH2");
         prof_cycles[1] += gettime() - cycles_start;
		 osd_CyclesSet(1, prof_cycles[1]);

#ifndef SCSP_PLUGIN
#ifdef USE_SCSP2
         PROFILE_START("SCSP");
         ScspExec(1);
         PROFILE_STOP("SCSP");
#endif
#else
         if(SCSCore->id == SCSCORE_SCSP2)
         {
            PROFILE_START("SCSP");
            SCSCore->Exec(1);
            PROFILE_STOP("SCSP");
         }
#endif

         yabsys.DecilineCount++;
#ifndef GEKKO
         if(yabsys.DecilineCount == 9)
#else
         if(yabsys.DecilineCount == declinenum - 1)
#endif
         {
            // HBlankIN
            PROFILE_START("hblankin");
            Vdp2HBlankIN();
            PROFILE_STOP("hblankin");
         }

         PROFILE_START("SCU");
         cycles_start = gettime();
         ScuExec(sh2cycles / 2);
         PROFILE_STOP("SCU");
         prof_cycles[2] += gettime() - cycles_start;
		 osd_CyclesSet(2, prof_cycles[2]);

      }

#ifndef SCSP_PLUGIN
#ifndef USE_SCSP2
      PROFILE_START("68K");
      M68KSync();  // Wait for the previous iteration to finish
      PROFILE_STOP("68K");
#endif
#else
      if(SCSCore->id == SCSCORE_SCSP1)
      {
         PROFILE_START("68K");
         M68KSync();  // Wait for the previous iteration to finish
         PROFILE_STOP("68K");
      }
#endif

#ifndef GEKKO
      if (!yabsys.DecilineMode || yabsys.DecilineCount == 10)
#else
      if (!yabsys.DecilineMode || yabsys.DecilineCount == declinenum)
#endif
      {
         // HBlankOUT
         PROFILE_START("hblankout");
         Vdp2HBlankOUT();
         PROFILE_STOP("hblankout");
#ifndef SCSP_PLUGIN
#ifndef USE_SCSP2
         PROFILE_START("SCSP");
         ScspExec();
         PROFILE_STOP("SCSP");
#endif
#else
         if(SCSCore->id == SCSCORE_SCSP1)
         {
            PROFILE_START("SCSP");
            SCSCore->Exec(0);  // 0 is dummy value
            PROFILE_STOP("SCSP");
         }
#endif
         yabsys.DecilineCount = 0;
         yabsys.LineCount++;
         if (yabsys.LineCount == yabsys.VBlankLineCount)
         {
            PROFILE_START("vblankin");
            // VBlankIN
            SmpcINTBACKEnd();
            Vdp2VBlankIN();
            PROFILE_STOP("vblankin");
         }
         else if (yabsys.LineCount == yabsys.MaxLineCount)
         {
            // VBlankOUT
            PROFILE_START("VDP1/VDP2");
            Vdp2VBlankOUT();
            yabsys.LineCount = 0;
            oneframeexec = 1;
            PROFILE_STOP("VDP1/VDP2");
         }
      }

      yabsys.UsecFrac += usecinc;
      PROFILE_START("SMPC");
      cycles_start = gettime();
      SmpcExec(yabsys.UsecFrac >> YABSYS_TIMING_BITS);
      PROFILE_STOP("SMPC");
      prof_cycles[3] += gettime() - cycles_start;
		osd_CyclesSet(2, prof_cycles[3]);

      PROFILE_START("CDB");
      cycles_start = gettime();
      Cs2Exec(yabsys.UsecFrac >> YABSYS_TIMING_BITS);
      PROFILE_STOP("CDB");
      prof_cycles[4] += gettime() - cycles_start;
	  osd_CyclesSet(2, prof_cycles[4]);
      yabsys.UsecFrac &= YABSYS_TIMING_MASK;

#ifndef SCSP_PLUGIN
#ifndef USE_SCSP2
      {
         int cycles;

         PROFILE_START("68K");
         cycles = m68kcycles;
	 saved_centicycles += m68kcenticycles;
         if (saved_centicycles >= 100) {
            cycles++;
            saved_centicycles -= 100;
         }
         M68KExec(cycles);
         PROFILE_STOP("68K");
      }
#endif
#else
      if(SCSCore->id == SCSCORE_SCSP1)
      {
         int cycles;

         PROFILE_START("68K");
         cycles = m68kcycles;
         saved_centicycles += m68kcenticycles;
         if (saved_centicycles >= 100) {
            cycles++;
            saved_centicycles -= 100;
         }
         M68KExec(cycles);
         PROFILE_STOP("68K");
      }
#endif

      PROFILE_STOP("Total Emulation");

   }

#ifndef SCSP_PLUGIN
#ifndef USE_SCSP2
   M68KSync();
#endif
#else
   if(SCSCore->id == SCSCORE_SCSP1)
      M68KSync();
#endif

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void YabauseStartSlave(void)
{
	if (yabsys.emulatebios) {
		SH2GetRegisters(SSH2, &SSH2->regs);
		SSH2->regs.R[15] = 0x06001000;
		SSH2->regs.VBR = 0x06000400;
		SSH2->regs.PC = mem_Read32(0x06000250);
		SH2SetRegisters(SSH2, &SSH2->regs);
	} else {
		SH2PowerOn(SSH2);
		SH2GetRegisters(SSH2, &SSH2->regs);
		SSH2->regs.PC = 0x20000200;
		SH2SetRegisters(SSH2, &SSH2->regs);
	}
	yabsys.IsSSH2Running = 1;
}

//////////////////////////////////////////////////////////////////////////////

void YabauseStopSlave(void)
{
	SH2Reset(SSH2);
	yabsys.IsSSH2Running = 0;
}

//////////////////////////////////////////////////////////////////////////////

u64 YabauseGetTicks(void) {
	return (u64) gettime();
}

//////////////////////////////////////////////////////////////////////////////

void YabauseSetVideoFormat(int type) {
   yabsys.IsPal = type;
   yabsys.MaxLineCount = type ? 313 : 263;
   yabsys.tickfreq = secs_to_ticks(1);
   yabsys.OneFrameTime =
      type ? (yabsys.tickfreq / 50) : (yabsys.tickfreq * 1001 / 60000);
   Vdp2Regs->TVSTAT = Vdp2Regs->TVSTAT | (type & 0x1);
   ScspChangeVideoFormat(type);
   YabauseChangeTiming(yabsys.CurSH2FreqType);
   lastticks = YabauseGetTicks();
}

//////////////////////////////////////////////////////////////////////////////

void YabauseSpeedySetup(void)
{
   u32 data;
   int i;

   if (yabsys.emulatebios)
      BiosInit();
   else
   {
      // Setup the vector table area, etc.(all bioses have it at 0x00000600-0x00000810)
      for (i = 0; i < 0x210; i+=4)
      {
         data = mem_Read32(0x00000600+i);
         mem_Write32(0x06000000+i, data);
      }

      // Setup the bios function pointers, etc.(all bioses have it at 0x00000820-0x00001100)
      for (i = 0; i < 0x8E0; i+=4)
      {
         data = mem_Read32(0x00000820+i);
         mem_Write32(0x06000220+i, data);
      }

      // I'm not sure this is really needed
      for (i = 0; i < 0x700; i+=4)
      {
         data = mem_Read32(0x00001100+i);
         mem_Write32(0x06001100+i, data);
      }

      // Fix some spots in 0x06000210-0x0600032C area
      mem_Write32(0x06000234, 0x000002AC);
      mem_Write32(0x06000238, 0x000002BC);
      mem_Write32(0x0600023C, 0x00000350);
      mem_Write32(0x06000240, 0x32524459);
      mem_Write32(0x0600024C, 0x00000000);
      mem_Write32(0x06000268, mem_Read32(0x00001344));
      mem_Write32(0x0600026C, mem_Read32(0x00001348));
      mem_Write32(0x0600029C, mem_Read32(0x00001354));
      mem_Write32(0x060002C4, mem_Read32(0x00001104));
      mem_Write32(0x060002C8, mem_Read32(0x00001108));
      mem_Write32(0x060002CC, mem_Read32(0x0000110C));
      mem_Write32(0x060002D0, mem_Read32(0x00001110));
      mem_Write32(0x060002D4, mem_Read32(0x00001114));
      mem_Write32(0x060002D8, mem_Read32(0x00001118));
      mem_Write32(0x060002DC, mem_Read32(0x0000111C));
      mem_Write32(0x06000328, 0x000004C8);
      mem_Write32(0x0600032C, 0x00001800);

      // Fix SCU interrupts
      for (i = 0; i < 0x80; i+=4)
         mem_Write32(0x06000A00+i, 0x0600083C);
   }

   // Set the cpu's, etc. to sane states

   // Set CD block to a sane state
   Cs2Area->reg.HIRQ = 0xFC1;
   Cs2Area->isdiskchanged = 0;
   Cs2Area->reg.CR1 = (Cs2Area->status << 8) | ((Cs2Area->options & 0xF) << 4) | (Cs2Area->repcnt & 0xF);
   Cs2Area->reg.CR2 = (Cs2Area->ctrladdr << 8) | Cs2Area->track;
   Cs2Area->reg.CR3 = (Cs2Area->index << 8) | ((Cs2Area->FAD >> 16) & 0xFF);
   Cs2Area->reg.CR4 = (u16) Cs2Area->FAD;
   Cs2Area->satauth = 4;

   // Set Master SH2 registers accordingly
   SH2GetRegisters(MSH2, &MSH2->regs);
   for (i = 0; i < 15; i++)
      MSH2->regs.R[i] = 0x00000000;
   MSH2->regs.R[15] = 0x06002000;
   MSH2->regs.SR.all = 0x00000000;
   MSH2->regs.GBR = 0x00000000;
   MSH2->regs.VBR = 0x06000000;
   MSH2->regs.MACH = 0x00000000;
   MSH2->regs.MACL = 0x00000000;
   MSH2->regs.PR = 0x00000000;
   SH2SetRegisters(MSH2, &MSH2->regs);

   // Set SCU registers to sane states
   ScuRegs->D1AD = ScuRegs->D2AD = 0;
   ScuRegs->D0EN = 0x101;
   ScuRegs->IST = 0x2006;
   ScuRegs->AIACK = 0x1;
   ScuRegs->ASR0 = ScuRegs->ASR1 = 0x1FF01FF0;
   ScuRegs->AREF = 0x1F;
   ScuRegs->RSEL = 0x1;

   // Set SMPC registers to sane states
   //XXX: this should be done in the smpc file
   SMPC_REG_COMREG = 0x10;
   SmpcInternalVars->resd = 0;

   // Set VDP1 registers to sane states
   Vdp1Regs->EDSR = 3;
   Vdp1Regs->localX = 160;
   Vdp1Regs->localY = 112;
   Vdp1Regs->systemclipX2 = 319;
   Vdp1Regs->systemclipY2 = 223;

   // Set VDP2 registers to sane states
   memset(Vdp2Regs, 0, sizeof(Vdp2));
   Vdp2Regs->TVMD = 0x8000;
   Vdp2Regs->TVSTAT = 0x020A;
   Vdp2Regs->CYCA0L = 0x0F44;
   Vdp2Regs->CYCA0U = 0xFFFF;
   Vdp2Regs->CYCA1L = 0xFFFF;
   Vdp2Regs->CYCA1U = 0xFFFF;
   Vdp2Regs->CYCB0L = 0xFFFF;
   Vdp2Regs->CYCB0U = 0xFFFF;
   Vdp2Regs->CYCB1L = 0xFFFF;
   Vdp2Regs->CYCB1U = 0xFFFF;
   Vdp2Regs->BGON = 0x0001;
   Vdp2Regs->PNCN0 = 0x8000;
   Vdp2Regs->MPABN0 = 0x0303;
   Vdp2Regs->MPCDN0 = 0x0303;
   Vdp2Regs->ZMXN0.all = 0x00010000;
   Vdp2Regs->ZMYN0.all = 0x00010000;
   Vdp2Regs->ZMXN1.all = 0x00010000;
   Vdp2Regs->ZMYN1.all = 0x00010000;
   Vdp2Regs->BKTAL = 0x4000;
   Vdp2Regs->SPCTL = 0x0020;
   Vdp2Regs->PRINA = 0x0007;
   Vdp2Regs->CLOFEN = 0x0001;
   Vdp2Regs->COAR = 0x0200;
   Vdp2Regs->COAG = 0x0200;
   Vdp2Regs->COAB = 0x0200;
   VIDSoftVdp2SetResolution(Vdp2Regs->TVMD);
#ifdef GEKKO
   yabsys.VBlankLineCount = 224+(Vdp2Regs->TVMD & 0x30);
#endif
}

//////////////////////////////////////////////////////////////////////////////

int YabauseQuickLoadGame(void)
{
   partition_struct * lgpartition;
   u8 *buffer;
   u32 addr;
   u32 size;
   u32 blocks;
   unsigned int i, i2;
   dirrec_struct dirrec;

   Cs2Area->outconcddev = Cs2Area->filter + 0;
   Cs2Area->outconcddevnum = 0;

   // read in lba 0/FAD 150
   if ((lgpartition = Cs2ReadUnFilteredSector(150)) == NULL)
      return -1;

   // Make sure we're dealing with a saturn game
   buffer = lgpartition->block[lgpartition->numblocks - 1]->data;

   YabauseSpeedySetup();

   if (memcmp(buffer, "SEGA SEGASATURN", 15) == 0)
   {
      // figure out how many more sectors we need to read
      size = (buffer[0xE0] << 24) |
             (buffer[0xE1] << 16) |
             (buffer[0xE2] << 8) |
              buffer[0xE3];
      blocks = size >> 11;
      if ((size % 2048) != 0)
         blocks++;


      // Figure out where to load the first program
      addr = (buffer[0xF0] << 24) |
             (buffer[0xF1] << 16) |
             (buffer[0xF2] << 8) |
              buffer[0xF3];

      // Free Block
      lgpartition->size = 0;
      Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
      lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
      lgpartition->numblocks = 0;

      // Copy over ip to 0x06002000
      for (i = 0; i < blocks; i++)
      {
         if ((lgpartition = Cs2ReadUnFilteredSector(150+i)) == NULL)
            return -1;

         buffer = lgpartition->block[lgpartition->numblocks - 1]->data;

         if (size >= 2048)
         {
            for (i2 = 0; i2 < 2048; i2++)
               mem_Write8(0x06002000 + (i * 0x800) + i2, buffer[i2]);
         }
         else
         {
            for (i2 = 0; i2 < size; i2++)
               mem_Write8(0x06002000 + (i * 0x800) + i2, buffer[i2]);
         }

         size -= 2048;

         // Free Block
         lgpartition->size = 0;
         Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
         lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
         lgpartition->numblocks = 0;
      }

      SH2WriteNotify(0x6002000, blocks<<11);

      // Ok, now that we've loaded the ip, now it's time to load the
      // First Program

      // Figure out where the first program is located
      if ((lgpartition = Cs2ReadUnFilteredSector(166)) == NULL)
         return -1;

      // Figure out root directory's location

      // Retrieve directory record's lba
      Cs2CopyDirRecord(lgpartition->block[lgpartition->numblocks - 1]->data + 0x9C, &dirrec);

      // Free Block
      lgpartition->size = 0;
      Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
      lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
      lgpartition->numblocks = 0;

      // Now then, fetch the root directory's records
      if ((lgpartition = Cs2ReadUnFilteredSector(dirrec.lba+150)) == NULL)
         return -1;

      buffer = lgpartition->block[lgpartition->numblocks - 1]->data;

      // Skip the first two records, read in the last one
      for (i = 0; i < 3; i++)
      {
         Cs2CopyDirRecord(buffer, &dirrec);
         buffer += dirrec.recordsize;
      }

      size = dirrec.size;
      blocks = size >> 11;
      if ((dirrec.size % 2048) != 0)
         blocks++;

      // Free Block
      lgpartition->size = 0;
      Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
      lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
      lgpartition->numblocks = 0;

      // Copy over First Program to addr
      for (i = 0; i < blocks; i++)
      {
         if ((lgpartition = Cs2ReadUnFilteredSector(150+dirrec.lba+i)) == NULL)
            return -1;

         buffer = lgpartition->block[lgpartition->numblocks - 1]->data;

         if (size >= 2048)
         {
            for (i2 = 0; i2 < 2048; i2++)
               mem_Write8(addr + (i * 0x800) + i2, buffer[i2]);
         }
         else
         {
            for (i2 = 0; i2 < size; i2++)
               mem_Write8(addr + (i * 0x800) + i2, buffer[i2]);
         }

         size -= 2048;

         // Free Block
         lgpartition->size = 0;
         Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
         lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
         lgpartition->numblocks = 0;
      }

      SH2WriteNotify(addr, blocks<<11);

      // Now setup SH2 registers to start executing at ip code
      SH2GetRegisters(MSH2, &MSH2->regs);
      MSH2->regs.PC = 0x06002E00;
      SH2SetRegisters(MSH2, &MSH2->regs);
   }
   else
   {
      // Ok, we're not. Time to bail!

      // Free Block
      lgpartition->size = 0;
      Cs2FreeBlock(lgpartition->block[lgpartition->numblocks - 1]);
      lgpartition->blocknum[lgpartition->numblocks - 1] = 0xFF;
      lgpartition->numblocks = 0;

      return -1;
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////
