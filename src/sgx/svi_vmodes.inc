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

enum {
	SVI_RM_NTSC_224, /* 224 lines non-interlaced */
	SVI_RM_NTSC_240, /* 240 lines non-interlaced */
	SVI_RM_NTSC_448, /* 448 lines interlaced */
	SVI_RM_NTSC_480, /* 480 lines interlaced */
	SVI_RM_NTSC_MAX
};


//fbWidth is modifiable depending on clockspeed (640 or 720)

//NTSC rendering modes

//For prograssive video (if component cables are there)
GXRModeObj TVNTSCProg[SVI_RM_NTSC_MAX] = {
	[SVI_RM_NTSC_224] = {
		.viTVMode        = VI_TVMODE_NTSC_PROG,
		.fbWidth         = 640,
		.efbHeight       = 448,
		.xfbHeight       = 448,
		.viXOrigin       = (VI_MAX_WIDTH_NTSC - 704)/2,
		.viYOrigin       = (VI_MAX_HEIGHT_NTSC - 448)/2,
		.viWidth         = 704,
		.viHeight        = 448,
		.xfbMode         = VI_XFBMODE_SF,
		.field_rendering = GX_FALSE,
		.aa              = GX_FALSE,
		.copy_interlaced = GX_COPY_PROGRESSIVE,
		.vfilter         = { 0, 0, 21, 22, 21, 0, 0},
		.sample_pattern  = {
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6}}
	},

	[SVI_RM_NTSC_240] = {
		.viTVMode        = VI_TVMODE_NTSC_PROG,
		.fbWidth         = 640,
		.efbHeight       = 480,
		.xfbHeight       = 480,
		.viXOrigin       = (VI_MAX_WIDTH_NTSC - 704)/2,
		.viYOrigin       = (VI_MAX_HEIGHT_NTSC - 480)/2,
		.viWidth         = 704,
		.viHeight        = 480,
		.xfbMode         = VI_XFBMODE_SF,
		.field_rendering = GX_FALSE,
		.aa              = GX_FALSE,
		.copy_interlaced = GX_COPY_PROGRESSIVE,
		.vfilter         = { 0, 0, 21, 22, 21, 0, 0},
		.sample_pattern  = {
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6}}
	},

	[SVI_RM_NTSC_448] = {
		.viTVMode        = VI_TVMODE_NTSC_PROG,
		.fbWidth         = 640,
		.efbHeight       = 448,
		.xfbHeight       = 448,
		.viXOrigin       = (VI_MAX_WIDTH_NTSC - 704)/2,
		.viYOrigin       = (VI_MAX_HEIGHT_NTSC - 448)/2,
		.viWidth         = 704,
		.viHeight        = 448,
		.xfbMode         = VI_XFBMODE_SF,
		.field_rendering = GX_FALSE,
		.aa              = GX_FALSE,
		.copy_interlaced = GX_COPY_PROGRESSIVE,
		.vfilter         = { 0, 0, 21, 22, 21, 0, 0},
		.sample_pattern  = {
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6}}
	},

	[SVI_RM_NTSC_480] = {
		.viTVMode        = VI_TVMODE_NTSC_PROG,
		.fbWidth         = 640,
		.efbHeight       = 480,
		.xfbHeight       = 480,
		.viXOrigin       = (VI_MAX_WIDTH_NTSC - 704)/2,
		.viYOrigin       = (VI_MAX_HEIGHT_NTSC - 480)/2,
		.viWidth         = 704,
		.viHeight        = 480,
		.xfbMode         = VI_XFBMODE_SF,
		.field_rendering = GX_FALSE,
		.aa              = GX_FALSE,
		.copy_interlaced = GX_COPY_PROGRESSIVE,
		.vfilter         = { 0, 0, 21, 22, 21, 0, 0},
		.sample_pattern  = {
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6}}
	}
};


//For native video (if component cables are not installed or native video is selected)

GXRModeObj TVNTSCNative[SVI_RM_NTSC_MAX] = {
	[SVI_RM_NTSC_224] = {
		.viTVMode        = VI_TVMODE_NTSC_DS,
		.fbWidth         = 640,
		.efbHeight       = 448,
		.xfbHeight       = 224,
		.viXOrigin       = (VI_MAX_WIDTH_NTSC - 704)/2,
		.viYOrigin       = (VI_MAX_HEIGHT_NTSC/2 - 448/2)/2,
		.viWidth         = 704,
		.viHeight        = 448,
		.xfbMode         = VI_XFBMODE_SF,
		.field_rendering = GX_FALSE,
		.aa              = GX_FALSE,
		.copy_interlaced = GX_COPY_INTLC_EVEN,
		.vfilter         = { 0, 0, 0, 16, 16, 16, 16},
		.sample_pattern  = {
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6}}
	},

	[SVI_RM_NTSC_240] = {
		.viTVMode        = VI_TVMODE_NTSC_DS,
		.fbWidth         = 640,
		.efbHeight       = 480,
		.xfbHeight       = 240,
		.viXOrigin       = (VI_MAX_WIDTH_NTSC - 704)/2,
		.viYOrigin       = (VI_MAX_HEIGHT_NTSC/2 - 480/2)/2,
		.viWidth         = 704,
		.viHeight        = 480,
		.xfbMode         = VI_XFBMODE_SF,
		.field_rendering = GX_FALSE,
		.aa              = GX_FALSE,
		.copy_interlaced = GX_COPY_INTLC_EVEN,
		.vfilter         = { 0, 0, 0, 16, 16, 16, 16},
		.sample_pattern  = {
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6}}
	},


	[SVI_RM_NTSC_448] = {
		.viTVMode        = VI_TVMODE_NTSC_INT,
		.fbWidth         = 640,
		.efbHeight       = 448,
		.xfbHeight       = 448,
		.viXOrigin       = (VI_MAX_WIDTH_NTSC - 704)/2,
		.viYOrigin       = (VI_MAX_HEIGHT_NTSC - 448)/2,
		.viWidth         = 704,
		.viHeight        = 448,
		.xfbMode         = VI_XFBMODE_DF,
		.field_rendering = GX_FALSE,
		.aa              = GX_FALSE,
		.copy_interlaced = GX_COPY_PROGRESSIVE,
		.vfilter         = { 0, 0, 21, 22, 21, 0, 0},
		.sample_pattern  = {
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6}}
	},

	[SVI_RM_NTSC_480] = {
		.viTVMode        = VI_TVMODE_NTSC_INT,
		.fbWidth         = 640,
		.efbHeight       = 480,
		.xfbHeight       = 480,
		.viXOrigin       = (VI_MAX_WIDTH_NTSC - 704)/2,
		.viYOrigin       = (VI_MAX_HEIGHT_NTSC - 480)/2,
		.viWidth         = 704,
		.viHeight        = 480,
		.xfbMode         = VI_XFBMODE_DF,
		.field_rendering = GX_FALSE,
		.aa              = GX_FALSE,
		.copy_interlaced = GX_COPY_PROGRESSIVE,
		.vfilter         = { 0, 0, 21, 22, 21, 0, 0},
		.sample_pattern  = {
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6},
			{6,6},{6,6},{6,6}}
	}
};


//PAL rendering modes