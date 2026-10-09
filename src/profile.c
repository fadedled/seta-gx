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

#ifdef DO_PROFILE

#include "profile.h"
#include "gui/osd.h"
#include <ogc/lwp_watchdog.h>


struct ProfCounter {
	char *name;
	u32	is_active;
	u32	value;
	u64	cycles_start;
	u32	calls;
} prof_counters[PROF_MAX];


void prof_AddCounter(u32 indx, char *name)
{
	prof_counters[indx].name = name;
	prof_counters[indx].is_active = 1;
	prof_counters[indx].calls = 0;
}

void prof_Start(u32 indx)
{
	prof_counters[indx].cycles_start = gettime();
	prof_counters[indx].calls++;
}

void prof_StopAndAdd(u32 indx)
{
	prof_counters[indx].value += gettime() - prof_counters[indx].cycles_start;
}

void prof_AddTime(u32 indx, u32 ticks)
{
	prof_counters[indx].value += ticks;
}

void prof_Draw(void)
{
	u32 y = 128+16, x = 0;

	for (u32 i = 0; i < PROF_MAX; ++i) {
		if (prof_counters[i].is_active) {
			x = 8;
			u64 ticks = prof_counters[i].value;
			//if (i == PROF_CDB && prof_counters[i].calls) {
			//	ticks /= prof_counters[i].calls;
			//}
			u32 us = (u32) (ticks_to_microsecs(ticks));
			osd_Printf(x, y, "%4s:%5d %d\n", prof_counters[i].name, us, prof_counters[i].calls);
			prof_counters[i].value = 0;
			prof_counters[i].calls = 0;
			y += 8;
		}
	}
}

#endif /* DO_PROFILE */