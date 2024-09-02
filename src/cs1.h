/*  Copyright 2003-2005 Guillaume Duhamel
    Copyright 2005 Theo Berkau

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

#ifndef __CS1_H__
#define __CS1_H__

#include "cs0.h"
#include "memory.h"

/* cs1.h
 * ---
 * CS1 cartridge area for backup RAM, since we save directly
 * to a file this is a useless feature. */

u8   cs1_Read8(u32);
u16  cs1_Read16(u32);
u32  cs1_Read32(u32);
void cs1_Write8(u32, u8);
void cs1_Write16(u32, u16);
void cs1_Write32(u32, u32);

#endif /*__CS1_H__*/
