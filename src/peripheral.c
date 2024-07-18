/*  Copyright 2005 Guillaume Duhamel
    Copyright 2005-2006 Theo Berkau

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

#include "debug.h"
#include "peripheral.h"
#include <ogcsys.h>
#include <wiiuse/wpad.h>

PerData per_data;

//XXX: this is for gamepad configuration
//static u8 gcpad_bitoffsets[16] = { 0, 0, 0, 0, 0, 0, 0, GC_BIT_A, GC_BIT_B, GC_BIT_Z2, GC_BIT_Y, GC_BIT_X, GC_BIT_Z };
//static u8 wcpad_bitoffsets[16];
//static u8 wpad_bitoffsets[16];


void per_initPads()
{
	per_data.ids[0] = PER_ID_DIGITAL;
}


static u32 per_updateDigitalGC(u32 indx, PADStatus *status)
{
	per_data.ids[indx] = PER_ID_DIGITAL;
	s8 axis_x = status->stickX;
	s8 axis_y = status->stickY;
	u32 btns = status->button | GC_AXIS_TO_DIGITAL(axis_x, axis_y);
	//Use substick as another button
	btns |=  (u32) ((status->substickY > 32) | (status->substickY < -32)) << GC_BIT_Z2;

	//XXX: get the user defined bits for the buttons
	u8 d0 = (u8) ((((btns >> GC_BIT_A) & 1) << PAD_DI_BIT_B) |
			(((btns >> GC_BIT_Z2) & 1) << PAD_DI_BIT_C) |
			(((btns >> GC_BIT_B) & 1) << PAD_DI_BIT_A) |
			(((btns >> GC_BIT_STR) & 1) << PAD_DI_BIT_STR) |
			(((btns >> GC_BIT_UP) & 1) << PAD_DI_BIT_UP) |
			(((btns >> GC_BIT_DOWN) & 1) << PAD_DI_BIT_DOWN) |
			(((btns >> GC_BIT_LEFT) & 1) << PAD_DI_BIT_LEFT) |
			(((btns >> GC_BIT_RIGHT) & 1) << PAD_DI_BIT_RIGHT));

	u8 d1 = (u8) ((((btns >> GC_BIT_L) & 1) << PAD_DI_BIT_L) |
			(((btns >> GC_BIT_Z) & 1) << PAD_DI_BIT_Z) |
			(((btns >> GC_BIT_X) & 1) << PAD_DI_BIT_Y) |
			(((btns >> GC_BIT_Y) & 1) << PAD_DI_BIT_X) |
			(((btns >> GC_BIT_R) & 1) << PAD_DI_BIT_R) | 0x3);

	per_data.data[per_data.data_size] = per_data.ids[indx];
	per_data.data[per_data.data_size + 1] = ~d0;
	per_data.data[per_data.data_size + 2] = ~d1;
	per_data.data_size += 3;		//XXX: get the byte size and add 1

	return (status->button & (PAD_BUTTON_START | PAD_TRIGGER_Z)) == (PAD_BUTTON_START | PAD_TRIGGER_Z);
}


u32 per_updatePads()
{
	//Do port stuff here
	u32 request_quit = 0;
	per_data.data_sent = 0;
	per_data.data_size = 1;
	per_data.data[0] = 0xF0;
	per_data.port2_offset = 1;

	//Fill the Pads with connected
	//XXX: this is only for 4 GC controllers... do same with Wii
	//XXX: handle multitap (only 6 more controllers)
	PADStatus padstatus[PAD_CHANMAX];

	PAD_Read(padstatus);
	for (u32 i = 0; i < PAD_CHANMAX; ++i) {
		if (padstatus[i].err == PAD_ERR_NONE) {
			//XXX: Check what type of pad it is
			//XXX:this hardcodes direct saturn controller... do a multitap insead (if needed)
			per_data.data[0] = 0xF1;
			per_data.data_size = 1;
			request_quit |= per_updateDigitalGC(1, &padstatus[i]);
			per_data.port2_offset = per_data.data_size;
			break;
		}
	}
	//XXX: no 2nd player yet
	per_data.data[per_data.data_size] = 0xF0;
	++per_data.data_size;

	return request_quit;
}


