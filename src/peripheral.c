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
//#include <joy.h>

PerPad perpad[PER_PADMAX];

PerData per_data;
//PADStatus status_gc[PAD_CHANMAX];
//JOYStatus status_64[JOY_CHANMAX];

//XXX: this is for gamepad configuration
//static u8 gcpad_bitoffsets[16] = { 0, 0, 0, 0, 0, 0, 0, GC_BIT_A, GC_BIT_B, GC_BIT_Z2, GC_BIT_Y, GC_BIT_X, GC_BIT_Z };
//static u8 wcpad_bitoffsets[16];
//static u8 wpad_bitoffsets[16];


void per_Init(void)
{
	//JOY_Init(status_64, status_gc);
	PAD_Init();
	WPAD_Init();
	per_data.ids[0] = PER_ID_DIGITAL;
}


/*

	u32 port_stat[2] = {PER_STAT_NONE, PER_STAT_NONE};
	u32 per_num = 0;

	//Reset ports
	per_data.data_sent = 0;
	per_data.data_size = 2;
	per_data.data[0] = PER_STAT_NONE;	//Port 1
	per_data.data[1] = PER_STAT_NONE;	//Port 2
	per_data.port2_offset = 1;

	//Fill with controllers from gc ports
	u32 exit_code = __per_ScanPorts(&per_num);


	u32 pad_types[PAD_CHANMAX];
	u32 connected = 0;
	u32 exit_code = 0;

	//Read the N64 and GC controllers
	JOY_Read(status_64, status_gc);
	for (u32 i = 0; i < PAD_CHANMAX; ++i) {
		if (status_64[i].err == PAD_ERR_NONE) {
			pad_types[i] = PAD_TYPE_N64PAD;
			connected |= 1 << i;
		} else if (status_gc[i].err == PAD_ERR_NONE) {
			pad_types[i] = PAD_TYPE_GCPAD;
			connected |= 1 << i;
		}
	}

	//Add pads
	for (u32 i = 0; i < PAD_CHANMAX; ++i) {
		if ((connected >> i) & 1) {
			PerPad *per = &perpad[*per_num];
			per->type = pad_types[i];
			if (per->type == PAD_TYPE_GCPAD) {
				per->y = status_gc[i].stickY;
				per->x = status_gc[i].stickX;
				per->prev_btn = per->btn;
				per->btn = status_gc[i].button;
				per->sx = status_gc[i].substickX;
				per->sy = status_gc[i].substickY;
				per_GCToSat(*per_num, &exit_code);
			} else {
				per->y = status_64[i].stickY;
				per->x = status_64[i].stickX;
				per->prev_btn = per->btn;
				per->btn = status_64[i].button;
				per_N64ToSat(*per_num, &exit_code);
			}
			*per_num = *per_num + 1;
		}
	}
*/


static void per_N64ToSat(u32 indx, u32 *exit_code)
{
	//s8 axis_x = perpad[indx].x;
	//s8 axis_y = perpad[indx].y;
	//TODO: only do this when using the ANALOGUE controller
	u32 btns = perpad[indx].btn;
	//TODO: UNCOMMENT
	//*exit_code |= (btns & (JOY_TRG_R | JOY_TRG_L | JOY_TRG_Z)) == (JOY_TRG_R | JOY_TRG_L | JOY_TRG_Z);

	//N64 controller is basically a Saturn controller, no need for remap
	u32 sat_btns =
	(((btns >> N64_BIT_CD) & 1)    << PAD_DI_BIT_B) |
	(((btns >> N64_BIT_A) & 1)     << PAD_DI_BIT_A) |
	(((btns >> N64_BIT_CR) & 1)    << PAD_DI_BIT_C) |
	(((btns >> N64_BIT_STR) & 1)   << PAD_DI_BIT_STR) |
	(((btns >> N64_BIT_UP) & 1)    << PAD_DI_BIT_UP) |
	(((btns >> N64_BIT_DOWN) & 1)  << PAD_DI_BIT_DOWN) |
	(((btns >> N64_BIT_LEFT) & 1)  << PAD_DI_BIT_LEFT) |
	(((btns >> N64_BIT_RIGHT) & 1) << PAD_DI_BIT_RIGHT) |
	(((btns >> N64_BIT_L) & 1)     << PAD_DI_BIT_L) |
	(((btns >> N64_BIT_B) & 1)     << PAD_DI_BIT_X) |
	(((btns >> N64_BIT_CL) & 1)    << PAD_DI_BIT_Y) |
	(((btns >> N64_BIT_CU) & 1)    << PAD_DI_BIT_Z) |
	(((btns >> N64_BIT_R) & 1)     << PAD_DI_BIT_R) |
	0x300;	//First 2 bits must be 0
	perpad[indx].btn = sat_btns;
}


