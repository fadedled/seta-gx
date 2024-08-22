/*  Copyright 2003-2004 Guillaume Duhamel
    Copyright 2004-2008 Theo Berkau
    Copyright 2006 Fabien Coulon

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



#include "vidsoft.h"
#include "vidshared.h"
#include "debug.h"
#include "vdp1.h"
#include "vdp2.h"
#include "osd/osd.h"

#ifdef HAVE_LIBGL
#define USE_OPENGL
#endif

#include "yui.h"

#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <math.h>
#include <gccore.h>

//TURN TO WII FORMAT (FOR TEXTURES)
#define COL2WII_16(alpha, col) (alpha | (col & 0x1F) << 11 | ((col & 0x3E0) << 14) | ((col & 0x7C00) << 17))
#define TO_RGB5A3(col) (((col & 0x1F) >> 1) | ((col & 0x3C0) >> 2) | ((col & 0x7800) >> 3))
#define COL2WII_32(alpha, col) (alpha << 24 | (col & 0xFF) << 16 | (col & 0xFF00) | ((col & 0xFF0000) >> 16))
#define COL2WII_16A(col) (((col & 0x8000) << 16) | (col & 0x1F) << 19 | ((col & 0x3E0) << 6) | ((col & 0x7C00) >> 7))
#define COL2WII_32A(col) ((col & 0xFF) << 16 | (col & 0xFF00FF00) | ((col & 0xFF0000) >> 16))
//TURN TO WII FORMAT (FOR NORMALCOLORS) UNUSED FOR NOW
#define COL2CPU_16(alpha, col) ((alpha & 0xFF) | (col & 0x1F) << 27 | ((col & 0x3E0) << 14) | ((col & 0x7C00) << 1))
#define COL2CPU_32(alpha, col) ((alpha & 0xFF) | (col & 0xFF) << 24 | ((col & 0xFF00) << 8) | ((col & 0xFF0000) >> 8))

#define COLOR_ADDt(b)		(b>0xFF?0xFF:(b<0?0:b))
#define COLOR_ADDb(b1,b2)	COLOR_ADDt((s32) (b1) + (b2))
#ifdef WORDS_BIGENDIAN
/* This is the wrong conversion with COLSAT2YAB?? owing to double conversions.
#define COLOR_ADD(l,r,g,b)      (l & 0xFF) | \
                                (COLOR_ADDb((l >> 8) & 0xFF, b) << 8) | \
                                (COLOR_ADDb((l >> 16) & 0xFF, g) << 16) | \
                                (COLOR_ADDb((l >> 24), r) << 24)
*/
#define COLOR_ADD(l,r,g,b) 		(l & 0xFF000000) | \
                                (COLOR_ADDb((l >> 16) & 0xFF, r) << 16) | \
                                (COLOR_ADDb((l >> 8) & 0xFF, g) << 8) | \
                                (COLOR_ADDb(l & 0xFF, b))

#define COLOR_ADD16LIT(l,r,g,b) (COLOR_ADDb((l << 3) & 0xF8, r)) |       \
                                (COLOR_ADDb((l >> 2) & 0xF8, g) << 8) |  \
                                (COLOR_ADDb((l >> 7) & 0xF8, b) << 16) | \
                                (l & 0x8000)
#else
#define COLOR_ADD(l,r,g,b)	COLOR_ADDb((l & 0xFF), r) | \
							(COLOR_ADDb((l >> 8) & 0xFF, g) << 8) | \
							(COLOR_ADDb((l >> 16) & 0xFF, b) << 16) | \
							(l & 0xFF000000)
#endif

u8 *vdp1framebuffer[2]= { NULL, NULL };
u8 *vdp1frontframebuffer;
u8 *vdp1backframebuffer;

//Window texture
//XXX: background texture data
//static u8 bg_alpha_tex[4][352 * 256] ATTRIBUTE_ALIGN(32);
static u8 *win_tex;
//static u8 win_tex[640 * 512] ATTRIBUTE_ALIGN(32);

//XXX: This is not used.
static struct Display {
	u32 x;
	u32 y;
	u32 w;
	u32 h;
	u32 scale_y;	// how much to scale texture in y direction
	u32 highres;	// how much to scale texture in x direction
} disp;

static int vdp1width;
static int vdp1height;
static int vdp1interlace;
static int vdp1pixelsize;

int vdp2width;
int vdp2height;
int rbg0width;

#define SCR_NBG0	0x08
#define SCR_NBG1	0x04
#define SCR_NBG2	0x02
#define SCR_NBG3	0x01
#define SCR_RBG0	0x10

//static int resxratio;
//static int resyratio;

int vdp2_x_hires = 0;
int vdp2_interlace = 0;
static int rbg0height = 0;
int bad_cycle_setting[6] = { 0 };


Mtx mat[8] ATTRIBUTE_ALIGN(32);

//Holds 3 matrices (Sprite, BG, Identity)
//f32 mat[36] ATTRIBUTE_ALIGN(32);

struct __gx_texobj
{
	u32 tex_filt;
	u32 tex_lod;
	u32 tex_size;
	u32 tex_maddr;
	u32 usr_data;
	u32 tex_fmt;
	u32 tex_tlut;
	u16 tex_tile_cnt;
	u8 tex_tile_type;
	u8 tex_flag;
};

//GX objects
//XXX: Can we optimize this?
GXTexObj tex_obj_vdp1;
GXTexObj tex_obj_bg[5];
GXTexObj tex_obj_nbg2_a;
GXTexObj tex_obj_ci8;
GXTexObj tex_obj_i8;
GXTexObj tex_obj_ci4;
GXTexObj tex_obj_i4;
GXTexObj tobj_rgb;
GXTexObj tobj_ci;
GXTexObj tobj_bitmap;
GXTlutObj tlut_obj;
//vram clone for converted textures
//Double VRAM because of tiles
u8 *wii_vram;
u32 tex_dirty[0x200];

//Vdp1 Framebuffer copy for vpd2
//u32 display_fb[640 * 480] ATTRIBUTE_ALIGN(32);
//extern u32 display_fb[256 * 352] ATTRIBUTE_ALIGN(32);
static u8 screen_list[8];
u32 bad_cycle_setting_nbg3 = 0;

typedef struct
{
   int pagepixelwh, pagepixelwh_bits, pagepixelwh_mask;
   int planepixelwidth, planepixelwidth_bits, planepixelwidth_mask;
   int planepixelheight, planepixelheight_bits, planepixelheight_mask;
   int screenwidth;
   int screenheight;
   int oldcellx, oldcelly, oldcellcheck;
   int xmask, ymask;
   u32 planetbl[16];
} screeninfo_struct;


//////////////////////////////////////////////////////////////////////////////
//Converts saturn bitmap 4bpp textures
static void wii_sat2tex4bpp(u32 addr, u32 w, u32 h)
{
	if ((addr + (w*h*2)) > 0x80000) {
		h = (0x80000 - addr) / w;
	}
	w >>= 3;
	volatile f32 *tmem = (f32*) GX_RedirectWriteGatherPipe(wii_vram + addr);
	f32 *src0 = (f32*) (Vdp2Ram + addr);
	f32 *src1 = src0 + w;
	f32 *src2 = src1 + w;
	f32 *src3 = src2 + w;
	f32 *src4 = src3 + w;
	f32 *src5 = src4 + w;
	f32 *src6 = src5 + w;
	f32 *src7 = src6 + w;

	register u32 lines = h >> 3;
	register u32 inc = w * 7;
	while (lines--) {
		u32 tiles = w;
		do {
			*tmem = *(src0++); *tmem = *(src1++);
			*tmem = *(src2++); *tmem = *(src3++);
			*tmem = *(src4++); *tmem = *(src5++);
			*tmem = *(src6++); *tmem = *(src7++);
		} while (--tiles);
		src0 += inc; src1 += inc;
		src2 += inc; src3 += inc;
		src4 += inc; src5 += inc;
		src6 += inc; src7 += inc;
	}
	GX_RestoreWriteGatherPipe();
}


//Converts saturn bitmap 16bpp textures
static void wii_sat2texRGBA(u32 addr, u32 w, u32 h)
{
	if ((addr + (w*h*2)) > 0x80000) {
		h = (0x80000 - addr) / w;
	}
	w >>= 2;
	volatile f64 *tmem = (f64*) GX_RedirectWriteGatherPipe(wii_vram);
	f64 *src0 = (f64*) (Vdp2Ram + addr);
	f64 *src1 = src0 + w;
	f64 *src2 = src1 + w;
	f64 *src3 = src2 + w;

	register u32 lines = h >> 2;
	register const u32 inc = w * 3;
	while (lines--) {
		u32 tiles = w;
		do {
			*tmem = *(src0++);
			*tmem = *(src1++);
			*tmem = *(src2++);
			*tmem = *(src3++);
		} while (--tiles);
		src0 += inc;
		src1 += inc;
		src2 += inc;
		src3 += inc;
	}
	GX_RestoreWriteGatherPipe();
}


//Converts saturn bitmap 16bpp textures
static void wii_sat2texRGBA32(u32 addr, u32 w, u32 h)
{
	w >>= 2;
	volatile f64 *tmem = (f64*) GX_RedirectWriteGatherPipe(wii_vram + addr);
	f64 *src0 = (f64*) (Vdp2Ram + addr);
	f64 *src1 = src0 + w;
	f64 *src2 = src1 + w;
	f64 *src3 = src2 + w;

	register u32 lines = h >> 2;
	register const u32 inc = w * 3;
	while (lines--) {
		u32 tiles = w;
		do {
			*tmem = *(src0++);
			*tmem = *(src1++);
			*tmem = *(src2++);
			*tmem = *(src3++);
		} while (--tiles);
		src0 += inc;
		src1 += inc;
		src2 += inc;
		src3 += inc;
	}
	GX_RestoreWriteGatherPipe();
}


