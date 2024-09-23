
#include "sgx.h"
#include "../vidshared.h"
#include "../vdp1.h"
#include "../vidsoft.h"
#include "../vdp2.h"
#include <malloc.h>

#define GX_LOAD_BP_REG(x)				\
do {								\
	wgPipe->U8 = 0x61;				\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)(x);		\
	asm volatile ("" ::: "memory" ); \
} while(0)

#define GX_LOAD_CP_REG(x, y)			\
do {								\
	wgPipe->U8 = 0x08;				\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U8 = (u8)(x);			\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)(y);		\
	asm volatile ("" ::: "memory" ); \
} while(0)

#define GX_LOAD_XF_REG(x, y)			\
do {								\
	wgPipe->U8 = 0x10;				\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)((x)&0xffff);		\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)(y);		\
	asm volatile ("" ::: "memory" ); \
} while(0)

#define GX_LOAD_XF_REGS(x, n)			\
do {								\
	wgPipe->U8 = 0x10;				\
	asm volatile ("" ::: "memory" ); \
	wgPipe->U32 = (u32)(((((n)&0xffff)-1)<<16)|((x)&0xffff));				\
	asm volatile ("" ::: "memory" ); \
} while(0)


#define SGX_TORGBA32(col) ((((col) & 0x1F) | (((col) & 0x3E0) << 3) | (((col) & 0x7C00) << 6)) << 11)
#define SGX_TORGB565(col) (((col) & 0x1F) | ((col << 1) & 0xFFC0))
#define SGX_FROMRGB565(col) (((col) & 0x1F) | ((col >> 1) & 0x7FE0))



Mtx vdp2mtx ATTRIBUTE_ALIGN(32);

//About 1 Meg of data combined
u8 *bg_tex ATTRIBUTE_ALIGN(32);
u8 *prop_tex ATTRIBUTE_ALIGN(32);


struct CellFormatData {
	u32 char_ctl;	/*Character control settings*/
	u32 color_fmt;	/*Color format*/
	u32 char_size; 	/*Size of char data (8 or 16 pixel)*/
	u32 ptrn_supp;		/*Constant data of pattern (cc.pri, palette and char)*/
	u32 ptrn_chr_shft;	/*Shift of character data in pattern name*/
	u32 ptrn_flip_shft;	/*Shift of flip data in pattern name*/
	u32 ptrn_pal_shft;	/*Shift of palette data in pattern name*/
	u32 ptrn_mask;		/*Mask of character & palette data in pattern name */

	u32 xscroll;			/*X increment*/
	u32 yscroll; 			/*Y increment*/
	u32 page_mask;		/*page mask*/
	u32 page_shft;		/*page shift*/
	u32 plane_mask;		/*plane mask*/
	u32 xmap_shft;		/*map bit shift for x axis*/
	u32 ymap_shft;		/*map bit shift for y axis*/
	u8 *map_addr[16]; 	/* addresses of maps*/
} cell;

extern u32 *tlut_data;
void SGX_Vdp2Init(void)
{
	//Set initial matrix
	guMtxIdentity(vdp2mtx);
	GX_LoadPosMtxImm(vdp2mtx, GXMTX_VDP2);
	bg_tex = (u8*) memalign(32, 704*512*2);
	prop_tex = (u8*) memalign(32, 704*512);
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
			//TODO: add color ram offset
			supp_data |= (cell.ptrn_supp & 0xE0) << 15;
			cell.ptrn_pal_shft = 12;
			cell.ptrn_mask |= 0x7F0000;
		}
	} else {
		cell.ptrn_mask = 0x00003FFF;
		cell.ptrn_flip_shft = 30;
		cell.ptrn_pal_shft = 0;
		cell.ptrn_chr_shft = 0;

		if (cell.color_fmt) {	//non 16 color count
			cell.ptrn_mask |= 0x700000;
		} else {	//16 color count
			//TODO: add color ram offset
			//supp_data |= ;
			cell.ptrn_mask |= 0x7F0000;
		}
	}
	cell.ptrn_supp = supp_data;


	cell.page_mask = (0x20 << (cell.char_size == 8)) - 1;
	cell.page_shft = 5 + (cell.char_size == 8);
	cell.xmap_shft = cell.page_shft + (cell.plane_mask & 1);
	cell.ymap_shft = cell.page_shft + (cell.plane_mask >>  1) - 1;
	cell.plane_mask = cell.plane_mask << (cell.page_shft << 1);
}

