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

#include <unistd.h>
#include <string.h>
#include "yabause.h"
#include "cart.h"
#include "cs2.h"
#include "config.h"
#include "debug.h"
#include "error.h"
#include "memory.h"
#include "m68kcore.h"
#include "peripheral.h"
#include "profile.h"
#include "scsp.h"
#include "scu.h"
#ifdef USE_SH2_OLD
	#include "sh2old/sh2core.h"
#else
	#include "sh2/sh2.h"
#endif
#include "osd/gui.h"
#include "osd/osd.h"
#include "smpc.h"
#include "vdp1.h"
#include "vdp2.h"
#include "sgx/svi.h"




#include <sys/time.h>

#include <ogc/lwp_watchdog.h>
#include <string.h>
extern char saves_dir[128];
extern int eachbackupramon;
int declinenum = 10; //10 in original yabause
int dividenumclock = 1; //1 in original yabause

#ifdef SYS_PROFILE_H
 #include SYS_PROFILE_H
#else
 #define DONT_PROFILE
 #include "profile.h"
#endif

//////////////////////////////////////////////////////////////////////////////

//Dynarec sh2

yabsys_struct yabsys;
char cd_path[512];
char bios_path[256];
char bup_path[512];
u64 tickfreq;

int lagframecounter;
int LagFrameFlag;
int framelength=16;
int framecounter;
u32 mem1_size;

//////////////////////////////////////////////////////////////////////////////
//Core Scheduler, based on SCU

/*

enum SkdEvent {
	EV_VDP1
	EV_VDP2
	EV_SCU_TIMER1
	EV_SCSP_SAMPLE
	EV_CDB_STATE
	EV_CDB_COMMAND
	EV_SMPC_COMMAND
};

typedef void (*SkdCallback)(void);

void skd_Reset()
{

}

void skd_RegisterEvent(SkdEvent ev, SkdCallback cb)
{

}

void skd_RescheduleEvent(SkdEvent ev, u32 cycles)
{

}

u32 skd_CyclesTillNextEvent(void)
{

}

void skd_ExecNextEvent(void)
{

}

void core_Init(CoreConfig *config) {
	mem_Init();


	core_Reset();
}

void core_Reset() {
	clockspeed = CLK_320;
	core_UpdateClockRatios();

	skd_Reset();
	sh2_Reset(&msh2);
	sh2_Reset(&ssh2);
	ss2h_enabled = 0;
	scu_Reset();
	vdp1_Reset();
	vdp2_Reset();
	smpc_Reset();
	scsp_Reset();
	cdb_Reset();
}


u32 core_LoadBIOS()
{

}

#define SYNC_CYCLES 32

struct CoreConfig {
	//u32 sync_cycles = 32;
} core;

u32 core_Run()
{
	u32 cycles = skd_CyclesTillNextEvent();

	u32 exec_cycles = cycles;
	if (scu_IsDMAActive()) {
		scu_Exec(exec_cycles);
	} else {
		exec_cycles = mshSpillover;
		mshSpillover = 0;
		if (ssh2_enabled) {
			u32 slave_cycles = sshSpillover;
			do {
				sh2_Exec(msh2, exec_cycles);
				sh2_Exec(ssh2, slave_cycles);
				scu_Exec(exec_cycles);
			} while (exec_cycles < cycles);
		} else {
			do {
				sh2_Exec(msh2, exec_cycles);
				scu_Exec(exec_cycles);
			} while (exec_cycles < cycles);
		}
	}
	skd_ExecNextEvent();
}


void core_ChangeTiming()
{

}

*/

#define CORE_STATE_RUNNING		0x1
#define CORE_STATE_EXIT_REQ		0x2
#define CORE_STATE_RESET_REQ	0x4
#define CORE_STATE_AUTOLOAD		0x8

struct Core_t {
	u32 state;
} core;

void core_RegisterReset(void)
{
	core.state |= CORE_STATE_RESET_REQ;
}


void core_RegisterExit(void)
{
	core.state |= CORE_STATE_EXIT_REQ;
}


