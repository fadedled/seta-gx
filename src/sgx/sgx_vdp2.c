
#include "sgx.h"
#include "../vdp1.h"
#include "../vdp2.h"
#include <malloc.h>

#define SGX_TORGBA32(col) ((((col) & 0x1F) | (((col) & 0x3E0) << 3) | (((col) & 0x7C00) << 6)) << 11)
#define SGX_TORGB565(col) (((col) & 0x1F) | ((col << 1) & 0xFFC0))
#define SGX_FROMRGB565(col) (((col) & 0x1F) | ((col >> 1) & 0x7FE0))



Mtx vdp2mtx ATTRIBUTE_ALIGN(32) = {
	{1.0, 0.0, 0.0, 0.0},
	{0.0, 1.0, 0.0, 0.0},
	{0.0, 0.0, 1.0, 0.0}
};

//The bits in these numbers are for generating the window tluts,
//they represent the first 8 enable values of the tlut. they are ordered
//with respect to the window enable and active zone bits found in VDP2 registers
//Only include the first 64 possible values since the other 64 can be caluclated by
//XOR'ing with the OP value (OR = 0x00, AND = 0xFF).
const u8 win_enable_bits[64] = {
	0xFF, 0xFF, 0x55, 0xAA, 0xFF, 0xFF, 0x55, 0xAA,
	0x33, 0x33, 0x11, 0x22, 0xCC, 0xCC, 0x44, 0x88,
	0xFF, 0xFF, 0x55, 0xAA, 0xFF, 0xFF, 0x55, 0xAA,
	0x33, 0x33, 0x11, 0x22, 0xCC, 0xCC, 0x44, 0x88,
	0x0F, 0x0F, 0x05, 0x0A, 0x0F, 0x0F, 0x05, 0x0A,
	0x03, 0x03, 0x01, 0x02, 0x0C, 0x0C, 0x04, 0x08,
	0xF0, 0xF0, 0x50, 0xA0, 0xF0, 0xF0, 0x50, 0xA0,
	0x30, 0x30, 0x10, 0x20, 0xC0, 0xC0, 0x40, 0x80,
};

u16 color_ofs_tlut[16] ATTRIBUTE_ALIGN(32);
u16 win_tlut[8][16] ATTRIBUTE_ALIGN(32);

//About 1 Meg of data combined
u8 *prop_tex;	//Priotiry texture
u8 *win_tex;	//Window texture
u8 *image_tex;  //Holds textures for 1st/2nd images and opaque pixels
u8 *screen_tex; //Holds textures for VDP2 screens

#define VDP2_SCREEN_TEX_NBG0	(screen_tex)
#define VDP2_SCREEN_TEX_NBG1
#define VDP2_SCREEN_TEX_NBG2
#define VDP2_SCREEN_TEX_NBG3
#define VDP2_SCREEN_TEX_NBG0_ZA
#define VDP2_SCREEN_TEX_NBG1_ZA
#define VDP2_SCREEN_TEX_NBG2_ZA
#define VDP2_SCREEN_TEX_NBG3_ZA

#define VDP2_IMAGE_TEX_OPAQUE		(image_tex + 0)
#define VDP2_IMAGE_TEX_1ST			(image_tex + (704*512*2))
#define VDP2_IMAGE_TEX_2ND			(image_tex + (704*512*4))
#define VDP2_IMAGE_TEX_OPAQUE_PRI	(image_tex + (704*512*6))	//TODO_NECESARY?
#define VDP2_IMAGE_TEX_TOP_PRI		(image_tex + (704*512*7))
#define VDP2_IMAGE_TEX_ALPHA		(image_tex + (704*512*8))


u8 cram_4bpp[PAGE_SIZE] ATTRIBUTE_ALIGN(32);
u8 cram_8bpp[PAGE_SIZE] ATTRIBUTE_ALIGN(32);
u8 cram_11bpp[PAGE_SIZE] ATTRIBUTE_ALIGN(32);

u32 vdp1_fb_w = SS_DISP_WIDTH;	/*framebuffer width visible in vdp2*/
u32 vdp1_fb_h = SS_DISP_HEIGHT;	/*framebuffer heigth visible in vdp2*/
u32 vdp1_fb_mtx = MTX_TEX_SCALED_N;				/*framebuffer scaling in vdp2*/

u32 vdp2_disp_w = SS_DISP_WIDTH;	/*display width*/
u32 vdp2_disp_h = SS_DISP_HEIGHT;	/*display height*/
u32 screen_enable = 0;			/*Enable bits per screen (Uses the PRI_ marcros for bit access)*/

static struct CellFormatData {
	u32 disp_ctl;	/*Screen display Control*/
	u32 char_ctl;	/*Character control settings*/
	u32 color_fmt;	/*Color format*/
	u32 char_size; 	/*Size of char data (8 or 16 pixel)*/
	u32 ptrn_supp;		/*Constant data of pattern (cc.pri, palette and char)*/
	u32 ptrn_chr_shft;	/*Shift of character data in pattern name*/
	u32 ptrn_flip_shft;	/*Shift of flip data in pattern name*/
	u32 ptrn_pal_shft;	/*Shift of palette data in pattern name*/
	u32 ptrn_mask;		/*Mask of character & palette data in pattern name */
	u32 cram_offset; 	/*Color ram offset*/
	u32 pri; 			/*Background priority*/
	u32 spec_pri; 		/*Special priority mode*/

	u32 xscroll;		/*X scroll*/
	u32 yscroll; 		/*Y scroll*/
	u32 xinc;			/*X increment*/
	u32 yinc; 			/*Y increment*/
	u32 page_mask;		/*page mask*/
	u32 page_shft;		/*page shift*/
	u32 plane_mask;		/*plane mask*/
	u32 xmap_shft;		/*map bit shift for x axis*/
	u32 ymap_shft;		/*map bit shift for y axis*/
	u32 *map_addr[16]; 	/* addresses of maps*/

	u32 *ls_table;		/* Line scroll table */
	u32 *vs_table;		/* Vertical Cell scroll table */
	u32 scr_ctl;			/* line/cell scroll control */

} cell;

struct {
	u32 xp;
	u32 yp;
	u32 dx;
	u32 dy;

	u32 kx; // C*(Zst - Pz)
	u32 ky; // F*(Zst - Pz)

	u32 czp; // C*(Zst - Pz)
	u32 fzp; // F*(Zst - Pz)

	u32 dxst; // DXst
	u32 dyst; // DYst
	u32 xst; // Xst - Px
	u32 yst; // Yst - Py
} rot_a, rot_b;

u32 line_scroll_data[256*3];
//u32 line_scroll_data[256*3];

static u32 __calcRotTable(u32 *rt)
{
	rot_a.xst  = *(rt++);
	rot_a.yst  = *(rt++);
	u32 z_st  = *(rt++);
	rot_a.dxst = *(rt++);
	rot_a.dyst = *(rt++);
	u32 dx    = *(rt++);
	u32 dy    = *(rt++);
	u32 a    = *(rt++);
	u32 b    = *(rt++);
	u32 c    = *(rt++);
	u32 d    = *(rt++);
	u32 e    = *(rt++);
	u32 f    = *(rt++);

	//Check fixed point
	u32 pv = *(rt++);
	u32 px = pv & 0xFFFF0000;
	u32 py = pv << 16;
	u32 pz = *(rt++) & 0xFFFF0000;

	//Check fixed point
	u32 cv = *(rt++);
	u32 cx = cv & 0xFFFF0000;
	u32 cy = cv << 16;
	u32 cz = *(rt++) & 0xFFFF0000;

	u32 mx = *(rt++);
	u32 my = *(rt++);

	// Check for normal screen
	// A = can be anything
	// either E is zero or DeltaY is zero, we cant have both
	// B = C = D = F = DeltaXst =  0
#if 0
	if ( //(e ^ dy)
		(b ^ 0) | (c ^ 0) | (d ^ 0) | (f ^ 0) | (rot_a.dxst ^ 0)) {
		// we can calculate scroll and increments
		u32 kx = *(rt++);
		u32 ky = *(rt++);

		x_scroll = fix_mul(kx, fix_mul(a, rot_a.x_st - px)) + fix_mul(a, px - cx) + cx + mx;
		y_scroll = fix_mul(ky, fix_mul(e, rot_a.y_st - py)) + fix_mul(b, py - cy) + cy + my;
		x_inc = fix_mul(ky, fix_mul(e, dx));
		y_inc = fix_mul(ky, fix_mul(e, rot_a.dyst));
		return 1;
	}

	//Precalc XY view coord after rotational conversion
	rot_a.x_st -= px;
	rot_a.y_st -= py;
	rot_a.xp = fp_mul(a, px-cy) + fp_mul(b, py-cy) +  fp_mul(c, pz-cz) + cx + mx;
	rot_a.yp = fp_mul(d, px-cy) + fp_mul(e, py-cy) +  fp_mul(f, pz-cz) + cy + my;
	rot_a.dx = fp_mul(a, dx) + fp_mul(b, dy);
	rot_a.dy = fp_mul(d, dx) + fp_mul(e, dy);
	rot_a.czp = fp_mul(c, z_st-pz);
	rot_a.fzp = fp_mul(f, z_st-pz);

	rot_a.kx = *(rt++);
	rot_a.ky = *(rt++);

	u32 kast = *(rt++);
	u32 dkast = *(rt++);
	u32 dkax = *(rt++);
#endif
	return 0;
}


