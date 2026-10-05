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

#ifndef __OSD_H__
#define __OSD_H__

/*
 * osd.h
 *--------------------
 * OSD for printing data
 */


#define OSD_COLOR_WHITE		0x90
#define OSD_COLOR_RED		0x91
#define OSD_COLOR_GREEN		0x92
#define OSD_COLOR_BLUE		0x93

//Simplified macro
#define osd_Printf(x, y, fmt, ...)  osd_PrintfColored(x, y, OSD_COLOR_WHITE, fmt, __VA_ARGS__)


void osd_Init(void);
int  osd_PrintfColored(int x, int y, int color, const char* fmt, ...);
void osd_Draw(void);


#endif /*__OSD_H__*/