static void __Vdp2ReadNBG(u32 bg_id)
{
	switch(bg_id) {
		case 0: {	//Normal BG 0
			cell.char_ctl = Vdp2Regs->CHCTLA & 0x7F;
			cell.ptrn_supp = Vdp2Regs->PNCN0;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 0) & 0x3;
			cell.xscroll = (((u32)Vdp2Regs->SCXIN0) << 8) | (((u32)Vdp2Regs->SCXDN0) >> 8);
			cell.yscroll = (((u32)Vdp2Regs->SCYIN0) << 8) | (((u32)Vdp2Regs->SCYDN0) >> 8);
			u32 map_shft = 11 + (cell.char_ctl & 1) + ((cell.ptrn_supp >> 15) & 1);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 0) & 0x7) << 6;
			cell.map_addr[0] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN0 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[1] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN0 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[2] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN0 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[3] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN0 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			//TODO: add RBG1
		} break;
		case 1: {	//Normal BG 1
			cell.char_ctl = (Vdp2Regs->CHCTLA >> 8) & 0x3F;
			cell.ptrn_supp = Vdp2Regs->PNCN1;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 2) & 0x3;
			cell.xscroll = (((u32)Vdp2Regs->SCXIN1) << 8) | (((u32)Vdp2Regs->SCXDN1) >> 8);
			cell.yscroll = (((u32)Vdp2Regs->SCYIN1) << 8) | (((u32)Vdp2Regs->SCYDN1) >> 8);
			u32 map_shft = 11 + (cell.char_ctl & 1) + ((cell.ptrn_supp >> 15) & 1);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 4) & 0x7) << 6;
			cell.map_addr[0] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN1 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[1] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN1 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[2] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN1 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[3] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN1 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
		} break;
		case 2: {	//Normal BG 2
			cell.char_ctl = (Vdp2Regs->CHCTLB & 0x1) | ((Vdp2Regs->CHCTLB << 3) & 0x10);
			cell.ptrn_supp = Vdp2Regs->PNCN2;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 4) & 0x3;
			cell.xscroll = (((u32)Vdp2Regs->SCXN2) << 8);
			cell.yscroll = (((u32)Vdp2Regs->SCYN2) << 8);
			u32 map_shft = 11 + (cell.char_ctl & 1) + ((cell.ptrn_supp >> 15) & 1);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 8) & 0x7) << 6;
			cell.map_addr[0] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN2 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[1] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN2 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[2] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN2 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[3] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN2 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
		} break;
		case 3: {	//Normal BG 3
			cell.char_ctl = ((Vdp2Regs->CHCTLB >> 4) & 0x1) | ((Vdp2Regs->CHCTLB >> 1) & 0x10);
			cell.ptrn_supp = Vdp2Regs->PNCN3;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 8) & 0x3;
			cell.xscroll = (((u32)Vdp2Regs->SCXN3) << 8);
			cell.yscroll = (((u32)Vdp2Regs->SCYN3) << 8);
			u32 map_shft = 11 + (cell.char_ctl & 1) + ((cell.ptrn_supp >> 15) & 1);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 12) & 0x7) << 6;
			cell.map_addr[0] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN3 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[1] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN3 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[2] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN3 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[3] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN3 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
		} break;
		case 4: {	//Rotation BG
			cell.char_ctl = (Vdp2Regs->CHCTLB >> 8) & 0x7F;
			cell.ptrn_supp = Vdp2Regs->PNCR;
			//??? Will not draw yet
		} break;
	}
	//TODO:Check if bitmap is used
	cell.color_fmt = (cell.char_ctl >> 4) & 0x7;
	if (!(cell.char_ctl & 0x2)) {
		cell.char_size = ((cell.char_ctl << 3) & 0x8) + 8;
		__Vdp2SetPatternData();
	}
}

static void SGX_Vdp2DrawCell(void)
{

}

static void SGX_Vdp2DrawBitmap(void)
{

}

/*Draws a simple Cell background:
 * All char color count.
 * All char sizes
 * All Ptrn Name data size
 * All plane sizes
 * 4 plane count
 * Scale function
 * Mosaic function
 * No line scroll
 * No vertical cell scroll function
 */
