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

#include "debug.h"
#include "peripheral.h"
#include <ogcsys.h>
#include <wiiuse/wpad.h>
#include <ogc/n64.h>

//For wii u version
#ifdef WII_U_VERSION
#include <wiidrc/wiidrc.h>

u32 wiiu_inited;
#endif /* WII_U_VERSION */


PerData per_data;
PerPad perpad[PER_PADMAX];


void per_Init(void)
{
	PAD_Init();
	WPAD_Init();
#ifdef WII_U_VERSION
	WiiDRC_Init();
	wiiu_inited = WiiDRC_Inited();
#endif
	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	per_data.ids[0] = PER_ID_DIGITAL;
}


static void per_N64ToSat(u32 indx, u32 *exit_code)
{
	//s8 axis_x = perpad[indx].x;
	//s8 axis_y = perpad[indx].y;
	//TODO: only do this when using the ANALOGUE controller
	u32 btns = perpad[indx].btn;
	*exit_code |= (btns & (N64_BUTTON_L | N64_BUTTON_Z)) == (N64_BUTTON_L | N64_BUTTON_Z);
	//TODO: UNCOMMENT

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
	0x0400;	//First 2 bits must be 0
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
		0x0400;	//First 2 bits must be 0
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
	0x0400;	//First 2 bits must be 0
	perpad[indx].btn = sat_btns;
}

static void per_ClassicToSat(u32 indx, u32 *exit_code)
{
	s8 axis_x = perpad[indx].x;
	s8 axis_y = perpad[indx].y;
	//TODO: only do this when using the ANALOGUE controller

	u32 btns = perpad[indx].btn;
	btns |= CLASSIC_AXIS_TO_DIGITAL(axis_x, axis_y);
	*exit_code |= (btns & WPAD_CLASSIC_BUTTON_HOME) | (btns & WPAD_BUTTON_HOME);

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
	0x0400;	//First 2 bits must be 0
	perpad[indx].btn = sat_btns;
}

#ifdef WII_U_VERSION
static void per_WiiUToSat(u32 indx, u32 *exit_code)
{
	s8 axis_x = perpad[indx].x;
	s8 axis_y = perpad[indx].y;
	//TODO: only do this when using the ANALOGUE controller

	u32 btns = perpad[indx].btn;
	btns |= CLASSIC_AXIS_TO_DIGITAL(axis_x, axis_y);
	*exit_code |= (btns & WIIDRC_BUTTON_HOME);

	//TODO: get the user defined bits for the buttons
	u32 sat_btns =
	(((btns >> WIIU_BIT_Y) & 1) << PAD_DI_BIT_A) |
	(((btns >> WIIU_BIT_B) & 1) << PAD_DI_BIT_B) |
	(((btns >> WIIU_BIT_A) & 1) << PAD_DI_BIT_C) |
	(((btns >> WIIU_BIT_PLUS) & 1) << PAD_DI_BIT_STR) |
	(((btns >> WIIU_BIT_UP) & 1) << PAD_DI_BIT_UP) |
	(((btns >> WIIU_BIT_DOWN) & 1) << PAD_DI_BIT_DOWN) |
	(((btns >> WIIU_BIT_LEFT) & 1) << PAD_DI_BIT_LEFT) |
	(((btns >> WIIU_BIT_RIGHT) & 1) << PAD_DI_BIT_RIGHT) |
	(((btns >> WIIU_BIT_L) & 1) << PAD_DI_BIT_L) |
	(((btns >> WIIU_BIT_X) & 1) << PAD_DI_BIT_X) |
	(((btns >> WIIU_BIT_ZL) & 1) << PAD_DI_BIT_Y) |
	(((btns >> WIIU_BIT_ZR) & 1) << PAD_DI_BIT_Z) |
	(((btns >> WIIU_BIT_R) & 1) << PAD_DI_BIT_R) |
	0x0400;	//First 2 bits must be 0
	perpad[indx].btn = sat_btns;
}
#endif /* WII_U_VERSION */

