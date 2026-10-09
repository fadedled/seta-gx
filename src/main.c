/********************************************************************************
 * Copyright (C) 2026 Evoca
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <malloc.h>
#include <wiiuse/wpad.h>
#include <ogcsys.h>
#include <gccore.h>
#include <fat.h>
#include <sdcard/wiisd_io.h>


//#include <di/di.h>
#include "gui/gui.h"
#include "sgx/svi.h"

#include "cart.h"
#include "cs2.h"
#include "snd.h"
#include "m68kcore.h"
#include "peripheral.h"
#include "vdp2.h"
#include "scsp.h"
#include "config.h"
#include "profile.h"
#include "yabause.h"
#include "memory.h"

/* Plugins (REMOVE THESE EVENTUALLY) */
CDInterface *CDCoreList[] = {
&DummyCD,
&ISOCD,
NULL
};

#ifdef SCSP_PLUGIN
SCSPInterface_struct *SCSCoreList[] = {
&SCSDummy,
&SCSScsp2,
&SCSScsp2,
NULL
};
#endif


void main_ResetCallback(void)
{
   	if(core_IsRunning()) {
		core_RegisterReset();
	}
}

void main_PowerCallback(void)
{
	if(core_IsRunning()) {
		core_RegisterExit();
	} else {
		//If in GUI then flatout exit
		exit(0);
	}
}


#define CACHE_PAGES 		128
#define SECTORS_PER_PAGE	32

String msg_box_btns[2] = {GUI_STRING("Quit"), GUI_STRING("Retry")};

//Usage
// sega-gx <game_filepath>
int main(int argc, char **argv)
{
	L2Enhance();
	SYS_SetResetCallback(main_ResetCallback);
	SYS_SetPowerCallback(main_PowerCallback);

	//Init video/gui
	SVI_Init();
	per_Init();
	gui_Init();

try_init:
	//Init of devices
	if (!fatInit(CACHE_PAGES, true)) {
		if (gui_MsgBox(msg_box_btns, 2, "Could not open any devices!") == 0) {
			return 0;
		}
		goto try_init;
	}

	//Load the config file
	const char* err_dir;
	if ((err_dir = config_Load())) {
		if (gui_MsgBox(msg_box_btns, 2, "Directory does not exist: %s", err_dir) == 0) {
			return 0;
		}
		goto try_init;
	}

	//Autoload the gamefile
	if (argc > 2 && argv[1] && argv[2]) {
		//Add the given file, if it fails then abort
		char* fname = core_SetAutoload(argv[1], argv[2]);
		if (access(fname, F_OK)) { // abort
			gui_MsgBox(msg_box_btns, 1,"Could not find file: %s", argv[2]);
			return 0;
		}
	} else if (gui_LoadGameList() == 0) {
		if (gui_MsgBox(msg_box_btns, 2, "No games found in: %s", conf.dir[CONF_GAMES]) == 0) {
			return 0;
		}
		goto try_init;
	}

	//Init everything else
	mem_allocate();
	snd_Init();

	prof_AddCounter(PROF_SH2M, "SH2M");
	prof_AddCounter(PROF_SH2S, "SH2S");
	prof_AddCounter(PROF_M68K, "M68K");
	prof_AddCounter(PROF_SCSP, "SCSP");
	prof_AddCounter(PROF_SCU, "SCU");
	prof_AddCounter(PROF_SMPC, "SMPC");
	prof_AddCounter(PROF_VDP1, "VDP1");
	prof_AddCounter(PROF_VDP2, "VDP2");
	prof_AddCounter(PROF_CDB, "CDB");
	prof_AddCounter(PROF_TOTAL, "TOTL");

	//Run the core
	while(core_Exec());

	return 0;
}
