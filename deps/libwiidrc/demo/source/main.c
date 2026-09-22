/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <gccore.h>
#include <stdio.h>
#include <wiiuse/wpad.h>
#include <wiidrc/wiidrc.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) 
{
	void *xfb = NULL;
	GXRModeObj *rmode = NULL;
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
	int x = 24, y = 32, w, h;
	w = rmode->fbWidth - (32);
	h = rmode->xfbHeight - (48);
	CON_InitEx(rmode, x, y, w, h);
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
	PAD_Init();
	WPAD_Init();
	WiiDRC_Init();
	bool inited = WiiDRC_Inited();
	bool connected = false;
	if(inited) //Wii VC can start with gamepad off
		connected = WiiDRC_Connected();
	while(1)
	{
		printf("\x1b[2J");
		printf("\x1b[37m");
		printf("WiiDRC v1.3 Demo by FIX94\n");
		printf("Press any button on a real Wiimote to exit\n");
		if(inited)
		{
			if(connected)
			{
				WiiDRC_ScanPads();
				const struct WiiDRCData *drcdat = WiiDRC_Data();
				printf("Left Stick X: %i, Y: %i; Right Stick X: %i, Y: %i\n", 
					drcdat->xAxisL, drcdat->yAxisL, drcdat->xAxisR, drcdat->yAxisR);
				if(drcdat->battery) printf("Battery: %i\n", drcdat->battery);
				else printf("Battery: Charging\n");
				if(WiiDRC_ShutdownRequested()) printf("Gamepad is asking for a shutdown\n");
				if(drcdat->button & WIIDRC_BUTTON_A) printf("A pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_B) printf("B pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_X) printf("X pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_Y) printf("Y pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_LEFT) printf("D-Pad Left pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_RIGHT) printf("D-Pad Right pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_UP) printf("D-Pad Up pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_DOWN) printf("D-Pad Down pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_ZL) printf("ZL pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_ZR) printf("ZR pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_L) printf("L pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_R) printf("R pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_PLUS) printf("Plus pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_MINUS) printf("Minus pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_HOME) printf("HOME pressed\n");
				if(drcdat->button & WIIDRC_BUTTON_SYNC) printf("SYNC pressed\n");
				if(drcdat->extra & WIIDRC_EXTRA_BUTTON_L3) printf("L3 pressed\n");
				if(drcdat->extra & WIIDRC_EXTRA_BUTTON_R3) printf("R3 pressed\n");
				if(drcdat->extra & WIIDRC_EXTRA_BUTTON_TV) printf("TV pressed\n");
				if(drcdat->extra & WIIDRC_EXTRA_OVERLAY_TV) printf("TV overlay\n");
				if(drcdat->extra & WIIDRC_EXTRA_OVERLAY_POWER) printf("Shutdown overlay\n");
			}
			else
				printf("Gamepad was not enabled when starting Wii VC.\n"); 
		}
		else
			printf("Not started in Wii VC or used fw.img was not patched for homebrew use.\n");
		VIDEO_WaitVSync();
		VIDEO_WaitVSync();
		PAD_ScanPads();
		WPAD_ScanPads();
		if(PAD_ButtonsHeld(0) || WPAD_ButtonsHeld(0))
			break;
	}
	return 0;
}