//Ge
static void __genScrollTable(u32 *src_data)
{
	if (cell.scr_ctl & 0xE) {
		return;
	}
	u32 *dst_data = line_scroll_data;
	u32 num_lines = 256; //Use display height
	u32 scx_inc = (cell.scr_ctl >> 1) & 1;
	u32 scy_inc = (cell.scr_ctl >> 2) & 1;
	u32 zmx_inc = (cell.scr_ctl >> 3) & 1;

	u32 scx_mask = -(scx_inc);
	u32 scy_mask = -(scy_inc);
	u32 zmx_mask = -(zmx_inc);
	u32 repeat = 1 << ((cell.scr_ctl >> 4) & 3);
	u32 xval = cell.xscroll;
	u32 yval = cell.yscroll;
	while (num_lines) {
		//Horizontal coords
		u32 xdat = (((*src_data >> 8) + xval) & scx_mask) | (cell.xscroll & ~scx_mask);
		src_data += scx_inc;
		//Vertical coords
		u32 ydat = (((*src_data >> 8) + yval) & scy_mask) | (cell.yscroll & ~scy_mask);
		src_data += scy_inc;
		//Vertical increment
		u32 sdat = ((*src_data >> 8) & zmx_mask) | (cell.xinc & ~zmx_mask);
		src_data += zmx_inc;
		//Store repeating data
		for (u32 i = 0, ypos = 0; i < repeat; ++i, ypos += cell.yinc) {
			*(dst_data++) = xdat;
			*(dst_data++) = ydat + ypos; // increment y coords
			*(dst_data++) = sdat;
		}
		yval += cell.yinc;
		xval += cell.xinc;
		num_lines -= repeat;
	}
}


static void __convert32bpp(u32 *dst, u32 *src, u32 dst_w, u32 dst_h, u32 src_w, u32 offset)
{
	u32 *scr_data = line_scroll_data;
	u32 *d0 = dst;
	u32 *d1 = dst + 2;
	u32 *d2 = dst + 4;
	u32 *d3 = dst + 6;

	u32 *d_r0 = dst + 8;
	u32 *d_r1 = dst + 10;
	u32 *d_r2 = dst + 12;
	u32 *d_r3 = dst + 14;

#if 0 //USE SCROLL
	u32 *s0 = src;
	u32 *s1 = s0 + src_w;
	u32 *s2 = s1 + src_w;
	u32 *s3 = s2 + src_w;

	u32 src_inc = (src_w - dst_w) + (src_w * 3);

	for (u32 i = 0; i < dst_h; i+=4) {
		for (u32 k = 0; k < dst_w; k+= 4) {
			// one tile
			for (u32 j = 0; j < 2; j++) {
				u32 v00 = *(s0++); u32 v10 = *(s1++); u32 v20 = *(s2++); u32 v30 = *(s3++);
				u32 v01 = *(s0++); u32 v11 = *(s1++); u32 v21 = *(s2++); u32 v31 = *(s3++);
				u32 t0 = v00; u32 t1 = v10; u32 t2 = v20; u32 t3 = v30;

				v00 = (v00 & ~0xFFFF) | ((v01 >> 16) & 0xFFFF); v01 = (v01 & 0xFFFF) | ((t0 << 16) & ~0xFFFF);
				v10 = (v10 & ~0xFFFF) | ((v11 >> 16) & 0xFFFF); v11 = (v11 & 0xFFFF) | ((t1 << 16) & ~0xFFFF);
				v20 = (v20 & ~0xFFFF) | ((v21 >> 16) & 0xFFFF); v21 = (v21 & 0xFFFF) | ((t2 << 16) & ~0xFFFF);
				v30 = (v30 & ~0xFFFF) | ((v31 >> 16) & 0xFFFF); v31 = (v31 & 0xFFFF) | ((t3 << 16) & ~0xFFFF);
				*(d0++) = v00; *(d_r0++) = v01;
				*(d1++) = v10; *(d_r1++) = v11;
				*(d2++) = v20; *(d_r2++) = v21;
				*(d3++) = v30; *(d_r3++) = v31;
			}
			//Move to next tile
			d0 += 14; d1 += 14; d2 += 14; d3 += 14;
			d_r0 += 14; d_r1 += 14; d_r2 += 14; d_r3 += 14;
		}
		//Move to next 4 lines
		s0 += src_inc; s1 += src_inc; s2 += src_inc; s3 += src_inc;
	}
#else
	u32 x_inc;
	u32 *s0 = src + ((*(scr_data++) >> 8) * src_w + (*(scr_data++) >> 8));
	x_inc = *(scr_data++);
	u32 *s1 = src + ((*(scr_data++) >> 8) * src_w + (*(scr_data++) >> 8));
	x_inc = *(scr_data++);
	u32 *s2 = src + ((*(scr_data++) >> 8) * src_w + (*(scr_data++) >> 8));
	x_inc = *(scr_data++);
	u32 *s3 = src + ((*(scr_data++) >> 8) * src_w + (*(scr_data++) >> 8));
	x_inc = *(scr_data++);

	u32 src_inc = (src_w - dst_w) + (src_w * 3);

	for (u32 i = 0; i < dst_h; i+=4) {
		for (u32 k = 0; k < dst_w; k+= 4) {
			// one tile
			for (u32 j = 0; j < 2; j++) {
				u32 v00 = *(s0++); u32 v10 = *(s1++); u32 v20 = *(s2++); u32 v30 = *(s3++);
				u32 v01 = *(s0++); u32 v11 = *(s1++); u32 v21 = *(s2++); u32 v31 = *(s3++);
				u32 t0 = v00; u32 t1 = v10; u32 t2 = v20; u32 t3 = v30;

				v00 = (v00 & ~0xFFFF) | ((v01 >> 16) & 0xFFFF); v01 = (v01 & 0xFFFF) | ((t0 << 16) & ~0xFFFF);
				v10 = (v10 & ~0xFFFF) | ((v11 >> 16) & 0xFFFF); v11 = (v11 & 0xFFFF) | ((t1 << 16) & ~0xFFFF);
				v20 = (v20 & ~0xFFFF) | ((v21 >> 16) & 0xFFFF); v21 = (v21 & 0xFFFF) | ((t2 << 16) & ~0xFFFF);
				v30 = (v30 & ~0xFFFF) | ((v31 >> 16) & 0xFFFF); v31 = (v31 & 0xFFFF) | ((t3 << 16) & ~0xFFFF);
				*(d0++) = v00; *(d_r0++) = v01;
				*(d1++) = v10; *(d_r1++) = v11;
				*(d2++) = v20; *(d_r2++) = v21;
				*(d3++) = v30; *(d_r3++) = v31;
			}
			//Move to next tile
			d0 += 14; d1 += 14; d2 += 14; d3 += 14;
			d_r0 += 14; d_r1 += 14; d_r2 += 14; d_r3 += 14;
		}
		//Move to next 4 lines
		//s0 = src + ((*(scr_data++) >> 8) * src_w + (*(scr_data++) >> 8));
		//x_inc = *(scr_data++);
		//s1 = src + ((*(scr_data++) >> 8) * src_w + (*(scr_data++) >> 8));
		//x_inc = *(scr_data++);
		//s2 = src + ((*(scr_data++) >> 8) * src_w + (*(scr_data++) >> 8));
		//x_inc = *(scr_data++);
		//s3 = src + ((*(scr_data++) >> 8) * src_w + (*(scr_data++) >> 8));
		//x_inc = *(scr_data++);
	}
#endif
	//Flush cache
	DCFlushRange(dst, dst_w * dst_h * 4);
}


