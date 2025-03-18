
#include "sgx.h"
#include "../vidshared.h"
#include "../vdp1.h"
#include "../vidsoft.h"
#include "../vdp2.h"
#include <malloc.h>

#define SGX_TORGBA32(col) ((((col) & 0x1F) | (((col) & 0x3E0) << 3) | (((col) & 0x7C00) << 6)) << 11)
#define SGX_TORGB565(col) (((col) & 0x1F) | ((col << 1) & 0xFFC0))
#define SGX_FROMRGB565(col) (((col) & 0x1F) | ((col >> 1) & 0x7FE0))



Mtx vdp2mtx ATTRIBUTE_ALIGN(32);

//About 1 Meg of data combined
u8 *bg_tex ATTRIBUTE_ALIGN(32);
u8 *prop_tex ATTRIBUTE_ALIGN(32);

u8 cram_4bpp[PAGE_SIZE] ATTRIBUTE_ALIGN(32);
u8 cram_8bpp[PAGE_SIZE] ATTRIBUTE_ALIGN(32);
u8 cram_11bpp[PAGE_SIZE] ATTRIBUTE_ALIGN(32);


u32 vdp1_fb_w = SS_DISP_WIDTH;	/*framebuffer width visible in vdp2*/
u32 vdp1_fb_h = SS_DISP_HEIGHT;	/*framebuffer heigth visible in vdp2*/