u32 core_IsRunning(void)
{
	return core.state & CORE_STATE_RUNNING;
}


char* core_SetAutoload(char* path, char* filename)
{
	core.state |= CORE_STATE_AUTOLOAD;
	sprintf(cd_path, "%s/%s", path, filename);
	return cd_path;
}


u32 core_Exec(void)
{
	u32 autoload = !(core.state & CORE_STATE_AUTOLOAD);
	if (autoload) {
		//Run GUI and get the selected filename
		if (!gui_Exec()) {
			return 0;
		}
		char *cd_filename = gui_GetSelectedFile();
		if (cd_filename != NULL) {
			sprintf(cd_path, "%s/%s", conf.dir[CONF_GAMES], cd_filename);
		} else {
			cd_path[0] = '\0';
		}
	}
	yabauseinit_struct yinit;
	int ret;
	FILE *fp;

	SVI_EnableNativeVideo(CONF_GET_OPT(CONF_OPT_NATIVE_VIDEO));

	memset(&yinit, 0, sizeof(yabauseinit_struct));
	//yinit.percoretype = PERCORE_WIICLASSIC;
	yinit.sh2coretype = 0;
	yinit.vidcoretype = 2;
	yinit.scspcoretype = 2;
	yinit.sndcoretype = 0;
	yinit.cdcoretype = CDCORE_ISO;
	yinit.m68kcoretype = 1;
	yinit.carttype = 0;
	yinit.regionid = REGION_AUTODETECT;
	sprintf(bios_path, "%s/%s", conf.dir[CONF_BIOS], "bios.bin");
	if (((fp = fopen(bios_path, "rb")) == NULL)) {
		yinit.biospath = NULL;
	} else {
		yinit.biospath = bios_path;
		fclose(fp);
	}

	yinit.mpegpath = NULL;
	yinit.cartpath = NULL;
	yinit.netlinksetting = NULL;
	yinit.videoformattype = 0;
	yinit.clocksync = 0;
	yinit.basetime = 0;
	yinit.usethreads = 0; // off (for now?)

	VIDEO_SetBlack(1);
	SVI_SetResolution(0x00D2);
	VIDEO_WaitVSync();

	core.state = CORE_STATE_RUNNING;

	if ((ret = YabauseInit(&yinit)) == 0) {
		ScspSetFrameAccurate(1);
		YabauseSetDecilineMode(1);
		VIDEO_SetBlack(0);

		while(!(core.state & CORE_STATE_EXIT_REQ)) {
			if (per_updatePads()) {
				core.state |= CORE_STATE_EXIT_REQ;
			} else {
				YabauseExec();
			}
			if (core.state & CORE_STATE_RESET_REQ) {
				YabauseReset();
				core.state = core.state & ~CORE_STATE_RESET_REQ;
			}
		}
		VIDEO_SetBlack(1);
		YabauseDeInit();
	}
	SVI_SetResolution(0x80D2);
	VIDEO_SetBlack(0);
	VIDEO_WaitVSync();
	return autoload;
}

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
#ifdef USE_SH2_OLD
	if (SH2Init(init->sh2coretype) != 0) {
		//YabSetError(YAB_ERR_CANNOTINIT, _("SH2"));
		return -1;
	}
#else
	sh2_Init();
#endif

#ifdef SCSP_PLUGIN
	if (ScspChangeCore(init->scspcoretype) != 0) {
		YabSetError(YAB_ERR_CANNOTINIT, "SCSP");
		return -1;
	}