extern u32 *tlut_data;

void SGX_Vdp2Init(void)
{
	//Set initial matrix
	guMtxIdentity(vdp2mtx);
	GX_LoadPosMtxImm(vdp2mtx, MTX_VDP2_POS_BG);
	prop_tex = (u8*) memalign(32, 704*512); //TODO: This is accounted for in 1st image so remove
	win_tex = (u8*) memalign(32, 704*256); //TODO: This should be half the size
	//Sreens (704*256 max) [4 SCROLL]
	//for each screen: ARGB1555 (2 bytes) & ZA [1/2 byte] (only when per pixel Z or when per pixel A)
	screen_tex = (u8*) memalign(32, 704*240*4*3);
	//Images (704*512 max) [1st image, 2nd image]
	//1st Image: ARGB1555 (2 bytes) + Z (1 byte)
	//2nd Image: ARGB1555 (2 bytes) + A (1 byte, can be from 1st image or 2nd image)
	image_tex = (u8*) memalign(32, 704*512*6);
}

static void __Vdp2CopyBackground(u32 bg_id)
{
	switch(bg_id) {
		case 0: {	//Normal BG 0
			GX_SetTexCopySrc(0, 0, vdp2_disp_w, 256);
			GX_SetTexCopyDst(vdp2_disp_w, 256, GX_TF_RGB5A3, GX_FALSE);
			GX_CopyTex(screen_tex + (704*256*2*0), GX_TRUE);
		} break;
		case 1: {	//Normal BG 1
			GX_SetTexCopySrc(0, 0, vdp2_disp_w, 256);
			GX_SetTexCopyDst(vdp2_disp_w, 256, GX_TF_RGB5A3, GX_FALSE);
			GX_CopyTex(screen_tex + (704*256*2*1), GX_TRUE);
		} break;
		case 2: {	//Normal BG 2
			GX_SetTexCopySrc(0, 0, vdp2_disp_w, 256);
			GX_SetTexCopyDst(vdp2_disp_w, 256, GX_TF_RGB5A3, GX_FALSE);
			GX_CopyTex(screen_tex + (704*256*2*2), GX_TRUE);
		} break;
		case 3: {	//Normal BG 3
			GX_SetTexCopySrc(0, 0, vdp2_disp_w, 256);
			GX_SetTexCopyDst(vdp2_disp_w, 256, GX_TF_RGB5A3, GX_FALSE);
			GX_CopyTex(screen_tex + (704*256*2*3), GX_TRUE);
		} break;
	}
	//GX_PixModeSync(); //Not necesary?
}


// Lower z-buffer has priority and screen information as follows:
// pix = [pri : 4][screen_id : 4]
// This means we can use a TLUT to generate
// the information of what pixels are affected with color offset and shadows
void __Vdp2DrawColorOffset(void)
{
	if (!(Vdp2Regs->CLOFEN & 0x7F)) {
		return;
	}

	//Copy the lower 8bit Z-buffer that has information on 8
	u32 w = MIN(vdp2_disp_w, 640);
	u32 h = vdp2_disp_h;

	GX_SetTexCopySrc(0, 0, w, h);
	GX_SetTexCopyDst(w, h, GX_CTF_Z8L, GX_FALSE);
	GX_CopyTex(prop_tex, GX_FALSE);

	//Generate the TLUT CLOFSL
	color_ofs_tlut[PRI_NGB1] = ((Vdp2Regs->CLOFEN & 0x02) << 8) | ((-((Vdp2Regs->CLOFSL >> 1) & 1)) & 0xFF);
	color_ofs_tlut[PRI_NGB0] = ((Vdp2Regs->CLOFEN & 0x01) << 8) | ((-((Vdp2Regs->CLOFSL >> 0) & 1)) & 0xFF);
	color_ofs_tlut[PRI_NGB2] = ((Vdp2Regs->CLOFEN & 0x04) << 8) | ((-((Vdp2Regs->CLOFSL >> 2) & 1)) & 0xFF);
	color_ofs_tlut[PRI_NGB3] = ((Vdp2Regs->CLOFEN & 0x08) << 8) | ((-((Vdp2Regs->CLOFSL >> 3) & 1)) & 0xFF);
	color_ofs_tlut[PRI_RGB0] = ((Vdp2Regs->CLOFEN & 0x10) << 8) | ((-((Vdp2Regs->CLOFSL >> 4) & 1)) & 0xFF);
	color_ofs_tlut[0]        = ((Vdp2Regs->CLOFEN & 0x20) << 8) | ((-((Vdp2Regs->CLOFSL >> 5) & 1)) & 0xFF);
	color_ofs_tlut[PRI_SPR]  = ((Vdp2Regs->CLOFEN & 0x40) << 8) | ((-((Vdp2Regs->CLOFSL >> 6) & 1)) & 0xFF);
	SGX_LoadTlut(color_ofs_tlut, TLUT_SIZE_16 | TLUT_INDX_CLROFS);

	//set the color offsets (CO_A -> KONST and CO_B -> RAS0)
	u32 co_a = ((Vdp2Regs->COAR & 0xFF) << 24) | ((Vdp2Regs->COAG & 0xFF) << 16) | ((Vdp2Regs->COAB & 0xFF) << 8);
	u32 co_b = ((Vdp2Regs->COBR & 0xFF) << 24) | ((Vdp2Regs->COBG & 0xFF) << 16) | ((Vdp2Regs->COBB & 0xFF) << 8);
	u32 mask_a = (((-(Vdp2Regs->COAR & 0x100)) & 0xFF00) << 16) | (((-(Vdp2Regs->COAG & 0x100)) & 0xFF00) << 8) | ((-(Vdp2Regs->COAB & 0x100)) & 0xFF00);
	u32 mask_b = (((-(Vdp2Regs->COBR & 0x100)) & 0xFF00) << 16) | (((-(Vdp2Regs->COBG & 0x100)) & 0xFF00) << 8) | ((-(Vdp2Regs->COBB & 0x100)) & 0xFF00);
	u32 ras_a = (co_a & ~mask_a);
	u32 invras_a = (~co_a & mask_a);
	u32 ras_b = (co_b & ~mask_b);
	u32 invras_b = (~co_b & mask_b);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	//Set up general TEV
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(1);
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);

	//TEXMAP6 is for VDP2 scaling
	SGX_SetOtherTex(GX_TEXMAP5, prop_tex, GX_TF_CI8, w, h, TLUT_FMT_IA8 | TLUT_INDX_CLROFS);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_POS, MTX_IDENTITY);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP5, GX_COLOR0A0);

	//GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetZMode(GX_DISABLE, GX_GREATER, GX_TRUE);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	//GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_KONST, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_KONST, GX_CC_RASC, GX_CC_TEXC, GX_CC_ZERO);
	//GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ADDHALF, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
	GX_SetCurrentMtx(MTX_IDENTITY);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);

	GX_PixModeSync(); //Not necesary?
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) (&ras_a)));
	GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT1, 4);
		GX_Position2s16(0, 0); GX_Color1u32(ras_b);
		GX_Position2s16(w, 0); GX_Color1u32(ras_b);
		GX_Position2s16(0, h); GX_Color1u32(ras_b);
		GX_Position2s16(w, h); GX_Color1u32(ras_b);
	GX_End();

	GX_SetBlendMode(GX_BM_SUBTRACT, GX_BL_ONE, GX_BL_ONE, GX_LO_CLEAR);
	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) (&invras_a)));
	GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT1, 4);
		GX_Position2s16(0, 0); GX_Color1u32(invras_b);
		GX_Position2s16(w, 0); GX_Color1u32(invras_b);
		GX_Position2s16(0, h); GX_Color1u32(invras_b);
		GX_Position2s16(w, h); GX_Color1u32(invras_b);
	GX_End();

}

