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


#ifndef __SH2_COMPILER_H__
#define __SH2_COMPILER_H__

#include <gccore.h>


extern u32 *drc_blocks;

void HashClearAll(void);
void HashSetDirty(u32 key);
void HashClearRange(u32 start_addr, u32 end_addr);

u32 _jit_GenBlock(u32 addr);

void sh2_DrcInit(void);
void sh2_DrcReset(void);
extern void sh2_DrcExec(SH2 *sh, s32 cycles);

#endif /* __SH2_COMPILER_H__ */