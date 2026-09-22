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

#include <malloc.h>
#include "svi.h"
#include "sgx.h"
#include "../profile.h"
#include "../osd/osd.h"
#include "svi_vmodes.inc"

#define COLOR_BLACK        (0x10801080)

#define DEFAULT_FIFO_SIZE       (256*1024)

enum TVModeType {
	TVMODE_A_ST = 0,	//320
	TVMODE_B_ST = 1,	//352
	TVMODE_A_HI = 2,	//640
	TVMODE_B_HI = 3		//704
};



extern void __VIClearFramebuffer(void* fb, u32 bytes, u32 color);
extern u32 vdp2_disp_w;
extern u32 vdp2_disp_h;
extern u32 vdp1_fb_w;
extern u32 vdp1_fb_h;
extern u32 vdp1_fb_mtx;

u32 vert_offset = 0;
u32 scale_mtx = MTX_TEX_SCALED_N;
u16 *xfb[2] = { NULL, NULL };
u32 fbsel = 0;
GXRModeObj *rmode_conf;
GXRModeObj *rmode;
u32 tvmode;
u32 svi_disp = 0;
void *gp_fifo;
u8 *fb_scale_tex ATTRIBUTE_ALIGN(32);		/*Texture for scaling x axis fb*/


u32 tvmd_curr = 0;

static void __svi_PretraceCB(u32 ret_count)
{
	// Set vertical and horizontal Resolution
	u32 width = SS_DISP_WIDTH + ((tvmd_curr & 1) << 5);
	u32 height = SS_DISP_HEIGHT + (((tvmd_curr & 0x30) > 0) << 4);
	u32 vdp2_x_hires = ((tvmd_curr >> 1) & 1);
	u32 vdp2_interlace = ((tvmd_curr & 0xC0) == 0xC0);
	svi_disp = ((height | vdp2_interlace) << 16) | (width | vdp2_x_hires);
	vdp2_disp_w = width << vdp2_x_hires;
	vdp2_disp_h = height << vdp2_interlace;
	vdp1_fb_w = width;
	vdp1_fb_h = height;
	scale_mtx = MTX_TEX_SCALED_N + ((1-vdp2_x_hires) << 1);
	vert_offset = ((((tvmd_curr & 0x30) == 0) << 3));
	tvmode = (tvmd_curr & 3);

	//TODO: DONT CHANGE THIS, ONLY CLEAR TO BLACK
	//const u32 xfb_size = 704 * 480 * VI_DISPLAY_PIX_SZ;
	//__VIClearFramebuffer(xfb[0], xfb_size, COLOR_BLACK);
	//__VIClearFramebuffer(xfb[1], xfb_size, COLOR_BLACK);
	if (rmode_conf != TVNTSCNative) {
		GX_SetDispCopyYScale((f32)(2 - vdp2_interlace));	//scale the XFB copy if not interlaced
		GX_Flush();
	}

	u32 vmode_indx = (vdp2_interlace << 1) | ((tvmd_curr & 0x30) > 0);
	rmode = &rmode_conf[vmode_indx];
	rmode->fbWidth = width << 1; //rmode->fbWidth = 704;
	//rmode->viYOrigin = vert_offset;
	//rmode->viHeight = height << 1;
	//rmode->viYOrigin = 4;//(VI_MAX_HEIGHT_NTSC - rmode->viHeight) / 2;
	VIDEO_Configure(rmode);
	VIDEO_Flush();
	VIDEO_SetPreRetraceCallback(NULL);
}