static void __Vdp2SetPatternData(void)
{
	u32 supp_data = cell.ptrn_supp & 0x8000;
	//One word pattern data
	if (supp_data) {
		// Get ColorCalc and Pri bits
		supp_data |= (cell.ptrn_supp & 0x0300) << 20;
		//Auxiliary mode defines flip function
		if (!(cell.ptrn_supp & 0x4000)) {
			cell.ptrn_flip_shft = 10;
			//Char size mode sets character number
			if (cell.char_size == 8) {
				supp_data |= (cell.ptrn_supp & 0x1F) << 10;
				cell.ptrn_chr_shft = 0;
				cell.ptrn_mask = 0x03FF;
			} else {
				supp_data |= (cell.ptrn_supp & 0x1C) << 10;
				supp_data |= (cell.ptrn_supp & 0x3);
				cell.ptrn_chr_shft = 2;
				cell.ptrn_mask = 0x0FFC;
			}
		} else {
			cell.ptrn_flip_shft = 30;
			//Char size mode sets character number
			if (cell.char_size == 8) {
				supp_data |= (cell.ptrn_supp & 0x1C) << 10;
				cell.ptrn_chr_shft = 0;
				cell.ptrn_mask = 0x0FFF;
			} else {
				supp_data |= (cell.ptrn_supp & 0x10) << 10;
				supp_data |= (cell.ptrn_supp & 0x3);
				cell.ptrn_chr_shft = 2;
				cell.ptrn_mask = 0x3FFC;
			}
		}
		//Color format defines palette
		if (cell.color_fmt) {	//non 16 color count
			cell.ptrn_pal_shft = 8;
			cell.ptrn_mask |= 0x700000;
		} else {	//16 color count
			supp_data |= (cell.ptrn_supp & 0xE0) << 15;
			cell.ptrn_pal_shft = 4;
			cell.ptrn_mask |= 0x7F0000;
		}
	} else {
		cell.ptrn_mask = 0x00003FFF; 	//14th bit is not used because of 512K of VRAM
		cell.ptrn_flip_shft = 30;
		cell.ptrn_pal_shft = 0;
		cell.ptrn_chr_shft = 0;

		if (cell.color_fmt) {	//non 16 color count
			cell.ptrn_mask |= 0x700000;
		} else {	//16 color count
			cell.ptrn_mask |= 0x7F0000;
		}
	}
	//If special priority mode 0 then mask off pri flag in pattern
	if (cell.spec_pri) {
		cell.ptrn_mask |= 0x20000000;
		cell.pri &= ~0x10;	//LSB is turned off
	}
	//TODO: add color ram offset (should TEST)
	u32 pal_ofs = (supp_data + (cell.cram_offset << 16)) & 0x700000;
	supp_data = (supp_data & ~0x700000) | pal_ofs;
	cell.ptrn_supp = supp_data;

	cell.page_mask = (0x20 << (cell.char_size == 8)) - 1;
	cell.page_shft = 5 + (cell.char_size == 8);
	cell.xmap_shft = cell.page_shft + (cell.plane_mask & 1);
	cell.ymap_shft = cell.page_shft + (cell.plane_mask >>  1) - 1;
	cell.plane_mask = (cell.plane_mask << (cell.page_shft << 1)) | ((1 << (cell.page_shft << 1)) - 1);
}

