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

#include <stdlib.h>
#include "cs1.h"
#include "cs0.h"


u8 cs1_Read8(u32 addr)
{
	return 0xFF;
}


u16 cs1_Read16(u32 addr)
{
	return 0xFFFF;
}


u32 cs1_Read32(u32 addr)
{
	return 0xFFFFFFFF;
}


void cs1_Write8(u32 addr, u8 val)
{
	//Does nothing
}


void cs1_Write16(u32 addr, u16 val)
{
	//Does nothing
}


void cs1_Write32(u32 addr, u32 val)
{
	//Does nothing
}