u32 vdp2_disp_w = SS_DISP_WIDTH;	/*display width*/
u32 vdp2_disp_h = SS_DISP_HEIGHT;	/*display height*/

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

	u32 xscroll;		/*X increment*/
	u32 yscroll; 		/*Y increment*/
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
	switch(bg_id) {
		case 0: {	//Normal BG 0
			cell.disp_ctl = Vdp2Regs->BGON;
			cell.char_ctl = Vdp2Regs->CHCTLA & 0x7F;
			cell.ptrn_supp = Vdp2Regs->PNCN0;
			cell.plane_mask = (Vdp2Regs->PLSZ >> 0) & 0x3;
			cell.spec_pri = (Vdp2Regs->SFPRMD >> 0) & 0x3;
			cell.xscroll = (((u32)Vdp2Regs->SCXIN0) << 8) | (((u32)Vdp2Regs->SCXDN0) >> 8);
			cell.yscroll = (((u32)Vdp2Regs->SCYIN0) << 8) | (((u32)Vdp2Regs->SCYDN0) >> 8);
			cell.cram_offset = (Vdp2Regs->CRAOFA << 4) & 0x70;
			u32 map_shft = 11 + (((cell.char_ctl << 1) & 2) ^ 2) + (((cell.ptrn_supp >> 15) & 1) ^ 1);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 0) & 0x7) << 6;
			cell.map_addr[0] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN0 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[1] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN0 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[2] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN0 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[3] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN0 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
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
			cell.cram_offset = (Vdp2Regs->CRAOFA) & 0x70;
			u32 map_shft = 11 + (((cell.char_ctl << 1) & 2) ^ 2) + (((cell.ptrn_supp >> 15) & 1) ^ 1);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 4) & 0x7) << 6;
			cell.map_addr[0] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN1 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[1] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN1 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[2] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN1 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[3] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN1 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
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
			u32 map_shft = 11 + (((cell.char_ctl << 1) & 2) ^ 2) + (((cell.ptrn_supp >> 15) & 1) ^ 1);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 8) & 0x7) << 6;
			cell.map_addr[0] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN2 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[1] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN2 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[2] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN2 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[3] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN2 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
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
			u32 map_shft = 11 + (((cell.char_ctl << 1) & 2) ^ 2) + (((cell.ptrn_supp >> 15) & 1) ^ 1);
			u32 map_offset = ((((u32)Vdp2Regs->MPOFN) >> 12) & 0x7) << 6;
			cell.map_addr[0] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN3 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[1] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPABN3 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[2] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN3 >> 0) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
			cell.map_addr[3] = Vdp2Ram + (((map_offset | (((Vdp2Regs->MPCDN3 >> 8) & 0x3F) & ~cell.plane_mask)) << map_shft) & 0x7FF00);
		} break;
		case 4: {	//Rotation BG
			cell.disp_ctl = Vdp2Regs->BGON >> 4;
			cell.char_ctl = (Vdp2Regs->CHCTLB >> 8) & 0x7F;
			cell.ptrn_supp = Vdp2Regs->PNCR;
			//TODO: RBG are not drawn yet
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
static void SGX_Vdp2DrawCellSimple(void)
{
	guMtxIdentity(vdp2mtx);
	guMtxScale(vdp2mtx, cell.char_size, cell.char_size, 0.0f);
	u32 char_ofs_mask = (cell.char_size << 8) - 1;
	//TODO: When scaling we must leave only integer values if we dont
	//want to worry with artifacts
	vdp2mtx[0][3] = -(((f32)((cell.xscroll & char_ofs_mask) >> 8)));
	vdp2mtx[1][3] = -(((f32)((cell.yscroll & char_ofs_mask) >> 8)));
	GX_LoadPosMtxImm(vdp2mtx, GXMTX_VDP2);
	GX_SetCurrentMtx(GXMTX_VDP2);

	//TODO: add scaling and screen dims to the format

	u32 x_tile = ((cell.xscroll >> 8) / cell.char_size);
	u32 y_tile = ((cell.yscroll >> 8) / cell.char_size);
	u32 x_max = (vdp2_disp_w / cell.char_size) + 1;
	u32 y_max = (vdp2_disp_h / cell.char_size) + 1;

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, cell.char_size, cell.char_size);
	//GX_SetPointSize(cell.char_size * 6, GX_TO_ONE);

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

			u32 ptrn;
			if (cell.ptrn_supp & 0x8000) {
				ptrn = *((u16*) (cell.map_addr[map] + (addr << 1)));
			} else {
				ptrn = *((u32*) (cell.map_addr[map] + (addr << 2)));
			}
			u32 flip = (ptrn >> cell.ptrn_flip_shft) & 0x3;
			flip = ((flip << 8) & 0x100) | (flip >> 1);
			u32 prcc = (ptrn + cell.ptrn_supp) & cell.ptrn_mask;
			u32 pal = ((((ptrn << cell.ptrn_pal_shft) & cell.ptrn_mask) + cell.ptrn_supp) >> 16) & 0x7F;
			u32 chr = ((((ptrn << cell.ptrn_chr_shft) & cell.ptrn_mask) + cell.ptrn_supp) << 5) & 0x7FFE0;
			u32 tex_maddr = 0x94000000 | (MEM_VIRTUAL_TO_PHYSICAL(Vdp2Ram + chr) >> 5);
			u32 tlut_addr = 0x98000000 | TLUT_FMT_RGB5A3 | TLUT_INDX_CRAM0 | pal;
			u32 vert = (((i & 0xFF) << 24) | ((j & 0xFF) << 16));

			//Set texture addr, tlut and Z offset from priority value
			GX_LOAD_BP_REG(tex_maddr);
			GX_LOAD_BP_REG(tlut_addr);
			GX_LOAD_XF_REGS(0x101C, 1); //Set the Viewport Z
			wgPipe->F32 = (f32) (16777215 - (cell.pri | ((prcc >> 25) & 0x10)));

			GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT5, 4);
			//GX_Begin(GX_POINTS, GX_VTXFMT5, 1);
			//wgPipe->U8 = GX_QUADS | GX_VTXFMT5;
			//wgPipe->U16 = 4;
				wgPipe->U32 = vert ^ flip;
				wgPipe->U32 = vert + (0x01000100 ^ flip);
				wgPipe->U32 = vert + (0x00010001 ^ flip);
				wgPipe->U32 = vert + (0x01010101 ^ flip);
			GX_End();
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
		cell.pri = (scr_pri << 4) | PRI_NGB3;
		__Vdp2ReadNBG(3);	//Draw NBG3
		SGX_Vdp2DrawCellSimple();
	}

	//If NBG0 2048/32786/16M mode is enabled, don't draw
	scr_pri = Vdp2Regs->PRINB & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x4;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 >= 2))) {
		cell.pri = (scr_pri << 4) | PRI_NGB2;
		__Vdp2ReadNBG(2);	//Draw NBG2
		SGX_Vdp2DrawCellSimple();
	}

	//Copy the screens to texture...

	// If NBG0 16M mode is enabled, don't draw
	scr_pri = (Vdp2Regs->PRINA >> 8) & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x2;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 == 4))) {
		cell.pri = (scr_pri << 4) | PRI_NGB1;
		__Vdp2ReadNBG(1);	//Draw NBG1

		if (cell.char_ctl & 0x2) {
			SGX_Vdp2DrawBitmap();
		} else {
			SGX_Vdp2DrawCellSimple();
		}
	}

	scr_pri = Vdp2Regs->PRINA & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x1;
	if (!(!(scr_pri) || !(scr_enable))) {
		cell.pri = (scr_pri << 4) | PRI_NGB0;
		__Vdp2ReadNBG(0);	//Draw NBG0

		if (cell.char_ctl & 0x2) {
			SGX_Vdp2DrawBitmap();
		} else {
			SGX_Vdp2DrawCellSimple();
		}
	}
	//TODO: handle Rotation BGs
	//TODO: handle Color calculation
	GX_SetScissor(0, 0, 640, 480);
	SGX_SetZOffset(0);
	GX_SetCopyClear((GXColor) {0x00, 0x00, 0x00, 0x00}, 0);
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_TRUE);
	//Copy the screens to texture...
	GX_SetTexCopySrc(0, 0, vdp1_fb_w, vdp1_fb_h);
	GX_SetTexCopyDst(vdp1_fb_w, vdp1_fb_h, GX_TF_RGB565, GX_FALSE);

	//GX_CopyTex(bg_tex, GX_TRUE);
}


