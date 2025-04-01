/*  Copyright 2008 Theo Berkau
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
#include <dirent.h>


//#include <di/di.h>
#include "osd/osd.h"
#include "osd/gui.h"

#include "cart.h"
#include "cs2.h"
#include "snd.h"
#include "m68kcore.h"
#include "peripheral.h"
#include "vdp2.h"
#include "yui.h"
#include "memory.h"

extern u8 num_button_WII[9];
extern u8 num_button_CLA[9];
extern u8 num_button_GCC[9];

/* Wii includes */
//#include "syswii.h"


int YuiExec(void);

void InitMenu(void);
void Check_Bios_Exist(void);
void Check_SD_Init(void);
void Check_Read_Dir(void);
void scroll_filelist(s8 delta);
void print_filelist(void);
void scroll_cartlist(s8 delta);
void print_cartlist(void);
void scroll_settinglist(s8 delta);
void print_settinglist(void);

/* Constants */

static int IsPal = 0;
volatile int done=0;
volatile int resetemu=0;
int running=1;

#ifdef AUTOLOADPLUGIN
u8 autoload = 0;
u8 returnhomebrew = 0;
//#include "homebrew.h"
//#define TITLE_ID(x,y) (((u64)(x) << 32) | (y))
char Exit_Dol_File[1024];
#endif

SH2Interface_struct *SH2CoreList[] = {
&SH2Interpreter,
&SH2DebugInterpreter,
NULL
};

PerInterface_struct *PERCoreList[] = {
&PERDummy,
NULL
};

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


//XXX: Change the directory.
char games_dir[64];
char saves_dir[64];
char carts_dir[512];
static char biospath[32];
char prev_itemnum[512];
static char buppath[512];
char settingpath[512];
static char bupfilename[512]="/bkram.bin";
static char isofilename[512]="";



extern int vdp2width, vdp2height;
int wii_width, wii_height;
extern int wii_ir_x;
extern int wii_ir_y;
extern int wii_ir_valid;

#define FILES_PER_PAGE	10

struct file *filelist = NULL;
u32 nb_files = 0;
#define CART_TYPE_NUM 11
#define SETTING_NUM 14

void gotoxy(int x, int y);
void OnScreenDebugMessage(char *string, ...);
void SetMenuItem();
void DoMenu();
int GameExec();
int LoadCue();
int Settings();
int CartSet();
int CartSetExec();
int BiosWith();
int BiosOnlySet();
int FrameskipOff();

void TexCopy_LoRes(u32 w, u32 h);


static s32 selected = 0, start = 0;
static s32 selectedcart = 7;
static int bioswith = 0;
static int frameskipoff = 0;
int specialcoloron = 1;
int eachbackupramon = 1;
int threadingscsp2on = 1;
int eachsettingon = 1;

int menuselect=1;
int setmenuselect=0;
int sounddriverselect=2;
int videodriverselect=2;
int m68kdriverselect=1;
int scspdriverselect=2;
int configurebuttonsselect=0;
int WIIconfigurebuttonsselect=0;
int CLAconfigurebuttonsselect=0;
int GCCconfigurebuttonsselect=0;
int settimingselect=0;
int aboutmenuselect=6;


extern int smpcperipheraltiming;
extern int smpcothertiming;
extern int declinenum;
extern int dividenumclock;

static BOOL flag_mount = FALSE;

GXTexObj tex_lores_fb;

extern int fpstoggle;
extern int fps;

void fat_remount()
{
   if(flag_mount)
   {
      fatUnmount("fat:/");
      flag_mount = FALSE;
   }
   fatInitDefault();
   flag_mount = TRUE;
}

void reset()
{
   resetemu=1;
}

void powerdown()
{
	done = 1;
	//SYS_ResetSystem(SYS_POWEROFF, 0, 0);  //Shutdown wii
	//SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0); // Goto wii menu
}

#define GUI_FILENAMES_MAX		15


GuiItems filename_items = { 0, 0, 0, GUI_FILENAMES_MAX, 0, 10, NULL };
char* game_name_strings;
u32 games_filecount;
u32 games_cursor;

