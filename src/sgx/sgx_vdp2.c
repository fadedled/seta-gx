
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
	u32 ptrn_supp;		/*Constant data of pattern*/
	u32 ptrn_char_shft;	/*Shift of character data in pattern name*/
	u32 ptrn_char_mask;	/*Mask of character data in pattern name */
	u32 ptrn_flip_shft;	/*Shift of flip data in pattern name*/
	u32 ptrn_flip_mask;	/*Mask of flip data in pattern name */
	u32 ptrn_pal_shft;	/*Shift of palette data in pattern name*/
	u32 ptrn_pal_mask;	/*Mask of palette data in pattern name */
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
	u32 supp_data = 0;
	cell.ptrn_char_shft = 0;
	cell.ptrn_char_mask = 0;
	cell.ptrn_flip_shft = 0;
	cell.ptrn_flip_mask = 0;
	cell.ptrn_pal_shft = 0;
	cell.ptrn_pal_mask = 0;
	//One word pattern data
	if (cell.ptrn_supp & 0x8000) {
		// Get ColorCalc and Pri bits
		supp_data = (cell.ptrn_supp & 0x0300) << 20;
		//Auxiliary mode defines flip function
		if (!(cell.ptrn_supp & 0x4000)) {
			cell.ptrn_flip_shft = 20;
			cell.ptrn_flip_mask = 0xC00;
			//Char size mode sets character number
			if (cell.char_size == 8) {
				supp_data |= (cell.ptrn_supp & 0x1F) << 10;
				cell.ptrn_char_shft = 0;
			} else {
				supp_data |= (cell.ptrn_supp & 0x1C) << 10;
				supp_data |= (cell.ptrn_supp & 0x3);
				cell.ptrn_char_shft = 2;
			}
			cell.ptrn_char_mask = 0x3FF;
		} else {
			//Char size mode sets character number
			if (cell.char_size == 8) {
				supp_data |= (cell.ptrn_supp & 0x1C) << 10;
				cell.ptrn_char_shft = 0;
			} else {
				supp_data |= (cell.ptrn_supp & 0x10) << 10;
				supp_data |= (cell.ptrn_supp & 0x3);
				cell.ptrn_char_shft = 2;
			}
			cell.ptrn_char_mask = 0xFFF;
		}
		//Color format defines palette
		if (cell.color_fmt) {	//non 16 color count
			cell.ptrn_pal_shft = 8;
			cell.ptrn_pal_mask = 0x7000;
		} else {	//16 color count
			supp_data |= (cell.ptrn_supp & 0xE0) << 15;
			cell.ptrn_pal_shft = 4;
			cell.ptrn_pal_mask = 0xF000;
		}
	}
	cell.ptrn_supp = supp_data;
}

static void __Vdp2ReadNBG(u32 bg_id)
{
	switch(bg_id) {
		case 0: {	//Normal BG 0
			cell.char_ctl = Vdp2Regs->CHCTLA & 0x7F;
			cell.ptrn_supp = Vdp2Regs->PNCN0;
		} break;
		case 1: {	//Normal BG 1
			cell.char_ctl = (Vdp2Regs->CHCTLA >> 8) & 0x3F;
			cell.ptrn_supp = Vdp2Regs->PNCN1;
		} break;
		case 2: {	//Normal BG 2
			cell.char_ctl = (Vdp2Regs->CHCTLB & 0x1) | ((Vdp2Regs->CHCTLB << 3) & 0x10);
			cell.ptrn_supp = Vdp2Regs->PNCN2;
		} break;
		case 3: {	//Normal BG 3
			cell.char_ctl = ((Vdp2Regs->CHCTLB >> 4) & 0x1) | ((Vdp2Regs->CHCTLB >> 1) & 0x10);
			cell.ptrn_supp = Vdp2Regs->PNCN3;
		} break;
		case 4: {	//Rotation BG
			cell.char_ctl = (Vdp2Regs->CHCTLB >> 8) & 0x7F;
			cell.ptrn_supp = Vdp2Regs->PNCR;
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
	GX_LoadPosMtxImm(vdp2mtx, GXMTX_VDP2);

	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);

	u32 x = 0;
	u32 y = 0;
}


//Begins the Vdp1 Drawing Process
void SGX_Vdp2Draw(void)
{
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
		SGX_Vdp2DrawScrollSimple();
	}

	//If NBG0 2048/32786/16M mode is enabled, don't draw
	scr_pri = Vdp2Regs->PRINB & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x4;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 >= 2))) {
		__Vdp2ReadNBG(2);	//Draw NBG2
		SGX_Vdp2DrawScrollSimple();
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
			SGX_Vdp2DrawScroll();
		}
	}

	scr_pri = Vdp2Regs->PRINA & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x1;
	if (!(!(scr_pri) || !(scr_enable))) {
		__Vdp2ReadNBG(0);	//Draw NBG0
		if (cell.char_ctl & 0x2) {
			SGX_Vdp2DrawBitmap();
		} else {
			SGX_Vdp2DrawScroll();
		}
	}
	//TODO: handle Rotation BGs

	//Copy the screens to texture...
}