//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static INLINE u32 FASTCALL Vdp2ColorRamGetColorSoft(u32 addr)
{
	u32 mode = Vdp2Internal.ColorMode >> 1;
	addr <<= (mode + 1);
	if (!mode) {
		u32 tmp = T2ReadWord(Vdp2ColorRam, addr & 0xFFF);
		/* we preserve MSB for special color calculation mode 3 (see Vdp2 user's manual 3.4 and 12.3) */
		//XXX: why do we change format??
		return (((tmp & 0x1F) << 3) | ((tmp & 0x03E0) << 6) | ((tmp & 0x7C00) << 9)) | ((tmp & 0x8000) << 16);
	}

	return T2ReadLong(Vdp2ColorRam, addr & 0xFFF);
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
//XXX: important to optimize this function
static INLINE void Vdp2PatternAddr(vdp2draw_struct *info)
{
   switch(info->patterndatasize)
   {
      case 1:
      {
         u16 tmp = T1ReadWord(Vdp2Ram, info->addr);

         info->addr += 2;
         info->specialfunction = (info->supplementdata >> 9) & 0x1;
         info->specialcolorfunction = (info->supplementdata >> 8) & 0x1;

         switch(info->colornumber)
         {
            case 0: // in 16 colors
               info->paladdr = ((tmp & 0xF000) >> 8) | ((info->supplementdata & 0xE0) << 3);
               break;
            default: // not in 16 colors
               info->paladdr = (tmp & 0x7000) >> 4;
               break;
         }

         switch(info->auxmode)
         {
            case 0:
               info->flipfunction = (tmp & 0xC00) >> 10;

               switch(info->patternwh)
               {
                  case 1:
                     info->charaddr = (tmp & 0x3FF) | ((info->supplementdata & 0x1F) << 10);
                     break;
                  case 2:
                     info->charaddr = ((tmp & 0x3FF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x1C) << 10);
                     break;
               }
               break;
            case 1:
               info->flipfunction = 0;

               switch(info->patternwh)
               {
                  case 1:
                     info->charaddr = (tmp & 0xFFF) | ((info->supplementdata & 0x1C) << 10);
                     break;
                  case 2:
                     info->charaddr = ((tmp & 0xFFF) << 2) | (info->supplementdata & 0x3) | ((info->supplementdata & 0x10) << 10);
                     break;
               }
               break;
         }

         break;
      }
      case 2: {
         u16 tmp1 = T1ReadWord(Vdp2Ram, info->addr);
         u16 tmp2 = T1ReadWord(Vdp2Ram, info->addr+2);
         info->addr += 4;
         info->charaddr = tmp2 & 0x7FFF;
         info->flipfunction = (tmp1 & 0xC000) >> 14;
         switch(info->colornumber) {
            case 0:
               info->paladdr = (tmp1 & 0x7F) << 4;
               break;
            default:
               info->paladdr = ((tmp1 & 0x70) << 4);
               break;
         }
         info->specialfunction = (tmp1 & 0x2000) >> 13;
         info->specialcolorfunction = (tmp1 & 0x1000) >> 12;
         break;
      }
   }

   if (!(Vdp2Regs->VRSIZE & 0x8000))
      info->charaddr &= 0x3FFF;

   info->charaddr *= 0x20; // selon Runik
   if (info->specialprimode == 1) {
      info->priority = (info->priority & 0xE) | (info->specialfunction & 1);
   }
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static INLINE u32 FASTCALL DoNothing(UNUSED void *info, u32 pixel)
{
   return pixel;
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static INLINE u32 FASTCALL DoColorOffset(void *info, u32 pixel)
{
    return COLOR_ADD(pixel, ((vdp2draw_struct *)info)->cor,
                     ((vdp2draw_struct *)info)->cog,
                     ((vdp2draw_struct *)info)->cob);
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static INLINE void ReadVdp2ColorOffset(Vdp2 * regs, vdp2draw_struct *info, int clofmask, int ccmask)
{
	info->cor = info->cog = info->cob = 0;
	if (regs->CLOFEN & clofmask)
	{
		// color offset enable
		//SEMI-Optimize
		if (regs->CLOFSL & clofmask)
		{
			// color offset B
			info->cor = (regs->COBR & 0xFF) | (-(regs->COBR & 0x100));
			info->cog = (regs->COBG & 0xFF) | (-(regs->COBG & 0x100));
			info->cob = (regs->COBB & 0xFF) | (-(regs->COBB & 0x100));
		}
		else
		{
			// color offset A
			info->cor = (regs->COAR & 0xFF) | (-(regs->COAR & 0x100));
			info->cog = (regs->COAG & 0xFF) | (-(regs->COAG & 0x100));
			info->cob = (regs->COAB & 0xFF) | (-(regs->COAB & 0x100));
		}
		info->PostPixelFetchCalc = &DoColorOffset;
	}
	else // color offset disable
		info->PostPixelFetchCalc = &DoNothing;

}

//HALF-DONE
static INLINE u8 gfx_GetColorOffset(int mask)
{
	// color offset enable
	if (Vdp2Regs->CLOFEN & mask)
	{
		//SEMI-Optimize
		if (Vdp2Regs->CLOFSL & mask) {
			// color offset B
			return GX_CC_C1;
		} else {
			// color offset A
			return GX_CC_C0;
		}
	}
	return GX_CC_ZERO;

}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static INLINE int Vdp2FetchPixel(vdp2draw_struct *info, int x, int y, u32 *color, u32 *dot)
{

   switch(info->colornumber)
   {
      case 0: // 4 BPP
         *dot = T1ReadByte(Vdp2Ram, ((info->charaddr + (((y * info->cellw) + x) / 2)) & 0x7FFFF));
         *dot >>= (~x & 0x1) << 2;
         if (!(*dot & 0xF) && info->transparencyenable) return 0;
         else
         {
            *color = Vdp2ColorRamGetColorSoft(info->coloroffset + (info->paladdr | (*dot & 0xF)));
            return 1;
         }
      case 1: // 8 BPP
         *dot = T1ReadByte(Vdp2Ram, ((info->charaddr + (y * info->cellw) + x) & 0x7FFFF));
         if (!(*dot & 0xFF) && info->transparencyenable) return 0;
         else
         {
            *color = Vdp2ColorRamGetColorSoft(info->coloroffset + (info->paladdr | (*dot & 0xFF)));
            return 1;
         }
      case 2: // 16 BPP(palette)
         *dot = T1ReadWord(Vdp2Ram, ((info->charaddr + ((y * info->cellw) + x) * 2) & 0x7FFFF));
         if ((*dot == 0) && info->transparencyenable) return 0;
         else
         {
            *color = Vdp2ColorRamGetColorSoft(info->coloroffset + *dot);
            return 1;
         }
      case 3: // 16 BPP(RGB)
         *dot = T1ReadWord(Vdp2Ram, ((info->charaddr + ((y * info->cellw) + x) * 2) & 0x7FFFF));
         if (!(*dot & 0x8000) && info->transparencyenable) return 0;
         else
         {
            //XXX: I think that eventualy you will need to do this
            //*color = COL2WII_16(0, dot);
			u32 clr = *dot;
            *color = (((clr & 0x1F) << 3) | ((clr & 0x03E0) << 6) | ((clr & 0x7C00) << 9));
            return 1;
         }
      case 4: // 32 BPP
         *dot = T1ReadLong(Vdp2Ram, ((info->charaddr + ((y * info->cellw) + x) * 4) & 0x7FFFF));
         if (!(*dot & 0x80000000) && info->transparencyenable) return 0;
         else
         {
            //XXX: I think that eventualy you will need to do this
            //*color = COL2WII_32(0, dot);
            *color = *dot & 0xFFFFFF;
            return 1;
         }
      default:
         return 0;
   }
}

//HALF-DONE
static INLINE u16 gfx_GetVpd2TexelSimple(vdp2draw_struct *info, u32 x, u32 y, u8 *a_mask)
{
	//XXX: this can be optimized
	u32 addr = ((y << 3) + x) >> !info->colornumber;
	u8 value = T1ReadByte(Vdp2Ram, ((info->charaddr + addr) & 0x7FFFF));
	//XXX: this is only 4bpp
	if (!info->colornumber) {
		value = (value >> ((~x & 0x1) << 2)) & 0xF;
	}
	//Mask off the transparent values if transparency is enabled
	return (info->coloroffset + (info->paladdr | value)) & (!value && info->transparencyenable ? 0x0 : 0x7FF);
}

//HALF-DONE
static INLINE u16 gfx_GetVpd2Texel(vdp2draw_struct *info, u32 x, u32 y)
{
	//XXX: this can be optimized
	u32 addr = (y * info->cellw) + x;
	u16 value = 0;
	//XXX: return value
	switch(info->colornumber)
	{
		case 0: // 4 BPP
			value = T1ReadByte(Vdp2Ram, ((info->charaddr + (addr / 2)) & 0x7FFFF));
			if (!value && info->transparencyenable)
				return 0x0000;
			else
				return info->coloroffset + (info->paladdr | value);
		case 1: // 8 BPP
			value = T1ReadByte(Vdp2Ram, ((info->charaddr + addr) & 0x7FFFF)) & 0xFF;
			if (!value && info->transparencyenable)
				return 0x0000;
			else
				return info->coloroffset + (info->paladdr | value);
		case 2: // 16 BPP(palette)
			value = T1ReadWord(Vdp2Ram, ((info->charaddr + (addr * 2)) & 0x7FFFF)) & 0x7FF;
			if (!value && info->transparencyenable)
				return 0x0000;
			else
				return info->coloroffset + (info->paladdr | value);
		case 3: // 16 BPP(RGB)
			//XXX: make this better
			value = T1ReadWord(Vdp2Ram, ((info->charaddr + (addr * 2)) & 0x7FFFF));
			if (!(value & 0x8000) && info->transparencyenable)
				return 0x0000;
			else
				return value | 0x8000;
		case 4: // 32 BPP
		 //NOT IMPLEMENTED YET
#if 0
         *dot = T1ReadLong(Vdp2Ram, ((info->charaddr + (addr * 4)) & 0x7FFFF));
         if (!(*dot & 0x80000000) && info->transparencyenable) return 0;
         else
         {
            //XXX: I think that eventualy you will need to do this
            //*color = COL2WII_32(0, dot);
            *color = *dot & 0xFFFFFF;
            return 1;
         }
 #endif
      default:
         return 0;
   }
}

//////////////////////////////////////////////////////////////////////////////
//HALF-DONE

GXTexObj tobj_win;

void gfx_DrawWindowTex(u32 wctl, u32 priority)
{
	//win_act contains active windows [sw, w1, w0]
	//for each active window we set its corresponding swapmode and comparison operation
	//win_inv says how it's correspoding konstant color is inverted (we want the == that makes sense)
	//if op is AND we want greater than 0, else we want less than 1 and Konstant colors are inverted.
	if (!(wctl & 0x2A)) {
		return;
	}

	GX_SetZCompLoc(GX_FALSE);
	SGX_SetZOffset(priority);
	GX_SetColorUpdate(GX_FALSE);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0,  GX_DIRECT);

	GX_SetCurrentMtx(GXMTX_IDENTITY);

	//TODO: Optimize this
	u32 comp_op = 7;
	u32 ch_count = 0;
	u8 op = (wctl >> 7) & 1;
	u8 op_mask = -op;
	u8 ch[4] = {GX_CH_ALPHA, GX_CH_ALPHA, GX_CH_ALPHA, GX_CH_ALPHA};
	u8 kcol[4] = {0xFF, 0xFF, 0xFF, 0xFF};
	for (u32 i = 0; i < 3; ++i) {
		if (wctl & 0x2) {
			ch[ch_count] = i;
			if (wctl & 0x1) {
				kcol[ch_count] = op_mask ^ 0xFF;
			} else {
				kcol[ch_count] = op_mask;
			}
			comp_op += 2;
			ch_count++;
		}
		wctl >>= 2;
	}
	GX_SetTevSwapModeTable(GX_TEV_SWAP2, ch[0], ch[1], ch[2], GX_CH_ALPHA);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP2);
	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) kcol));

	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);

	GX_InitTexObj(&tobj_win, win_tex, disp.w, disp.h, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&tobj_win, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_LoadTexObj(&tobj_win, GX_TEXMAP0);

	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_KONST, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, comp_op, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CA_KONST, GX_CC_ZERO);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);

	if (op) {
		GX_SetAlphaCompare(GX_EQUAL, 0xFF, GX_AOP_AND, GX_ALWAYS, 0);
	} else {
		GX_SetAlphaCompare(GX_NEQUAL, 0xFF, GX_AOP_AND, GX_ALWAYS, 0);
	}
	//Draw the window
	GX_Begin(GX_QUADS, GX_VTXFMT1, 4);		// Draw A Quad
		GX_Position2s16(0, 0);				// Top Left
		GX_TexCoord2u16(0, 0);
		GX_Position2s16(disp.w, 0);			// Top Right
		GX_TexCoord2u16(disp.w, 0);
		GX_Position2s16(disp.w, disp.h);	// Bottom Right
		GX_TexCoord2u16(disp.w, disp.h);
		GX_Position2s16(0, disp.h);			// Bottom Left
		GX_TexCoord2u16(0, disp.h);
	GX_End();								// Done Drawing The Quad

	//Restore previous settings
	GX_SetColorUpdate(GX_TRUE);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
}


static void gfx_DrawWindow(u32 win_ctl, u16 *win_pos)
{
	u32 h_shft = disp.highres ^ 1;
	if (win_ctl & 0x80000000) {
		u32 line_addr = (win_ctl & 0x7FFFE) << 1;
		GX_Begin(GX_LINES, GX_VTXFMT1, disp.h * 2);
		for (u32 i = 0; i < disp.h; ++i) {
			u16 x0 = T1ReadWord(Vdp2Ram, line_addr);
			u16 x1 = T1ReadWord(Vdp2Ram, line_addr + 2);
			line_addr += 4;

			if (x1 == 0xFFFF) {
				x0 = 0;
				x1 = 0;
			}

			GX_Position2s16(x0 >> h_shft, i + 1);
			GX_Position2s16(x1 >> h_shft, i + 1);
		}
		GX_End();
	} else {
		//XXX: this is only for W0
		u16 x0 = win_pos[0] >> h_shft;
		u16 y0 = win_pos[1] + 1;
		u16 x1 = win_pos[2] >> h_shft;
		u16 y1 = win_pos[3] + 1;
		GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
			GX_Position2s16(x0, y0);
			GX_Position2s16(x1, y0);
			GX_Position2s16(x1, y1);
			GX_Position2s16(x0, y1);
		GX_End();
	}
}


void gfx_WindowTextureGen(void)
{
	const GXColor w0_kc = {0xFF, 0x00, 0x00, 0xFF};	//red
	const GXColor w1_kc = {0x00, 0xFF, 0x00, 0xFF};	//blue
	const GXColor sw_kc = {0x00, 0x00, 0xFF, 0xFF};	//green

	GX_SetScissor(0, 0, disp.w, disp.h);	//Use actual values disp.w and disp.h

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0 | GX_TEXMAP_DISABLE, GX_COLORNULL);

	GX_SetCurrentMtx(GXMTX_IDENTITY);

	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP1);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);

	//TODO: Check if we need to clear the screen
	GX_SetBlendMode(GX_BM_LOGIC, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_Begin(GX_QUADS, GX_VTXFMT1, 4);		// Draw A Quad
		GX_Position2s16(0, 0);				// Top Left
		GX_Position2s16(disp.w, 0);			// Top Right
		GX_Position2s16(disp.w, disp.h);	// Bottom Right
		GX_Position2s16(0, disp.h);			// Bottom Left
	GX_End();								// Done Drawing The Quad

	//Draw Window 0
	GX_SetBlendMode(GX_BM_LOGIC, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_OR);
	GX_SetTevKColor(GX_KCOLOR0, w0_kc);
	gfx_DrawWindow(Vdp2Regs->LWTA0.all, &Vdp2Regs->WPSX0);

	//Draw Window 1
	GX_SetTevKColor(GX_KCOLOR0, w1_kc);
	gfx_DrawWindow(Vdp2Regs->LWTA1.all, &Vdp2Regs->WPSX1);

	//Sprite window (Only draw if sprite window is used)
	//Change tev to accept textures
	//TODO: Add this feature
	if (0) {
		GX_SetTevKColor(GX_KCOLOR0, sw_kc);
		GX_SetVtxDesc(GX_VA_TEX0,  GX_DIRECT);
		//gfx_DrawWindow(40, 40, 200, 100);
	}

	//Copy the red component of FB
	GX_Flush();
	GX_DrawDone();
	GX_SetTexCopySrc(0, 0, disp.w, disp.h);
	GX_SetTexCopyDst(disp.w, disp.h, GX_TF_RGB565, GX_FALSE);
	GX_CopyTex(win_tex, GX_TRUE);
	GX_PixModeSync();
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_AND);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
}


//DONE
static INLINE int TestBothWindow(int wctl, clipping_struct *clip, int x, int y)
{
    return 1;
}


//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static INLINE void GeneratePlaneAddrTable(vdp2draw_struct *info, u32 *planetbl, void FASTCALL (* PlaneAddr)(void *, int))
{
	u32 i, range = (info->mapwh * info->mapwh);

	for (i = 0; i < range; ++i) {
		PlaneAddr(info, i);
		planetbl[i] = info->addr;
	}
}

//////////////////////////////////////////////////////////////////////////////