const u32 const_vert_colors[40] ATTRIBUTE_ALIGN(32) = {
	0x00000000, 0x00000008, 0x00000010, 0x00000018,
	0x00000021, 0x00000029, 0x00000031, 0x00000039,
	0x00000042, 0x0000004A, 0x00000052, 0x0000005A,
	0x00000063, 0x0000006B, 0x00000073, 0x0000007B,
	0x00000084, 0x0000008C, 0x00000094, 0x0000009C,
	0x000000A5, 0x000000AD, 0x000000B5, 0x000000BD,
	0x000000C6, 0x000000CE, 0x000000D6, 0x000000DE,
	0x000000E7, 0x000000EF, 0x000000F7, 0x000000FF,

	0x00000001, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

void SVI_Init(void)
{
	VIDEO_Init();
	if (VIDEO_HaveComponentCable()) {

	}
	SVI_EnableNativeVideo(0);
	rmode = VIDEO_GetPreferredMode(NULL);

	Mtx44 perspective;
	const u16 w = VIDEO_PadFramebufferWidth(704);
	const u16 h = 480;
	const u32 xfb_size = w * h * VI_DISPLAY_PIX_SZ;
	//Allocate framebuffers
	xfb[0] = MEM_K0_TO_K1(memalign(32, xfb_size));
	xfb[1] = MEM_K0_TO_K1(memalign(32, xfb_size));
	fb_scale_tex = (u8*) memalign(32, xfb_size);

	__VIClearFramebuffer(xfb[0], xfb_size, COLOR_BLACK);
	__VIClearFramebuffer(xfb[1], xfb_size, COLOR_BLACK);

	VIDEO_SetBlack(1);
	VIDEO_Configure(rmode);
	VIDEO_Flush();
	VIDEO_WaitVSync();

	// Initialize GX
	gp_fifo = memalign(32, DEFAULT_FIFO_SIZE);
	GX_Init(gp_fifo, DEFAULT_FIFO_SIZE);

	GX_SetCopyClear((GXColor){0, 0, 0, 0}, 0);
	GX_SetDispCopyGamma(GX_GM_1_0);

	GX_SetCopyFilter(GX_FALSE, rmode->sample_pattern, GX_FALSE, rmode->vfilter);
	GX_SetFieldMode(GX_DISABLE, ((rmode->viHeight == 2*rmode->xfbHeight)? GX_ENABLE : GX_DISABLE));
	GX_SetDither(GX_DISABLE);
	GX_SetCopyClamp(GX_CLAMP_NONE);

	GX_InvVtxCache();
	GX_InvalidateTexAll();
	GX_ClearVtxDesc();

	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS,  GX_POS_XY,  GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST,   GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS,  GX_POS_XY,  GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST,  GX_U16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	//VDP1 vertex format for 16bpp
	GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_POS,  GX_POS_XY,   GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_TEX0, GX_TEX_ST,   GX_U8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT2, GX_VA_CLR0, GX_CLR_RGB, GX_RGB565, 0);
	//VDP1 vertex format for 8bpp
	GX_SetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS,  GX_POS_XY,   GX_S16, 1);
	GX_SetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST,   GX_U8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT3, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA6, 0);

	//VDP2 vertex format
	GX_SetVtxAttrFmt(GX_VTXFMT5, GX_VA_POS,  GX_POS_XYZ,   GX_U8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT5, GX_VA_TEX0, GX_TEX_ST,   GX_U8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT5, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

	GX_SetVtxAttrFmt(GX_VTXFMT6, GX_VA_POS,  GX_POS_XY,   GX_U8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT6, GX_VA_TEX0, GX_TEX_ST,   GX_U8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT6, GX_VA_CLR0, GX_CLR_RGB, GX_RGB565, 0);

	//GUI vertex format
	GX_SetVtxAttrFmt(GX_VTXFMT4, GX_VA_POS,  GX_POS_XY,   GX_S16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT4, GX_VA_TEX0, GX_TEX_ST,   GX_U8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT4, GX_VA_CLR0, GX_CLR_RGB, GX_RGB565, 0);

	//Swap Mode Tables
	GX_SetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA); //Normal swap mode
	GX_SetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_BLUE, GX_CH_GREEN, GX_CH_RED, GX_CH_ALPHA); //For sega saturn palettes/RGB
	GX_SetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_ALPHA, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE); //For VDP1 drawing of fb
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

	GX_SetZCompLoc(GX_FALSE);
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_TRUE);

	GX_SetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHTNULL, GX_DF_NONE, GX_AF_NONE);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, MTX_IDENTITY);
	GX_SetCurrentMtx(MTX_IDENTITY);

	//Create vertex arrays for transparency
	//GX_SetArray(GX_VA_CLR0, const_vert_colors, 4);

	//TODO: Check that the values are stored correctly.
	guOrtho(perspective, 0, 480.0, 0.0, 640.0, 0.0, GX_MAX_Z24+1);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	GX_SetViewport(0, 0, 640, 480, 0.0f, 1.0f);
	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);

	//Reset various parameters
	GX_SetCoPlanar(GX_DISABLE);
	GX_SetClipMode(GX_CLIP_ENABLE);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetColorUpdate(GX_ENABLE);
	GX_SetAlphaUpdate(GX_DISABLE);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetDispCopyGamma(GX_GM_1_0);
	GX_SetCopyClamp(GX_CLAMP_NONE);

	SGX_Init();

	SVI_SetResolution(0x80D2);
	VIDEO_SetNextFramebuffer(xfb[0]);
	VIDEO_SetBlack(0);
	VIDEO_Flush();
	VIDEO_WaitVSync();
}

void SVI_EnableNativeVideo(u32 enable)
{
	rmode_conf = (enable ? TVNTSCNative :TVNTSCProg);
}

void SVI_ResetViewport(void)
{
	//GX_SetViewport(0, 0, 640, 480, 0.0f, 256.0f);
}

void SVI_SetResolution(u32 tvmd)
{
	//Only change the resolution when the mode has changed
	if (tvmd_curr != tvmd) {
		tvmd_curr = tvmd;
		VIDEO_SetPreRetraceCallback(__svi_PretraceCB);
	}
}