/* Modified from libogc2's pad.c */
u32 _per_ScanJoybusPads(u32 *exit_code)
{
	PADStatus padstatus[PAD_CHANMAX];
	N64Status n64status;

	u32 pad_status = 0;
	u32 padBit;
	u32 resetBits = 0;

	PAD_Read(padstatus);
	for (s32 i = 0; i < PAD_CHANMAX; ++i) {
		padBit = PAD_CHAN_BIT(i);

		switch(padstatus[i].err) {
		case PAD_ERR_NONE:
			perpad[i].type     = PAD_TYPE_GCPAD;
			perpad[i].x        = padstatus[i].stickX;
			perpad[i].y        = padstatus[i].stickY;
			perpad[i].sx       = padstatus[i].substickX;
			perpad[i].sy       = padstatus[i].substickY;
			perpad[i].prev_btn = perpad[i].btn;
			perpad[i].btn      = padstatus[i].button;
			per_GCToSat(i, exit_code);
			pad_status |= 1 << i;
			break;

		case PAD_ERR_NO_CONTROLLER:
			switch(SI_Probe(i)) {
			case SI_N64_CONTROLLER:
				N64_GetResponse(i, &n64status);

				switch(n64status.err) {
				case N64_ERR_READY:
					perpad[i].type     = PAD_TYPE_N64PAD;
					perpad[i].x        = n64status.stickX;
					perpad[i].y        = n64status.stickY;
					perpad[i].sx       = 0;
					perpad[i].sy       = 0;
					perpad[i].prev_btn = perpad[i].btn;
					perpad[i].btn      = n64status.button;
					per_N64ToSat(i, exit_code);
					pad_status |= 1 << i;
					break;

				case N64_ERR_NO_CONTROLLER:
					N64_EnablePolling(padBit);
					goto no_controller;
					break;

				default:
					goto not_ready;
					break;
				}
				break;

			case SI_GC_CONTROLLER:
			case SI_GC_WAVEBIRD:
				resetBits |= padBit;
no_controller:
			default:
				resetBits |= padBit;
				break;
			}
			break;

not_ready:
		default:
			pad_status |= 1 << i;
			break;
		}
	}
	if(resetBits) {
		PAD_Reset(resetBits);
	}
	return pad_status;
}