static INLINE void FASTCALL Vdp2MapCalcXY(vdp2draw_struct *info, u32 *x, u32 *y,
                                 screeninfo_struct *sinfo)
{
	int planenum;
	const int pagesize_bits = info->pagewh_bits*2;
	const int cellwh = (2 + info->patternwh);

	const int check = ((y[0] >> cellwh) << 16) | (x[0] >> cellwh);
	//if ((x[0] >> cellwh) != sinfo->oldcellx || (y[0] >> cellwh) != sinfo->oldcelly)
	if(check != sinfo->oldcellcheck)
	{
		sinfo->oldcellx = x[0] >> cellwh;
		sinfo->oldcelly = y[0] >> cellwh;
		sinfo->oldcellcheck = (sinfo->oldcelly << 16) | sinfo->oldcellx;

		// Calculate which plane we're dealing with
		planenum = ((y[0] >> sinfo->planepixelheight_bits) * info->mapwh) + (x[0] >> sinfo->planepixelwidth_bits);
		x[0] = (x[0] & sinfo->planepixelwidth_mask);
		y[0] = (y[0] & sinfo->planepixelheight_mask);

		// Fetch and decode pattern name data
		info->addr = sinfo->planetbl[planenum];

		// Figure out which page it's on(if plane size is not 1x1)
		info->addr += ((  ((y[0] >> sinfo->pagepixelwh_bits) << pagesize_bits) << info->planew_bits) +
						(   (x[0] >> sinfo->pagepixelwh_bits) << pagesize_bits) +
						(((y[0] & sinfo->pagepixelwh_mask) >> cellwh) << info->pagewh_bits) +
						((x[0] & sinfo->pagepixelwh_mask) >> cellwh)) << (info->patterndatasize_bits+1);

		Vdp2PatternAddr(info); // Heh, this could be optimized
	}

	// Figure out which pixel in the tile we want
	//THIS IS OPTIMIZED
	u32 fx = info->flipfunction & 0x1;
	u32 fy = (info->flipfunction & 0x2) >> 1;
	u32 ptrn = info->patternwh_bits << 3;
	//I THINK THIS IS GOOD for flip_x/flip_y:
	u32 flip_x = (-fx) & 0x7;
	u32 flip_y = (-fy) & (0x7 + ptrn);
	u32 bit_x = x[0] ^ (fx << 3);
	u32 bit_y = y[0] ^ (fy << 3);
	u32 off_y = (bit_y & ptrn) + (bit_x & ptrn);

	y[0] = ((y[0] ^ flip_y) & (0x7 + ptrn)) + off_y;
	x[0] = ((x[0] ^ flip_x) & 0x7);
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static INLINE void SetupScreenVars(vdp2draw_struct *info, screeninfo_struct *sinfo, void FASTCALL (* PlaneAddr)(void *, int))
{
	u32 comp = !info->isbitmap;

	sinfo->pagepixelwh = 0;
	sinfo->pagepixelwh_bits = 0;
	sinfo->pagepixelwh_mask = 0;
	sinfo->planepixelwidth = 0;
	sinfo->planepixelwidth_bits = 0;
	sinfo->planepixelwidth_mask = 0;
	sinfo->planepixelheight = 0;
	sinfo->planepixelheight_bits = 0;
	sinfo->planepixelheight_mask = 0;
	sinfo->screenwidth = 0;
	sinfo->screenheight = 0;
	sinfo->oldcellx = -comp;
	sinfo->oldcelly = -comp;
	sinfo->oldcellcheck = -comp;
	sinfo->xmask = info->cellw - 1;
	sinfo->ymask = info->cellh - 1;

	if (comp) {
		sinfo->pagepixelwh = 512;
		sinfo->pagepixelwh_bits = 9;
		sinfo->pagepixelwh_mask = 511;

		sinfo->planepixelwidth = info->planew * sinfo->pagepixelwh;
		sinfo->planepixelwidth_bits = info->planew + 0x8;
		sinfo->planepixelwidth_mask = (1 << (sinfo->planepixelwidth_bits)) - 1;

		sinfo->planepixelheight = info->planeh * sinfo->pagepixelwh;
		sinfo->planepixelheight_bits = info->planeh + 0x8;
		sinfo->planepixelheight_mask = (1<<(sinfo->planepixelheight_bits))-1;

		sinfo->screenwidth  = info->mapwh * sinfo->planepixelwidth;
		sinfo->screenheight = info->mapwh * sinfo->planepixelheight;
		sinfo->xmask = sinfo->screenwidth - 1;
		sinfo->ymask = sinfo->screenheight - 1;
		GeneratePlaneAddrTable(info, sinfo->planetbl, PlaneAddr);
	}
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL GetAlpha(vdp2draw_struct * info, u32 color, u32 dot)
{
	//XXX: this must be optimized, this should be outside when calculating each value (specialcolorfunction / dot / color)
	u32 val = 0x00;
	if (((info->specialcolormode == 1) || (info->specialcolormode == 2)) && ((info->specialcolorfunction & 1) == 0)) {
		/* special color calculation mode 1 and 2 enables color calculation only when special color function = 1 */
		val = 0x1FF;
	} else if (info->specialcolormode == 2) {
		/* special color calculation 2 enables color calculation according to lower bits of the color code */
		if ((info->specialcode & (1 << ((dot & 0xF) >> 1))) == 0) {
			val = 0x1FF;
		}
	} else if ((info->specialcolormode == 3) && ((color & 0x80000000) == 0)) {
		/* special color calculation mode 3 enables color calculation only for dots with MSB = 1 */
		val =  0xFF;
	}
	return info->alpha | val;
}

#if 0
static u8 FASTCALL CalcAlpha(vdp2draw_struct * info, u32 color, u32 ptrn_bit)
{
	//XXX: this must be optimized, this should be outside when calculating each value (specialcolorfunction / dot / color)
	switch (info->specialcolormode) {
		//Per screen
		case 0: {return info->alpha};
		//Per character
		case 1: {

		}
		//Per dot
		case 2:	return 0xFF; // Use another TEV stage
		//Per color
		case 3: return 0xFF; // Use a special color palette
	}
}
#endif


//////////////////////////////////////////////////////////////////////////////

//DONE
u32 PixelIsSpecialPriority(u32 specialcode, u32 dot)
{
	dot = (dot & 0xf) >> 1;
	return (specialcode >> dot) & 1;
}

//////////////////////////////////////////////////////////////////////////////

//DONE
static void Vdp2GetInterlaceInfo(int *start_line, int *line_increment)
{
	//*start_line = vdp2_is_odd_frame & vdp2_interlace;
	*start_line = 0 & vdp2_interlace;
	*line_increment = 1 + vdp2_interlace;
}

//////////////////////////////////////////////////////////////////////////////


//Tests to see if 32-byte algined memory block is full of zeroes
static u32 FASTCALL memIsZeroTest( u32 *mem, u32 size)
{
    register u32 *m = mem;
	register u32 val = 0;
	size >>= 5; //32-byte aligned
	while (size) {
		val |= *(m++);
		val |= *(m++);
		val |= *(m++);
		val |= *(m++);
		val |= *(m++);
		val |= *(m++);
		val |= *(m++);
		val |= *(m++);
		--size;
	}
	return !val;
}

//Draws bitmap screens
static void FASTCALL gfx_DrawBitmap(vdp2draw_struct *info)
{
	screeninfo_struct sinfo;

	//Dont draw if increments are zero
	if (info->coordincy == 0.0 || info->coordincy == 0.0) {
		return;
	}

	GX_SetScissor(0, 0, disp.w, disp.h);

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);
	u32 trn_code = info->transparencyenable << 7;
	u32 tlut_pos = ((info->coloroffset + info->paladdr) >> 4) & 0x7F;
	//XXX: Convert textures before loading
	switch (info->colornumber) {
		case 0: { // 4bpp
			//wii_sat2tex4bpp(Vdp2Ram + info->charaddr, info->cellw >> 1, info->cellh);
			GX_InitTexObjCI(&tobj_bitmap, Vdp2Ram + info->charaddr, info->cellw, info->cellh,
				  GX_TF_CI4, GX_REPEAT, GX_REPEAT, GX_FALSE, TLUT_INDX(trn_code, tlut_pos));
		} break;
		case 1: { // 8bpp
			//wii_sat2texRGBA(Vdp2Ram + info->charaddr, info->cellw >> 1, info->cellh);
			GX_InitTexObjCI(&tobj_bitmap, Vdp2Ram + info->charaddr, info->cellw, info->cellh,
				  GX_TF_CI8, GX_REPEAT, GX_REPEAT, GX_FALSE, TLUT_INDX(trn_code << 1, tlut_pos));
		} break;
		case 2: { // 16bpp (11 bits used)
			//XXX: color palette is wrong?
			//wii_sat2texRGBA(Vdp2Ram + info->charaddr, info->cellw, info->cellh);
			GX_InitTexObjCI(&tobj_bitmap, Vdp2Ram + info->charaddr, info->cellw, info->cellh,
				  GX_TF_CI14, GX_REPEAT, GX_REPEAT, GX_FALSE, TLUT_INDX(0, 0));
		} break;
		case 3: {
			//wii_sat2texRGBA(Vdp2Ram + info->charaddr, info->cellw, info->cellh);
			GX_InitTexObj(&tobj_bitmap, Vdp2Ram + info->charaddr, info->cellw, info->cellh,
				  GX_TF_RGB5A3, GX_REPEAT, GX_REPEAT, GX_FALSE);
		} break;
		case 4: {
			//wii_sat2texRGBA(Vdp2Ram + info->charaddr, info->cellw, info->cellh);
			GX_InitTexObj(&tobj_bitmap, Vdp2Ram  + info->charaddr, info->cellw, info->cellh,
				  GX_TF_RGBA8, GX_REPEAT, GX_REPEAT, GX_FALSE);
		} break;
	}
	GX_InitTexObjFilterMode(&tobj_bitmap, GX_NEAR, GX_NEAR);
	GX_LoadTexObj(&tobj_bitmap, GX_TEXMAP0);

	GX_SetNumTevStages(1);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_A8_GT, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_KONST, GX_CA_ZERO);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);

	const u32 inc_xy = 8 << info->patternwh_bits;
	const u32 mask_xy = (8 << info->patternwh_bits) - 1;
	const u32 pix_h = (disp.h + inc_xy) * info->coordincy;
	const u32 pix_w = (disp.w + inc_xy) * info->coordincx;

	//We don't need to use the matrix except for z value
	mat[GXMTX_VDP2_BG][0][0] = 1.0;
	mat[GXMTX_VDP2_BG][1][1] = 1.0;
	mat[GXMTX_VDP2_BG][0][3] = 0.0;
	mat[GXMTX_VDP2_BG][1][3] = 0.0;
	GX_LoadPosMtxImm(mat[GXMTX_VDP2_BG], GXMTX_VDP2_BG);
	GX_SetCurrentMtx(GXMTX_VDP2_BG);

	SGX_SetZOffset((info->priority << 4) + info->prioffs);

	u32 y = info->y & ~mask_xy;
	//Calculate Alpha
	GXColor konst = {0, 0, 0, 0};
	konst.a = GetAlpha(info, 0, 0);
	GX_SetTevKColor(GX_KCOLOR0, konst);

	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	//XXX: Dont use color
	GX_Begin(GX_QUADS, GX_VTXFMT1, 4);
		GX_Position2s16(0, 0);
		GX_Color1u32(info->alpha);
		GX_TexCoord2u16(0.0f, 0.0f);

		GX_Position2s16(disp.w, 0);
		GX_Color1u32(info->alpha);
		GX_TexCoord2u16(disp.w, 0.0f);

		GX_Position2s16(disp.w, disp.h);
		GX_Color1u32(info->alpha);
		GX_TexCoord2u16(disp.w, disp.h);

		GX_Position2s16(0, disp.h);
		GX_Color1u32(info->alpha);
		GX_TexCoord2u16(0.0f, disp.h);
	GX_End();

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_FALSE, 1, 1);
	GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);
}


//Draws NBG2 and NBG3 scroll screen using GX quads
static void FASTCALL gfx_DrawScroll(vdp2draw_struct *info)
{
	screeninfo_struct sinfo;

	//Dont draw if increments are zero
	if (info->coordincy == 0.0 || info->coordincy == 0.0) {
		return;
	}

	SetupScreenVars(info, &sinfo, info->PlaneAddr);

	gfx_DrawWindowTex(info->wctl, (info->priority << 4) + info->prioffs);

	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	//XXX: ADD WINDOW SUPPORT (Just draw window and use Z-compare)
	const u32 pagesize_bits = info->pagewh_bits << 1;
	const u32 cellwh = (3 + info->patternwh_bits);
	const u32 inc_xy = 8 << info->patternwh_bits;
	const u32 mask_xy = (8 << info->patternwh_bits) - 1;
	const u32 pix_h = (disp.h + inc_xy) * info->coordincy;
	const u32 pix_w = (disp.w + inc_xy) * info->coordincx;

	GX_SetScissor(0, 0, disp.w, disp.h);
	GX_SetNumTevStages(1);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_A8_GT, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_KONST, GX_CA_ZERO);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);

	u32 block_size = 32 << (info->patternwh_bits * 2);
	//XXX: hack, should use other TLUT instead with transparent 0 bit
	u32 trn_code = info->transparencyenable << 7;
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, inc_xy, inc_xy);
	if (info->colornumber) {
		//Only change format...
		//GX_InitTexObjCI(&tobj_ci, Vdp2Ram, inc_xy, inc_xy, GX_TF_CI8, GX_CLAMP, GX_CLAMP, GX_FALSE, 0);
		trn_code <<= 1;
		block_size <<= 1;
		SGX_BeginVdp2Scroll(GX_TF_CI8, inc_xy);
		SGX_CellConverterSet(info->patternwh_bits, SPRITE_8BPP);

		//TODO: This should be changed
		switch (info->colornumber) {
			case 2: { // 16bpp (11 bits used)
				SGX_BeginVdp2Scroll(GX_TF_CI14, inc_xy);
				SGX_CellConverterSet(info->patternwh_bits, SPRITE_8BPP);
			} break;
			case 3: {
				SGX_BeginVdp2Scroll(GX_TF_RGB5A3, inc_xy);
			} break;
			case 4: {
				SGX_BeginVdp2Scroll(GX_TF_RGBA8, inc_xy);
			} break;
		}
	} else {
		//GX_InitTexObjCI(&tobj_ci, Vdp2Ram, inc_xy, inc_xy, GX_TF_CI4, GX_CLAMP, GX_CLAMP, GX_FALSE, 0);
		SGX_BeginVdp2Scroll(GX_TF_CI4, inc_xy);
	}
	//GX_InitTexObjFilterMode(&tobj_ci, GX_NEAR, GX_NEAR);

	mat[GXMTX_VDP2_BG][0][0] = 1.0 / info->coordincx;
	mat[GXMTX_VDP2_BG][1][1] = 1.0 / info->coordincy;
	mat[GXMTX_VDP2_BG][0][3] = -((f32) (info->x & mask_xy));
	mat[GXMTX_VDP2_BG][1][3] = -((f32) (info->y & mask_xy));
	GX_LoadPosMtxImm(mat[GXMTX_VDP2_BG], GXMTX_VDP2_BG);
	GX_SetCurrentMtx(GXMTX_VDP2_BG);

	SGX_SetZOffset((info->priority << 4) + info->prioffs);

	u32 blankchar = 0xffffffff;
	u32 y = info->y & ~mask_xy;
	GXColor konst = {0, 0, 0, 0};

	for (s32 j = 0; j < pix_h; j += inc_xy, y += inc_xy) {
		y &= sinfo.ymask;
		//XXX: this should be mostly useless
		u32 x = info->x & ~mask_xy;
		info->LoadLineParams(info, j);
		//u8 line_alpha = (info->enable ? 0xFF : 0x00);	//This goes in the clipping window values
		for (s32 i = 0; i < pix_w; i += inc_xy, x += inc_xy) {
			x &= sinfo.xmask;
			//u8 alpha = line_alpha;



			//XXX: Per-Pixel Priority still not implemented
			//XXX: Alpha Still not implemented
			// Calculate which plane we're dealing with
			u32 planenum = ((y >> sinfo.planepixelheight_bits) * info->mapwh) + (x >> sinfo.planepixelwidth_bits);
			//XXX: useless?
			u32 x0 = x & sinfo.planepixelwidth_mask;
			u32 y0 = y & sinfo.planepixelheight_mask;

			// Fetch and decode pattern name data
			info->addr = sinfo.planetbl[planenum];

			// Figure out which page it's on(if plane size is not 1x1)
			info->addr += ((  ((y0 >> sinfo.pagepixelwh_bits) << pagesize_bits) << info->planew_bits) +
							(   (x0 >> sinfo.pagepixelwh_bits) << pagesize_bits) +
							(((y0 & sinfo.pagepixelwh_mask) >> cellwh) << info->pagewh_bits) +
							((x0 & sinfo.pagepixelwh_mask) >> cellwh)) << (info->patterndatasize_bits+1);
			//XXX: Optimize this function
			Vdp2PatternAddr(info);


			//test if the char address is a blank tile
			//XXX: make this better
			if (trn_code && memIsZeroTest((u32*) (Vdp2Ram + info->charaddr), block_size)) {
				continue;
			}
			SGX_SetZOffset((info->priority << 4) + info->prioffs);

			//Calculate Alpha
			u32 alpha = GetAlpha(info, 0, 0);
#if 0
			//XXX: Additive blending not working?
			if (alpha & 0x100) {
				GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
			} else {
				switch ((Vdp2Regs->CCCTL ) & 0x3) {
					case 0: GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR); break;	//TOP
					case 1: GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR); break;	//ADD
					case 2: GX_SetBlendMode(GX_BM_BLEND, GX_BL_DSTALPHA, GX_BL_INVDSTALPHA, GX_LO_CLEAR); break;	//BOTTOM
				}
			}
#endif
			konst.a = alpha;
			GX_SetTevKColor(GX_KCOLOR0, konst);

			u32 tlut_pos = ((info->coloroffset + info->paladdr) >> 4) & 0x7F;
			//GX_InitTexObjData(&tobj_ci, Vdp2Ram + info->charaddr);
			//GX_InitTexObjTlut(&tobj_ci, TLUT_INDX(trn_code, tlut_pos));
			//GX_LoadTexObj(&tobj_ci, GX_TEXMAP0);
			SGX_SetVdp2Texture(Vdp2Ram + info->charaddr, TLUT_INDX(trn_code, tlut_pos));
			u32 flip = ((info->flipfunction << 8) | (info->flipfunction >> 1)) & 0x0101;
			//XXX: Dont use color
			GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
				GX_Position2s16(i, j);
				GX_Color1u32(info->alpha);
				GX_TexCoord1u16(0x0000 ^ flip);
				GX_Position2s16(i + inc_xy, j);
				GX_Color1u32(info->alpha);
				GX_TexCoord1u16(0x0100 ^ flip);
				GX_Position2s16(i + inc_xy, j + inc_xy);
				GX_Color1u32(info->alpha);
				GX_TexCoord1u16(0x0101 ^ flip);
				GX_Position2s16(i, j + inc_xy);
				GX_Color1u32(info->alpha);
				GX_TexCoord1u16(0x0001 ^ flip);
			GX_End();
		}
	}

	SGX_SpriteConverterSet(1, SPRITE_4BPP, 0);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_FALSE, 1, 1);
}

//////////////////////////////////////////////////////////////////////////////