void SVI_CopyXFB(u32 x, u32 y)
{
	GX_SetDispCopyYScale(1.0);	//scale the XFB copy if not interlaced
	GX_CopyDisp(xfb[fbsel] + (y * 704) + x, GX_TRUE);
	GX_SetDispCopyYScale((f32)(2 - (vdp2_disp_h > 352)));	//scale the XFB copy if not interlaced
}


void SVI_CopyFrame(void)
{
	osd_Draw();

	GX_SetScissor(0, 0, 640, 480);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	//Set up general TEV
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(0);
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	//TEXMAP6 is for VDP2 scaling
	SGX_SetOtherTex(GX_TEXMAP6, fb_scale_tex, GX_TF_RGB565, vdp2_disp_w, vdp2_disp_h, 0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_POS, scale_mtx);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP6, GX_COLORNULL);

	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_TRUE);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ADDHALF, GX_CS_SCALE_2, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetCurrentMtx(MTX_IDENTITY);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);

	switch (tvmode) {
		case TVMODE_A_ST: {
			GX_SetTexCopySrc(0, 0, vdp2_disp_w, vdp2_disp_h);
			GX_SetTexCopyDst(vdp2_disp_w, vdp2_disp_h, GX_TF_RGB565, GX_FALSE);
			GX_CopyTex(fb_scale_tex, GX_TRUE);
			GX_PixModeSync(); //Not necesary?

			u32 w = 640;
			u32 h = vdp2_disp_h;

			GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT4, 4);
			GX_Position2s16(0, 0);
			GX_Position2s16(w, 0);
			GX_Position2s16(0, h);
			GX_Position2s16(w, h);
			GX_End();

			GX_SetDispCopySrc(0, 0, w, 240);
			GX_SetDispCopyDst(w, 480);
			GX_DrawDone();
			GX_CopyDisp(xfb[fbsel], GX_TRUE);
		} break;
		case TVMODE_B_ST: {
			GX_SetTexCopySrc(0, 0, vdp2_disp_w, vdp2_disp_h);
			GX_SetTexCopyDst(vdp2_disp_w, vdp2_disp_h, GX_TF_RGB565, GX_FALSE);
			GX_CopyTex(fb_scale_tex, GX_TRUE);
			GX_PixModeSync(); //Not necesary?

			u32 w = 640;
			u32 h = vdp2_disp_h;

			GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT4, 4);
				GX_Position2s16(0, 0);
				GX_Position2s16(w, 0);
				GX_Position2s16(0, h);
				GX_Position2s16(w, h);
			GX_End();

			GX_SetDispCopySrc(0, 0, w, 240);
			GX_SetDispCopyDst(704, 480);
			GX_DrawDone();
			GX_CopyDisp(xfb[fbsel], GX_TRUE);

			GX_SetCurrentMtx(MTX_MOVED_640);
			GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT4, 4);
			GX_Position2s16(w, 0);
			GX_Position2s16(w+64, 0);
			GX_Position2s16(w, h);
			GX_Position2s16(w+64, h);
			GX_End();
			GX_SetDispCopySrc(0, 0, 64, 240);
			GX_DrawDone();
			GX_CopyDisp(xfb[fbsel] + w, GX_TRUE);
		} break;
		case TVMODE_A_HI: {
			GX_SetDispCopySrc(0, 0, 640, vdp2_disp_h);
			GX_SetDispCopyDst(640, vdp2_disp_h);
			GX_DrawDone();
			GX_CopyDisp(xfb[fbsel], GX_TRUE);
		} break;
		case TVMODE_B_HI: {
			//TODO: need to add the other strip
			GX_SetDispCopySrc(0, 0, 640, vdp2_disp_h);
			GX_SetDispCopyDst(704, vdp2_disp_h);
			GX_DrawDone();
			GX_CopyDisp(xfb[fbsel], GX_TRUE);
		} break;
	}
}


void SVI_ClearFrame(void)
{
	//Copy black
	GX_SetDispCopySrc(0, 0, 640, vdp2_disp_h);
	GX_SetDispCopyDst(rmode->fbWidth, vdp2_disp_h);
	GX_CopyDisp(xfb[fbsel], GX_TRUE);
	if (rmode->fbWidth == 704) {
		GX_SetDispCopySrc(0, 0, 64, vdp2_disp_h);
		GX_CopyDisp(xfb[fbsel]+640, GX_TRUE);
	}
}

void SVI_SwapBuffers(u32 wait_vsync)
{
	VIDEO_SetNextFramebuffer(xfb[fbsel]);
	VIDEO_Flush();
	//Profile total frame time
	prof_StopAndAdd(PROF_TOTAL);
	if (wait_vsync) {
		VIDEO_WaitVSync();
	}
	prof_Start(PROF_TOTAL);
	fbsel ^= 1;
}
