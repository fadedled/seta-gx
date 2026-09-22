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

#ifndef __CORE_CONFIG_H__
#define __CORE_CONFIG_H__

#include <gctypes.h>

/* Config Devices */
#define CONFIG_DEVICE_SD	1
#define CONFIG_DEVICE_USB 	2

/* Config Errors */
#define CONFIG_ERROR_NO_FILE			1
#define CONFIG_ERROR_NO_CORRECT_PATH	2


/* Config directory paths */
enum {
	CONF_BIOS,
	CONF_GAMES,
	CONF_SAVES,
	//No need to check dirs below
	CONF_CARTS,
	CONF_COVERS,
	CONF_MAXDIR
};

/* Needed directory path */
#define CONF_DIR_NEEDED             (CONF_SAVES+1)


/* Config options */
enum {
	CONF_OPT_SHOW_FPS,
	CONF_OPT_NATIVE_VIDEO,
	//CONF_OPT_INPUT_TYPE,
	CONF_OPT_MAX
};

#define CONF_OPT_SHOW_PROFILE       8 /* Hardcoded value */
#define CONF_GET_OPT(opt)           ((conf.options >> (opt*2)) & 0x3)


typedef struct CoreConfig_t {
	char dir[CONF_MAXDIR][64];
	u32 per_mapping[16];
	u32 devices;
	u32 options;
} CoreConfig;

extern CoreConfig conf;

const char* config_Load(void);
void  config_SetOption(u32 opt, u32 value);
void  config_Save(void);
void  config_SetPerTypeMapping(u32 type, u32 mapping);


#endif /*__CORE_CONFIG_H__*/
