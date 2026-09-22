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

#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "peripheral.h"


#define CONF_MAX_DEVICES			2

#define CONF_VIDEO_MODE_NATIVE		1
#define CONF_USE_CART				2
#define CONF_SKIP_BIOS				4
#define CONF_USE_MULTITAP			8


static const char* __conf_device_names[CONF_MAX_DEVICES] = {
	"sd",
	"usb",
};

static const char* __conf_default_paths[CONF_MAXDIR] = {
	[CONF_BIOS]   = "apps/SetaGX",
	[CONF_CARTS]  = "apps/SetaGX",
	[CONF_GAMES]  = "vgames/Saturn",
	[CONF_SAVES]  = "saves/Saturn",
	[CONF_COVERS] = "covers/Saturn"
};

CoreConfig conf;			/* Global Core Configuration */


const char* config_Load(void)
{
	u32 loaded_path = 0;
	char tmpstr[64];
	// Check and load config file settings for all devices
	for (u32 i = 0; i < CONF_MAX_DEVICES; ++i) {
		FILE *fp;
		sprintf(tmpstr, "%s:/apps/SetaGX/setagx.cfg", __conf_device_names[i]);
		fp = fopen(tmpstr, "r");
		if (fp) {
			while(fgets(tmpstr, 64, fp) != NULL) {
				switch(tmpstr[0]) { //Check option
					case 'b': sscanf(tmpstr, "bios = %s\n",  conf.dir[CONF_BIOS]); loaded_path |= (1 << CONF_BIOS); break;
					case 'c':
						if (tmpstr[1] == 'a') {
							sscanf(tmpstr, "carts = %s\n", conf.dir[CONF_CARTS]); loaded_path |= (1 << CONF_CARTS);
						} else if (tmpstr[1] == 'o') {
							sscanf(tmpstr, "covers = %s\n", conf.dir[CONF_COVERS]); loaded_path |= (1 << CONF_COVERS);
						} break;
					case 'g': sscanf(tmpstr, "games = %s\n", conf.dir[CONF_GAMES]); loaded_path |= (1 << CONF_GAMES); break;
					case 's': sscanf(tmpstr, "saves = %s\n", conf.dir[CONF_SAVES]); loaded_path |= (1 << CONF_SAVES); break;
					case 'o': sscanf(tmpstr, "options = %x\n", &conf.options); break;
					default: break;
				}
			}
			fclose(fp);
			break; //stop searching
		}
	}

	//Check if all directories exist for needed paths
	for (int j = 0; j < CONF_DIR_NEEDED; ++j) {
		if ((loaded_path >> j) & 1) { // if path is loaded from config
			if (access(conf.dir[j], F_OK)) {
				return conf.dir[j];
			}
		} else { //check defaults for both devices
			//sd card
			sprintf(conf.dir[j], "%s:/%s", __conf_device_names[0], __conf_default_paths[j]);
			if (access(conf.dir[j], F_OK)) {
				//try with usb now
				sprintf(conf.dir[j], "%s:/%s", __conf_device_names[1], __conf_default_paths[j]);
				if (access(conf.dir[j], F_OK)) {
					return __conf_default_paths[j]; // Show generic path
				}
			}
		}
	}

	//Setup defaults for device for optional paths
	for (u32 j = CONF_DIR_NEEDED; j < CONF_MAXDIR; ++j) {
		if (!((loaded_path >> j) & 1)) { //path was not loaded from config
			//sd card
			sprintf(conf.dir[j], "%s:/%s", __conf_device_names[0], __conf_default_paths[j]);
			if (access(conf.dir[j], F_OK)) {
				//set to usb in case it's there
				sprintf(conf.dir[j], "%s:/%s", __conf_device_names[1], __conf_default_paths[j]);
			}
		}
	}

	return NULL;
}


void config_Save(void)
{
	/*
	char filename[64];
	sprintf(filename, "%s:/apps/SetaGX/setagx.conf", __conf_device_names[used_device]);
	FILE *fp;
	fp = fopen(filename, "w");
	if (!fp) {
		return;
	}

	fprintf(fp, "#Config file for Seta GX\n"
				"#Paths start with sd:// or usb:// depending on preferred device\n"
				"#bios  - path to bios folder (if not set it defaults to d)\n"
				"#games - path to games folder\n"
				"#saves - path to saves folder\n");
	fprintf(fp, "bios=%s\n",  conf.dir[CONF_BIOS]);
	fprintf(fp, "carts=%s\n", conf.dir[CONF_CARTS]);
	fprintf(fp, "games=%s\n", conf.dir[CONF_GAMES]);
	fprintf(fp, "saves=%s\n", conf.dir[CONF_SAVES]);
	fprintf(fp, "options=%x\n", conf.options);
	fclose(fp);
	*/
}

void config_SetOption(u32 opt, u32 value)
{
	u32 mask = 0x3 << (opt*2);
	conf.options = (conf.options & ~mask) | ((value & 0x3) << (opt*2));
}

void config_SetPerTypeMapping(u32 type, u32 mapping)
{
	conf.per_mapping[type] = mapping;
}