extern void YuiPartialSwapBuffers(u32 offset);

void SGX_Vdp2Postprocess(void)
{
	GX_SetCurrentMtx(GXMTX_IDENTITY);
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
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
	SGX_SetTex(bg_tex, GX_TF_RGB565, vdp2_disp_w, vdp2_disp_h, 0);

	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

	GX_PixModeSync(); //Not necesary?
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 32, vdp2_disp_h);
	switch (vdp2_disp_w) {
		case 320: { //Display must be zoomed 2x
			GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
				GX_Position2s16(0, 0);
				GX_TexCoord1u16(0x0000);
				GX_Position2s16(640, 0);
				GX_TexCoord1u16(0x0A00);
				GX_Position2s16(640, vdp2_disp_h);
				GX_TexCoord1u16(0x0A01);
				GX_Position2s16(0, vdp2_disp_h);
				GX_TexCoord1u16(0x0001);
			GX_End();
			SVI_CopyXFB();
		} break;
		case 352: { //Display must be zoomed 2x and copied in parts
			GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
			GX_Position2s16(0, 0);
			GX_TexCoord1u16(0x0000);
			GX_Position2s16(640, 0);
			GX_TexCoord1u16(0x0B00);
			GX_Position2s16(640, vdp2_disp_h);
			GX_TexCoord1u16(0x0B01);
			GX_Position2s16(0, vdp2_disp_h);
			GX_TexCoord1u16(0x0001);
			GX_End();
			SVI_CopyXFB();
		} break;
		case 640: { //Do not scale
			GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
				GX_Position2s16(0, 0);
				GX_TexCoord1u16(0x0000);
				GX_Position2s16(640, 0);
				GX_TexCoord1u16(0x1400);
				GX_Position2s16(640, vdp2_disp_h);
				GX_TexCoord1u16(0x1401);
				GX_Position2s16(0, vdp2_disp_h);
				GX_TexCoord1u16(0x0001);
			GX_End();
			SVI_CopyXFB();
		} break;
		case 704: {
			GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
			GX_Position2s16(0, 0);
			GX_TexCoord1u16(0x0000);
			GX_Position2s16(640, 0);
			GX_TexCoord1u16(0x1600);
			GX_Position2s16(640, vdp2_disp_h);
			GX_TexCoord1u16(0x1601);
			GX_Position2s16(0, vdp2_disp_h);
			GX_TexCoord1u16(0x0001);
			GX_End();
			SVI_CopyXFB();
		} break;
	}
}