static void per_GCToSat(u32 indx, u32 *exit_code)
{
	s8 axis_x = perpad[indx].x;
	s8 axis_y = perpad[indx].y;
	//TODO: only do this when using the ANALOGUE controller
	u32 btns = perpad[indx].btn | GC_AXIS_TO_DIGITAL(axis_x, axis_y);
	//Use C-stick as another button
	btns |=  (u32) ((perpad[indx].sy > 32) | (perpad[indx].sy < -32)) << GC_BIT_Z2;
	*exit_code |= (btns & (PAD_BUTTON_START | PAD_TRIGGER_Z)) == (PAD_BUTTON_START | PAD_TRIGGER_Z);

	//TODO: get the user defined bits for the buttons
	u32 sat_btns =
		(((btns >> GC_BIT_A) & 1) << PAD_DI_BIT_B) |
		(((btns >> GC_BIT_B) & 1) << PAD_DI_BIT_A) |
		(((btns >> GC_BIT_X) & 1) << PAD_DI_BIT_C) |
		(((btns >> GC_BIT_STR) & 1) << PAD_DI_BIT_STR) |
		(((btns >> GC_BIT_UP) & 1) << PAD_DI_BIT_UP) |
		(((btns >> GC_BIT_DOWN) & 1) << PAD_DI_BIT_DOWN) |
		(((btns >> GC_BIT_LEFT) & 1) << PAD_DI_BIT_LEFT) |
		(((btns >> GC_BIT_RIGHT) & 1) << PAD_DI_BIT_RIGHT) |
		(((btns >> GC_BIT_L) & 1) << PAD_DI_BIT_L) |
		(((btns >> GC_BIT_Z2) & 1) << PAD_DI_BIT_X) |
		(((btns >> GC_BIT_Y) & 1) << PAD_DI_BIT_Y) |
		(((btns >> GC_BIT_Z) & 1) << PAD_DI_BIT_Z) |
		(((btns >> GC_BIT_R) & 1) << PAD_DI_BIT_R) |
		0x300;	//First 2 bits must be 0
	perpad[indx].btn = sat_btns;
}

static void per_WiiToSat(u32 indx, u32 *exit_code)
{
	s8 axis_x = 0;
	s8 axis_y = 0;
	//TODO: only do this when using the ANALOGUE controller
	u32 btns = perpad[indx].btn | CLASSIC_AXIS_TO_DIGITAL(axis_x, axis_y);
	*exit_code |= (btns & WPAD_BUTTON_HOME);

	//TODO: get the user defined bits for the buttons
	u32 sat_btns =
	(((btns >> WII_BIT_A) & 1) << PAD_DI_BIT_A) |
	(((btns >> WII_BIT_1) & 1) << PAD_DI_BIT_B) |
	(((btns >> WII_BIT_2) & 1) << PAD_DI_BIT_C) |
	(((btns >> WII_BIT_PLUS) & 1) << PAD_DI_BIT_STR) |
	(((btns >> WII_BIT_RIGHT) & 1) << PAD_DI_BIT_UP) |
	(((btns >> WII_BIT_LEFT) & 1) << PAD_DI_BIT_DOWN) |
	(((btns >> WII_BIT_UP) & 1) << PAD_DI_BIT_LEFT) |
	(((btns >> WII_BIT_DOWN) & 1) << PAD_DI_BIT_RIGHT) |
	//(((btns >> WII_BIT_L) & 1) << PAD_DI_BIT_L) |
	(((btns >> WII_BIT_MINUS) & 1) << PAD_DI_BIT_X) |
	(((btns >> WII_BIT_B) & 1) << PAD_DI_BIT_Y) |
	//(((btns >> WII_BIT_ZR) & 1) << PAD_DI_BIT_Z) |
	//(((btns >> WII_BIT_R) & 1) << PAD_DI_BIT_R) |
	0x300;	//First 2 bits must be 0
	perpad[indx].btn = sat_btns;
}

static void per_ClassicToSat(u32 indx, u32 *exit_code)
{
	s8 axis_x = perpad[indx].x;
	s8 axis_y = perpad[indx].y;
	//TODO: only do this when using the ANALOGUE controller

	u32 btns = perpad[indx].btn;
	btns |= CLASSIC_AXIS_TO_DIGITAL(axis_x, axis_y);
	*exit_code |= (btns & WPAD_CLASSIC_BUTTON_HOME);

	//TODO: get the user defined bits for the buttons
	u32 sat_btns =
	(((btns >> WCL_BIT_Y) & 1) << PAD_DI_BIT_A) |
	(((btns >> WCL_BIT_B) & 1) << PAD_DI_BIT_B) |
	(((btns >> WCL_BIT_A) & 1) << PAD_DI_BIT_C) |
	(((btns >> WCL_BIT_PLUS) & 1) << PAD_DI_BIT_STR) |
	(((btns >> WCL_BIT_UP) & 1) << PAD_DI_BIT_UP) |
	(((btns >> WCL_BIT_DOWN) & 1) << PAD_DI_BIT_DOWN) |
	(((btns >> WCL_BIT_LEFT) & 1) << PAD_DI_BIT_LEFT) |
	(((btns >> WCL_BIT_RIGHT) & 1) << PAD_DI_BIT_RIGHT) |
	(((btns >> WCL_BIT_L) & 1) << PAD_DI_BIT_L) |
	(((btns >> WCL_BIT_X) & 1) << PAD_DI_BIT_X) |
	(((btns >> WCL_BIT_ZL) & 1) << PAD_DI_BIT_Y) |
	(((btns >> WCL_BIT_ZR) & 1) << PAD_DI_BIT_Z) |
	(((btns >> WCL_BIT_R) & 1) << PAD_DI_BIT_R) |
	0x300;	//First 2 bits must be 0
	perpad[indx].btn = sat_btns;
}