//HALF-DONE
int Rbg0CheckRam(void)
{
	//both banks are divided
	if (((Vdp2Regs->RAMCTL >> 8) & 3) == 3) {
		//ignore delta kax if the coefficient table
		//bank is unspecified
		return !(Vdp2Regs->RAMCTL & ((Vdp2Regs->RAMCTL & 0xAA) >> 1));
	}

   return 0;
}

//HALF-DONE
static void FASTCALL Vdp2DrawRotationFP(vdp2draw_struct *info, vdp2rotationparameterfp_struct *parameter)
{
   int i, j;
   u32 x, y;
   screeninfo_struct sinfo;
   vdp2rotationparameterfp_struct *p = &parameter[info->rotatenum];
   clipping_struct clip[2];
   u32 linewnd0addr, linewnd1addr;

   clip[0].xstart = clip[0].ystart = clip[0].xend = clip[0].yend = 0;
   clip[1].xstart = clip[1].ystart = clip[1].xend = clip[1].yend = 0;
   ReadWindowData(info->wctl, clip);
   linewnd0addr = linewnd1addr = 0;
   ReadLineWindowData(&info->islinewindow, info->wctl, &linewnd0addr, &linewnd1addr);

   Vdp2ReadRotationTableFP(info->rotatenum, p);

	if (!p->coefenab) {
		fixed32 xmul, ymul, C, F;

		// Rotate Bg Regardless of rotation
		GenerateRotatedVarFP(p, &xmul, &ymul, &C, &F);

		// Do simple rotation
		CalculateRotationValuesFP(p);

		SetupScreenVars(info, &sinfo, info->PlaneAddr);

		for (j = 0; j < vdp2height; j++) {
			info->LoadLineParams(info, j);
			ReadLineWindowClip(info->islinewindow, clip, &linewnd0addr, &linewnd1addr);

			for (i = 0; i < rbg0width; i++) {
				u32 color, dot;

				if (!TestBothWindow(info->wctl, clip, i, j))
					continue;

				x = GenerateRotatedXPosFP(p, i, xmul, ymul, C) & sinfo.xmask;
				y = GenerateRotatedYPosFP(p, i, xmul, ymul, F) & sinfo.ymask;

				// Convert coordinates into graphics
				if (!info->isbitmap) {
					// Tile
					Vdp2MapCalcXY(info, &x, &y, &sinfo);
				}

				// Fetch pixel
				if (Vdp2FetchPixel(info, x, y, &color, &dot)) {
					//XXX: PUTPIXEL DIEPENDING ON INTERLACED.
					//TitanPutPixel(info->priority, i, j, info->PostPixelFetchCalc(info, COL2WII_32(GetAlpha(info, color, dot), color)), info->linescreen);
				}
			}
			xmul += p->deltaXst;
			ymul += p->deltaYst;
		}
	}
   else
   {
      fixed32 xmul, ymul, C, F;
      u32 coefx, coefy;
      u32 rcoefx, rcoefy;
      u32 lineAddr, lineColor, lineInc;
      u16 lineColorAddr;

      fixed32 xmul2, ymul2, C2, F2;
      u32 coefx2, coefy2;
      u32 rcoefx2, rcoefy2;
      screeninfo_struct sinfo2;
      vdp2rotationparameterfp_struct *p2 = NULL;

      clipping_struct rpwindow[2];
      int userpwindow = 0;
      int isrplinewindow = 0;
      u32 rplinewnd0addr, rplinewnd1addr;

      if ((Vdp2Regs->RPMD & 3) == 2)
         p2 = &parameter[1 - info->rotatenum];
      else if ((Vdp2Regs->RPMD & 3) == 3)
      {
         ReadWindowData(Vdp2Regs->WCTLD, rpwindow);
         rplinewnd0addr = rplinewnd1addr = 0;
         ReadLineWindowData(&isrplinewindow, Vdp2Regs->WCTLD, &rplinewnd0addr, &rplinewnd1addr);
         userpwindow = 1;
         p2 = &parameter[1 - info->rotatenum];
      }

      GenerateRotatedVarFP(p, &xmul, &ymul, &C, &F);

      // Rotation using Coefficient Tables(now this stuff just gets wacky. It
      // has to be done in software, no exceptions)
      CalculateRotationValuesFP(p);

      SetupScreenVars(info, &sinfo, p->PlaneAddr);
      coefx = coefy = 0;
      rcoefx = rcoefy = 0;

      if (p2 != NULL)
      {
         Vdp2ReadRotationTableFP(1 - info->rotatenum, p2);
         GenerateRotatedVarFP(p2, &xmul2, &ymul2, &C2, &F2);
         CalculateRotationValuesFP(p2);
         SetupScreenVars(info, &sinfo2, p2->PlaneAddr);
         coefx2 = coefy2 = 0;
         rcoefx2 = rcoefy2 = 0;
      }

      if (Rbg0CheckRam())//sonic r / all star baseball 97
      {
         if (p->coefenab && p->coefmode == 0)
         {
            p->deltaKAx = 0;
         }

         if (p2 && p2->coefenab && p2->coefmode == 0)
         {
            p2->deltaKAx = 0;
         }
      }

      if (info->linescreen)
      {
         if ((info->rotatenum == 0) && (Vdp2Regs->KTCTL & 0x10))
            info->linescreen = 2;
         else if (Vdp2Regs->KTCTL & 0x1000)
            info->linescreen = 3;
         if (Vdp2Regs->VRSIZE & 0x8000)
            lineAddr = (Vdp2Regs->LCTA.all & 0x7FFFF) << 1;
         else
            lineAddr = (Vdp2Regs->LCTA.all & 0x3FFFF) << 1;

         lineInc = Vdp2Regs->LCTA.part.U & 0x8000 ? 2 : 0;
      }

      for (j = 0; j < rbg0height; j++)
      {
         if (p->deltaKAx == 0)
         {
            Vdp2ReadCoefficientFP(p,
                                  p->coeftbladdr +
                                  (coefy + touint(rcoefy)) *
                                  p->coefdatasize);
         }
         if ((p2 != NULL) && p2->coefenab && (p2->deltaKAx == 0))
         {
            Vdp2ReadCoefficientFP(p2,
                                  p2->coeftbladdr +
                                  (coefy2 + touint(rcoefy2)) *
                                  p2->coefdatasize);
         }

         if (info->linescreen > 1)
         {
            lineColorAddr = (T1ReadWord(Vdp2Ram, lineAddr) & 0x780) | p->linescreen;
            lineColor = Vdp2ColorRamGetColorSoft(lineColorAddr);
            lineAddr += lineInc;
            //TitanPutLineHLine(info->linescreen, j, COL2WII_32(0x3F, lineColor));
         }

         info->LoadLineParams(info, j);
         ReadLineWindowClip(info->islinewindow, clip, &linewnd0addr, &linewnd1addr);

         if (userpwindow)
            ReadLineWindowClip(isrplinewindow, rpwindow, &rplinewnd0addr, &rplinewnd1addr);

         for (i = 0; i < rbg0width; i++)
         {
            u32 color, dot;

            if (p->deltaKAx != 0)
            {
               Vdp2ReadCoefficientFP(p,
                                     p->coeftbladdr +
                                     (coefy + coefx + toint(rcoefx + rcoefy)) *
                                     p->coefdatasize);
               coefx += toint(p->deltaKAx);
               rcoefx += decipart(p->deltaKAx);
            }
            if ((p2 != NULL) && p2->coefenab && (p2->deltaKAx != 0))
            {
               Vdp2ReadCoefficientFP(p2,
                                     p2->coeftbladdr +
                                     (coefy2 + coefx2 + toint(rcoefx2 + rcoefy2)) *
                                     p2->coefdatasize);
               coefx2 += toint(p2->deltaKAx);
               rcoefx2 += decipart(p2->deltaKAx);
            }

            if (!TestBothWindow(info->wctl, clip, i, j))
               continue;

            if (((! userpwindow) && p->msb) || (userpwindow && (! TestBothWindow(Vdp2Regs->WCTLD, rpwindow, i, j))))
            {
			   if ((p2 == NULL) || (p2->coefenab && p2->msb)) continue;

               x = GenerateRotatedXPosFP(p2, i, xmul2, ymul2, C2);
               y = GenerateRotatedYPosFP(p2, i, xmul2, ymul2, F2);

               switch(p2->screenover) {
                  case 0:
                     x &= sinfo2.xmask;
                     y &= sinfo2.ymask;
                     break;
                  case 1:
                     VDP2LOG("Screen-over mode 1 not implemented");
                     x &= sinfo2.xmask;
                     y &= sinfo2.ymask;
                     break;
                  case 2:
                     if ((x > sinfo2.xmask) || (y > sinfo2.ymask)) continue;
                     break;
                  case 3:
                     if ((x > 512) || (y > 512)) continue;
               }

               // Convert coordinates into graphics
               if (!info->isbitmap)
               {
                  // Tile
                  Vdp2MapCalcXY(info, &x, &y, &sinfo2);
               }
            }
            else if (p->msb) continue;
            else
            {
               x = GenerateRotatedXPosFP(p, i, xmul, ymul, C);
               y = GenerateRotatedYPosFP(p, i, xmul, ymul, F);

               switch(p->screenover) {
                  case 0:
                     x &= sinfo.xmask;
                     y &= sinfo.ymask;
                     break;
                  case 1:
                     VDP2LOG("Screen-over mode 1 not implemented");
                     x &= sinfo.xmask;
                     y &= sinfo.ymask;
                     break;
                  case 2:
                     if ((x > sinfo.xmask) || (y > sinfo.ymask)) continue;
                     break;
                  case 3:
                     if ((x > 512) || (y > 512)) continue;
               }

               // Convert coordinates into graphics
               if (!info->isbitmap)
               {
                  // Tile
                  Vdp2MapCalcXY(info, &x, &y, &sinfo);
               }
            }

            // Fetch pixel
            if (!Vdp2FetchPixel(info, x, y, &color, &dot))
            {
               continue;
            }
			//XXX: PUTPIXEL DIEPENDING ON INTERLACED.
            //TitanPutPixel(info->priority, i, j, info->PostPixelFetchCalc(info, COL2WII_32(GetAlpha(info, color, dot), color)), info->linescreen);
         }
         xmul += p->deltaXst;
         ymul += p->deltaYst;
         coefx = 0;
         rcoefx = 0;
         coefy += toint(p->deltaKAst);
         rcoefy += decipart(p->deltaKAst);

         if (p2 != NULL)
         {
            xmul2 += p2->deltaXst;
            ymul2 += p2->deltaYst;
            if (p2->coefenab)
            {
               coefx2 = 0;
               rcoefx2 = 0;
               coefy2 += toint(p2->deltaKAst);
               rcoefy2 += decipart(p2->deltaKAst);
            }
         }
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

//XXX: this is bad
static void gfx_drawBackScreen(void)
{
	//Show Backscreen.
	if (Vdp2Regs->TVMD & 0x100) {
		u32 scrAddr;
		u8 ofs_reg = gfx_GetColorOffset(0x20);	//sprite mask

		if (Vdp2Regs->VRSIZE & 0x8000)
			scrAddr = (((Vdp2Regs->BKTAU & 0x7) << 16) | Vdp2Regs->BKTAL) << 1;
		else
			scrAddr = (((Vdp2Regs->BKTAU & 0x3) << 16) | Vdp2Regs->BKTAL) << 1;

		u32 dot = T1ReadWord(Vdp2Ram, scrAddr);

		//SET UP GX TEV STAGES for textures
		GX_SetNumTevStages(1);
		GX_SetNumTexGens(0);
		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_KONST, GX_CC_ZERO, GX_CC_ZERO, ofs_reg);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

		GXColor konst = {(dot & 0x1F) << 3, (dot & 0x3E0) >> 2, (dot & 0x7C00) >> 7, 0};
		GX_SetTevKColor(GX_KCOLOR0, konst);
		GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
		GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);

		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
			GX_Position2s16(0, 0);
			GX_Color1u32(0x000000FF);
			GX_TexCoord2f32(0.0, 0.0);
			GX_Position2s16(disp.w, 0);
			GX_Color1u32(0x000000FF);
			GX_TexCoord2f32(1.0, 0.0);
			GX_Position2s16(disp.w, disp.h << disp.scale_y);
			GX_Color1u32(0x000000FF);
			GX_TexCoord2f32(1.0, 1.0);
			GX_Position2s16(0, disp.h << disp.scale_y);
			GX_Color1u32(0x000000FF);
			GX_TexCoord2f32(0.0, 1.0);
		GX_End();
	}

#if 0
   // Only draw black if TVMD's DISP and BDCLMD bits are cleared
   if ((Vdp2Regs->TVMD & 0x8000) == 0 && (Vdp2Regs->TVMD & 0x100) == 0)
   {
      // Draw Black
      for (j = 0; j < vdp2height; j++);
         //TitanPutBackHLine(j, COL2WII_32(0x3F, 0));
   }
   else
   {
      // Draw Back Screen
      u32 scrAddr;
      u16 dot;
	  vdp2draw_struct info = { 0 };

      ReadVdp2ColorOffset(Vdp2Regs, &info, (1 << 5), 0);

      if (Vdp2Regs->VRSIZE & 0x8000)
         scrAddr = (((Vdp2Regs->BKTAU & 0x7) << 16) | Vdp2Regs->BKTAL) << 1;
      else
         scrAddr = (((Vdp2Regs->BKTAU & 0x3) << 16) | Vdp2Regs->BKTAL) << 1;

      if (Vdp2Regs->BKTAU & 0x8000)
      {
         // Per Line
         for (i = 0; i < vdp2height; i++)
         {
            dot = T1ReadWord(Vdp2Ram, scrAddr);
            scrAddr += 2;

            //TitanPutBackHLine(i, info.PostPixelFetchCalc(&info, COL2WII_16(0x3f, dot)));
         }
      }
      else
      {
         // Single Color
         dot = T1ReadWord(Vdp2Ram, scrAddr);

         for (j = 0; j < vdp2height; j++);
            //TitanPutBackHLine(j, info.PostPixelFetchCalc(&info, COL2WII_16(0x3f, dot)));
	  }
   }
#endif
}



//////////////////////////////////////////////////////////////////////////////


//HALF-DONE
static void Vdp2DrawLineScreen(void)
{
   u32 scrAddr;
   u16 color;
   u32 dot;
   int i;

   /* no need to go further if no screen is using the line screen */
   if (Vdp2Regs->LNCLEN == 0)
      return;

   scrAddr = (Vdp2Regs->LCTA.all & (0x3FFFF | ((Vdp2Regs->VRSIZE & 0x8000) << 3))) << 1;

	u32 alpha = (Vdp2Regs->CCRLB & 0x1f) << 1;

   if (Vdp2Regs->LCTA.part.U & 0x8000)
   {
      /* per line */
      for (i = 0; i < vdp2height; i++)
      {
         color = T1ReadWord(Vdp2Ram, scrAddr) & 0x7FF;
         dot = Vdp2ColorRamGetColorSoft(color);
         scrAddr += 2;

         //TitanPutLineHLine(1, i, COL2WII_32(alpha, dot));
      }
   }
   else
   {
      /* single color, implemented but not tested... */
      color = T1ReadWord(Vdp2Ram, scrAddr) & 0x7FF;
      dot = Vdp2ColorRamGetColorSoft(color);
      for (i = 0; i < vdp2height; i++);
         //TitanPutLineHLine(1, i, COL2WII_32(alpha, dot));
   }
}

//////////////////////////////////////////////////////////////////////////////

//DONE
static void LoadLineParamsNBG0(vdp2draw_struct * info, u32 line)
{
   	if (line < 270) {
		info->specialprimode = vdp2_lines[line].SFPRMD & 0x3;
		info->enable = (vdp2_lines[line].BGON & 0x21) != 0;	//nbg0 or rbg1
	}
   //GeneratePlaneAddrTable(info, sinfo->planetbl, info->PlaneAddr);//sonic 2, 2 player mode
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static void Vdp2DrawNBG0(void)
{
	vdp2draw_struct info = {0};
	vdp2rotationparameterfp_struct parameter[2];
	//NOT YET IMPLEMENTED
	//info.titan_which_layer = TITAN_NBG0;
	//info.titan_shadow_enabled = (regs->SDCTL >> 0) & 1;

	parameter[0].PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2ParameterAPlaneAddr;
	parameter[1].PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2ParameterBPlaneAddr;

	if (Vdp2Regs->BGON & 0x20) {		// RBG1 mode
		//XXX: not implemented
		return;
		info.enable = Vdp2Regs->BGON & 0x20;

		// Read in Parameter B
		Vdp2ReadRotationTableFP(1, &parameter[1]);

		if((info.isbitmap = Vdp2Regs->CHCTLA & 0x2) != 0) {
			// Bitmap Mode
			ReadBitmapSize(&info, Vdp2Regs->CHCTLA >> 2, 0x3);

			info.charaddr = (Vdp2Regs->MPOFR & 0x70) * 0x2000;
			info.paladdr = (Vdp2Regs->BMPNA & 0x7) << 8;
			info.flipfunction = 0;
			info.specialfunction = 0;
			info.specialcolorfunction = (Vdp2Regs->BMPNA & 0x10) >> 4;
		}
		else {
			// Tile Mode
			info.mapwh = 4;
			ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 12);
			ReadPatternData(&info, Vdp2Regs->PNCN0, Vdp2Regs->CHCTLA & 0x1);
		}

		info.rotatenum = 1;
		info.rotatemode = 0;
		info.PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2ParameterBPlaneAddr;
	}
	else if (Vdp2Regs->BGON & 0x1) {	// NBG0 mode
		info.enable = Vdp2Regs->BGON & 0x1;
		info.x = Vdp2Regs->SCXIN0 & 0x7FF;
		info.y = Vdp2Regs->SCYIN0 & 0x7FF;

		if ((info.isbitmap = Vdp2Regs->CHCTLA & 0x2) != 0) {
			// Bitmap Mode
			ReadBitmapSize(&info, Vdp2Regs->CHCTLA >> 2, 0x3);

			info.charaddr = (Vdp2Regs->MPOFN & 0x7) * 0x20000;
			info.paladdr = (Vdp2Regs->BMPNA & 0x7) << 8;
			info.flipfunction = 0;
			info.specialfunction = 0;
			info.specialcolorfunction = (Vdp2Regs->BMPNA & 0x10) >> 4;
		}
		else {
			// Tile Mode
			info.mapwh = 2;

			ReadPlaneSize(&info, Vdp2Regs->PLSZ);
			ReadPatternData(&info, Vdp2Regs->PNCN0, Vdp2Regs->CHCTLA & 0x1);
		}

		info.coordincx = (Vdp2Regs->ZMXN0.all & 0x7FF00) / (float) 65536;
		info.coordincy = (Vdp2Regs->ZMYN0.all & 0x7FF00) / (float) 65536;
		info.PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2NBG0PlaneAddr;
	}

	info.transparencyenable = !(Vdp2Regs->BGON & 0x100);
	info.specialprimode = Vdp2Regs->SFPRMD & 0x3;
	info.colornumber = (Vdp2Regs->CHCTLA & 0x70) >> 4;

	//Check color calculation
	u8 alpha = ~((((Vdp2Regs->CCRNA & 0x1F) + 1) << 3) - 1);
	if (Vdp2Regs->CCCTL & 0x1) {
		info.alpha = alpha;
		//XXX: Make this correct
		if (Vdp2Regs->CCCTL & 0x200) {
			info.alpha = ~alpha;
		}
		//XXX: chagnes the dst alpha value.
		//if (Vdp2Regs->CCCTL & 0x100) {
		//	info.alpha = ~alpha;
		//}	info.pri = PRI_NBG3((Vdp2Regs->PRINB >> 8) & 0x7);

	} else {
		info.alpha = 0xFF;
	}

	info.specialcolormode = Vdp2Regs->SFCCMD & 0x3;
	info.specialcode = (Vdp2Regs->SFCODE >> (Vdp2Regs->SFSEL & 0x1 << 3)) & 0xFF;
	info.linescreen = (Vdp2Regs->LNCLEN & 0x1);
	info.coloroffset = (Vdp2Regs->CRAOFA & 0x7) << 8;
	ReadVdp2ColorOffset(Vdp2Regs, &info, 0x1, 0x1);
	info.priority = Vdp2Regs->PRINA & 0x7;
	info.prioffs = 10;

	ReadMosaicData(&info, 0x1);
	ReadLineScrollData(&info, Vdp2Regs->SCRCTL & 0xFF, Vdp2Regs->LSTA0.all);
	info.isverticalscroll = (Vdp2Regs->SCRCTL & 1);
	if (info.isverticalscroll) {
		info.verticalscrolltbl = (Vdp2Regs->VCSTA.all & 0x7FFFE) << 1;
		info.verticalscrollinc = 4 << ((Vdp2Regs->SCRCTL >> 8) & 1);
	}
	info.wctl = Vdp2Regs->WCTLA;
	info.LoadLineParams = (void (*)(void *, u32)) LoadLineParamsNBG0;

	if (info.enable == 1) {
		//XXX: NBG0 draw... not implemented
		if (info.isbitmap) {
			gfx_DrawBitmap(&info);
		} else {
			gfx_DrawScroll(&info);
		}
	}
	else {
		// RBG1 draw
		//Vdp2DrawRotationFP(&info, parameter);
	}
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static void LoadLineParamsNBG1(vdp2draw_struct * info, u32 line)
{
	if (line < 270) {
		info->specialprimode = (vdp2_lines[line].SFPRMD >> 2) & 0x3;
		info->enable = vdp2_lines[line].BGON & 0x2;	//f1 challenge map when zoomed out
	}
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static void Vdp2DrawNBG1(void)
{
	vdp2draw_struct info = {0};

	info.enable = Vdp2Regs->BGON & 0x2;
	info.transparencyenable = !(Vdp2Regs->BGON & 0x200);
	info.specialprimode = (Vdp2Regs->SFPRMD >> 2) & 0x3;

	info.colornumber = (Vdp2Regs->CHCTLA & 0x3000) >> 12;

	if((info.isbitmap = Vdp2Regs->CHCTLA & 0x200) != 0) {
		ReadBitmapSize(&info, Vdp2Regs->CHCTLA >> 10, 0x3);

		info.x = Vdp2Regs->SCXIN1 & 0x7FF;
		info.y = Vdp2Regs->SCYIN1 & 0x7FF;

		info.charaddr = ((Vdp2Regs->MPOFN & 0x70) >> 4) * 0x20000;
		info.paladdr = Vdp2Regs->BMPNA & 0x700;
		info.flipfunction = 0;
		info.specialfunction = 0;
		info.specialcolorfunction = (Vdp2Regs->BMPNA & 0x1000) >> 12;
	}
	else {
		info.mapwh = 2;

		ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 2);

		info.x = Vdp2Regs->SCXIN1 & 0x7FF;
		info.y = Vdp2Regs->SCYIN1 & 0x7FF;

		ReadPatternData(&info, Vdp2Regs->PNCN1, Vdp2Regs->CHCTLA & 0x100);
	}

	//Check color calculation
	u8 alpha = ~(((((Vdp2Regs->CCRNA & 0x1F00) >> 8) + 1) << 3) - 1);
	if (Vdp2Regs->CCCTL & 0x2) {
		info.alpha = alpha;
		//XXX: Make this correct
		if (Vdp2Regs->CCCTL & 0x200) {
			info.alpha = ~alpha;
		}
		//XXX: chagnes the dst alpha value.
		//if (Vdp2Regs->CCCTL & 0x100) {
		//	info.alpha = ~alpha;
		//}
	} else {
		info.alpha = 0xFF;
	}

	info.specialcolormode = (Vdp2Regs->SFCCMD >> 2) & 0x3;
	if (Vdp2Regs->SFSEL & 0x2)
		info.specialcode = Vdp2Regs->SFCODE >> 8;
	else
		info.specialcode = Vdp2Regs->SFCODE & 0xFF;
	info.linescreen = 0;
	if (Vdp2Regs->LNCLEN & 0x2)
		info.linescreen = 1;

	info.coloroffset = (Vdp2Regs->CRAOFA & 0x70) << 4;
	ReadVdp2ColorOffset(Vdp2Regs, &info, 0x2, 0x2);
	info.coordincx = (Vdp2Regs->ZMXN1.all & 0x7FF00) / (float) 65536;
	info.coordincy = (Vdp2Regs->ZMYN1.all & 0x7FF00) / (float) 65536;

	info.priority = (Vdp2Regs->PRINA >> 8) & 0x7;
	info.prioffs = 8;

	info.PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2NBG1PlaneAddr;


	ReadMosaicData(&info, 0x2);
	ReadLineScrollData(&info, Vdp2Regs->SCRCTL >> 8, Vdp2Regs->LSTA1.all);
	if (Vdp2Regs->SCRCTL & 0x100)
	{
		info.isverticalscroll = 1;
		if (Vdp2Regs->SCRCTL & 0x1)
		{
			info.verticalscrolltbl = 4 + ((Vdp2Regs->VCSTA.all & 0x7FFFE) << 1);
			info.verticalscrollinc = 8;
		}
		else
		{
			info.verticalscrolltbl = (Vdp2Regs->VCSTA.all & 0x7FFFE) << 1;
			info.verticalscrollinc = 4;
		}
	}
	else
		info.isverticalscroll = 0;
	info.wctl = Vdp2Regs->WCTLA >> 8;

	info.LoadLineParams = (void (*)(void *, u32)) LoadLineParamsNBG1;

	//XXX: Not implemented yet
	gfx_DrawScroll(&info);
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static void LoadLineParamsNBG2(vdp2draw_struct * info, u32 line)
{
	if (line < 270) {
		info->specialprimode = (vdp2_lines[line].SFPRMD >> 4) & 0x3;
		info->enable = vdp2_lines[line].BGON & 0x4;
	}
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static void Vdp2DrawNBG2(void)
{
	vdp2draw_struct info;

	info.enable = Vdp2Regs->BGON & 0x4;
	info.transparencyenable = !(Vdp2Regs->BGON & 0x400);
	info.specialprimode = (Vdp2Regs->SFPRMD >> 4) & 0x3;

	info.colornumber = (Vdp2Regs->CHCTLB & 0x2) >> 1;
	info.mapwh = 2;

	ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 4);
	info.x = Vdp2Regs->SCXN2 & 0x7FF;
	info.y = Vdp2Regs->SCYN2 & 0x7FF;
	ReadPatternData(&info, Vdp2Regs->PNCN2, Vdp2Regs->CHCTLB & 0x1);

	//Check color calculation
	u8 alpha = ~(((Vdp2Regs->CCRNB + 1) << 3) - 1);
	if (Vdp2Regs->CCCTL & 0x4) {
		info.alpha = alpha;
		//XXX: Make this correct
		if (Vdp2Regs->CCCTL & 0x200) {
			info.alpha = ~alpha;
		}
		//XXX: chagnes the dst alpha value.
		//if (Vdp2Regs->CCCTL & 0x100) {
		//	info.alpha = ~alpha;
		//}
	} else {
		info.alpha = 0xFF;
	}

	info.specialcolormode = (Vdp2Regs->SFCCMD >> 4) & 0x3;
	if (Vdp2Regs->SFSEL & 0x4)
		info.specialcode = Vdp2Regs->SFCODE >> 8;
	else
		info.specialcode = Vdp2Regs->SFCODE & 0xFF;
	info.linescreen = 0;
	if (Vdp2Regs->LNCLEN & 0x4)
		info.linescreen = 1;

	info.coloroffset = Vdp2Regs->CRAOFA & 0x700;
	ReadVdp2ColorOffset(Vdp2Regs, &info, 0x4, 0x4);
	info.coordincx = info.coordincy = 1;

	info.priority = Vdp2Regs->PRINB & 0x7;
	info.prioffs = 6;
	info.PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2NBG2PlaneAddr;

	//screen_enable |= 0x4;

	ReadMosaicData(&info, 0x4);
	info.islinescroll = 0;
	info.isverticalscroll = 0;
	info.wctl = Vdp2Regs->WCTLB;
	info.isbitmap = 0;

	info.LoadLineParams = (void (*)(void *, u32)) LoadLineParamsNBG2;

	gfx_DrawScroll(&info);
	/*
	Vdp2DrawScrollSimple(&info, bg_tex[2], bg_alpha_tex[2], win_tex);
	//XXX: This must be stored to make the rendering more accurate
	u32 tex_height = disp.h / info.mosaicymask;
	u32 tex_width = disp.w / info.mosaicxmask;
	//XXX: remember that this is allways GX_TF_CI14 but for other types it can be other types.
	DCStoreRange(bg_tex[2], sizeof(u16) * tex_width * tex_height);
	GX_InitTexObjCI(&tex_obj_bg[2], bg_tex[2], tex_width, tex_height, GX_TF_CI14, GX_CLAMP, GX_CLAMP, GX_FALSE, 0x0);
	GX_InitTexObjLOD(&tex_obj_bg[2], GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	*/
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static void LoadLineParamsNBG3(vdp2draw_struct * info, u32 line)
{
   	if (line < 270) {
		info->specialprimode = (vdp2_lines[line].SFPRMD >> 6) & 0x3;
		info->enable = vdp2_lines[line].BGON & 0x8;
	}
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static void Vdp2DrawNBG3(void)
{
	vdp2draw_struct info = {0};

	if (bad_cycle_setting_nbg3) {
		return;
	}
	info.enable = Vdp2Regs->BGON & 0x8;
	info.transparencyenable = !(Vdp2Regs->BGON & 0x800);
	info.specialprimode = (Vdp2Regs->SFPRMD >> 6) & 0x3;

	info.colornumber = (Vdp2Regs->CHCTLB & 0x20) >> 5;
	info.mapwh = 2;

	ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 6);
	info.x = Vdp2Regs->SCXN3 & 0x7FF;
	info.y = Vdp2Regs->SCYN3 & 0x7FF;
	ReadPatternData(&info, Vdp2Regs->PNCN3, Vdp2Regs->CHCTLB & 0x10);

	//XXX: Make this correct
	//Check color calculation
	u8 alpha = ~(((((Vdp2Regs->CCRNB & 0x1F00) >> 8) + 1) << 3) - 1);
	if (Vdp2Regs->CCCTL & 0x8) {
		info.alpha = alpha;
		//XXX: Make this correct
		if (Vdp2Regs->CCCTL & 0x200) {
			info.alpha = ~alpha;
		}
		//XXX: chagnes the dst alpha value.
		//if (Vdp2Regs->CCCTL & 0x100) {
		//	info.alpha = ~alpha;
		//}
	} else {
		info.alpha = 0xFF;
	}

	info.specialcolormode = (Vdp2Regs->SFCCMD >> 6) & 0x3;
	if (Vdp2Regs->SFSEL & 0x8)
		info.specialcode = Vdp2Regs->SFCODE >> 8;
	else
		info.specialcode = Vdp2Regs->SFCODE & 0xFF;
	info.linescreen = 0;
	if (Vdp2Regs->LNCLEN & 0x8)
		info.linescreen = 1;

	info.coloroffset = (Vdp2Regs->CRAOFA & 0x7000) >> 4;
	ReadVdp2ColorOffset(Vdp2Regs, &info, 0x8, 0x8);
	info.coordincx = info.coordincy = 1;

	info.priority = (Vdp2Regs->PRINB >> 8) & 0x7;
	info.prioffs = 4;
	info.PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2NBG3PlaneAddr;

	//screen_enable |= 0x4;

	ReadMosaicData(&info, 0x8);
	info.islinescroll = 0;
	info.isverticalscroll = 0;
	info.wctl = Vdp2Regs->WCTLB >> 8;
	info.isbitmap = 0;

	info.LoadLineParams = (void (*)(void *, u32)) LoadLineParamsNBG3;
		gfx_DrawScroll(&info);
	/*
	Vdp2DrawScrollSimple(&info, bg_tex[3], bg_alpha_tex[2], win_tex);
	//XXX: This must be stored to make the rendering more accurate
	u32 tex_height = disp.h / info.mosaicymask;
	u32 tex_width = disp.w / info.mosaicxmask;
	//XXX: remember that this is allways GX_TF_CI14 but for other types it can be other types.
	DCStoreRange(bg_tex[3], sizeof(u16) * tex_width * tex_height);
	GX_InitTexObjCI(&tex_obj_bg[3], bg_tex[3], tex_width, tex_height, GX_TF_CI14, GX_CLAMP, GX_CLAMP, GX_FALSE, 0x0);
	GX_InitTexObjLOD(&tex_obj_bg[3], GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	*/
}

//////////////////////////////////////////////////////////////////////////////

//DONE
static void LoadLineParamsRBG0(vdp2draw_struct * info, u32 line)
{
	if (line < 270) {
		info->specialprimode = (vdp2_lines[line].SFPRMD >> 8) & 0x3;
		info->enable = vdp2_lines[line].BGON & 0x10;	//USED?
	}
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
static void Vdp2DrawRBG0(void)
{
   vdp2draw_struct info = {0};
   vdp2rotationparameterfp_struct parameter[2];

   //info.titan_which_layer = TITAN_RBG0;
   //info.titan_shadow_enabled = (regs->SDCTL >> 4) & 1;

   parameter[0].PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2ParameterAPlaneAddr;
   parameter[1].PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2ParameterBPlaneAddr;

   info.enable = Vdp2Regs->BGON & 0x10;
   info.priority = Vdp2Regs->PRIR & 0x7;
   info.prioffs = 12;

   if (!(info.enable & Vdp2External.disptoggle))
      return;
   info.transparencyenable = !(Vdp2Regs->BGON & 0x1000);
   info.specialprimode = (Vdp2Regs->SFPRMD >> 8) & 0x3;

   info.colornumber = (Vdp2Regs->CHCTLB & 0x7000) >> 12;

   // Figure out which Rotation Parameter we're using
   switch (Vdp2Regs->RPMD & 0x3)
   {
      case 0:
         // Parameter A
         info.rotatenum = 0;
         info.rotatemode = 0;
         info.PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2ParameterAPlaneAddr;
         break;
      case 1:
         // Parameter B
         info.rotatenum = 1;
         info.rotatemode = 0;
         info.PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2ParameterBPlaneAddr;
         break;
      case 2:
         // Parameter A+B switched via coefficients
      case 3:
         // Parameter A+B switched via rotation parameter window
      default:
         info.rotatenum = 0;
         info.rotatemode = 1 + (Vdp2Regs->RPMD & 0x1);
         info.PlaneAddr = (void FASTCALL (*)(void *, int))&Vdp2ParameterAPlaneAddr;
         break;
   }

   Vdp2ReadRotationTableFP(info.rotatenum, &parameter[info.rotatenum]);

   if((info.isbitmap = Vdp2Regs->CHCTLB & 0x200) != 0)
   {
      // Bitmap Mode
      ReadBitmapSize(&info, Vdp2Regs->CHCTLB >> 10, 0x1);

      if (info.rotatenum == 0)
         // Parameter A
         info.charaddr = (Vdp2Regs->MPOFR & 0x7) * 0x20000;
      else
         // Parameter B
         info.charaddr = (Vdp2Regs->MPOFR & 0x70) * 0x2000;

      info.paladdr = (Vdp2Regs->BMPNB & 0x7) << 8;
      info.flipfunction = 0;
      info.specialfunction = 0;
      info.specialcolorfunction = (Vdp2Regs->BMPNB & 0x10) >> 4;
   }
   else
   {
      // Tile Mode
      info.mapwh = 4;

      if (info.rotatenum == 0)
         // Parameter A
         ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 8);
      else
         // Parameter B
         ReadPlaneSize(&info, Vdp2Regs->PLSZ >> 12);

      ReadPatternData(&info, Vdp2Regs->PNCR, Vdp2Regs->CHCTLB & 0x100);
   }

   if (Vdp2Regs->CCCTL & 0x210)
      info.alpha = ((~Vdp2Regs->CCRR & 0x1F) << 1) + 1;
   else
      info.alpha = 0x3F;
   if ((Vdp2Regs->CCCTL & 0x210) == 0x210) info.alpha |= 0x80;
   else if ((Vdp2Regs->CCCTL & 0x110) == 0x110) info.alpha |= 0x80;
   info.specialcolormode = (Vdp2Regs->SFCCMD >> 8) & 0x3;
   if (Vdp2Regs->SFSEL & 0x10)
      info.specialcode = Vdp2Regs->SFCODE >> 8;
   else
      info.specialcode = Vdp2Regs->SFCODE & 0xFF;
   info.linescreen = 0;
   if (Vdp2Regs->LNCLEN & 0x10)
      info.linescreen = 1;

   info.coloroffset = (Vdp2Regs->CRAOFB & 0x7) << 8;

   ReadVdp2ColorOffset(Vdp2Regs, &info, 0x10, 0x10);
   info.coordincx = info.coordincy = 1;

   ReadMosaicData(&info, 0x10);
   info.islinescroll = 0;
   info.isverticalscroll = 0;
   info.wctl = Vdp2Regs->WCTLC;

   info.LoadLineParams = (void (*)(void *, int)) LoadLineParamsRBG0;

   Vdp2DrawRotationFP(&info, parameter);
}

//////////////////////////////////////////////////////////////////////////////

typedef void (*DrawScreenFunc)(void);

//XXX: only use fucntions that are implemented
const DrawScreenFunc draw_fun_array[5] = {
	Vdp2DrawNBG0,
	Vdp2DrawNBG1,
	Vdp2DrawNBG2,
	Vdp2DrawNBG3,
	NULL
};

//HALF-DONE
static void LoadLineParamsSprite(vdp2draw_struct * info, u32 line)
{

}

//////////////////////////////////////////////////////////////////////////////




int VIDSoftInit(void)
{
	guMtxIdentity(mat[GXMTX_IDENTITY]);
	guMtxIdentity(mat[GXMTX_VDP1]);
	guMtxIdentity(mat[GXMTX_VDP2_BG]);

	//GX_SetArray(GX_POSMTXARRAY, mat, sizeof(f32) * 24);
	//GX_LoadPosMtxIdx(0, GX_PNMTX0);
	GX_LoadPosMtxImm(mat[GXMTX_IDENTITY], GXMTX_IDENTITY);
	GX_SetCurrentMtx(GXMTX_IDENTITY);
	memset(tex_dirty, 0, 0x200 << 2);

	// //GX_InitTexObj(&tex_obj_bg[2], bg_tex[2], 1, 1, GX_TF_CI4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&tex_obj_bg[2], GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);

	GX_InitTexObj(&tex_obj_i4, wii_vram, 8, 8, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObj(&tex_obj_i8, wii_vram, 8, 8, GX_TF_I8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjCI(&tex_obj_ci4, wii_vram, 8, 8, GX_TF_CI4, GX_CLAMP, GX_CLAMP, GX_FALSE, 0);
	GX_InitTexObjCI(&tex_obj_ci8, wii_vram, 8, 8, GX_TF_CI8, GX_CLAMP, GX_CLAMP, GX_FALSE, 0);
	GX_InitTexObjLOD(&tex_obj_i4, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexObjLOD(&tex_obj_i8, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexObjLOD(&tex_obj_ci4, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexObjLOD(&tex_obj_ci8, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);

	//Main texture objects for all drawing
	GX_InitTexObj(&tobj_rgb, Vdp1Ram, 8, 8, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&tobj_rgb, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexObjCI(&tobj_ci, Vdp1Ram, 8, 8, GX_TF_CI4, GX_CLAMP, GX_CLAMP, GX_FALSE, 0);
	GX_InitTexObjLOD(&tobj_ci, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexObj(&tobj_bitmap, Vdp1Ram, 8, 8, GX_TF_CI4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&tobj_bitmap, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);

	GX_InitTexObj(&tex_obj_vdp1, display_fb, 8, 8, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&tex_obj_vdp1, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);

	if ((win_tex = (u8 *)memalign(32, 704 * 512 * sizeof(u16))) == NULL)
      return -1;


   vdp1backframebuffer = vdp1framebuffer[0];
   vdp1frontframebuffer = vdp1framebuffer[1];
   //rbg0width = vdp2width = 320;
   //vdp2height = 224;

	//initial display
	disp.x = 0;
	disp.y = 8;
	disp.w = 320;
	disp.h = 224;
	disp.scale_y = 1;
	disp.highres = 0;

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void VIDSoftDeInit(void)
{
	free(win_tex);
}

//////////////////////////////////////////////////////////////////////////////

//DONE
int VIDSoftVdp1Reset(void)
{
	Vdp1Regs->userclipX1 = Vdp1Regs->systemclipX1 = 0;
	Vdp1Regs->userclipY1 = Vdp1Regs->systemclipY1 = 0;
	Vdp1Regs->userclipX2 = Vdp1Regs->systemclipX2 = 512;
	Vdp1Regs->userclipY2 = Vdp1Regs->systemclipY2 = 256;
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

u8 priority_arr[8];
u8 colorcalc_arr[8];

void VIDSoftVdp1DrawStart(void)
{
	u32 bit0 = Vdp1Regs->TVMR & 1;
	u32 bit1 = (Vdp1Regs->TVMR >> 1) & 1;
	vdp1interlace = (Vdp1Regs->FBCR >> 3) & 1;
	// Rotation 8-bit, Unused, will make standard
	vdp1width = 512 << (bit1 & bit0);
	vdp1height = 512 >> (bit1 | (bit0 ^ 1));
	vdp1pixelsize = 2 - bit0;

	//XXX: Useless??
	VIDSoftVdp1EraseFrameBuffer();

	//GX_LoadPosMtxIdx(0, GX_PNMTX0);
	//Load vdp1 matrix... should we clear the values?
	GX_LoadPosMtxImm(mat[GXMTX_VDP1], GXMTX_VDP1);
	GX_SetCurrentMtx(GXMTX_VDP1);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 8, 1);

	//SET UP GX TEV STAGES...
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(1);
    //GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	//XXX: this is for paletted sprites, Konst is for transparency, gouraud is always active and half
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	SGX_BeginVdp1();
	//XXX: we can make this faster
	priority_arr[0] = (Vdp2Regs->PRISA & 0x7) << 4;
	priority_arr[1] = ((Vdp2Regs->PRISA >> 8) & 0x7) << 4;
	priority_arr[2] = (Vdp2Regs->PRISB & 0x7) << 4;
	priority_arr[3] = ((Vdp2Regs->PRISB >> 8) & 0x7) << 4;
	priority_arr[4] = (Vdp2Regs->PRISC & 0x7) << 4;
	priority_arr[5] = ((Vdp2Regs->PRISC >> 8) & 0x7) << 4;
	priority_arr[6] = (Vdp2Regs->PRISD & 0x7) << 4;
	priority_arr[7] = ((Vdp2Regs->PRISD >> 8) & 0x7) << 4;

	//XXX: this is wrong
	colorcalc_arr[0] = ((~Vdp2Regs->CCRSA & 0x1F) << 1) + 1;
	colorcalc_arr[1] = ((~Vdp2Regs->CCRSA >> 7) & 0x3E) + 1;
	colorcalc_arr[2] = ((~Vdp2Regs->CCRSB & 0x1F) << 1) + 1;
	colorcalc_arr[3] = ((~Vdp2Regs->CCRSB >> 7) & 0x3E) + 1;
	colorcalc_arr[4] = ((~Vdp2Regs->CCRSC & 0x1F) << 1) + 1;
	colorcalc_arr[5] = ((~Vdp2Regs->CCRSC >> 7) & 0x3E) + 1;
	colorcalc_arr[6] = ((~Vdp2Regs->CCRSD & 0x1F) << 1) + 1;
	colorcalc_arr[7] = ((~Vdp2Regs->CCRSD >> 7) & 0x3E) + 1;

}

//////////////////////////////////////////////////////////////////////////////

void VIDSoftVdp1DrawEnd(void)
{
}

//////////////////////////////////////////////////////////////////////////////

vdp1cmd_struct cmd;


static void getGouraudColors(u32 *c)
{
	u32 gouraudTableAddress = (((u32) cmd.CMDGRDA) << 3);
	u16 c0 = T1ReadWord(Vdp1Ram, gouraudTableAddress    );
	u16 c1 = T1ReadWord(Vdp1Ram, gouraudTableAddress + 2);
	u16 c2 = T1ReadWord(Vdp1Ram, gouraudTableAddress + 4);
	u16 c3 = T1ReadWord(Vdp1Ram, gouraudTableAddress + 6);

	c[0] = COL2WII_16(0, c0);
	c[1] = COL2WII_16(0, c1);
	c[2] = COL2WII_16(0, c2);
	c[3] = COL2WII_16(0, c3);
}


#define THRESHOLD	(1e-8)

static void vid_ExtendPolygon(s16 ax, s16 ay, s16 *bx, s16 *by)
{
	f32 dx = (f32) (*bx - ax);
	f32 dy = (f32) (*by - ay);
	f32 len = fabsf(sqrtf(dx*dx + dy*dy));
	*bx += dx >= (THRESHOLD	* len);
	*by += dy >= (THRESHOLD	* len);
}



void VidSoftTexConvert(u32 ram_addr)
{
	//Address to valid vdp1 RAM range
	//XXX: This function is too much for now, but maybe its useful for other things
	Vdp1ReadCommand(&cmd, ram_addr);
	u32 char_addr = (((u32) cmd.CMDSRCA) << 3) & 0x7FFE0;
	//u32 color_lut = (cmd.CMDCOLR << 3) & 0x7FFE0;
	u32 spr_w = ((cmd.CMDSIZE & 0x3F00) >> 5);
	//XXX: Is this right?
	u32 spr_h = (cmd.CMDSIZE & 0xFF);// + (cmd.CMDSIZE & 0x7 ? 0x8 : 0x0);
	//u32 is_solid = cmd.CMDPMOD & 0x40;

	u32 bit_tex = 1 << ((char_addr >> 5) & 0x1F);
	//u32 bit_clr = 1 << ((color_lut >> 5) & 0x1F);
	u32 dirty_tex = char_addr >> 10;
	//u32 dirty_clr = color_lut >> 10;
	if (!spr_w || !spr_h) {
		return;
	}
	if (tex_dirty[dirty_tex] & bit_tex) {
		return;
	}
	tex_dirty[dirty_tex] |= bit_tex;


	switch ((cmd.CMDPMOD >> 3) & 0x7) {
		case 0: // Colorbank 4-bit		RED
		case 1: // LUT 4-bit 			Green
			wii_sat2tex4bpp(char_addr, spr_w, spr_h);
			break;
		case 2: // Colorbank 8-bit (6)	BLUE
		case 3: // Colorbank 8-bit (7)	MAGENTA
		case 4: // Colorbank 8-bit (8)	CYAN
			wii_sat2texRGBA(char_addr, spr_w >> 1, spr_h);
			break;
		case 5: // RGB					WHITE
			wii_sat2texRGBA(char_addr, spr_w, spr_h);
			break;
	}
}


static void Tev_SetNonTexturedPart(void)
{
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(0);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_KONST, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
}

static void Tev_SetTexturedPart(void)
{
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_SUBHALF, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_A8_GT, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_KONST, GX_CA_ZERO);
}

static u32 modeToColor(void)
{
	//Address to valid vdp1 RAM range
	u8 *chr_addr = Vdp1Ram + ((cmd.CMDSRCA & 0xFFFC) << 3);
	u32 spr_w = (cmd.CMDSIZE & 0x3F00) >> 5;
	//XXX: make the sprite height a multiple of 8 or increase the height if unaligned
	u32 spr_h = (cmd.CMDSIZE & 0xF8);
	spr_h += ((cmd.CMDSIZE & 0x7) != 0 || (cmd.CMDSRCA & 3)) << 3;

	u32 tex_mode = (cmd.CMDPMOD >> 3) & 0x7;
	u32 colr = cmd.CMDCOLR;
	spritepixelinfo_struct spi = {0};
	if ((tex_mode != 1) && (tex_mode != 5)) {
		Vdp1GetSpritePixelInfo(Vdp2Regs->SPCTL & 0xF, &colr, &spi);
	}
	//Set priority
	SGX_SetZOffset(priority_arr[spi.priority] + 14);

	if (!(*((u32*)chr_addr)) && (cmd.CMDSIZE == 0x0101)) {
		SGX_SetZOffset(14);
	} else {
		SGX_SetZOffset(priority_arr[spi.priority] + 14);
	}

	//Check transparent code
	u32 trn_code = ((cmd.CMDPMOD & 0x40) ^ 0x40) << 1;
	u32 tlut_pos = colr + ((Vdp2Regs->CRAOFB << 4) & 0x700);

	//XXX: this can be optimized
	switch (tex_mode) {
		case 0: // Colorbank 4-bit
			SGX_TlutLoadCRAMImm(tlut_pos & 0x7F0, trn_code, GX_TLUT_16);
			//Change address and size
			SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_INDX_IMM4);
			SGX_SpriteConverterSet(spr_w >> 3, SPRITE_4BPP, cmd.CMDSRCA & 3);
			return 0x7f7f7f00; break;
		case 1: // LUT 4-bit
			u32 colorlut = (colr << 3) & 0x7FFFF;
			SGX_SetZOffset(priority_arr[0] + 14);
			//Upload palette
			//XXX: palette uploading should be done once per frame (for color bank, the other will be done per sprite)
			if (trn_code) {
				u32 *pal = MEM_K0_TO_K1(Vdp1Ram + colorlut);
				*pal &= 0xFFFFu;
			}
			GX_InitTlutObj(&tlut_obj, Vdp1Ram + colorlut, GX_TL_RGB5A3, 16);
			GX_LoadTlut(&tlut_obj, TLUT_INDX_IMM4);
			//Change address and size
			SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_INDX_IMM4);
			SGX_SpriteConverterSet(spr_w >> 3, SPRITE_4BPP, cmd.CMDSRCA & 3);
			return 0x7f7f7f00; break;
		case 2: // Colorbank 6-bit
		case 3: // Colorbank 7-bit
		case 4: // Colorbank 8-bit
			SGX_TlutLoadCRAMImm(tlut_pos & 0x7F0, trn_code, 2 << tex_mode);
			//Change address and size
			SGX_SetTex(chr_addr, GX_TF_CI8, spr_w, spr_h, TLUT_INDX_IMM8);
			SGX_SpriteConverterSet(spr_w >> 3, SPRITE_8BPP, cmd.CMDSRCA & 3);
			return 0x7f7f7f00; break;
		case 5: // RGB
			SGX_SetZOffset(priority_arr[0] + 14);
			SGX_TlutLoadCRAMImm(0, 0, GX_TLUT_16);	//XXX: prevents flickering... for now
			SGX_SetTex(chr_addr, GX_TF_RGB5A3, spr_w, spr_h, 0);
			SGX_SpriteConverterSet(spr_w >> 3, SPRITE_16BPP, cmd.CMDSRCA & 3);
			return 0x7f7f7f00; break;
	}
	return 0;	//Non textured
}

//HALF-DONE
void VIDSoftVdp1NormalSpriteDraw()
{
	s16 ax,ay,bx,by,cx,cy,dx,dy;
	Vdp1ReadCommand(&cmd, Vdp1Regs->addr);
	u32 spriteWidth = ((cmd.CMDSIZE & 0x3F00) >> 8);
	u32 spriteHeight = (cmd.CMDSIZE & 0xFF);
	if (!spriteWidth || !spriteHeight) {
		return;
	}

	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	//THIS DEFINES THE SQUARE
	ax = cmd.CMDXA;
	ay = cmd.CMDYA;
	cx = ax + (spriteWidth << 3);
	cy = ay + spriteHeight;

	//UNECESARY INFO?
	bx = cx;
	by = ay;
	dx = ax;
	dy = cy;

	Tev_SetTexturedPart();
	//XXX: Combine these effects for half transaprency (Half luminence/transparency/shadow)
	u32 mesh = (cmd.CMDPMOD & 0x0100) >> 6;
	//GX_TEV_KASEL_1 -> 0
	//GX_TEV_KASEL_1_2 -> 4
	//COLOR CALCULATION
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, mesh);

	//Get Colors... XXX: this is unecesary since this is textured
	u32 col[4];
	col[3] = col[2] = col[1] = col[0] = modeToColor();
	//XXX: has gouraud?
	if (cmd.CMDPMOD & 0x4) {
		getGouraudColors(col);
	}

	//Flip the sprite
	//XXX: using s16 is inefficient, use the whole u32
	u32 tex_flip = (-(cmd.CMDCTRL & 0x10) << 4) & 0x3F00;
	tex_flip 	|= (-(cmd.CMDCTRL & 0x20) >> 5) & 0x00FF;
	u32 spr_size = cmd.CMDSIZE & 0x3FFF;

	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(ax, ay);
		GX_Color1u32(col[0]);
		GX_TexCoord1u16(spr_size & (0x0000 ^ tex_flip));
		GX_Position2s16(bx, by);
		GX_Color1u32(col[1]);
		GX_TexCoord1u16(spr_size & (0x3F00 ^ tex_flip));
		GX_Position2s16(cx, cy);
		GX_Color1u32(col[2]);
		GX_TexCoord1u16(spr_size & (0x3FFF ^ tex_flip));
		GX_Position2s16(dx, dy);
		GX_Color1u32(col[3]);
		GX_TexCoord1u16(spr_size & (0x00FF ^ tex_flip));
	GX_End();
}

//HALF-DONE
void VIDSoftVdp1ScaledSpriteDraw()
{
	s32 ax,ay,bx,by,cx,cy,dx,dy;
	int x0,y0,x1,y1;
	Vdp1ReadCommand(&cmd, Vdp1Regs->addr);
	u32 spriteWidth = ((cmd.CMDSIZE & 0x3F00) >> 8);
	u32 spriteHeight = (cmd.CMDSIZE & 0xFF);
	if (!spriteWidth || !spriteHeight) {
		return;
	}

	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	x0 = cmd.CMDXA;
	y0 = cmd.CMDYA;

	switch ((cmd.CMDCTRL >> 8) & 0xF) {
		case 0x0: // Only two coordinates
		default:
			x1 = ((int)cmd.CMDXC) - x0 + 1;
			y1 = ((int)cmd.CMDYC) - y0 + 1;
			break;
		case 0x5: // Upper-left
			x1 = ((int)cmd.CMDXB) + 1;
			y1 = ((int)cmd.CMDYB) + 1;
			break;
		case 0x6: // Upper-Center
			x1 = ((int)cmd.CMDXB);
			y1 = ((int)cmd.CMDYB);
			x0 = x0 - x1/2;
			x1++;
			y1++;
			break;
		case 0x7: // Upper-Right
			x1 = ((int)cmd.CMDXB);
			y1 = ((int)cmd.CMDYB);
			x0 = x0 - x1;
			x1++;
			y1++;
			break;
		case 0x9: // Center-left
			x1 = ((int)cmd.CMDXB);
			y1 = ((int)cmd.CMDYB);
			y0 = y0 - y1/2;
			x1++;
			y1++;
			break;
		case 0xA: // Center-center
			x1 = ((int)cmd.CMDXB);
			y1 = ((int)cmd.CMDYB);
			x0 = x0 - x1/2;
			y0 = y0 - y1/2;
			x1++;
			y1++;
			break;
		case 0xB: // Center-right
			x1 = ((int)cmd.CMDXB);
			y1 = ((int)cmd.CMDYB);
			x0 = x0 - x1;
			y0 = y0 - y1/2;
			x1++;
			y1++;
			break;
		case 0xD: // Lower-left
			x1 = ((int)cmd.CMDXB);
			y1 = ((int)cmd.CMDYB);
			y0 = y0 - y1;
			x1++;
			y1++;
			break;
		case 0xE: // Lower-center
			x1 = ((int)cmd.CMDXB);
			y1 = ((int)cmd.CMDYB);
			x0 = x0 - x1/2;
			y0 = y0 - y1;
			x1++;
			y1++;
			break;
		case 0xF: // Lower-right
			x1 = ((int)cmd.CMDXB);
			y1 = ((int)cmd.CMDYB);
			x0 = x0 - x1;
			y0 = y0 - y1;
			x1++;
			y1++;
			break;
	}

	ax = x0;
	ay = y0;

	bx = x1 + x0;
	by = ay;

	cx = x1 + x0;
	cy = y1 + y0;

	dx = ax;
	dy = y1 + y0;


	Tev_SetTexturedPart();
	//XXX: Combine these effects for half transaprency (Half luminence/transparency/shadow)
	u32 mesh = (cmd.CMDPMOD & 0x0100) >> 6;
	//GX_TEV_KASEL_1 -> 0
	//GX_TEV_KASEL_1_2 -> 4
	//COLOR CALCULATION
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, mesh);

	//Get Colors... XXX: this is unecesary since this is textured
	u32 col[4];
	col[3] = col[2] = col[1] = col[0] = modeToColor();
	//XXX: has gouraud?
	if (cmd.CMDPMOD & 0x4) {
		getGouraudColors(col);
	}

	//Flip the sprite
	//XXX: using s16 is inefficient, use the whole u32
	u32 tex_flip = (-(cmd.CMDCTRL & 0x10) << 4) & 0x3F00;
	tex_flip 	|= (-(cmd.CMDCTRL & 0x20) >> 5) & 0x00FF;
	u32 spr_size = cmd.CMDSIZE & 0x3FFF;

	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(ax, ay);
		GX_Color1u32(col[0]);
		GX_TexCoord1u16(spr_size & (0x0000 ^ tex_flip));
		GX_Position2s16(bx, by);
		GX_Color1u32(col[1]);
		GX_TexCoord1u16(spr_size & (0x3F00 ^ tex_flip));
		GX_Position2s16(cx, cy);
		GX_Color1u32(col[2]);
		GX_TexCoord1u16(spr_size & (0x3FFF ^ tex_flip));
		GX_Position2s16(dx, dy);
		GX_Color1u32(col[3]);
		GX_TexCoord1u16(spr_size & (0x00FF ^ tex_flip));
	GX_End();
}

//HALF-DONE
void VIDSoftVdp1DistortedSpriteDraw()
{
	s16 ax, ay, bx, by, cx, cy, dx, dy;
	Vdp1ReadCommand(&cmd, Vdp1Regs->addr);
	u32 spriteWidth = ((cmd.CMDSIZE & 0x3F00) >> 8);
	u32 spriteHeight = (cmd.CMDSIZE & 0xFF);
	if ((!spriteWidth || !spriteHeight) && !(cmd.CMDCTRL & 0x4)) {
		return;
	}
	//XXX: modify the points to fill leftsize pixels
    ax = cmd.CMDXA;
    ay = cmd.CMDYA;
    bx = cmd.CMDXB;
    by = cmd.CMDYB;
    cx = cmd.CMDXC;
    cy = cmd.CMDYC;
    dx = cmd.CMDXD;
    dy = cmd.CMDYD;

	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	u32 col[4] = {0};
	//XXX: Combine these effects for half transaprency (Half luminence/transparency/shadow)
	u32 alpha = (cmd.CMDPMOD & 0x0100) >> 6;
	//GX_TEV_KASEL_1 -> 0
	//GX_TEV_KASEL_1_2 -> 4
	//COLOR CALCULATION

	//Get Colors... XXX: this is unecesary since this is textured
	if (cmd.CMDCTRL & 0x4) {
		//XXX: use konst colors
		Tev_SetNonTexturedPart();
		//Only check msb if VDP2 specifies that it has RGB values
		u32 msb = (cmd.CMDCOLR >> 15) & (Vdp2Regs->SPCTL >> 5);
		u32 color = cmd.CMDCOLR;
		if (msb) {
			SGX_SetZOffset(priority_arr[0] + 14);
			GX_SetTevKAlphaSel(GX_TEVSTAGE0, alpha);
		} else {
			spritepixelinfo_struct spi = {0};
			Vdp1GetSpritePixelInfo(Vdp2Regs->SPCTL & 0xF, &color, &spi);
			if (!color) {
				return;
			}
			//Set priority
			SGX_SetZOffset(priority_arr[spi.priority] + 14);
			//XXX: Get colorbank value
			color = *((u16*)(Vdp2ColorRam + ((color + ((Vdp2Regs->CRAOFB << 4) & 0x700)) << 3)));
			GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
		}
		GXColor konst = {(color & 0x1F) << 3, (color & 0x3E0) >> 2, (color & 0x7C00) >> 7, 0};
		GX_SetTevKColor(GX_KCOLOR0, konst);
		GX_SetTevKAlphaSel(GX_TEVSTAGE0, alpha);
		GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
	} else {
		Tev_SetTexturedPart();
		GX_SetTevKAlphaSel(GX_TEVSTAGE0, alpha);
		col[3] = col[2] = col[1] = col[0] = modeToColor();
	}


	//XXX: has gouraud?
	if (cmd.CMDPMOD & 0x4) {
		getGouraudColors(col);
	}

	//Flip the sprite
	//XXX: using s16 is inefficient, use the whole u32
	u32 tex_flip = (-(cmd.CMDCTRL & 0x10) << 4) & 0x3F00;
	tex_flip 	|= (-(cmd.CMDCTRL & 0x20) >> 5) & 0x00FF;
	u32 spr_size = cmd.CMDSIZE & 0x3FFF;

	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(ax, ay);
		GX_Color1u32(col[0]);
		GX_TexCoord1u16(spr_size & (0x0000 ^ tex_flip));
		GX_Position2s16(bx, by);
		GX_Color1u32(col[1]);
		GX_TexCoord1u16(spr_size & (0x3F00 ^ tex_flip));
		GX_Position2s16(cx, cy);
		GX_Color1u32(col[2]);
		GX_TexCoord1u16(spr_size & (0x3FFF ^ tex_flip));
		GX_Position2s16(dx, dy);
		GX_Color1u32(col[3]);
		GX_TexCoord1u16(spr_size & (0x00FF ^ tex_flip));
	GX_End();
}

void vid_Vdp1PolygonDraw()
{
	s16 ax, ay, bx, by, cx, cy, dx, dy;
	Vdp1ReadCommand(&cmd, Vdp1Regs->addr);

	if (!cmd.CMDCOLR) {
		return;
	}

	//XXX: modify the points to fill leftsize pixels
    ax = cmd.CMDXA;
    ay = cmd.CMDYA;
    bx = cmd.CMDXB;
    by = cmd.CMDYB;
    cx = cmd.CMDXC;
    cy = cmd.CMDYC;
    dx = cmd.CMDXD;
    dy = cmd.CMDYD;

	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	u32 col[4] = {0};
	//XXX: Combine these effects for half transaprency (Half luminence/transparency/shadow)
	u32 alpha = (cmd.CMDPMOD & 0x0100) >> 6;

	Tev_SetNonTexturedPart();
	u32 msb = (cmd.CMDCOLR >> 15);
	if (msb) {
		GX_SetTevKAlphaSel(GX_TEVSTAGE0, alpha);
	} else {
		//XXX: Get colorbank value
		GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
	}

	GXColor konst = {(cmd.CMDCOLR & 0x1F) << 3, (cmd.CMDCOLR & 0x3E0) >> 2, (cmd.CMDCOLR & 0x7C00) >> 7, 0};
	GX_SetTevKColor(GX_KCOLOR0, konst);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, alpha);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);

	//XXX: has gouraud?
	if (cmd.CMDPMOD & 0x4) {
		getGouraudColors(col);
	}

	//Send command
	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(ax, ay);
		GX_Color1u32(col[0]);
		GX_Position2s16(bx, by);
		GX_Color1u32(col[1]);
		GX_Position2s16(cx, cy);
		GX_Color1u32(col[2]);
		GX_Position2s16(dx, dy);
		GX_Color1u32(col[3]);
	GX_End();

}




//HALF-DONE
void VIDSoftVdp1PolylineDraw(void)
{
	s16 ax, ay, bx, by, cx, cy, dx, dy;

	Vdp1ReadCommand(&cmd, Vdp1Regs->addr);

    ax = cmd.CMDXA;
    ay = cmd.CMDYA;
    bx = cmd.CMDXB;
    by = cmd.CMDYB;
    cx = cmd.CMDXC;
    cy = cmd.CMDYC;
    dx = cmd.CMDXD;
    dy = cmd.CMDYD;

	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	Tev_SetNonTexturedPart();
	GXColor konst = {(cmd.CMDCOLR & 0x1F) << 3, (cmd.CMDCOLR & 0x3E0) >> 2, (cmd.CMDCOLR & 0x7C00) >> 7, 0};
	GX_SetTevKColor(GX_KCOLOR0, konst);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);

	//XXX: Combine these effects for half transaprency (Half luminence/transparency/shadow)
	u32 mesh = (cmd.CMDPMOD & 0x0100) >> 6;
	//GX_TEV_KASEL_1 -> 0
	//GX_TEV_KASEL_1_2 -> 4
	//COLOR CALCULATION
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, mesh);

	GX_Begin(GX_LINESTRIP, GX_VTXFMT2, 5);
		GX_Position2s16(ax, ay);
		GX_Color1u32(0);
		GX_Position2s16(bx, by);
		GX_Color1u32(0);
		GX_Position2s16(cx, cy);
		GX_Color1u32(0);
		GX_Position2s16(dx, dy);
		GX_Color1u32(0);
		GX_Position2s16(ax, ay);
		GX_Color1u32(0);
	GX_End();
}

//HALF-DONE
void VIDSoftVdp1LineDraw(void)
{
	s16 ax, ay, bx, by;
	Vdp1ReadCommand(&cmd, Vdp1Regs->addr);

    ax = cmd.CMDXA;
    ay = cmd.CMDYA;
    bx = cmd.CMDXB;
    by = cmd.CMDYB;

	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	Tev_SetNonTexturedPart();
	GXColor konst = {(cmd.CMDCOLR & 0x1F) << 3, (cmd.CMDCOLR & 0x3E0) >> 2, (cmd.CMDCOLR & 0x7C00) >> 7, 0};
	GX_SetTevKColor(GX_KCOLOR0, konst);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);

	//XXX: Combine these effects for half transaprency (Half luminence/transparency/shadow)
	u32 mesh = (cmd.CMDPMOD & 0x0100) >> 6;
	//GX_TEV_KASEL_1 -> 0
	//GX_TEV_KASEL_1_2 -> 4
	//COLOR CALCULATION
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, mesh);

	GX_Begin(GX_LINES, GX_VTXFMT2, 2);
		GX_Position2s16(ax, ay);
		GX_Color1u32(0);
		GX_Position2s16(bx, by);
		GX_Color1u32(0);
	GX_End();
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
void VIDSoftVdp1UserClipping(void)
{
   Vdp1Regs->userclipX1 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xC);
   Vdp1Regs->userclipY1 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xE);
   Vdp1Regs->userclipX2 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x14);
   Vdp1Regs->userclipY2 = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x16);
}