static void SGX_Vdp2DrawCellSimple(void)
{
	guMtxIdentity(vdp2mtx);
	guMtxScale(vdp2mtx, cell.char_size, cell.char_size, 0.0f);
	//guMtxTrans(vdp2mtx, ((f32)(cell.xscroll & -(cell.char_size << 8))) / 256.0f,
	//		   ((f32)(cell.yscroll & -(cell.char_size << 8))) / 256.0f, 0.0f);
	GX_LoadPosMtxImm(vdp2mtx, GXMTX_VDP2);
	GX_SetCurrentMtx(GXMTX_VDP2);
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);

	//TODO: add scaling and screen dims to the format
	u32 x_max = (352 / cell.char_size) + 1;
	u32 y_max = (240 / cell.char_size) + 1;

	u32 x_tile = ((cell.xscroll >> 8) / cell.char_size);
	u32 y_tile = ((cell.yscroll >> 8) / cell.char_size);

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, cell.char_size, cell.char_size);
	switch (cell.color_fmt) {
		case 0:{
			SGX_BeginVdp2Scroll(GX_TF_I4, cell.char_size);
			GX_SetNumIndStages(0);
			GX_SetTevDirect(GX_TEVSTAGE0);
		} break;
		case 1: {
			SGX_BeginVdp2Scroll(GX_TF_I8, cell.char_size);
			SGX_CellConverterSet(cell.char_size >> 4, SPRITE_8BPP);
		} break;
		case 2: { // 16bpp (11 bits used)
			SGX_BeginVdp2Scroll(GX_TF_IA8, cell.char_size);
			SGX_CellConverterSet(cell.char_size >> 4, SPRITE_8BPP);
		} break;
		case 3: {
			SGX_BeginVdp2Scroll(GX_TF_RGB5A3, cell.char_size);
		} break;
		case 4: {
			SGX_BeginVdp2Scroll(GX_TF_RGBA8, cell.char_size);
		} break;
	}

	for (u32 y = 0; y < y_max; ++y) {
		for (u32 x = 0; x < x_max; ++x) {
			//Get pattern data
			u32 yaddr = ((y_tile & cell.page_mask) | ((y_tile & (cell.page_mask+1)) << 1)) << cell.page_shft;
			u32 xaddr = ((x_tile & cell.page_mask) | ((x_tile & (cell.page_mask+1)) << cell.page_shft));
			u32 map = ((y_tile >> cell.ymap_shft) & 2) | ((x_tile >> cell.xmap_shft) & 1);
			u32 addr = (yaddr | xaddr) & cell.plane_mask;

			u32 ptrn;
			if (cell.ptrn_supp & 0x8000) {
				ptrn = *((u16*) (cell.map_addr[map] + (addr << 1)));
			} else {
				ptrn = *((u32*) (cell.map_addr[map] + (addr << 2)));
			}
			u32 flip = (ptrn >> cell.ptrn_flip_shft) & 0x3;
			u32 prcc = ptrn + cell.ptrn_supp;
			u32 pal = ((((ptrn << cell.ptrn_pal_shft) & cell.ptrn_mask) + cell.ptrn_supp) >> 16) & 0x7F;
			u32 chr = ((((ptrn << cell.ptrn_chr_shft) & cell.ptrn_mask) + cell.ptrn_supp) << 5) & 0x7FFE0;

			SGX_SetVdp2Texture(Vdp2Ram + chr + (x << 5), 0);

			//TODO: Change tex matrix to do flipping
			//GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, flip);

			//TODO: Set VtxFrmt5
			GX_Begin(GX_QUADS, GX_VTXFMT5, 4);
				GX_Position2u8(x, y);
				GX_TexCoord1u16(0x0000);
				GX_Position2u8(x + 1, y);
				GX_TexCoord1u16(0x0100);
				GX_Position2u8(x + 1, y + 1);
				GX_TexCoord1u16(0x0101);
				GX_Position2u8(x, y + 1);
				GX_TexCoord1u16(0x0001);
			GX_End();

			//Get next pattern address
			++x_tile;
		}
		++y_tile;
	}
}


//Begins the Vdp1 Drawing Process
void SGX_Vdp2Draw(void)
{
	GX_SetScissor(0, 0, 640, 480);
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
	//GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP2, GX_COLORNULL);

	//SGX_SetTex(output_tex, GX_TF_RGB5A3, 352, 240, 0);
	//SGX_SetOtherTex(GX_TEXMAP2, alpha_tex, GX_TF_IA8, 352, 240, 0);

	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

	//Setup GX things (vertex format, )
	u32 scr_pri = 0;
	u32 scr_enable = 0;
	// If NBG0 16M mode is enabled, don't draw
	// If NBG1 2048/32786 is enabled, don't draw
	scr_pri = (Vdp2Regs->PRINB >> 8) & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x8;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 == 4) ||
		(Vdp2Regs->BGON & 0x2 && (Vdp2Regs->CHCTLA & 0x3000) >> 12 >= 2))) {
		__Vdp2ReadNBG(3);	//Draw NBG3
		GX_SetViewport(352, 240, 640, 480, 0.0f, 1.0f);
		SGX_Vdp2DrawCellSimple();
	}

	//If NBG0 2048/32786/16M mode is enabled, don't draw
	scr_pri = Vdp2Regs->PRINB & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x4;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 >= 2))) {
		__Vdp2ReadNBG(2);	//Draw NBG2
		GX_SetViewport(0, 240, 640, 480, 0.0f, 1.0f);
		SGX_Vdp2DrawCellSimple();
	}

	//Copy the screens to texture...

	// If NBG0 16M mode is enabled, don't draw
	scr_pri = (Vdp2Regs->PRINA >> 8) & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x2;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 == 4))) {
		__Vdp2ReadNBG(1);	//Draw NBG1
		if (cell.char_ctl & 0x2) {
			SGX_Vdp2DrawBitmap();
		} else {
			GX_SetViewport(352, 0, 640, 480, 0.0f, 1.0f);
			SGX_Vdp2DrawCellSimple();
		}
	}

	scr_pri = Vdp2Regs->PRINA & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x1;
	if (!(!(scr_pri) || !(scr_enable))) {
		__Vdp2ReadNBG(0);	//Draw NBG0
		if (cell.char_ctl & 0x2) {
			SGX_Vdp2DrawBitmap();
		} else {
			GX_SetViewport(0, 0, 640, 480, 0.0f, 1.0f);
			SGX_Vdp2DrawCellSimple();
		}
	}
	//TODO: handle Rotation BGs
	//Copy the screens to texture...
	GX_SetViewport(0, 0, 640, 480, 0.0f, 1.0f);
}