#endif

	if (VideoInit(1) != 0) {
		YabSetError(YAB_ERR_CANNOTINIT, "Video");
		return -1;
	}

	if (Cs2Init(init->carttype, init->cdcoretype, cd_path, NULL, NULL, NULL) != 0) {
		YabSetError(YAB_ERR_CANNOTINIT, "CS2");
		return -1;
	}

	Cs2GetRegionID(); //for getting itemnum

	if (cart_Init(init->carttype, init->cartpath) != 0) {
		YabSetError(YAB_ERR_CANNOTINIT, "Cartridge");
		return -1;
	}

	MappedMemoryInit();

	//Get the BUP savefile
	char gameid[16];
	u32 j = 0;
	for (u32 i = 0; i < 10; ++i) {
		char c = cdip->itemnum[i];
		if ( '-' <= c && c <= 'z') {
			gameid[j++] = c;
		}
	}
	gameid[j] = '\0';
	sprintf(bup_path, "%s/%s.sav", conf.dir[CONF_SAVES], gameid);
	bup_ReadSave(bup_path);
	bup_ram_written = 0;

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

	if (Vdp1Init() != 0) {
		YabSetError(YAB_ERR_CANNOTINIT, "VDP1");
		return -1;
	}
	if (Vdp2Init() != 0) {
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


	if (LoadBios(init->biospath) != 0) {
		YabSetError(YAB_ERR_FILENOTFOUND, (void *)init->biospath);
		return -2;
	}
	YabauseResetNoLoad();

	//NOTE: QuickLoad is disabled for now
	yabsys.usequickload = 0;
	if (yabsys.usequickload) {
		if (YabauseQuickLoadGame() != 0) {
			YabauseResetNoLoad();
		}
	}

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void YabauseDeInit(void) {
#ifdef USE_SH2_OLD
	SH2DeInit();
#else
	sh2_Deinit();
#endif
	bup_WriteSave(bup_path);
	//if (SaveBackupRam(bup_path))
    //     YabSetError(YAB_ERR_FILEWRITE, (void *)bup_path);

   cart_Deinit();
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
#ifdef USE_SH2_OLD
	SH2Reset(MSH2);
#else
	sh2_Reset(&msh2);
#endif
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
#ifdef USE_SH2_OLD
	SH2PowerOn(MSH2);
#else
	sh2_PowerOn(&msh2);
#endif

}

//////////////////////////////////////////////////////////////////////////////

void YabauseReset(void) {
	YabauseResetNoLoad();
	if (yabsys.usequickload) {
		if (YabauseQuickLoadGame() != 0) {
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
      /* 11.2896MHz / 50Hz / 313 lines / declinenum calls/line = 721.4 cycles/ declinenum call */
      m68kcycles = yabsys.DecilineMode ? (7214/declinenum)/10 : 716;
      m68kcenticycles = yabsys.DecilineMode ? 7214-((7214/declinenum)/10)*100 : 20;
   }
   else
   {
      /* 11.2896MHz / 60Hz / 263 lines / declinenum calls/line = 715.4 cycles/ declinenum call */
      m68kcycles = yabsys.DecilineMode ? (7154/declinenum)/10 : 716;
      m68kcenticycles = yabsys.DecilineMode ? 7154-((7154/declinenum)/10)*100 : 20;
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
	mem1_size = SYS_GetArena1Size();
	while (!oneframeexec) {
      if (yabsys.DecilineMode) {
         // Since we run the SCU with half the number of cycles we send
         // to SH2Exec(), we always compute an even number of cycles here
         // and leave any odd remainder in SH2CycleFrac.
         yabsys.SH2CycleFrac += cyclesinc;
         u32 sh2cycles = (yabsys.SH2CycleFrac >> (YABSYS_TIMING_BITS + 1)) << 1;
         yabsys.SH2CycleFrac &= ((YABSYS_TIMING_MASK << 1) | 1);
			//Run the main SH2
			prof_Start(PROF_SH2M);
#ifdef USE_SH2_OLD
			SH2Exec(MSH2, sh2cycles);
#else
			//TODO: Cycles are not the same
			sh2_Exec(&msh2, sh2cycles);
#endif
			prof_StopAndAdd(PROF_SH2M);
			//Run the secondary SH2
			prof_Start(PROF_SH2S);
			if (yabsys.IsSSH2Running)
#ifdef USE_SH2_OLD
				SH2Exec(SSH2, sh2cycles);
#else
			//TODO: Cycles are not the same
				sh2_Exec(&ssh2, sh2cycles);
#endif
			prof_StopAndAdd(PROF_SH2S);

#ifndef SCSP_PLUGIN
#ifdef USE_SCSP2
         ScspExec(1);
#endif
#else
         if(SCSCore->id == SCSCORE_SCSP2)
         {
            SCSCore->Exec(1);
         }
#endif

         yabsys.DecilineCount++;
         if(yabsys.DecilineCount == declinenum - 1) {
            // HBlankIN
            Vdp2HBlankIN();
         }

		//SCU
		prof_Start(PROF_SCU);
		ScuExec(sh2cycles / 2);
		prof_StopAndAdd(PROF_SCU);
      }

#ifndef SCSP_PLUGIN
#ifndef USE_SCSP2
      M68KSync();  // Wait for the previous iteration to finish
#endif
#else
      if(SCSCore->id == SCSCORE_SCSP1)
      {
		 M68KSync();  // Wait for the previous iteration to finish
      }
#endif

      if (!yabsys.DecilineMode || yabsys.DecilineCount == declinenum) {
         // HBlankOUT
         Vdp2HBlankOUT();
#ifndef SCSP_PLUGIN
#ifndef USE_SCSP2
         ScspExec();
#endif
#else
         if(SCSCore->id == SCSCORE_SCSP1) {
            SCSCore->Exec(0);  // 0 is dummy value
         }
#endif
         yabsys.DecilineCount = 0;
         yabsys.LineCount++;
         if (yabsys.LineCount == yabsys.VBlankLineCount)
         {
            // VBlankIN
            SmpcINTBACKEnd();
            Vdp2VBlankIN();
         }
         else if (yabsys.LineCount == yabsys.MaxLineCount)
         {
            // VBlankOUT
            Vdp2VBlankOUT();
            yabsys.LineCount = 0;
            oneframeexec = 1;
         }
      }

      yabsys.UsecFrac += usecinc;
	  prof_Start(PROF_SMPC);
      SmpcExec(yabsys.UsecFrac >> YABSYS_TIMING_BITS);
      prof_StopAndAdd(PROF_SMPC);

	//CD BLOCK
	Cs2Exec(yabsys.UsecFrac >> YABSYS_TIMING_BITS);
	yabsys.UsecFrac &= YABSYS_TIMING_MASK;

#ifndef SCSP_PLUGIN
#ifndef USE_SCSP2
      {
         int cycles;

         cycles = m68kcycles;
	 saved_centicycles += m68kcenticycles;
         if (saved_centicycles >= 100) {
            cycles++;
            saved_centicycles -= 100;
         }
         M68KExec(cycles);
      }
#endif
#else
      if(SCSCore->id == SCSCORE_SCSP1)
      {
         int cycles;

         cycles = m68kcycles;
         saved_centicycles += m68kcenticycles;
         if (saved_centicycles >= 100) {
            cycles++;
            saved_centicycles -= 100;
         }
         M68KExec(cycles);
      }
#endif
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void YabauseStartSlave(void)
{
#ifdef USE_SH2_OLD
	SH2PowerOn(SSH2);
	SH2GetRegisters(SSH2, &SSH2->regs);
	SSH2->regs.PC = 0x20000200;
	SH2SetRegisters(SSH2, &SSH2->regs);
#else
	sh2_PowerOn(&ssh2);
	ssh2.pc = 0x20000200;
#endif
	yabsys.IsSSH2Running = 1;
}

//////////////////////////////////////////////////////////////////////////////

void YabauseStopSlave(void)
{
#ifdef USE_SH2_OLD
	SH2Reset(SSH2);
#else
	sh2_Reset(&ssh2);
#endif
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
#ifdef USE_SH2_OLD
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
#else
	for (i = 0; i < 15; i++) {
		msh2.r[i] = 0x00000000;
	}
	msh2.r[15] = 0x06002000;
	msh2.sr = 0x00000000;
	msh2.gbr = 0x00000000;
	msh2.vbr = 0x06000000;
	msh2.mach = 0x00000000;
	msh2.macl = 0x00000000;
	msh2.pr = 0x00000000;
#endif

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
   SVI_SetResolution(Vdp2Regs->TVMD);
   yabsys.VBlankLineCount = 224+(Vdp2Regs->TVMD & 0x30);
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

#ifdef USE_SH2_OLD
		SH2WriteNotify(0x6002000, blocks<<11);
#else
		sh2_WriteNotify(0x6002000, blocks<<11);
#endif
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

#ifdef USE_SH2_OLD
		SH2WriteNotify(addr, blocks<<11);
      // Now setup SH2 registers to start executing at ip code
      SH2GetRegisters(MSH2, &MSH2->regs);
      MSH2->regs.PC = 0x06002E00;
      SH2SetRegisters(MSH2, &MSH2->regs);
#else
	  	msh2.pc = 0x06002E00;
		//msh2.r[15] = Cs2GetMasterStackAddress();
		sh2_WriteNotify(addr, blocks<<11);
#endif

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
//Per game settings for cartridges
#if 0
u64 gameid_1MB[] = {
	"T-1521G/x20" //Astra Superstars
	"T-9904G/x20" //Cotton 2
	"T-3111G/x20" //Metal Slug
	"T-3114G/x20"
	"T-22206G"    //Noël 3
	"T-22205G"
	"T-1217G/x20" //Cyberbots: FullMetal Madness
	"T-1216G/x20"
	"GS-9107/x20" //Fighter's History Dynamite
	"GS-9153/x20"
	"T-20109G"    //Friends: Seishun no Kagayaki
	"T-14411G"    //Groove On Fight: Gouketsuji Ichizoku 3
	"T-14413G"
	"T-1215G/x20" //Marvel Super Heroes
	"T-1214H/x20"
	"T-7032H-"
	"T-7032H-"
	"T-7032H-"
	"T-20114G"    //Pia Carrot e Youkoso!! 2
	"T-3105G/x20" //Real Bout Garou Densetsu Best Collection
	"T-3124G/x20"
	"T-3117G/x20" //Real Bout Garou Densetsu Special
	"T-3119G/x20"
	"T-3123G/x20" //Samurai Spirits Best Collection
	"T-3106G/x20" //Samurai Spirits Zankurou Musouken
	"T-3104G/x20"
	"T-16509G"    //Super Real Maajan P7
	"T-16510G"
	"T-3108G/x20" //The King of Fighters '96
	"T-3109G/x20"
	"T-3110G/x20" //The King of Fighters '96 + '95 (Gentei KOF Double Pack)
	"T-3120G/x20" //The King of Fighters '97
	"T-3121G/x20"
	"T-3125G/x20" //The King of Fighters Best Collection
	"T-1516G/x20" //Waku Waku 7
	"T-1515G/x20"
};

u64 gameid_4MB[] = {
	"T-1230G/x20" //Pocket Fighter†
	"T-1247G/x20" //Street Fighter Zero 3
	"T-1246G/x20"
	"T-1228G/x20" //Vampire Savior: The Lord of Vampire
	"T-1229G/x20"
	"T-1227G/x20" //X-Men vs. Street Fighter
	"T-1226G/x20"
	"T-1239G/x20" //Marvel Super Heroes vs. Street Fighter
	"T-1238G/x20"
	"T-1249G/x20" //Final Fight Revenge
	"T-1248G/x20"
	"T-1224G/x20" //Dungeons & Dragons Collection
	"T-1245G/x20"
};

static u32 __core_CheckID(u64 id, u64 *id_list, u32 entries) {
	for (u32 i = 0; i < entries; ++i) {
		if (*id_list == id) {
			return 1;
		}
		id_list++;
	}
	return 0;
}

static u32 core_DetectCartridge(u8 *game_id) {
	u64 id = *((u64*) game_id);
	__core_CheckID(id, gameid_1MB, sizeof(gameid_1MB) / sizeof(*gameid_1MB));
	__core_CheckID(id, gameid_4MB, sizeof(gameid_4MB) / sizeof(*gameid_4MB));
	//__core_CheckID(id, gameid_1MB, sizeof(gameid_1MB) / sizeof(*gameid_1MB));

}
	#endif