//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
void VIDSoftVdp1SystemClipping(void)
{
	u32 sysclip_x = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x14);
	u32 sysclip_y = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0x16);
	//XXX: limit to screen
	sysclip_x = (sysclip_x > vdp2width ? sysclip_x : vdp2width);
	sysclip_y = (sysclip_y > vdp2height ? sysclip_y : vdp2height);
	GX_SetScissor(0, 0, sysclip_x, sysclip_y);
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
void VIDSoftVdp1LocalCoordinate(void)
{
	mat[GXMTX_VDP1][0][3] = (f32) ((s16) T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xC));
	mat[GXMTX_VDP1][1][3] = (f32) ((s16) T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xE));
	//DCStoreRange(mat, 0x20);
	//GX_LoadPosMtxIdx(0, GX_PNMTX0);
	//mat[GXMTX_VDP1][2][3] = (f32) (pritority);
	GX_LoadPosMtxImm(mat[GXMTX_VDP1], GXMTX_VDP1);
	//Vdp1Regs->localX = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xC);
	//Vdp1Regs->localY = T1ReadWord(Vdp1Ram, Vdp1Regs->addr + 0xE);
}

//////////////////////////////////////////////////////////////////////////////

//DONE??
int VIDSoftVdp2Reset(void)
{
   return 0;
}