static void __Vdp2ReadNBG(u32 bg_id)
{
	u32 map_offset = 0;
	switch(bg_id) {
		case 0: {	//Normal BG 0
			cell.disp_ctl = Vdp2Regs->BGON;
			cell.char_ctl = Vdp2Regs->CHCTLA & 0x7F;
			cell.ptrn_supp = Vdp2Regs->PNCN0;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 0) & 0x3;
			cell.spec_pri = (Vdp2Regs->SFPRMD >> 0) & 0x3;
			cell.xscroll = (((u32)Vdp2Regs->SCXIN0) << 8) | (((u32)Vdp2Regs->SCXDN0) >> 8);
			cell.yscroll = (((u32)Vdp2Regs->SCYIN0) << 8) | (((u32)Vdp2Regs->SCYDN0) >> 8);
			cell.xinc = ((u32)Vdp2Regs->ZMXN0.all) >> 8;
			cell.yinc = ((u32)Vdp2Regs->ZMYN0.all) >> 8;
			cell.cram_offset = (Vdp2Regs->CRAOFA << 4) & 0x70;
			u32 map_shft = 11 + ((((cell.char_ctl << 1) & 2) | ((cell.ptrn_supp >> 15) & 1)) ^ 3);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 0) & 0x7) << 6;
			cell.pri = ((Vdp2Regs->PRINA & 0x7) << 4) | PRI_NGB0;
			cell.map_addr[0] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN0 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[1] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN0 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[2] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN0 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[3] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN0 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.scr_ctl = Vdp2Regs->SCRCTL;
			u32 *scroll_data = (u32*) (Vdp2Ram + ((Vdp2Regs->LSTA0.all) << 1));
			__genScrollTable(scroll_data);
			//TODO: add RBG1
		} break;
		case 1: {	//Normal BG 1
			cell.disp_ctl = Vdp2Regs->BGON >> 1;
			cell.char_ctl = (Vdp2Regs->CHCTLA >> 8) & 0x3F;
			cell.ptrn_supp = Vdp2Regs->PNCN1;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 2) & 0x3;
			cell.spec_pri = (Vdp2Regs->SFPRMD >> 2) & 0x3;
			cell.xscroll = (((u32)Vdp2Regs->SCXIN1) << 8) | (((u32)Vdp2Regs->SCXDN1) >> 8);
			cell.yscroll = (((u32)Vdp2Regs->SCYIN1) << 8) | (((u32)Vdp2Regs->SCYDN1) >> 8);
			cell.xinc = ((u32)Vdp2Regs->ZMXN1.all) >> 8;
			cell.yinc = ((u32)Vdp2Regs->ZMYN1.all) >> 8;
			cell.cram_offset = (Vdp2Regs->CRAOFA) & 0x70;
			u32 map_shft = 11 + ((((cell.char_ctl << 1) & 2) | ((cell.ptrn_supp >> 15) & 1)) ^ 3);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 4) & 0x7) << 6;
			cell.pri = (((Vdp2Regs->PRINA >> 8) & 0x7) << 4) | PRI_NGB1;
			cell.map_addr[0] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN1 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[1] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN1 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[2] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN1 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[3] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN1 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.scr_ctl = Vdp2Regs->SCRCTL >> 8;
			u32 *scroll_data = (u32*) (Vdp2Ram + ((Vdp2Regs->LSTA1.all) << 1));
			__genScrollTable(scroll_data);
		} break;
		case 2: {	//Normal BG 2
			cell.disp_ctl = Vdp2Regs->BGON >> 2;
			cell.char_ctl = (Vdp2Regs->CHCTLB & 0x1) | ((Vdp2Regs->CHCTLB << 3) & 0x10);
			cell.ptrn_supp = Vdp2Regs->PNCN2;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 4) & 0x3;
			cell.spec_pri = (Vdp2Regs->SFPRMD >> 4) & 0x3;
			cell.xscroll = (((u32)Vdp2Regs->SCXN2) << 8);
			cell.yscroll = (((u32)Vdp2Regs->SCYN2) << 8);
			cell.cram_offset = (Vdp2Regs->CRAOFA >> 4) & 0x70;
			u32 map_shft = 11 + ((((cell.char_ctl << 1) & 2) | ((cell.ptrn_supp >> 15) & 1)) ^ 3);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 8) & 0x7) << 6;
			cell.pri = ((Vdp2Regs->PRINB & 0x7) << 4) | PRI_NGB2;
			cell.map_addr[0] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN2 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[1] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN2 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[2] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN2 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[3] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN2 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
		} break;
		case 3: {	//Normal BG 3
			cell.disp_ctl = Vdp2Regs->BGON >> 3;
			cell.char_ctl = ((Vdp2Regs->CHCTLB >> 4) & 0x1) | ((Vdp2Regs->CHCTLB >> 1) & 0x10);
			cell.ptrn_supp = Vdp2Regs->PNCN3;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 6) & 0x3;
			cell.spec_pri = (Vdp2Regs->SFPRMD >> 6) & 0x3;
			cell.xscroll = (((u32)Vdp2Regs->SCXN3) << 8);
			cell.yscroll = (((u32)Vdp2Regs->SCYN3) << 8);
			cell.cram_offset = (Vdp2Regs->CRAOFA >> 8) & 0x70;
			u32 map_shft = 11 + ((((cell.char_ctl << 1) & 2) | ((cell.ptrn_supp >> 15) & 1)) ^ 3);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 12) & 0x7) << 6;
			cell.pri = (((Vdp2Regs->PRINB >> 8) & 0x7) << 4) | PRI_NGB3;
			cell.map_addr[0] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN3 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[1] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN3 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[2] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN3 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[3] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN3 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
		} break;
		case 4: {	//Rotation BG
			cell.disp_ctl = Vdp2Regs->BGON >> 4;
			cell.char_ctl = (Vdp2Regs->CHCTLB >> 8) & 0x7F;
			cell.ptrn_supp = Vdp2Regs->PNCR;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 8) & 0x3; // This is per, Rotation parameter, only A is used, for B: (Vdp2Regs->PLSZ >> 12) & 0x3
			cell.spec_pri = (Vdp2Regs->SFPRMD >> 8) & 0x3;
			//TODO: these are set elsewhere
			cell.xscroll = 0; cell.xinc = 0;
			cell.yscroll = 0; cell.xinc = 0;
			cell.cram_offset = (Vdp2Regs->CRAOFA << 4) & 0x70;
			u32 map_shft = 11 + ((((cell.char_ctl << 1) & 2) | ((cell.ptrn_supp >> 15) & 1)) ^ 3);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFR) >> 0) & 0x7) << 6; // This is per, Rotation parameter, only A is used, for B: ((((u32)Vdp2Regs->MPOFR) >> 4) & 0x7) << 6
			cell.pri = (((Vdp2Regs->PRIR >> 0) & 0x7) << 4) | PRI_RGB0;
			cell.map_addr[0]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABRA >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[1]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABRA >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[2]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDRA >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[3]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDRA >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[4]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPEFRA >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[5]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPEFRA >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[6]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPGHRA >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[7]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPGHRA >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[8]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPIJRA >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[9]  = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPIJRA >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[10] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPKLRA >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[11] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPKLRA >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[12] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPMNRA >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[13] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPMNRA >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[14] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPOPRA >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			cell.map_addr[15] = (u32*) (Vdp2Ram + (((map_offset | (((Vdp2Regs->MPOPRA >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00));
			//TODO: RBG are not drawn yet
		} break;
	}

	cell.color_fmt = (cell.char_ctl >> 4) & 0x7;
	if (!(cell.char_ctl & 0x2)) { //Is in Cell format
		cell.char_size = ((cell.char_ctl << 3) & 0x8) + 8;
		__Vdp2SetPatternData();
	} else { // Is in Bitmap format
		u32 bm_palreg = 0;
		switch(bg_id) {
			case 0: {	//Normal BG 0
				bm_palreg = Vdp2Regs->BMPNA;
			} break;
			case 1: {	//Normal BG 1
				bm_palreg = Vdp2Regs->BMPNA >> 8;
			} break;
			case 4: {	//Rotation BG
				bm_palreg = Vdp2Regs->BMPNB;
			} break;
		}
		cell.ptrn_supp = (bm_palreg & 0x7) << 8;
		cell.map_addr[0] = (u32*) (Vdp2Ram + ((map_offset << 11) & 0x7FF00));
		//TODO: Get PRI and CC bits
	}
}

static void SGX_Vdp2DrawCell(void)
{

}

static void SGX_Vdp2DrawBitmap(void)
{
	GX_SetCurrentMtx(MTX_IDENTITY);

	//TODO: add scaling and screen dims to the format

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);
	u32 scroll_x = cell.xscroll;
	u32 scroll_y = cell.yscroll;
	//GX_SetPointSize(cell.char_size * 6, GX_TO_ONE);
	u32 bm_mode = 0;
	u32 bm_wdith = 512 << bm_mode;
	u32 bm_height = 256;
	switch (cell.color_fmt) {
		case 0:{ // 4bpp
			SGX_BeginVdp2Bitmap(GX_TF_CI4, bm_wdith, bm_height);
			SGX_BitmapConverterSet(bm_mode, SPRITE_4BPP);
			u8 *cram_tlut = (cell.disp_ctl & 0x100 ? cram_11bpp : cram_4bpp);
			SGX_LoadTlut(cram_tlut, TLUT_SIZE_2K | TLUT_INDX_CRAM0);
		} break;
		case 1: { // 8bpp
			SGX_BeginVdp2Bitmap(GX_TF_CI8, bm_wdith, bm_height);
			SGX_BitmapConverterSet(bm_mode, SPRITE_8BPP);
			u8 *cram_tlut = (cell.disp_ctl & 0x100 ? cram_11bpp : cram_8bpp);
			SGX_LoadTlut(cram_tlut, TLUT_SIZE_2K | TLUT_INDX_CRAM0);
		} break;
		case 2: { // 16bpp (11 bits used)
			SGX_BeginVdp2Bitmap(GX_TF_CI14, bm_wdith, bm_height);
			SGX_BitmapConverterSet(bm_mode, SPRITE_16BPP);
			SGX_LoadTlut(cram_11bpp, TLUT_SIZE_2K | TLUT_INDX_CRAM0);
		} break;
		case 3: { // 16bpp (RGBA)
			SGX_BeginVdp2Bitmap(GX_TF_RGB5A3, bm_wdith, bm_height);
			SGX_BitmapConverterSet(bm_mode, SPRITE_16BPP);
		} break;
		case 4: { // 32bpp (RGBA)
			//Does nothing
			SGX_BeginVdp2Bitmap(GX_TF_RGBA8, vdp2_disp_w, vdp2_disp_h);
		} break;
	}

	//TODO: Set the correct palette values and texture address
	u32 pal = ((cell.ptrn_supp) >> 16) & 0x7F;
	u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(cell.map_addr[0]) >> 5);
	u32 tlut_addr = 0x98000000 | TLUT_FMT_RGB5A3 | TLUT_INDX_CRAM0 | pal;
	//Set texture addr, tlut and Z offset from priority value
	GX_LOAD_BP_REG(tex_maddr);
	GX_LOAD_BP_REG(tlut_addr);
	//GX_LOAD_XF_REGS(0x101C, 1); //Set the Viewport Z
	//wgPipe->F32 = (f32) (16777216 - (cell.pri)); //;| ((prcc >> 25) & 0x10)));
	//GX_LOAD_XF_REGS(0x1025, 1); //Set the Projection registers
	//wgPipe->F32 = - (0.00000006f * (cell.pri | ((prcc >> 25) & 0x10)));
	if (cell.color_fmt != 4) {
		u32 scroll_w = scroll_x + vdp2_disp_w;
		u32 scroll_h = scroll_y + vdp2_disp_h;
		GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT1, 4);
			GX_Position2s16(          0,           0); GX_TexCoord2u16(scroll_x, scroll_y);
			GX_Position2s16(vdp2_disp_w,           0); GX_TexCoord2u16(scroll_w, scroll_y);
			GX_Position2s16(          0, vdp2_disp_h); GX_TexCoord2u16(scroll_x, scroll_h);
			GX_Position2s16(vdp2_disp_w, vdp2_disp_h); GX_TexCoord2u16(scroll_w, scroll_h);
		GX_End();
	} else { // 32bpp bitmap
		u32 bm_offset = (scroll_y * bm_wdith) + scroll_x;
		//Convert rtexture
		//__convert32bpp((u32*)screen_tex,(u32*)cell.map_addr[0], vdp2_disp_w, vdp2_disp_h, bm_wdith, 0);
		//tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(screen_tex) >> 5);
		//GX_LOAD_BP_REG(tex_maddr);

		GX_SetNumIndStages(0);
		GX_SetTevDirect(GX_TEVSTAGE0);

		//First pass, create the texture
		GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT1, 4);
			GX_Position2s16(          0,           0); GX_TexCoord2u16(          0,           0);
			GX_Position2s16(vdp2_disp_w,           0); GX_TexCoord2u16(vdp2_disp_w,           0);
			GX_Position2s16(          0, vdp2_disp_h); GX_TexCoord2u16(          0, vdp2_disp_h);
			GX_Position2s16(vdp2_disp_w, vdp2_disp_h); GX_TexCoord2u16(vdp2_disp_w, vdp2_disp_h);
		GX_End();
	}
}

