
#include "sgx.h"
#include "../vidshared.h"
#include "../vdp1.h"
#include "../vidsoft.h"
#include "../vdp2.h"
#include <malloc.h>

#define NUM_SPRITE_WIDTHS	64



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

extern u32 *tlut_data;
void SGX_Vdp2Init(void)
{
	//Set initial matrix
	guMtxIdentity(vdp2mtx);
	GX_LoadPosMtxImm(vdp2mtx, GXMTX_VDP2);
	bg_tex = (u8*) memalign(32, 704*512*2);
	prop_tex = (u8*) memalign(32, 704*512);
}


static void SGX_Vdp2ReadNBG(u32 bg_id)
{

}

static void SGX_Vdp2DrawScroll(void)
{

}

static void SGX_Vdp2DrawBitmap(void)
{

}

static void SGX_Vdp2DrawScrollSimple(void)
{

}


//Begins the Vdp1 Drawing Process
void SGX_Vdp2Draw(void)
{
	//Setup GX things (vertex format, )

	// If NBG0 16M mode is enabled, don't draw
	// If NBG1 2048/32786 is enabled, don't draw
	scr_pri = (Vdp2Regs->PRINB >> 8) & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x8;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 == 4) ||
		(Vdp2Regs->BGON & 0x2 && (Vdp2Regs->CHCTLA & 0x3000) >> 12 >= 2))) {
		SGX_Vdp2ReadNBG(SCR_NBG3);	//Draw NBG3
		SGX_Vdp2DrawScrollSimple();
	}

	//If NBG0 2048/32786/16M mode is enabled, don't draw
	scr_pri = Vdp2Regs->PRINB & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x4;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 >= 2))) {
		SGX_Vdp2ReadNBG(SCR_NBG2);	//Draw NBG2
		SGX_Vdp2DrawScrollSimple();
	}

	//Copy the screens to texture...

	// If NBG0 16M mode is enabled, don't draw
	scr_pri = (Vdp2Regs->PRINA >> 8) & 0x7;
	scr_enable = Vdp2Regs->BGON & 0x2;
	if (!(!(scr_pri) || !(scr_enable) ||
		(Vdp2Regs->BGON & 0x1 && (Vdp2Regs->CHCTLA & 0x70) >> 4 == 4))) {
		SGX_Vdp2ReadNBG(SCR_NBG1);	//Draw NBG1
		if (1) {
			SGX_Vdp2DrawScroll();
		} else {
			SGX_Vdp2DrawBitmap();
		}
	}

	u32 scr_pri = Vdp2Regs->PRINA & 0x7;
	u32 scr_enable = Vdp2Regs->BGON & 0x1;
	if (!(!(scr_pri) || !(scr_enable))) {
		SGX_Vdp2ReadNBG(SCR_NBG0);	//Draw NBG0
		if (1) {
			SGX_Vdp2DrawScroll();
		} else {
			SGX_Vdp2DrawBitmap();
		}
	}
	//TODO: handle Rotation BGs

	//Copy the screens to texture...
}