//////////////////////////////////////////////////////////////////////////////

void VIDSoftVdp2DrawStart(void)
{
	if (Vdp2Regs->CYCA0L == 0x5566 &&
      Vdp2Regs->CYCA0U == 0x47ff &&
      Vdp2Regs->CYCA1L == 0xffff &&
      Vdp2Regs->CYCA1U == 0xffff &&
      Vdp2Regs->CYCB0L == 0x12ff &&
      Vdp2Regs->CYCB0U == 0x03ff &&
      Vdp2Regs->CYCB1L == 0xffff &&
      Vdp2Regs->CYCB1U == 0xffff) {
      bad_cycle_setting_nbg3 = 1;
   } else {
      bad_cycle_setting_nbg3 = 0;
	}
}

//////////////////////////////////////////////////////////////////////////////

void VIDSoftVdp2DrawEnd(void)
{
	//XXX: Move this to drawscreens
	//VidsoftDrawSprite();
	//XXX: Delete titan stuff
	//{
		//int titanblendmode = TITAN_BLEND_TOP;
		//if (Vdp2Regs->CCCTL & 0x100) titanblendmode = TITAN_BLEND_ADD;
		//else if (Vdp2Regs->CCCTL & 0x200) titanblendmode = TITAN_BLEND_BOTTOM;
		//TitanRender(dispbuffer, titanblendmode);
	//}
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
#if 0
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_LoadPosMtxImm((f32 (*)[4]) (mat + MAT_IDENTITY), GX_PNMTX0);
#else
	//if in interlaced mode, do not scale image

	if (!disp.scale_y) {
		return;
	}

	//XXX: We can make this async mode using callbacks
	GX_Flush();
	GX_DrawDone();
	GX_SetTexCopySrc(0, 0, disp.w, disp.h);
	GX_SetTexCopyDst(disp.w, disp.h, GX_TF_RGBA8, GX_FALSE);
	GX_CopyTex(display_fb, GX_TRUE);

	GX_PixModeSync();

	//GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetCurrentMtx(GXMTX_IDENTITY);
	GX_SetScissor(0, 0, 640, disp.h << disp.scale_y);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	//Load these only when changed
	u8 ofs_reg = gfx_GetColorOffset(0x40);	//sprite mask

	SGX_SetZOffset(255);
	//SET UP GX TEV STAGES for textures
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, ofs_reg);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP1);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	GX_InitTexObj(&tex_obj_vdp1, display_fb, disp.w, disp.h, GX_TF_RGBA8, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&tex_obj_vdp1, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_LoadTexObj(&tex_obj_vdp1, GX_TEXMAP0);

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position2s16(0, 0);
		GX_TexCoord2f32(0.0, 0.0);
		GX_Position2s16(disp.w << !disp.highres, 0);
		GX_TexCoord2f32(1.0, 0.0);
		GX_Position2s16(disp.w << !disp.highres, disp.h << disp.scale_y);
		GX_TexCoord2f32(1.0, 1.0);
		GX_Position2s16(0, disp.h << disp.scale_y);
		GX_TexCoord2f32(0.0, 1.0);
	GX_End();

	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
#endif

}