u32 per_updatePads()
{
	u32 port_stat[2] = {PER_STAT_NONE, PER_STAT_NONE};
	u32 per_num = 0;

	//Reset ports
	u32 exit_code = 0;
	per_data.data_sent = 0;
	per_data.data_size = 2;
	per_data.data[0] = PER_STAT_NONE;	//Port 1
	per_data.data[1] = PER_STAT_NONE;	//Port 2
	per_data.port2_offset = 1;

	//Fill with gc controllers
	u32 connected = PAD_ScanPads();
	for (u32 i = 0; i < PAD_CHANMAX; ++i) {
		if ((connected >> i) & 1) {
			perpad[per_num].type = PAD_TYPE_GCPAD;
			perpad[per_num].x = PAD_StickX(i);
			perpad[per_num].y = PAD_StickY(i);
			perpad[per_num].sx = PAD_SubStickX(i);
			perpad[per_num].sy = PAD_SubStickY(i);
			perpad[per_num].prev_btn = perpad[per_num].btn;
			perpad[per_num].btn = PAD_ButtonsHeld(i);
			per_GCToSat(per_num, &exit_code);
			++per_num;
		}
	}

	//Fill with wiimote
	for (u32 i = 0; i < PAD_CHANMAX; ++i) {
		WPADData *wpad = WPAD_Data(i);
		u32 exp_type;
		u32 err = WPAD_Probe(i, &exp_type);
		WPAD_ReadPending(i, NULL);
		if (err == WPAD_ERR_NONE) {
			if (exp_type == WPAD_EXP_NONE) {	//Wiimote used
				perpad[per_num].type = PAD_TYPE_WIIMOTE;
				perpad[per_num].x = 0;
				perpad[per_num].y = 0;
				perpad[per_num].prev_btn = perpad[per_num].btn;
				perpad[per_num].btn = wpad->btns_h;
				per_WiiToSat(per_num, &exit_code);
				++per_num;
			} else if (exp_type == WPAD_EXP_CLASSIC) { //Classic controller used
				if (wpad->exp.classic.type == 2) { //Wii U Pro pad
					perpad[per_num].type = PAD_TYPE_WIIUPRO;
				} else { //Normal Classic controller
					perpad[per_num].type = PAD_TYPE_CLASSIC;
				}
				perpad[per_num].x = (s16) (wpad->exp.classic.ljs.pos.x);
				perpad[per_num].y = (s16) (wpad->exp.classic.ljs.pos.y);
				perpad[per_num].prev_btn = perpad[per_num].btn;
				perpad[per_num].btn = wpad->btns_h;
				per_ClassicToSat(per_num, &exit_code);
				++per_num;
			}
		}
	}

	//Set the port stats
	port_stat[0] = (per_num ? (per_num == 8 ? PER_STAT_MULTITAP : PER_STAT_DIRECT) : PER_STAT_NONE);
	port_stat[1] = (per_num > 1 ? (per_num > 2 ? PER_STAT_MULTITAP : PER_STAT_DIRECT) : PER_STAT_NONE);
	u32 size = 0;
	u32 port_count = 0;
	u32 port_ofs = 0b00000011 + (per_num == 8 ? 2 : 0);

	//Generate Peripheral data for SMPC
	for (u32 i = 0; i < per_num; ++i) {
		if ((port_ofs >> i) & 1) {	//See if port stat must be set
			per_data.data[size++] = port_stat[port_count++];
		}
		if (perpad[i].type != PAD_TYPE_NONE) {
			per_data.data[size++] = PER_ID_DIGITAL;
			per_data.data[size++] = ~(perpad[i].btn & 0xFF);
			per_data.data[size++] = ~((perpad[i].btn >> 0x8) & 0xFF);
		}
	}
	//Fill rest of data with no input
	if (2 < per_num && per_num < 8){ //Fill the rest of multitap
		for (u32 i = per_num; i < 7; ++i) {
			per_data.data[size++] = PER_ID_NONE;
		}
	} else if (per_num == 1) {	//Only have one controller
		per_data.data[size++] = PER_STAT_NONE;
	}

	//Fill the rest of the pads
	while (per_num < PER_PADMAX) {
		perpad[per_num].type = PAD_TYPE_NONE;
		perpad[per_num].x = 0;
		perpad[per_num].y = 0;
		perpad[per_num].sx = 0;
		perpad[per_num].sy = 0;
		perpad[per_num].btn = 0;
		++per_num;
	}

	per_data.data_size = size;
	return exit_code;
}