static int fn_compare(const void *x, const void *y) {
        return strcasecmp(((String *)x)->data, ((String *)y)->data);
}

s32 games_LoadList()
{

	u32 str_pos = 0;
	struct dirent *entry;
	DIR *dp = opendir(games_dir);
	if (!dp) {
		return -1;
	}
	//Allocate memory for filename storing
	game_name_strings = (char*) calloc(0x10000, sizeof(*game_name_strings));	//64KB for now
	filename_items.item = (String*) calloc(1024, sizeof(*filename_items.item));	//1024 entries
	games_filecount = 0;

	while ((entry = readdir(dp))) {
		//Copy the string to memory
		u32 len = 0;
		char *str_src = entry->d_name;
		char *str_dst = &game_name_strings[str_pos];
		while (*str_src != 0) {
			*str_dst = *str_src;
			++len;
			++str_dst;
			++str_src;
		}
		++len;
		*str_dst = '\0';
		//XXX: Be careful with this implementation, can have problems
		if (len > 4 && !strcmp(".cue", str_dst - 4)) {
			filename_items.item[games_filecount].len = len;
			filename_items.item[games_filecount].data = &game_name_strings[str_pos];
			games_filecount++;
			str_pos += len;
		} else if (len > 4 && !strcmp(".chd", str_dst - 4)) {
			filename_items.item[games_filecount].len = len;
			filename_items.item[games_filecount].data = &game_name_strings[str_pos];
			games_filecount++;
			str_pos += len;
		}
	}

	filename_items.count = games_filecount;
	qsort(filename_items.item, games_filecount, sizeof(*filename_items.item), fn_compare);
	closedir(dp);
	return 0;
}



#define MENU_BOX_SELECT		0x3

u32 menu_select_index = 0;
u32 axis_button = 0;
u32 axis_button_prev = 0;
u32 menu_num_items = 4;
u32 wait_for_sync = 1;



void menu_Init(void)
{
	//filename_list_init();
	axis_button = 0;
	axis_button_prev = 0;
	wait_for_sync = 1;
}


void menu_Handle(void)
{
	u32 buttons;
	per_updatePads();
	buttons = PER_BUTTONS_DOWN(0);
	u32 btn_a = PAD_DI_C;
	u32 btn_b = PAD_DI_B;
	if (perpad[0].type == PAD_TYPE_GCPAD) {
		btn_a = PAD_DI_B;
		btn_b = PAD_DI_A;
	} else if (perpad[0].type == PAD_TYPE_NONE) {
		return;
	}

	if(buttons & PAD_DI_DOWN) {
		if (filename_items.cursor == filename_items.count - 1) {
			filename_items.cursor = 0;
			filename_items.disp_offset = 0;
		}
		else {
			u32 disp_end = filename_items.disp_offset + filename_items.disp_count;
			if ((disp_end == filename_items.cursor + 5) & (disp_end < filename_items.count) ) {
				filename_items.disp_offset = (filename_items.cursor + 6) - filename_items.disp_count;
			}
			filename_items.cursor++;
		}
	}
	else if(buttons & PAD_DI_UP) {
		if (filename_items.cursor == 0) {
			filename_items.cursor = filename_items.count - 1;
			filename_items.disp_offset = (filename_items.count - filename_items.disp_count);
			filename_items.disp_offset = (filename_items.disp_offset > filename_items.count ? 0 : filename_items.disp_offset);
		}
		else {
			if ((filename_items.disp_offset + 5 == filename_items.cursor) & (filename_items.disp_offset > 0)) {
				filename_items.disp_offset = (filename_items.cursor - 6);
			}
			filename_items.cursor--;
		}
	}
	else if (buttons & btn_a) {	//Uses the C button as A button
		strcpy(isofilename, games_dir);
		strcat(isofilename, "/");
		strcat(isofilename, filename_items.item[filename_items.cursor].data);
		YuiExec();
		SVI_SetResolution(0x80D2);
		//iso_loaded = 1;
	}
	else if (buttons & btn_b) {	//Uses the B button as B button
		//XXX: ask to quit?
		mem_Deinit();
		exit(0);
		//SYS_ResetSystem(SYS_SHUTDOWN, 0, 0);
	}
	//gui_menu_boxes[MENU_BOX_SELECT].color_bg = (((gui_menu_boxes[MENU_BOX_SELECT].color_bg) + 0x040802) & 0x7F7F7F00) | 0xAA;
}