/*Draws a simple Cell background:
 * All char color count. []
 * All char sizes []
 * All Ptrn Name data size [x]
 * All plane sizes [x]
 * 4 plane count [x]
 * No arbitrary scale function
 * Mosaic function []
 * No line scroll
 * No vertical cell scroll function
 */
#define USE_POINTS 1

static void SGX_Vdp2DrawCellSimple(void)
{
	guMtxIdentity(vdp2mtx);
	u32 char_ofs_mask = (cell.char_size << 8) - 1;
	//TODO: When scaling we must leave only integer values if we dont
	//want to worry with artifacts
	vdp2mtx[0][0] = vdp2mtx[1][1] = cell.char_size;
	vdp2mtx[2][2] = -1.0f;
	vdp2mtx[0][3] = -(((f32)((cell.xscroll & char_ofs_mask) >> 8))) + (cell.char_size >> 1);
	vdp2mtx[1][3] = -(((f32)((cell.yscroll & char_ofs_mask) >> 8))) + (cell.char_size >> 1);
	GX_LoadPosMtxImm(vdp2mtx, MTX_VDP2_POS_BG);
	GX_SetCurrentMtx(MTX_VDP2_POS_BG);

	//TODO: add scaling and screen dims to the format

	u32 x_tile = ((cell.xscroll >> 8) / cell.char_size);
	u32 y_tile = ((cell.yscroll >> 8) / cell.char_size);
	u32 x_max = (vdp2_disp_w / cell.char_size) + 1;
	u32 y_max = (vdp2_disp_h / cell.char_size) + 1;

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, cell.char_size, cell.char_size);
#if USE_POINTS
	GX_SetPointSize(cell.char_size * 6, GX_TO_ONE);
#endif

	switch (cell.color_fmt) {
		case 0:{ // 4bpp
			SGX_BeginVdp2Scroll(GX_TF_CI4, cell.char_size);
			GX_SetNumIndStages(0);
			GX_SetTevDirect(GX_TEVSTAGE0);
			u8 *cram_tlut = (cell.disp_ctl & 0x100 ? cram_11bpp : cram_4bpp);
			SGX_LoadTlut(cram_tlut, TLUT_SIZE_2K | TLUT_INDX_CRAM0);
		} break;
		case 1: { // 8bpp
			SGX_BeginVdp2Scroll(GX_TF_CI8, cell.char_size);
			SGX_CellConverterSet(cell.char_size >> 4, SPRITE_8BPP);
			u8 *cram_tlut = (cell.disp_ctl & 0x100 ? cram_11bpp : cram_8bpp);
			SGX_LoadTlut(cram_tlut, TLUT_SIZE_2K | TLUT_INDX_CRAM0);
		} break;
		case 2: { // 16bpp (11 bits used)
			SGX_BeginVdp2Scroll(GX_TF_CI14, cell.char_size);
			SGX_CellConverterSet(cell.char_size >> 4, SPRITE_16BPP);
			SGX_LoadTlut(cram_11bpp, TLUT_SIZE_2K | TLUT_INDX_CRAM0);
		} break;
		case 3: { // 16bpp (RGBA)
			SGX_CellConverterSet(cell.char_size >> 4, SPRITE_16BPP);
			SGX_BeginVdp2Scroll(GX_TF_RGB5A3, cell.char_size);
		} break;
		case 4: { // 32bpp (RGBA)
			SGX_BeginVdp2Scroll(GX_TF_RGBA8, cell.char_size);
		} break;
	}

	for (u32 j = 0; j < y_max; ++j) {
		u32 y = j + y_tile;
		u32 yaddr = ((y & cell.page_mask) | ((y & (cell.page_mask+1)) << 1)) << cell.page_shft;
		u32 ymap = ((y >> cell.ymap_shft) & 2);
		for (u32 i = 0; i < x_max; ++i) {
			u32 x = i + x_tile;
			//Get pattern data
			u32 xaddr = ((x & cell.page_mask) | ((x & (cell.page_mask+1)) << cell.page_shft));
			u32 addr = (yaddr | xaddr) & cell.plane_mask;
			u32 map = ymap | ((x >> cell.xmap_shft) & 1);

#if 1
			u32 bpp = (cell.ptrn_supp >> 15) & 1;
			//u32 ptrn = *((u32*) (cell.map_addr[map] + ((addr & ~bpp) << 2))) >> ((bpp & ~addr) << 4);
			u32 ptrn = cell.map_addr[map][addr & ~bpp] >> ((bpp & ~addr) << 4);
#else
			u32 ptrn;
			if (cell.ptrn_supp & 0x8000) {
				ptrn = *((u16*) (cell.map_addr[map] + (addr << 1)));
			} else {
				ptrn = *((u32*) (cell.map_addr[map] + (addr << 2)));
			}
#endif
			u32 flip = (ptrn >> cell.ptrn_flip_shft) & 0x3;
			flip = ((flip << 8) & 0x100) | (flip >> 1);
			u32 prcc = (ptrn + cell.ptrn_supp) & cell.ptrn_mask;
			u32 pal = ((((ptrn << cell.ptrn_pal_shft) & cell.ptrn_mask) + cell.ptrn_supp) >> 16) & 0x7F;
			u32 chr = ((((ptrn << cell.ptrn_chr_shft) & cell.ptrn_mask) + cell.ptrn_supp) << 5) & 0x7FFE0;
			u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(Vdp2Ram + chr) >> 5);
			u32 tlut_addr = 0x98000000 | TLUT_FMT_RGB5A3 | TLUT_INDX_CRAM0 | pal;
			//u32 vert = (((i & 0xFF) << 24) | ((j & 0xFF) << 16));
			u32 vert = (((i & 0xFF) << 8) | ((j & 0xFF) << 0));

			//Set texture addr, tlut and Z offset from priority value
			GX_LOAD_BP_REG(tex_maddr);
			GX_LOAD_BP_REG(tlut_addr);
			//GX_LOAD_XF_REGS(0x101C, 1); //Set the Viewport Z
			//wgPipe->F32 = (f32) (16777216 - (cell.pri | ((prcc >> 25) & 0x10)));
			u32 pri = cell.pri | ((prcc >> 25) & 0x10);
			u32 val = (((chr >> 5) & 0x7F) << 8) | ((chr >> (5+8)) & 0x7F);
#if	USE_POINTS
			// X:8 Y:8 Z:8 T:16
			GX_Begin(GX_POINTS, GX_VTXFMT5, 1);
				wgPipe->U16 = vert;
				wgPipe->U8  = pri;
				wgPipe->U16 = flip;
			GX_End();
#else
			GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT5, 4);
			//GX_Begin(GX_POINTS, GX_VTXFMT5, 1);
			//wgPipe->U8 = GX_QUADS | GX_VTXFMT5;
			//wgPipe->U16 = 4;
				wgPipe->U32 = vert + flip;
				wgPipe->U32 = vert + (0x01000100 ^ flip);
				wgPipe->U32 = vert + (0x00010001 ^ flip);
				wgPipe->U32 = vert + (0x01010101 ^ flip);
			GX_End();
#endif
		}
	}
}

/*
 * Draws a complex Cell background that adds LineScroll and Vertical cell scroll
 */