u32 per_updatePads()
{
	//Reset ports
	u32 per_num = 0;
	u32 exit_code = 0;
	per_data.data_sent = 0;
	per_data.data_size = 2;
	per_data.data[0] = PER_STAT_NONE;	//Port 1
	per_data.data[1] = PER_STAT_NONE;	//Port 2
	per_data.port2_offset = 1;

	//Fill with joybus controllers
	u32 per_status = _per_ScanJoybusPads(&exit_code);

#ifdef WII_U_VERSION
	if (wiiu_inited && WiiDRC_Connected()) {
		WiiDRC_ScanPads();
		const struct WiiDRCData *drcdat = WiiDRC_Data();
		//Search for first free port
		while ((per_status >> per_num) & 1) {
			per_num++;
		}
		perpad[per_num].type = PAD_TYPE_WIIUPAD;
		perpad[per_num].x = (s8) drcdat->xAxisL;
		perpad[per_num].y = (s8) drcdat->yAxisL;
		perpad[per_num].prev_btn = perpad[per_num].btn;
		perpad[per_num].btn = drcdat->button;
		per_WiiUToSat(per_num, &exit_code);
		per_status |= 1 << per_num;
		++per_num;
	}
#endif

	//Fill rest of entries with wiimotes
	for (u32 i = 0; i < PAD_CHANMAX; ++i) {
		//Search for first free port
		while ((per_status >> per_num) & 1) {
			per_num++;
		}

		WPADData *wpad = WPAD_Data(i);
		u32 exp_type;
		u32 err = WPAD_Probe(i, &exp_type);
		WPAD_ReadPending(i, NULL);
		if (err == WPAD_ERR_NONE) {
			if (exp_type == WPAD_EXP_CLASSIC) { //Classic controller used
				perpad[per_num].type = PAD_TYPE_CLASSIC;
				perpad[per_num].x = wpad->exp.classic.ljs.pos.x - wpad->exp.classic.ljs.center.x;
				perpad[per_num].y = wpad->exp.classic.ljs.pos.y - wpad->exp.classic.ljs.center.y;
				perpad[per_num].prev_btn = perpad[per_num].btn;
				perpad[per_num].btn = (wpad->btns_h & 0xFFFF) | (wpad->exp.classic.btns << 16);
				per_ClassicToSat(per_num, &exit_code);
				per_status |= 1 << per_num;
				++per_num;
			} else {	//Default to wiimote
				perpad[per_num].type = PAD_TYPE_WIIMOTE;
				perpad[per_num].x = 0;
				perpad[per_num].y = 0;
				perpad[per_num].prev_btn = perpad[per_num].btn;
				perpad[per_num].btn = wpad->btns_h;
				per_WiiToSat(per_num, &exit_code);
				per_status |= 1 << per_num;
				++per_num;
			}
		}
	}

	//Fill the rest of the pads
	for (u32 i = 0; i < PER_PADMAX; ++i) {
		if (!((per_status >> i) & 1)){
			perpad[i].type = PAD_TYPE_NONE;
			perpad[i].x = 0;
			perpad[i].y = 0;
			perpad[i].sx = 0;
			perpad[i].sy = 0;
			perpad[i].btn = 0;
		}
	}


	//Generate Peripheral data for SMPC
	u32 size = 0;
	//TODO: make multitap optional (or use game id table)
	if (0) { //Use of multitap
		if (per_status & 0x80) { //All controllers are connected
			//setup multitap
			per_data.data[size++] = PER_STAT_MULTITAP;
			for (u32 i = 0; i < 6; ++i) {
				if (perpad[i].type != PAD_TYPE_NONE) {
					per_data.data[size++] = PER_ID_DIGITAL;
					per_data.data[size++] = ~(perpad[i].btn & 0xFF);
					per_data.data[size++] = ~((perpad[i].btn >> 0x8) & 0xFF);
				} else {
					per_data.data[size++] = PER_ID_NONE;
				}
			}
			//add last peripheral data
			per_data.data[size++] = PER_STAT_MULTITAP;
			for (u32 i = 6; i < 8; ++i) {
				if (perpad[i].type != PAD_TYPE_NONE) {
					per_data.data[size++] = PER_ID_DIGITAL;
					per_data.data[size++] = ~(perpad[i].btn & 0xFF);
					per_data.data[size++] = ~((perpad[i].btn >> 0x8) & 0xFF);
				} else {
					per_data.data[size++] = PER_ID_NONE;
				}
			}
		} else { // Up until 7 controllers conected
			//setup multitap
			per_data.data[size++] = PER_STAT_MULTITAP;
			for (u32 i = 0; i < 6; ++i) {
				if (perpad[i].type != PAD_TYPE_NONE) {
					per_data.data[size++] = PER_ID_DIGITAL;
					per_data.data[size++] = ~(perpad[i].btn & 0xFF);
					per_data.data[size++] = ~((perpad[i].btn >> 0x8) & 0xFF);
				} else {
					per_data.data[size++] = PER_ID_NONE;
				}
			}
			//add last peripheral data
			per_data.data[size++] = per_status & (1 << 6) ? PER_STAT_DIRECT : PER_STAT_NONE;
			if (per_status & 0x2) {
				per_data.data[size++] = PER_ID_DIGITAL;
				per_data.data[size++] = ~(perpad[6].btn & 0xFF);
				per_data.data[size++] = ~((perpad[6].btn >> 0x8) & 0xFF);
			}
		}
	} else {
		per_data.data[size++] = per_status & 0x1 ? PER_STAT_DIRECT : PER_STAT_NONE;
		if (per_status & 0x1) {
			per_data.data[size++] = PER_ID_DIGITAL;
			per_data.data[size++] = ~(perpad[0].btn & 0xFF);
			per_data.data[size++] = ~((perpad[0].btn >> 0x8) & 0xFF);
		}
		per_data.data[size++] = per_status & 0x2 ? PER_STAT_DIRECT : PER_STAT_NONE;
		if (per_status & 0x2) {
			per_data.data[size++] = PER_ID_DIGITAL;
			per_data.data[size++] = ~(perpad[1].btn & 0xFF);
			per_data.data[size++] = ~((perpad[1].btn >> 0x8) & 0xFF);
		}
	}
/*
	//Set the port stats
	port_stat[0] = (per_status & 0x1 ? (per_status & 0x80 ? PER_STAT_MULTITAP : PER_STAT_DIRECT) : PER_STAT_NONE);
	port_stat[1] = (per_status & 0x2 ? (per_status & 0xFC ? PER_STAT_MULTITAP : PER_STAT_DIRECT) : PER_STAT_NONE);
	u32 port_count = 0;
	u32 port_ofs = 0b00000011 + (per_num == 8 ? 2 : 0);

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
*/
	per_data.data_size = size;
	return exit_code;
}



void per_SetMappingForType(u32 type, u32 new_mapping)
{

}