int ResetSettings(void)
{
   bioswith = 0;
   selectedcart = 7;
   videodriverselect = 2;
   sounddriverselect = 2;
   m68kdriverselect = 1;
   scspdriverselect = 2;
   //selected;
   frameskipoff = 0;
   specialcoloron = 1;
   smpcperipheraltiming = 1000;
   smpcothertiming = 1050;
   eachbackupramon = 1;
   declinenum = 15;
   dividenumclock = 1;
   threadingscsp2on = 0;
// buttons
   num_button_WII[0] = 3;
   num_button_WII[1] = 1;
   num_button_WII[2] = 0;
   num_button_WII[3] = 4;
   num_button_WII[4] = 2;
   num_button_WII[5] = 6;
   num_button_WII[6] = 6;
   num_button_WII[7] = 6;
   num_button_WII[8] = 5;
   num_button_CLA[0] = 3;
   num_button_CLA[1] = 1;
   num_button_CLA[2] = 0;
   num_button_CLA[3] = 2;
   num_button_CLA[4] = 9;
   num_button_CLA[5] = 8;
   num_button_CLA[6] = 7;
   num_button_CLA[7] = 6;
   num_button_CLA[8] = 5;
   num_button_GCC[0] = 3;
   num_button_GCC[1] = 1;
   num_button_GCC[2] = 0;
   num_button_GCC[3] = 2;
   num_button_GCC[4] = 8;
   num_button_GCC[5] = 5;
   num_button_GCC[6] = 7;
   num_button_GCC[7] = 6;
   num_button_GCC[8] = 4;

   SetMenuItem();

   menuselect=1;
   return 0;
}


int main(int argc, char **argv)
{
	char *device_path = NULL;
	L2Enhance();
	SYS_SetResetCallback(reset);
	SYS_SetPowerCallback(powerdown);

	usleep(500000);

	//XXX: change the mount code... just check once...
	//fatInitDefault();
	if(fatMountSimple("sd", &__io_wiisd)) {
		device_path = "sd:/";
	} else if (fatMountSimple("usb", &__io_usbstorage)){
		device_path = "usb:/";
	} else {
		//Device not found notice
	}

	//Copy the routes
	sprintf(biospath, "%s%s", device_path, "apps/SetaGX/bios.bin");
	sprintf(games_dir, "%s%s", device_path, "vgames/Saturn");
	sprintf(saves_dir, "%s%s", device_path, "saves/Saturn");

	bioswith = 1;			//bioswith
	selectedcart = 7;		//cartridge
	videodriverselect = 2;	//videodriver
	sounddriverselect = 0;	//sounddriver
	m68kdriverselect = 1;	//m68kdriver
	scspdriverselect = 2;	//scsodriver
	selected = 0;			//fileselected
	if(selected >= FILES_PER_PAGE) start = selected - FILES_PER_PAGE + 1;
	frameskipoff = 1;	//frameskipoff
	specialcoloron = 1;	//specialcoloron
	smpcperipheraltiming = 1000; //smpcperipheraltiming
	smpcothertiming = 1050; //smpcothertiming
	eachbackupramon = 1; //eachbackupramon
	declinenum = 10; //declinenum //STANDARD
	dividenumclock = 1; //dividenumclock
	threadingscsp2on = 0; //threadingscsp2on	//No threading

	SVI_Init();

	mem_allocate();
	snd_Init();
	menu_Init();
	games_LoadList();
	per_Init();

	osd_ProfAddCounter(PROF_SH2M, "SH2M");
	osd_ProfAddCounter(PROF_SH2S, "SH2S");
	osd_ProfAddCounter(PROF_M68K, "M68K");
	osd_ProfAddCounter(PROF_SCSP, "SCSP");
	osd_ProfAddCounter(PROF_SCU, "SCU");
	osd_ProfAddCounter(PROF_SMPC, "SMPC");
	osd_ProfAddCounter(PROF_VDP1, "VDP1");
	osd_ProfAddCounter(PROF_VDP2, "VDP2");
	osd_ProfAddCounter(PROF_CDB, "CDB");

	CON_EnableBarnacle(EXI_CHANNEL_0, EXI_DEVICE_1);
	printf("what is this number? %d\n", 42);

	while(1) {
		menu_Handle();
		gui_Draw(&filename_items);
		SVI_CopyFrame();
		SVI_SwapBuffers(1);
	}

	return 0;
}