static void SGX_Vdp2DrawCellLineScroll(void)
{
	guMtxIdentity(vdp2mtx);
	//guMtxScale(vdp2mtx, cell.char_size, cell.char_size, 0.0f);
	//u32 char_ofs_mask = (cell.char_size << 8) - 1;
	//TODO: When scaling we must leave only integer values if we dont
	//want to worry with artifacts
	//vdp2mtx[0][3] = -(((f32)((cell.xscroll & char_ofs_mask) >> 8))) + 4;
	//vdp2mtx[1][3] = -(((f32)((cell.yscroll & char_ofs_mask) >> 8))) + 4;
	GX_LoadPosMtxImm(vdp2mtx, MTX_VDP2_POS_BG);
	GX_SetCurrentMtx(MTX_VDP2_POS_BG);

	//TODO: add scaling and screen dims to the format

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);
	//only 1 pixel line (test)
	GX_SetLineWidth(6, GX_TO_ONE);

	switch (cell.color_fmt) {
		case 0:{ // 4bpp
			SGX_BeginVdp2Scroll(GX_TF_CI4, cell.char_size);
			GX_SetNumIndStages(0);
			GX_SetTevDirect(GX_TEVSTAGE0);
			u8 *cram_tlut = (cell.disp_ctl & 0x100 ? cram_11bpp : cram_4bpp);
			SGX_LoadTlut(cram_tlut, TLUT_SIZE_2K | TLUT_INDX_CRAM0);
		} break;
		case 1: { // 8bpp
			SGX_BeginVdp2Scroll(GX_TF_CI8, cell.char_size);
			SGX_CellConverterSet(cell.char_size >> 4, SPRITE_8BPP);
			u8 *cram_tlut = (cell.disp_ctl & 0x100 ? cram_11bpp : cram_8bpp);
			SGX_LoadTlut(cram_tlut, TLUT_SIZE_2K | TLUT_INDX_CRAM0);
		} break;
		case 2: { // 16bpp (11 bits used)
			SGX_BeginVdp2Scroll(GX_TF_CI14, cell.char_size);
			SGX_CellConverterSet(cell.char_size >> 4, SPRITE_16BPP);
			SGX_LoadTlut(cram_11bpp, TLUT_SIZE_2K | TLUT_INDX_CRAM0);
		} break;
		case 3: { // 16bpp (RGBA)
			SGX_CellConverterSet(cell.char_size >> 4, SPRITE_16BPP);
			SGX_BeginVdp2Scroll(GX_TF_RGB5A3, cell.char_size);
		} break;
		case 4: { // 32bpp (RGBA)
			SGX_BeginVdp2Scroll(GX_TF_RGBA8, cell.char_size);
		} break;
	}


	//TODO: USe char_size 8->3, 16->4
	const u32 char_size = 8 + (3 + (cell.char_size >> 4));
	//TODO: Dont use vdp2_disp_h when rendering to texture, this is for initial test
	u32 *scr_data = line_scroll_data;
	for (u32 j = 0; j < vdp2_disp_h; ++j) {
		u32 x_scroll = *(scr_data++);
		u32 y_scroll = *(scr_data++);
		u32 x_inc = *(scr_data++);
		u32 x_max = (vdp2_disp_w / (u32) x_inc) + 1;
		//TODO: Set the projection matrix for Y and X coord position and scale
		SGX_SetLineScroll(x_scroll, x_inc, j);
		for (u32 i = 0; i < x_max; ++i) {
			u32 y = y_scroll >> char_size;
			u32 x = x_scroll >> char_size;

			//Get pattern data
			u32 yaddr = ((y & cell.page_mask) | ((y & (cell.page_mask+1)) << 1) | (x & (cell.page_mask+1))) << cell.page_shft;
			u32 xaddr = x & cell.page_mask;
			u32 addr = (yaddr | xaddr) & cell.plane_mask;
			u32 map = ((y >> cell.ymap_shft) & 2) | ((x >> cell.xmap_shft) & 1);

#if 0
			u32 bpp = (cell.ptrn_supp >> 15);
			u32 ptrn = *((u32*) (cell.map_addr[map] + (addr << (bpp + 1)))) >> (bpp << 4);
#else
			u32 ptrn;
			if (cell.ptrn_supp & 0x8000) {
				ptrn = *((u16*) (cell.map_addr[map] + (addr << 1)));
			} else {
				ptrn = *((u32*) (cell.map_addr[map] + (addr << 2)));
			}
#endif

			u32 flip = (ptrn >> cell.ptrn_flip_shft) & 0x3;
			flip = ((flip << 8) & 0x100) | (flip >> 1);
			u32 prcc = (ptrn + cell.ptrn_supp) & cell.ptrn_mask;
			u32 pal = ((((ptrn << cell.ptrn_pal_shft) & cell.ptrn_mask) + cell.ptrn_supp) >> 16) & 0x7F;
			u32 chr = ((((ptrn << cell.ptrn_chr_shft) & cell.ptrn_mask) + cell.ptrn_supp) << 5) & 0x7FFE0;
			u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(Vdp2Ram + chr) >> 5);
			u32 tlut_addr = 0x98000000 | TLUT_FMT_RGB5A3 | TLUT_INDX_CRAM0 | pal;
			u32 vert = (i & 0xFF) << 24;

			//Set texture addr, tlut and Z offset from priority value
			GX_LOAD_BP_REG(tex_maddr);
			GX_LOAD_BP_REG(tlut_addr);
			GX_LOAD_XF_REGS(0x101C, 1); //Set the Viewport Z
			wgPipe->F32 = (f32) (16777216 - (cell.pri | ((prcc >> 25) & 0x10)));
			//GX_LOAD_XF_REGS(0x1025, 1); //Set the Projection registers
			//wgPipe->F32 = - (0.00000006f * (cell.pri | ((prcc >> 25) & 0x10)));

			// TODO: Use another type of vertex format
			GX_Begin(GX_LINES, GX_VTXFMT5, 2);
				wgPipe->U32 = vert + flip;
				wgPipe->U32 = vert + (0x01000100 ^ flip);
			GX_End();
		}
	}
	SVI_ResetViewport();
}

static void __SGX_DrawWindow(u32 win_ctl, u16 *win_pos)
{
	//Only use
	//TODO: Fix this stuff for the diferent resolutions
	u32 horizontal_shft = vdp2_disp_w < 400; //disp.highres ^ 1;
	if (win_ctl & 0x80000000) {
		u32 line_addr = (win_ctl & 0x7FFFE) << 1;
		GX_Begin(GX_LINES, GX_VTXFMT1, vdp1_fb_h * 2);
		for (u32 i = 0; i < vdp1_fb_h; ++i) {
			u16 x0 = T1ReadWord(Vdp2Ram, line_addr);
			u16 x1 = T1ReadWord(Vdp2Ram, line_addr + 2);
			line_addr += 4;

			if (x1 == 0xFFFF) {	//Not permitted
				x0 = 0;
				x1 = 0;
			}

			GX_Position2s16(x0 >> horizontal_shft, i + 1);
			GX_Position2s16(x1 >> horizontal_shft, i + 1);
		}
		GX_End();
	} else {
		u16 x0 = win_pos[0] >> horizontal_shft;
		u16 y0 = win_pos[1] + 1;
		u16 x1 = win_pos[2] >> horizontal_shft;
		u16 y1 = win_pos[3] + 1;
		GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT2, 4);
			GX_Position2s16(x0, y0);
			GX_Position2s16(x1, y0);
			GX_Position2s16(x0, y1);
			GX_Position2s16(x1, y1);
		GX_End();
	}
}

static void __SGX_GenWindowTex(void)
{
	//Window bits are OR'ed and we generate a 4bpp texture
	//then by using a TLUT we can obtain transparent pixels on each screen
	const GXColor w0_kc = {0x10, 0x00, 0x00, 0xFF};
	const GXColor w1_kc = {0x20, 0x00, 0x00, 0xFF};
	const GXColor sw_kc = {0x40, 0x00, 0x00, 0xFF};

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_POS, MTX_IDENTITY);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0 | GX_TEXMAP_DISABLE, GX_COLORNULL);
	GX_SetCurrentMtx(MTX_IDENTITY);

	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
	GX_SetBlendMode(GX_BM_LOGIC, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_OR);

	//TODO: check if windows must be made available or not
	//First get mask of drawn screens, then AND with enabled
	//windows for screens, if bit is 0 then we don't need to
	//draw the window
	//Draw Window 0
	GX_SetTevKColor(GX_KCOLOR0, w0_kc);
	__SGX_DrawWindow(Vdp2Regs->LWTA0.all, &Vdp2Regs->WPSX0);

	//Draw Window 1
	GX_SetTevKColor(GX_KCOLOR0, w1_kc);
	__SGX_DrawWindow(Vdp2Regs->LWTA1.all, &Vdp2Regs->WPSX1);

	//Sprite window (Only draw if sprite window is used)
	//Change tev to accept textures
	//TODO: Add this feature
	u32 win_mask = 0xF;
	if (0) {
		GX_SetTevKColor(GX_KCOLOR0, sw_kc);
		GX_SetVtxDesc(GX_VA_TEX0,  GX_DIRECT);
		win_mask = 0x3F;
		//gfx_DrawWindow(40, 40, 200, 100);
	}

	//Copy the red component of FB
	//TODO: Use the vdp2 real height not the vdp1 height
	GX_SetTexCopySrc(0, 0, vdp2_disp_w, vdp2_disp_h);
	GX_SetTexCopyDst(vdp2_disp_w, 256, GX_CTF_R4, GX_FALSE);
	GX_CopyTex(win_tex, GX_TRUE);
	GX_PixModeSync();
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_AND);

	//Generate TLUTs for windows
	u8 wclt[8] = {
		[0] = Vdp2Regs->WCTLD & 0xFF, //Rotation
		[1] = Vdp2Regs->WCTLD >> 8,   //Color Calc
		[PRI_NGB0] = Vdp2Regs->WCTLA & 0xFF,
		[PRI_NGB1] = Vdp2Regs->WCTLA >> 8,
		[PRI_NGB2] = Vdp2Regs->WCTLB & 0xFF,
		[PRI_NGB3] = Vdp2Regs->WCTLB >> 8,
		[PRI_RGB0] = Vdp2Regs->WCTLB & 0xFF,
		[PRI_SPR]  = Vdp2Regs->WCTLB >> 8,
	};
	for (u32 s = 2; s < 8; ++s) {
		u32 op = (-(wclt[s] >> 7)) & 0xFF;
		u32 wctl = wclt[s] & win_mask;
		u32 wbits = win_enable_bits[wctl] ^ op;
		if (wbits == 0xFF) {
			//Do not use window
		} else if (wbits == 0x00) {
			//Do not draw screen
			screen_enable &= ~(1 << s);
		} else {
			//Generate window
			for (u32 i = 0; i < 8; ++i) {
				win_tlut[s][i] = (-(wbits & 1)) << 8;
				wbits >>= 1;
			}
		}
	}

}