//////////////////////////////////////////////////////////////////////////////


//HALF-DONE
void VIDSoftVdp2DrawScreens(void)
{
	//Set up Tev stages and everything we need
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_INDEX8);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	//Set Pixel format for alpha channel
	//GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
	//GX_SetDither(GX_DISABLE);

	//SET UP GX TEV STAGES...
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(0);
    GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	//XXX: this is for paletted sprites, Konst is for transparency, gouraud is always active and half
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_RASA, GX_CA_ZERO, GX_CA_ZERO, GX_CC_ZERO);

		//int titanblendmode = TITAN_BLEND_TOP;
		//if (Vdp2Regs->CCCTL & 0x100) titanblendmode = TITAN_BLEND_ADD;
		//else if (Vdp2Regs->CCCTL & 0x200) titanblendmode = TITAN_BLEND_BOTTOM;
	switch ((Vdp2Regs->CCCTL >> 8) & 0x3) {
		case 0: GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR); break;	//TOP
		case 1: GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR); break;	//ADD
		case 2: GX_SetBlendMode(GX_BM_BLEND, GX_BL_DSTALPHA, GX_BL_INVDSTALPHA, GX_LO_CLEAR); break;	//BOTTOM
	}


	//XXX: Draw Screens by priority, this HLE will override various specs of hardware
	//I assume the visual quality wont degrade, only improve since i dont think devs where assholes.
	//We will calculate everything in the following way:
	//- Sort screens by priority (if special priority is used then do double pass in its respective priority.
	//- For each priority:
	// First draw the clip window and draw the screens in order.
	// Then draw the clip window and the sprites in order.
	//NOTES:
	//- Two options, include linescreen/color offset in TEV or pass them as a single color using Blend (better)
	//I believe that top/bottom/add color calculation will work by just changing the GX_Blend SRC/DST alpha values
	//Finally, we will use RGBA6 when bottom color calculation is used, else we use RGB8
	u32 scr_pri = Vdp2Regs->PRINA & 0x7;
	u32 scr_enable = Vdp2Regs->BGON & 0x1;
	if (!(!(scr_pri) || !(scr_enable))) {
		screen_list[scr_pri] |= SCR_NBG0;
	}

	scr_pri = (Vdp2Regs->PRINA >> 8) & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x2;
	//UPDATED
	// If NBG0 16M mode is enabled, don't draw
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 == 4))) {
		screen_list[scr_pri] |= SCR_NBG1;
	}

	//If NBG0 2048/32786/16M mode is enabled, don't draw
	scr_pri = Vdp2Regs->PRINB & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x4;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 >= 2))) {
		screen_list[scr_pri] |= SCR_NBG2;
	}

	// If NBG0 16M mode is enabled, don't draw
	// If NBG1 2048/32786 is enabled, don't draw
	scr_pri = (Vdp2Regs->PRINB >> 8) & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x8;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 == 4) ||
		(Vdp2Regs->BGON & 0x2 && (Vdp2Regs->CHCTLA & 0x3000) >> 12 >= 2))) {
		screen_list[scr_pri] |= SCR_NBG3;
	}

	for (u32 pri = 1; pri < 8; ++pri) {
		u32 i = 3;
		u8 screens = screen_list[pri];
		screen_list[pri] = 0;
		while (screens) {
			if (screens & 1) {
				draw_fun_array[i]();
			}
			i = (i ? i - 1 : 4);
			screens >>= 1;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
extern void gx_ChangeVideo(u32 y_ofs, u32 width, u32 screen_width);
//HALF-DONE
void VIDSoftVdp2SetResolution(u16 TVMD)
{
	//XXX: useless
	// Horizontal Resolution
	rbg0width = 320 + ((TVMD & 1) << 5);
	vdp2_x_hires = ((TVMD >> 1) & 1);
	vdp2width = rbg0width << vdp2_x_hires;
	// Vertical Resolution
	rbg0height = 224 + (TVMD & 0x30);
	vdp2_interlace = ((TVMD  & 0xC0) == 0xC0);
	vdp2height = rbg0height << vdp2_interlace;

	disp.x = 0;
	disp.y = 0x10 - (TVMD & 0x10);
	disp.highres = ((TVMD >> 1) & 1);
	u32 output_w = (320 + ((TVMD & 1) << 5)) << 1;
	// We will allways draw the highres version
	if (disp.highres) {
		disp.w = 640;
		output_w = 640;	//not necesary?
	} else {
		disp.w = 320 + ((TVMD & 1) << 5);
	}
	//If non interlaced, we must scale the output
	disp.scale_y = ((TVMD & 0xC0) != 0xC0);
	disp.h = (224 + (TVMD & 0x30)) << !disp.scale_y;

	GX_SetScissor(0, 0, disp.w, disp.h);
	gx_ChangeVideo(disp.y, 640, output_w);
}

//////////////////////////////////////////////////////////////////////////////

//DONE
void VIDSoftOnScreenDebugMessage(char *string, ...)
{
	//DOES NOTHING
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
void VIDSoftVdp1SwapFrameBuffer(void)
{
	if ((~Vdp1Regs->FBCR & 2) | Vdp1External.manualchange) {
		//XXX: unused
		//u8 *temp = vdp1frontframebuffer;
		//vdp1frontframebuffer = vdp1backframebuffer;
		//vdp1backframebuffer = temp;
		Vdp1External.manualchange = 0;
	}
}

//////////////////////////////////////////////////////////////////////////////

//HALF-DONE
void VIDSoftVdp1EraseFrameBuffer(void)
{
	//int i,i2;
	//int w,h;

	if (!(Vdp1Regs->FBCR & 2) || Vdp1External.manualerase)
	{
#if 0
		h = (Vdp1Regs->EWRR & 0x1FF) + 1;
		if (h > vdp1height) h = vdp1height;
		w = ((Vdp1Regs->EWRR >> 6) & 0x3F8) + 8;
		if (w > vdp1width) w = vdp1width;

		if (vdp1pixelsize == 2)
		{
			for (i2 = (Vdp1Regs->EWLR & 0x1FF); i2 < h; i2++)
			{
				for (i = ((Vdp1Regs->EWLR >> 6) & 0x1F8); i < w; i++)
					((u16 *)vdp1backframebuffer)[(i2 * vdp1width) + i] = Vdp1Regs->EWDR;
			}
		}
		else
		{
			w = (Vdp1Regs->EWRR >> 9) << 4;

			for (i2 = (Vdp1Regs->EWLR & 0x1FF); i2 < h; i2++)
			{
				for (i = ((Vdp1Regs->EWLR >> 6) & 0x1F8); i < w; i++) {
					int pos = (i2 * vdp1width) + i;
					if (pos < 0x3FFFF)
						vdp1backframebuffer[pos] = Vdp1Regs->EWDR & 0xFF;
				}
			}
		}
#endif
		Vdp1External.manualerase = 0;
	}
}