int YuiExec()
{
	yabauseinit_struct yinit;
	int ret;
	FILE *fp;
	//XXX: wait for Vsync is off...
	wait_for_sync = 0;

	WPAD_SetDataFormat(WPAD_CHAN_ALL,WPAD_FMT_BTNS_ACC_IR);

	memset(&yinit, 0, sizeof(yabauseinit_struct));
	//yinit.percoretype = PERCORE_WIICLASSIC;
	yinit.sh2coretype = SH2CORE_INTERPRETER;
	yinit.vidcoretype = videodriverselect;
	yinit.scspcoretype = scspdriverselect;
	yinit.sndcoretype = sounddriverselect;
	yinit.cdcoretype = CDCORE_ISO;
	yinit.m68kcoretype = m68kdriverselect;
	yinit.carttype = 0;
	yinit.regionid = REGION_AUTODETECT;
	if (!bioswith || ((fp = fopen(biospath, "rb")) == NULL)) {
		yinit.biospath = NULL;
		bioswith = 0;
	} else {
		yinit.biospath = biospath;
		fclose(fp);
	}
	yinit.cdpath = isofilename;
	strcpy(buppath, saves_dir);
	strcat(buppath, bupfilename);
	yinit.buppath = buppath;
	yinit.mpegpath = NULL;
	yinit.cartpath = NULL;
	yinit.netlinksetting = NULL;
	yinit.videoformattype = IsPal;
	yinit.clocksync = 0;
	yinit.basetime = 0;
	yinit.usethreads = threadingscsp2on;

	// Hijack the fps display
	//VIDSoft.OnScreenDebugMessage = OnScreenDebugMessage;
	done = 0;
	if ((ret = YabauseInit(&yinit)) == 0)
	{
		ScspSetFrameAccurate(1);
		YabauseSetDecilineMode(1);


      while(!done)
      {
			u32 result = 0;
			if (per_updatePads()) {
				done = 1;
				result = 0;
			} else {
				char msg[128] = {0};
				sprintf(msg, "MEM1 used: %d, available: %d", 0x01800000 - SYS_GetArena1Size(), SYS_GetArena1Size());
				osd_MsgAdd(20, 28, 0xFFFFFFFF, msg);
				result = YabauseExec();
			}
		//XXX: recover memory used...
         if (result != 0){
            return -1;
		}
         if (resetemu)
         {
            YabauseReset();
            resetemu = 0;
            SYS_SetResetCallback(reset);
         }
      }
      if(strlen(cdip->itemnum)!=0)
         strcpy(prev_itemnum, cdip->itemnum);
      YabauseDeInit();
#ifdef AUTOLOADPLUGIN
      if(autoload) autoload=0;
#endif
      done=0;
      resetemu=0;
   }
   else
   {
      while(!done)
         VIDEO_WaitVSync();
   }
	wait_for_sync = 1;
	return 0;
}

void YuiErrorMsg(const char *string)
{
   if (strncmp(string, "Master SH2 invalid opcode", 25) == 0)
   {
      if (!running)
         return;
      running = 0;
      printf("%s\n", string);
   }
}


