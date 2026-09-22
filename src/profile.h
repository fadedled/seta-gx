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

#ifndef __PROFILE_H__
#define __PROFILE_H__

#include <gctypes.h>

#ifdef DO_PROFILE


//Profile index names for ease of use, if you can modify the names to your liking
enum ProfIndx {
	PROF_SH2M,
	PROF_SH2S,
	PROF_M68K,
	PROF_SCSP,
	PROF_SCU,
	PROF_SMPC,
	PROF_VDP1,
	PROF_VDP2,
	PROF_CDB,
	PROF_TOTAL,
	PROF_MAX 	 //Always last value
};

void prof_AddCounter(u32 indx, char *name);
void prof_AddTime(u32 indx, u32 ticks);
void prof_Start(u32 indx);
void prof_StopAndAdd(u32 indx);
void prof_Draw(void);	//All counters reset to 0

#else

//Ignore function calls
#define prof_AddCounter(...)
#define prof_AddTime(...)
#define prof_Start(...)
#define prof_StopAndAdd(...)
#define prof_Draw(...)

#endif

#endif /* __PROFILE_H__ */

