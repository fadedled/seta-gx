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

enum {
	SVI_RM_224, /* 224 lines non-interlaced */
	SVI_RM_240, /* 240 lines non-interlaced */
	SVI_RM_256, /* 256 lines non-interlaced (PAL ONLY) */
	SVI_RM_448, /* 448 lines interlaced */
	SVI_RM_480, /* 480 lines interlaced */
	SVI_RM_512, /* 512 lines interlaced (PAL ONLY) */
	SVI_RM_MAX
};


//fbWidth is modifiable depending on clockspeed (640 or 720)

//NTSC rendering modes
GXRModeObj RM_NTSC_224P = {
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
};

GXRModeObj RM_NTSC_240P = {
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
};

GXRModeObj RM_NTSC_448I = {
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
	.vfilter         = { 0, 0, 21, 22, 21, 0, 0}, /* TODO: filter? */
	.sample_pattern  = {
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6}}
};

GXRModeObj RM_NTSC_480I = {
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
	.vfilter         = { 0, 0, 21, 22, 21, 0, 0}, /* TODO: filter? */
	.sample_pattern  = {
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6}}
};

GXRModeObj RM_NTSC_448P = {
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
};

GXRModeObj RM_NTSC_480P = {
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
};


//PAL rendering modes (TODO:_DO this)
GXRModeObj RM_PAL_224P = {
	.viTVMode        = VI_TVMODE_PAL_DS,
	.fbWidth         = 640,
	.efbHeight       = 448,
	.xfbHeight       = 224,
	.viXOrigin       = (VI_MAX_WIDTH_PAL - 704)/2,
	.viYOrigin       = (VI_MAX_HEIGHT_PAL/2 - 448/2)/2,
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
};

GXRModeObj RM_PAL_240P = {
	.viTVMode        = VI_TVMODE_PAL_DS,
	.fbWidth         = 640,
	.efbHeight       = 480,
	.xfbHeight       = 240,
	.viXOrigin       = (VI_MAX_WIDTH_PAL - 704)/2,
	.viYOrigin       = (VI_MAX_HEIGHT_PAL/2 - 480/2)/2,
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
};

GXRModeObj RM_PAL_256P = {
	.viTVMode        = VI_TVMODE_PAL_DS,
	.fbWidth         = 640,
	.efbHeight       = 512,
	.xfbHeight       = 256,
	.viXOrigin       = (VI_MAX_WIDTH_PAL - 704)/2,
	.viYOrigin       = (VI_MAX_HEIGHT_PAL/2 - 512/2)/2,
	.viWidth         = 704,
	.viHeight        = 512,
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
};

GXRModeObj RM_PAL_448I = {
	.viTVMode        = VI_TVMODE_PAL_INT,
	.fbWidth         = 640,
	.efbHeight       = 448,
	.xfbHeight       = 448,
	.viXOrigin       = (VI_MAX_WIDTH_PAL - 704)/2,
	.viYOrigin       = (VI_MAX_HEIGHT_PAL - 448)/2,
	.viWidth         = 704,
	.viHeight        = 448,
	.xfbMode         = VI_XFBMODE_DF,
	.field_rendering = GX_FALSE,
	.aa              = GX_FALSE,
	.copy_interlaced = GX_COPY_PROGRESSIVE,
	.vfilter         = { 0, 0, 21, 22, 21, 0, 0}, /* TODO: filter? */
	.sample_pattern  = {
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6}}
};

GXRModeObj RM_PAL_480I = {
	.viTVMode        = VI_TVMODE_PAL_INT,
	.fbWidth         = 640,
	.efbHeight       = 480,
	.xfbHeight       = 480,
	.viXOrigin       = (VI_MAX_WIDTH_PAL - 704)/2,
	.viYOrigin       = (VI_MAX_HEIGHT_PAL - 480)/2,
	.viWidth         = 704,
	.viHeight        = 480,
	.xfbMode         = VI_XFBMODE_DF,
	.field_rendering = GX_FALSE,
	.aa              = GX_FALSE,
	.copy_interlaced = GX_COPY_PROGRESSIVE,
	.vfilter         = { 0, 0, 21, 22, 21, 0, 0}, /* TODO: filter? */
	.sample_pattern  = {
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6}}
};

GXRModeObj RM_PAL_512I = {
	.viTVMode        = VI_TVMODE_PAL_INT,
	.fbWidth         = 640,
	.efbHeight       = 512,
	.xfbHeight       = 512,
	.viXOrigin       = (VI_MAX_WIDTH_PAL - 704)/2,
	.viYOrigin       = (VI_MAX_HEIGHT_PAL - 512)/2,
	.viWidth         = 704,
	.viHeight        = 512,
	.xfbMode         = VI_XFBMODE_DF,
	.field_rendering = GX_FALSE,
	.aa              = GX_FALSE,
	.copy_interlaced = GX_COPY_PROGRESSIVE,
	.vfilter         = { 0, 0, 21, 22, 21, 0, 0}, /* TODO: filter? */
	.sample_pattern  = {
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6},
		{6,6},{6,6},{6,6}}
};

GXRModeObj RM_PAL_448P = {
	.viTVMode        = VI_TVMODE_PAL_PROG,
	.fbWidth         = 640,
	.efbHeight       = 448,
	.xfbHeight       = 448,
	.viXOrigin       = (VI_MAX_WIDTH_PAL - 704)/2,
	.viYOrigin       = (VI_MAX_HEIGHT_PAL - 448)/2,
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
};

GXRModeObj RM_PAL_480P = {
	.viTVMode        = VI_TVMODE_PAL_PROG,
	.fbWidth         = 640,
	.efbHeight       = 480,
	.xfbHeight       = 480,
	.viXOrigin       = (VI_MAX_WIDTH_PAL - 704)/2,
	.viYOrigin       = (VI_MAX_HEIGHT_PAL - 480)/2,
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
};

GXRModeObj RM_PAL_512P = {
	.viTVMode        = VI_TVMODE_PAL_PROG,
	.fbWidth         = 640,
	.efbHeight       = 512,
	.xfbHeight       = 512,
	.viXOrigin       = (VI_MAX_WIDTH_PAL - 704)/2,
	.viYOrigin       = (VI_MAX_HEIGHT_PAL - 512)/2,
	.viWidth         = 704,
	.viHeight        = 512,
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
};