void SGX_Vdp2GenCRAM(void) {
	u32 *src = (u32*) Vdp2ColorRam;
	u32 *dst_4bpp = (u32*) cram_4bpp;
	u32 *dst_8bpp = (u32*) cram_8bpp;
	u32 *dst_11bpp = (u32*) cram_11bpp;
	//u32 *dst_msb = cram_msb;
	for (u32 i = 0; i < 2048; i += 2) {
		u32 color = *src | 0x80008000;
		//Mask first color when multiple of 16 or 256
		*dst_4bpp = color & (0xFFFFFFFF >> (!(i & 0xF) << 4));
		*dst_8bpp = color & (0xFFFFFFFF >> (!(i & 0xFF) << 4));
		//*dst_11bpp = color & (0xFFFFFFFF >> (!(i & 0x7FF) << 4));
		*dst_11bpp = color;
		//dst_msb = color & 0xf0
		src++;
		dst_4bpp++;
		dst_8bpp++;
		dst_11bpp++;
		//dst_msb++;
	}

	DCFlushRange(cram_4bpp, sizeof(cram_4bpp));
	DCFlushRange(cram_8bpp, sizeof(cram_8bpp));
	DCFlushRange(cram_11bpp, sizeof(cram_11bpp));
}


//Begins the Vdp1 Drawing Process
void SGX_Vdp2Draw(void)
{
	GX_SetScissor(0, 0, vdp2_disp_w, vdp2_disp_h);
	// Check for enabled screens and create bit mask
	// stores screen bits as

	/*
		1.Draw all scroll screens and store
		2.Draw Top image normally and obtain highest priority
		3.draw all screens again in Order below highest priority
		5. add line screen
		4. draw top image over the result
		Create an array of 48 values,
		each value is a screen in the following repeating order:
		NBG3 -> NBG2 -> NBG1 -> NBG0 -> RBG3 -> SPR
		For each scroll screen if special priority is happening then
		move from one repeating pattern to it's next value, if
		while moving we see another screen tthat must be shown then
	*/
	u32 screen_bits = 0;
	u32 scr_pri = 0;
	u32 scr_enable = 0;
	// If NBG0 16M mode is enabled, don't draw
	// If NBG1 2048/32786 is enabled, don't draw
	scr_pri = (Vdp2Regs->PRINB >> 8) & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x8;
	screen_enable = (!(!(scr_pri) || !(scr_enable) ||
					(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 == 4) ||
					(Vdp2Regs->BGON & 0x2 && (Vdp2Regs->CHCTLA & 0x3000) >> 12 >= 2))) << PRI_NGB3;

	//If NBG0 2048/32786/16M mode is enabled, don't draw
	scr_pri = Vdp2Regs->PRINB & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x4;
	screen_enable |= (!(!(scr_pri) || !(scr_enable) ||
					(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 >= 2))) << PRI_NGB2;

	// If NBG0 16M mode is enabled, don't draw
	scr_pri = (Vdp2Regs->PRINA >> 8) & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x2;
	screen_enable |= (!(!(scr_pri) || !(scr_enable) ||
					(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 == 4))) << PRI_NGB1;

	scr_pri = Vdp2Regs->PRINA & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x1;
	screen_enable |= (!(!(scr_pri) || !(scr_enable))) << PRI_NGB0;

	//Clear with back color
	u32 backcolor_addr = ((u32)(Vdp2Regs->BKTAU & 0x3) << 16) | (u32) Vdp2Regs->BKTAL;
	u32 backcolor = T1ReadWord(Vdp2Ram, (backcolor_addr << 1));
	GX_SetCopyClear((GXColor){backcolor & 0x1F, (backcolor >> 5) & 0x1F, (backcolor >> 10) & 0x1F, 0}, 0);
	__SGX_GenWindowTex();

	//Setup for Vdp2 drawing stuff
	GX_SetCopyClear((GXColor){0, 0, 0, 0}, 0);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	//Set up general TEV
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetZMode(GX_ENABLE, GX_GREATER, GX_TRUE);
	//GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP2, GX_COLORNULL);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP1);
	//SGX_SetTex(output_tex, GX_TF_RGB5A3, 352, 240, 0);
	//SGX_SetOtherTex(GX_TEXMAP2, alpha_tex, GX_TF_IA8, 352, 240, 0);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);


	if (screen_enable & (1 << PRI_NGB3)) {
		__Vdp2ReadNBG(3);	//Draw NBG3
		SGX_Vdp2DrawCellSimple();
	}

	if (screen_enable & (1 << PRI_NGB2)) {
		__Vdp2ReadNBG(2);	//Draw NBG2
		SGX_Vdp2DrawCellSimple();
	}

	if (screen_enable & (1 << PRI_NGB1)) {
		__Vdp2ReadNBG(1);	//Draw NBG1
		if (cell.char_ctl & 0x2) {
			SGX_Vdp2DrawBitmap();
		} else {
			SGX_Vdp2DrawCellSimple();
		}
	}

	if (screen_enable & (1 << PRI_NGB0)) {
		__Vdp2ReadNBG(0);	//Draw NBG0
		if (cell.char_ctl & 0x2) {
			SGX_Vdp2DrawBitmap();
		} else {
			SGX_Vdp2DrawCellSimple();
		}
	}

	SGX_Vdp1DrawFramebuffer();
	//TODO: handle Rotation BGs
	//VDP2 Process will be as follows:
	//First generate a value that flags enabled screens
	//Calculate what screens have opaque pixels and which do not, here we have two cases:
	//1) using Additive blending (advantage, no need to order)
	//	Here we just draw opaque pixels at the same time as transparent pixels (no issues with ordering).
	//	we can do this with the following blending: color = src_pix*1 + dst_pix*invsrc_alpha (where alpha is 1 or 0)
	// if there is a transparent pixel we just use invsrc_alpha = 1 and multiply color with alpha in tev
	//
	//2) using Mixing/blending (advantage no need to draw twice most of the time)
	//	Generate a sort of "display list" that draws in order (back to front), for each transparent
	//	screen we also check if there are possible color calculated pixels
	//	in sprite buffer above, if there are then we redraw with range of priorities above, in the best
	//	case we only draw once (is all opaque or every other screen uses no CC) or twice.
	//
	//	As a first version we can simply draw each screen (would be up to 3 times)
	//	Draw more than once, we can defer special case tiles to display list if the following happens:
	//		1) per pixel CC (must have opaque part and transparent part) (only when using additive transparncy)
	//		2) CC window is used (same as above)	(only when using additive transparncy)
	//		3) per pixel pri that uses CC (exept when using per pixel pri and per pixel cc that uses Special Function Code) (worst case draw twice)

	//Copy z-buffer as priority and apply Color offset and shadow
	__Vdp2DrawColorOffset();
	//Copy the screens to texture...
}




