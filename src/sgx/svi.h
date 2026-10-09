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

#ifndef __SVI_H__
#define __SVI_H__

#include <gccore.h>

#define SS_DISP_WIDTH		320
#define SS_DISP_HEIGHT		224
#define SVI_DISP_HIGHRES	1
#define SVI_DISP_INTERLACE	2

extern u32 svi_disp;

void SVI_Init(void);
void SVI_CopyXFB(u32 x, u32 y);
void SVI_SwapBuffers(u32 wait_vsync);
void SVI_SetResolution(u32 tvmd);
void SVI_RestoreTVMode(void);
void SVI_CopyFrame(void);
void SVI_ClearFrame(void);
void SVI_ClearAllFrames(void);
void SVI_ResetViewport(void);
void SVI_EnableNativeVideo(u32 enable);

#endif //__SVI_H__
