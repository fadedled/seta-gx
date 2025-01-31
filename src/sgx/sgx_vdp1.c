
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
#define SGX_TORGBA6(col) ((((col) & 0x1F) | (((col) & 0x3E0) << 1) | (((col) & 0x7C00) << 2)) << 7)
#define SGX_TORGB565(col) (((col) & 0x1F) | ((col << 1) & 0xFFC0))
#define SGX_CONST_TO_RGBA6(col) 	(((col & 0xFC00) << 8) | ((col & 0x7FF) << 7))
#define SGX_RGB4TO565(col) ((((col) & 0x700) << 4) | (((col) & 0xF0) << 3))
#define SGX_FROMRGB565(col) (((col) & 0x1F) | ((col >> 1) & 0x7FE0))

struct Vdp1Pix {
	u32 type;
	u32 color_mask;
	u32 prcc_shft;
	u32 win_shft;
} vdp1pix;

Mtx vdp1mtx ATTRIBUTE_ALIGN(32);

//About 1 Meg of data combined
u8 *color_tex ATTRIBUTE_ALIGN(32);
u8 *color_rgb_tex ATTRIBUTE_ALIGN(32);
u8 *output_tex ATTRIBUTE_ALIGN(32);

u8  *win_tex ATTRIBUTE_ALIGN(32);
u8  *prcc_tex ATTRIBUTE_ALIGN(32);

u32 sys_clipx = 320;
u32 sys_clipy = 240;

u32 is_processed = 0;

//Texture for mesh creating
static u8 mesh_tex[] ATTRIBUTE_ALIGN(32) = {
	0xF0, 0xF0, 0xF0, 0xF0,
	0x0F, 0x0F, 0x0F, 0x0F,
	0xF0, 0xF0, 0xF0, 0xF0,
	0x0F, 0x0F, 0x0F, 0x0F,
	0xF0, 0xF0, 0xF0, 0xF0,
	0x0F, 0x0F, 0x0F, 0x0F,
	0xF0, 0xF0, 0xF0, 0xF0,
	0x0F, 0x0F, 0x0F, 0x0F
};

static u32 prcc_tlut[128] ATTRIBUTE_ALIGN(32);

extern u32 *tlut_data;
void SGX_Vdp1Init(void)
{
	//Set initial matrix
	guMtxIdentity(vdp1mtx);
	SGX_InitTex(GX_TEXMAP0, TEXREG(0x0000, TEXREG_SIZE_128K), 0);
	SGX_InitTex(GX_TEXMAP1, TEXREG(0x20000, TEXREG_SIZE_32K), 0);
	SGX_InitTex(GX_TEXMAP2, TEXREG(0x28000, TEXREG_SIZE_32K), 0);
	SGX_InitTex(GX_TEXMAP3, TEXREG(0x0000, TEXREG_SIZE_128K), 0);
	GX_LoadPosMtxImm(vdp1mtx, GXMTX_VDP1);
	color_tex = (u8*) memalign(32, 704*512);
	color_rgb_tex = color_tex + (704*256);
	output_tex = (u8*) memalign(32, 704*512);
	win_tex  = (u8*) memalign(32, 704*256);
	prcc_tex = win_tex + (704*128);
	GX_TexModeSync();
}

void SGX_Vdp1Deinit(void)
{
	free(color_tex);
	free(win_tex);
	free(output_tex);
}


static const uint8_t priority_shift[16] =
{ 14-8, 13-8, 14-8, 13-8,  13-8, 12-8, 12-8, 12-8,  7, 7, 6, 0,  7, 7, 6, 0 };
static const uint8_t priority_mask[16] =
{  3,  7,  1,  3,   3,  7,  7,  7,  1, 1, 3, 0,  1, 1, 3, 0 };
static const uint8_t colorcalc_shift[16] =
{ 11-8, 11-8, 11-8, 11-8,  10-8, 11-8, 10-8,  9-8,  0, 6, 0, 6,  0, 6, 0, 6 };
static const uint8_t colorcalc_mask[16] =
{  7,  3,  7,  3,   7,  1,  3,  7,  0, 1, 0, 3,  0, 1, 0, 3 };
static const uint16_t color_mask[16] =
{  0x7FF, 0x7FF, 0x7FF, 0x7FF,  0x3FF, 0x7FF, 0x3FF, 0x1FF,
	0x7F,  0x3F,  0x3F,  0x3F,   0xFF,  0xFF,  0xFF,  0xFF };

static const uint16_t clut_addr[16] =
	{  0x380, 0x380, 0x380, 0x380,  0x3C0, 0x380, 0x3C0, 0x3E0,
		0x3F8,  0x3FC,  0x3FC,  0x3FC,   0x3F0,  0x3F0,  0x3F0,  0x3F0};



//Begins the Vdp1 Drawing Process
void SGX_Vdp1Begin(void)
{
	//TODO: Should be used? NO
	VIDSoftVdp1EraseFrameBuffer();

	//GX_LoadPosMtxIdx(0, GX_PNMTX0);
	GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
	//TODO: Load vdp1 matrix... should we clear the values? YES

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

	//Set up general TEV
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_TEXMTX0);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);

	//XXX: this is for paletted sprites, Konst is for transparency, gouraud is always active and half
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_BGR24_GT, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_KONST, GX_CA_ZERO);

	//TODO: This blending does not work always
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetDstAlpha(GX_TRUE, 0x00);

	//ONLY FOR CONSTATNS
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1_2);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

	//Set how the textures are set up
	//TODO: do this in another function
	SGX_InitTex(GX_TEXMAP0, TEXREG(0x0000, TEXREG_SIZE_128K), 0);

	//Draw the mesh z-texture
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);

	GX_SetColorUpdate(GX_TRUE);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 8, 1);
	GX_SetTexCoordBias(GX_TEXCOORD0, GX_DISABLE, GX_ENABLE);
	GX_LoadPosMtxImm(vdp1mtx, GXMTX_VDP1);
	GX_SetCurrentMtx(GXMTX_VDP1);
	GX_SetScissor(0, 0, 320, 240);
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_DISABLE);
	GX_SetZTexture(GX_ZT_DISABLE, GX_TF_Z8, 0);

	//Store format
	vdp1pix.type = Vdp2Regs->SPCTL & 0xF;
	vdp1pix.color_mask = color_mask[vdp1pix.type];

	is_processed = 0;
}


static void __Vdp1LoadPrCcTlut(void)
{
	u16 pri_arr[8] = {PRI_SPR, PRI_SPR, PRI_SPR, PRI_SPR, PRI_SPR, PRI_SPR, PRI_SPR, PRI_SPR};
	u16 cc_arr[8];
	pri_arr[0] |= (Vdp2Regs->PRISA & 0x7) << 4;
	pri_arr[1] |= ((Vdp2Regs->PRISA >> 8) & 0x7) << 4;
	pri_arr[2] |= (Vdp2Regs->PRISB & 0x7) << 4;
	pri_arr[3] |= ((Vdp2Regs->PRISB >> 8) & 0x7) << 4;
	pri_arr[4] |= (Vdp2Regs->PRISC & 0x7) << 4;
	pri_arr[5] |= ((Vdp2Regs->PRISC >> 8) & 0x7) << 4;
	pri_arr[6] |= (Vdp2Regs->PRISD & 0x7) << 4;
	pri_arr[7] |= ((Vdp2Regs->PRISD >> 8) & 0x7) << 4;
	cc_arr[0] = (Vdp2Regs->CCRSA & 0x1F) << 11;
	cc_arr[1] = ((Vdp2Regs->CCRSA >> 8) & 0x1F) << 11;
	cc_arr[2] = (Vdp2Regs->CCRSB & 0x1F) << 11;
	cc_arr[3] = ((Vdp2Regs->CCRSB >> 8) & 0x1F) << 11;
	cc_arr[4] = (Vdp2Regs->CCRSC & 0x1F) << 11;
	cc_arr[5] = ((Vdp2Regs->CCRSC >> 8) & 0x1F) << 11;
	cc_arr[6] = (Vdp2Regs->CCRSD & 0x1F) << 11;
	cc_arr[7] = ((Vdp2Regs->CCRSD >> 8) & 0x1F) << 11;
	cc_arr[0] |= (cc_arr[0] >> 5) & 0xFF00;
	cc_arr[1] |= (cc_arr[1] >> 5) & 0xFF00;
	cc_arr[2] |= (cc_arr[2] >> 5) & 0xFF00;
	cc_arr[3] |= (cc_arr[3] >> 5) & 0xFF00;
	cc_arr[4] |= (cc_arr[4] >> 5) & 0xFF00;
	cc_arr[5] |= (cc_arr[5] >> 5) & 0xFF00;
	cc_arr[6] |= (cc_arr[6] >> 5) & 0xFF00;
	cc_arr[7] |= (cc_arr[7] >> 5) & 0xFF00;


	u32 *dst = prcc_tlut;
	//TODO: Check if pri bits must be modifiedd if using RGB or windows
	u32 pri_mask = priority_mask[vdp1pix.type];
	u32 pri_shf = priority_shift[vdp1pix.type];
	u32 cc_mask = colorcalc_mask[vdp1pix.type];
	u32 cc_shf = colorcalc_shift[vdp1pix.type];

	//Most significant 16 bits are 0 since in prcc_tex if LSB is 0 then the pixel is transparent
	for (u32 i = 0; i < 256; i += 2) {
		u32 pri = (i >> pri_shf) & pri_mask;
		u32 pix = pri_arr[pri];
		if (pix) {	//If priority is 0 then set as transparent
			//TODO: test color calc depending on priority
			const u32 does_color_calc = 0;
			if (does_color_calc) {
				pix |= cc_arr[(i >> cc_shf) & cc_mask];
			} else {
				pix |= 0xFF00;
			}
		}
		*dst = pix | (pix << 16);
		++dst;
	}
	*prcc_tlut &= 0xFFFF;

	DCFlushRange(prcc_tlut, 0x200);
	SGX_LoadTlut(prcc_tlut, TLUT_SIZE_256 | TLUT_INDX_PPCC);
};


//Vdp1SetFramebufferTEV
void SGX_Vdp1DrawFramebuffer(void)
{
	GX_InvalidateTexAll();
	u32 col_offset = (Vdp2Regs->CRAOFB << 4) & 0x700;
	u32 tlut_indx = clut_addr[vdp1pix.type];
	SGX_LoadTlut(Vdp2ColorRam + (col_offset << 1), TLUT_SIZE_2K | tlut_indx);
	GX_SetScissor(0, 0, 640, 480);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetCurrentMtx(GXMTX_IDENTITY);
	//TODO: This is only for the meantime, should use GX_GREATER
	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetZMode(GX_ENABLE, GX_GREATER, GX_TRUE);
	//Load the Priority and Colorcalculation TLUT
	__Vdp1LoadPrCcTlut();

	//Set up TEV depending on sprite bitdepth
	if (1) { 	//16bpp framebuffer
		//TODO: Add texture
		u32 tev = 0;
		GX_SetNumTexGens(1);
		GX_SetNumChans(0);
		GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

		GXColor convk = {0x7F, 0xFF, 0x7F, 0xff};
		GX_SetTevKColor(GX_KCOLOR0, convk);
		GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
		GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_R);

		GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP1);
		GX_SetTevSwapMode(GX_TEVSTAGE1, GX_TEV_SWAP0, GX_TEV_SWAP1);
		GX_SetTevSwapMode(GX_TEVSTAGE2, GX_TEV_SWAP0, GX_TEV_SWAP0);
		GX_SetTevSwapMode(GX_TEVSTAGE3, GX_TEV_SWAP0, GX_TEV_SWAP0);

		GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 352, 240);

		//Get CRAM colors using CI14 and RGB565
		SGX_SetTex(color_tex, GX_TF_CI14, 352, 240, TLUT_FMT_RGB5A3 | tlut_indx);
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

		//If MSB is for RGB format then mix with RGB color
		u32 tev_stage = GX_TEXMAP1;
		if (Vdp2Regs->SPCTL & 0x20) {
			GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP1, GX_COLORNULL);
			SGX_InitTex(GX_TEXMAP1, TEXREG(0x2000, TEXREG_SIZE_32K), 0);
			SGX_SetOtherTex(GX_TEXMAP1, color_rgb_tex, GX_TF_RGB5A3, 352, 240, 0);
			GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
			GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_CPREV, GX_CC_TEXC, GX_CC_TEXA, GX_CC_ZERO);
			GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
			GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
			tev_stage++;
			//GX_SetNumTevStages(2);
			GX_SetNumTevStages(3);

		} else {
			//GX_SetNumTevStages(1);

			GX_SetNumTevStages(2);
		}
		//Get priority (as z-texture) and color calculation (as alpha)
		GX_SetTevOrder(tev_stage, GX_TEXCOORD0, GX_TEXMAP2, GX_COLORNULL);
		SGX_SetOtherTex(GX_TEXMAP2, prcc_tex, GX_TF_CI8, 352, 240, TLUT_FMT_IA8 | TLUT_INDX_PPCC);
		GX_SetTevColorOp(tev_stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevColorIn(tev_stage, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_CPREV);
		GX_SetTevAlphaOp(tev_stage, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaIn(tev_stage, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

		GX_SetZTexture(GX_ZT_REPLACE, GX_TF_Z8, 0);
	} else {	//8bpp framebuffer

	}

	//GX_SetNumIndStages(0);
	//GX_SetTevDirect(GX_TEVSTAGE0);

	GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
		GX_Position2s16(0, 0);
		GX_TexCoord1u16(0x0000);
		GX_Position2s16(352*2, 0);
		GX_TexCoord1u16(0x0100);
		GX_Position2s16(352*2, 240*2);
		GX_TexCoord1u16(0x0101);
		GX_Position2s16(0, 240*2);
		GX_TexCoord1u16(0x0001);
	GX_End();
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 8, 8);
	GX_SetTexCoordBias(GX_TEXCOORD0, GX_DISABLE, GX_DISABLE);
	GX_SetZTexture(GX_ZT_DISABLE, GX_TF_Z8, 0);
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_TRUE);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetTevDirect(GX_TEVSTAGE1);
	GX_SetScissor(0, 0, 640, 480);
	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP1 | GX_TEXMAP_DISABLE, GX_COLORNULL);
}


static void __Vdp1Convert16bpp(void)
{
	//The RGB5A3 in the Wii looses information that needs to be stored by
	//Saturns 16bpp format. This means we must modify the framebuffer before
	//a copy. We copy using the

	GX_SetCopyClear((GXColor) {0x00, 0x00, 0x00, 0x00}, 0);
	//We shift EFB's bits to make one copy
	GX_ClearVtxDesc();
	GX_SetColorUpdate(GX_TRUE);
	GX_SetAlphaUpdate(GX_TRUE);

	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetNumTevStages(1);
	GX_SetNumTexGens(1);
	GX_SetNumChans(0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	//Check if there is mixed RGB and palette format
	if (Vdp2Regs->SPCTL & 0x20) {
		GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0 | GX_TEXMAP_DISABLE, GX_COLORNULL);

		//Get in RGB565 colors, Reds MSB is cleared
		GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
		GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);
		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_SUB, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

		//TODO: This blending does not work always
		GX_SetColorUpdate(GX_FALSE);
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_DSTALPHA, GX_BL_ONE, GX_LO_OR);
		GX_SetDstAlpha(GX_FALSE, 0x00);
		GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
		GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_TRUE);
		GX_SetCurrentMtx(GXMTX_IDENTITY);

		//Extend Alpha for correct RGBA5551 copying
		GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
			GX_Position2s16(0, 0);
			GX_Position2s16(352, 0);
			GX_Position2s16(352, 240);
			GX_Position2s16(0, 240);
		GX_End();
		//Copy RGB colors
		GX_SetColorUpdate(GX_TRUE);
		GX_SetAlphaUpdate(GX_TRUE);
		GX_SetTexCopySrc(0, 0, 352, 240);
		GX_SetTexCopyDst(352, 240, GX_TF_RGB5A3, GX_FALSE);
		GX_CopyTex(color_rgb_tex, GX_FALSE);

		//Mask RGB colors for dot color copy
		GX_SetTevKColor(GX_KCOLOR0, (GXColor) {0x80, 0x00, 0x00, 0x00});
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_DSTALPHA, GX_BL_INVDSTALPHA, GX_LO_OR);
		GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
			GX_Position2s16(0, 0);
			GX_Position2s16(352, 0);
			GX_Position2s16(352, 240);
			GX_Position2s16(0, 240);
		GX_End();
	}
	//Copy dot color data as is
	GX_SetTexCopyDst(352, 240, GX_TF_RGB565, GX_FALSE);
	GX_CopyTex(color_tex, GX_TRUE);
	GX_PixModeSync(); //Not necesary?

	//Generate Priority and ColorCalc texture...
	GX_SetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
	GX_SetVtxDesc(GX_VA_TEX0,  GX_DIRECT);
	GX_SetTevKColor(GX_KCOLOR0, (GXColor) {0x01, 0x01, 0x01, 0x01});
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	//Set swap mode to get A->R and I->G
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP2);
	SGX_SetTex(color_tex, GX_TF_IA8, 352, 240, 0);
	//Check if IA is zero to test transparent pixels (LSB is on if it is not transparent)
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_COMP_RGB8_EQ, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_KONST, GX_CC_TEXC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_GR16_GT, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_KONST, GX_CA_ZERO);

	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 352, 240);
	GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
	GX_Position2s16(0, 0);
	GX_TexCoord1u16(0x0000);
	GX_Position2s16(352, 0);
	GX_TexCoord1u16(0x0100);
	GX_Position2s16(352, 240);
	GX_TexCoord1u16(0x0101);
	GX_Position2s16(0, 240);
	GX_TexCoord1u16(0x0001);
	GX_End();

	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
	GX_SetTexCopyDst(352, 240, GX_CTF_R8, GX_FALSE);
	GX_CopyTex(prcc_tex, GX_TRUE);
	GX_PixModeSync(); //Not necesary?
}

//Ends the VDP1 Drawing, copies the FB to memory
//and then proceses it depending on the sprite type
void SGX_Vdp1End(void)
{
	SGX_SpriteConverterSet(0, SPRITE_NONE, 0);
	//Copy colors
	if (1) { //16bpp framebuffer
		__Vdp1Convert16bpp();
	} else {

		//GX_CopyTex(color_tex, GX_TRUE);
	}

}


static void __SGX_GetGouraud(u32 *c)
{
	u16 *addr = (u16*) (Vdp1Ram + (((u32) vdp1cmd->GRDA) << 3));
	u32 c0 = *addr++;
	u32 c1 = *addr++;
	u32 c2 = *addr++;
	u32 c3 = *addr++;

	c[0] |= SGX_TORGB565(c0);
	c[1] |= SGX_TORGB565(c1);
	c[2] |= SGX_TORGB565(c2);
	c[3] |= SGX_TORGB565(c3);
}


static void __SGX_Vdp1SetConstantPart(u32 is_rgb)
{
	//TODO: implement this correctly
	u32 is_16bpp = 1;
	//cannot do color calc on 8bpp & non rgb part:
	if (is_16bpp && is_rgb) {
		//is 16bpp and rgb part
		u32 tev_bias = (vdp1cmd->PMOD & 4 ? GX_TB_SUBHALF : GX_TB_ZERO);
		GX_SetDstAlpha(GX_TRUE, 0x7F);
		switch (vdp1cmd->PMOD & 3) {
			case 0: //Replace with MSB on
				GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, tev_bias, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
				GX_SetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
				GX_SetAlphaUpdate(GX_TRUE);
				break;
			case 1: //Shadow
				GX_SetBlendMode(GX_BM_BLEND, GX_BL_ZERO, GX_BL_DSTALPHA, GX_LO_CLEAR);
				GX_SetAlphaUpdate(GX_FALSE);
				//TODO: Set Z compare?
				break;
			case 2: //Half-Luminance
				GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, tev_bias, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
				GX_SetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
				GX_SetAlphaUpdate(GX_TRUE);
				break;
			case 3: //Half-Transparency
				GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, tev_bias, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
				GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_DSTALPHA, GX_LO_CLEAR);
				GX_SetAlphaUpdate(GX_TRUE);
				break;
		}
	} else { //Replace with MSB off
		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_CLEAR);
		GX_SetAlphaUpdate(GX_TRUE);
		GX_SetDstAlpha(GX_TRUE, 0x00); //Only when RGB or window
	}
	//TODO: MSB is to set the highest R bit
	//if MSB on then only set alpha
	if (vdp1cmd->PMOD & 0x8000) {
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_ZERO, GX_BL_ONE, GX_LO_CLEAR);
		GX_SetAlphaUpdate(GX_TRUE);
		GX_SetDstAlpha(GX_TRUE, 0x7F); //Only when RGB or window
	}
	//Set mesh processing
	//TODO: use another tev stage
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_DISABLE);
#if 0
	if (vdp1cmd->PMOD & 0x100) {
		GX_SetZMode(GX_ENABLE, GX_NEQUAL, GX_DISABLE);
	} else {
		GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_DISABLE);
	}
#endif
}

static u32 __SGX_Vdp1SetMode(u32 w, u32 h)
{
	//Address to valid vdp1 RAM range
	u8 *chr_addr = Vdp1Ram + ((vdp1cmd->SRCA & 0xFFFC) << 3);
	u32 spr_w = w << 3;
	//Make the sprite height a multiple of 8
	u32 spr_h = h & 0xF8;
	//TODO: Check if always adding +8 is better
	spr_h += ((h & 0x7) != 0 || (vdp1cmd->SRCA & 3)) << 3;

	u32 tex_mode = (vdp1cmd->PMOD >> 3) & 0x7;
	u32 colr = vdp1cmd->COLR;
	u32 is_rgb = vdp1cmd->PMOD & 0x4;

	//Check transparent code
	u32 trn_code = ((vdp1cmd->PMOD & 0x40) ^ 0x40) << 1;
	if(vdp1cmd->PMOD & 0x40) {	//Draw all bits
		GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
	} else {
		GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	}

	switch (tex_mode) {
		case 0: // Colorbank 4-bit
			__SGX_Vdp1SetConstantPart(0);
			GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_BGR24_GT, GX_TB_ZERO, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
			SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_FMT_RGB565 | TLUT_INDX_CLRBANK);
			SGX_SpriteConverterSet(w, SPRITE_4BPP, vdp1cmd->SRCA & 3);
			return colr & 0xFFF0;
		case 1: // LUT 4-bit
			u32 colorlut = (colr << 3) & 0x7FFFF;
			//Check for colorbanking...

			u32 *pal = MEM_K0_TO_K1(Vdp1Ram + colorlut);
			u32 cb = *pal & 0x7FF0;
			u32 msb = *pal & 0x80008000;
			for (u32 i = 1; i < 8; ++i) {
				msb |= *pal & 0x80008000; //Make sure it follows the correct format
				++pal;
			}
			if (!msb) {	//It uses colorbank code
				__SGX_Vdp1SetConstantPart(0);
				GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_BGR24_GT, GX_TB_ZERO, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
				SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_FMT_RGB565 | TLUT_INDX_CLRBANK);
				SGX_SpriteConverterSet(w, SPRITE_4BPP, vdp1cmd->SRCA & 3);
				return cb & 0xFFF0;
			}
			//Its RGB code.
			if (trn_code) {
				pal = MEM_K0_TO_K1(Vdp1Ram + colorlut);
				*pal &= 0xFFFFu;
			}
			SGX_LoadTlut(Vdp1Ram + colorlut, TLUT_SIZE_16 | TLUT_INDX_IMM4);
			__SGX_Vdp1SetConstantPart(1);
			GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_A8_GT, GX_TB_ZERO, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
			SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_FMT_RGB5A3 | TLUT_INDX_IMM4);
			SGX_SpriteConverterSet(w, SPRITE_4BPP, vdp1cmd->SRCA & 3);
			return 0;
		case 2: // Colorbank 6-bit
		case 3: // Colorbank 7-bit
		case 4: // Colorbank 8-bit
			__SGX_Vdp1SetConstantPart(0);
			GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_BGR24_GT, GX_TB_ZERO, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
			SGX_SetTex(chr_addr, GX_TF_CI8, spr_w, spr_h, TLUT_FMT_RGB565 | TLUT_INDX_CLRBANK);
			SGX_SpriteConverterSet(w, SPRITE_8BPP, vdp1cmd->SRCA & 3);
			return colr & 0xFFC0;	 //TODO: This is wrong
		case 5: // RGB
			__SGX_Vdp1SetConstantPart(is_rgb | 1);
			GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_A8_GT, GX_TB_ZERO, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
			SGX_SetTex(chr_addr, GX_TF_RGB5A3, spr_w, spr_h, 0);
			SGX_SpriteConverterSet(w, SPRITE_16BPP, vdp1cmd->SRCA & 3);
			return 0;
	}
	return 0;
}

//Textured parts
//==============================================
void SGX_Vdp1DrawNormalSpr(void)
{
	//Check if it is a valid sprite (tex size is > 0 for w and h)
	u32 spr_size = vdp1cmd->SIZE & 0x3FFF;
	u32 w = spr_size >> 8;
	u32 h = spr_size & 0xFF;
	if ((!w) | (!h)) {
		return;
	}

	//Get/set the constant values
	//Flip operation
	u32 tex_flip = (-(vdp1cmd->CTRL & 0x10) << 4) & 0x3F00;
	tex_flip 	|= (-(vdp1cmd->CTRL & 0x20) >> 5) & 0x00FF;

	//Get the vertex positions
	s16 x0 = vdp1cmd->XA;
	s16 y0 = vdp1cmd->YA;
	s16 x1 = x0 + (w << 3);
	s16 y1 = y0 + h;

	//Set up the texture processing depending on mode.
	GX_SetNumTexGens(1);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	u32 konst = __SGX_Vdp1SetMode(w, h);
	u32 colors[4] = {konst, konst, konst, konst};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}

	//Draw the sprite
	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(x0, y0);
		GX_Color1u16(colors[0]);
		GX_TexCoord1u16(spr_size & (0x0000 ^ tex_flip));
		GX_Position2s16(x1, y0);
		GX_Color1u16(colors[1]);
		GX_TexCoord1u16(spr_size & (0x3F00 ^ tex_flip));
		GX_Position2s16(x1, y1);
		GX_Color1u16(colors[2]);
		GX_TexCoord1u16(spr_size & (0x3FFF ^ tex_flip));
		GX_Position2s16(x0, y1);
		GX_Color1u16(colors[3]);
		GX_TexCoord1u16(spr_size & (0x00FF ^ tex_flip));
	GX_End();
}

void SGX_Vdp1DrawScaledSpr(void)
{
	//Check if it is a valid sprite (tex size is > 0 for w and h)
	u32 spr_size = vdp1cmd->SIZE & 0x3FFF;
	u32 w = spr_size >> 8;
	u32 h = spr_size & 0xFF;
	if ((!w) | (!h)) {
		return;
	}

	//Get/set the constant values
	//Flip operation
	u32 tex_flip = (-(vdp1cmd->CTRL & 0x10) << 4) & 0x3F00;
	tex_flip 	|= (-(vdp1cmd->CTRL & 0x20) >> 5) & 0x00FF;

	//Get the vertex positions
	s32 x0 = vdp1cmd->XA;
	s32 y0 = vdp1cmd->YA;
	s32 x1 = vdp1cmd->XB;
	s32 y1 = vdp1cmd->YB;

	u32 zp = (vdp1cmd->CTRL >> 8) & 0xF;
	if ((0b0001000100011111 >> zp) & 1) {
		x1 = (vdp1cmd->XC) - x0;
		y1 = (vdp1cmd->YC) - y0;
	} else {
		u32 yshf = ((zp >> 2) & 1) ^ 1;
		u32 ymask = -((zp >> 3) & 1);
		u32 xshf = (zp & 1) ^ 1;
		u32 xmask = -((zp >> 1) & 1);
		x0 -= (x1 >> xshf) & xmask;
		y0 -= (y1 >> yshf) & ymask;
	}
	x1 += x0 + 1;
	y1 += y0 + 1;

	//Set up the texture processing depending on mode.
	GX_SetNumTexGens(1);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	u32 konst = __SGX_Vdp1SetMode(w, h);
	u32 colors[4] = {konst, konst, konst, konst};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}

	//Draw the sprite
	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(x0, y0);
		GX_Color1u16(colors[0]);
		GX_TexCoord1u16(spr_size & (0x0000 ^ tex_flip));
		GX_Position2s16(x1, y0);
		GX_Color1u16(colors[1]);
		GX_TexCoord1u16(spr_size & (0x3F00 ^ tex_flip));
		GX_Position2s16(x1, y1);
		GX_Color1u16(colors[2]);
		GX_TexCoord1u16(spr_size & (0x3FFF ^ tex_flip));
		GX_Position2s16(x0, y1);
		GX_Color1u16(colors[3]);
		GX_TexCoord1u16(spr_size & (0x00FF ^ tex_flip));
	GX_End();
}

void SGX_Vdp1DrawDistortedSpr(void)
{
	//Check if it is a valid sprite (tex size is > 0 for w and h)
	u32 spr_size = vdp1cmd->SIZE & 0x3FFF;
	u32 w = spr_size >> 8;
	u32 h = spr_size & 0xFF;
	if ((!w) | (!h)) {
		return;
	}

	//Get/set the constant values
	//Flip operation
	u32 tex_flip = (-(vdp1cmd->CTRL & 0x10) << 4) & 0x3F00;
	tex_flip 	|= (-(vdp1cmd->CTRL & 0x20) >> 5) & 0x00FF;

	//Get the vertex positions
	s32 x0 = vdp1cmd->XA;
	s32 y0 = vdp1cmd->YA;
	s32 x1 = vdp1cmd->XB;
	s32 y1 = vdp1cmd->YB;
	s32 x2 = vdp1cmd->XC;
	s32 y2 = vdp1cmd->YC;
	s32 x3 = vdp1cmd->XD;
	s32 y3 = vdp1cmd->YD;
	//TODO: Extend to leftmost edge
	s32 cx = x0 + ((x1 - x0) >> 1) + ((x3 - x0) >> 1) + ((x0 - x1 + x2 - x3) >> 2);
	s32 cy = y0 + ((y1 - y0) >> 1) + ((y3 - y0) >> 1) + ((y0 - y1 + y2 - y3) >> 2);

	x0 += (((u32)(x0 - cx) >> 31) ^ 1);
	y0 += (((u32)(y0 - cy) >> 31) ^ 1);
	x1 += (((u32)(x1 - cx) >> 31) ^ 1);
	y1 += (((u32)(y1 - cy) >> 31) ^ 1);
	x2 += (((u32)(x2 - cx) >> 31) ^ 1);
	y2 += (((u32)(y2 - cy) >> 31) ^ 1);
	x3 += (((u32)(x3 - cx) >> 31) ^ 1);
	y3 += (((u32)(y3 - cy) >> 31) ^ 1);

	//Set up the texture processing depending on mode.
	GX_SetNumTexGens(1);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	u32 konst = __SGX_Vdp1SetMode(w, h);
	u32 colors[4] = {konst, konst, konst, konst};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}

	//Draw the sprite
	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(x0, y0);
		GX_Color1u16(colors[0]);
		GX_TexCoord1u16(spr_size & (0x0000 ^ tex_flip));
		GX_Position2s16(x1, y1);
		GX_Color1u16(colors[1]);
		GX_TexCoord1u16(spr_size & (0x3F00 ^ tex_flip));
		GX_Position2s16(x2, y2);
		GX_Color1u16(colors[2]);
		GX_TexCoord1u16(spr_size & (0x3FFF ^ tex_flip));
		GX_Position2s16(x3, y3);
		GX_Color1u16(colors[3]);
		GX_TexCoord1u16(spr_size & (0x00FF ^ tex_flip));
	GX_End();
}


//Non-textured parts
//==============================================
void SGX_Vdp1DrawPolygon(void)
{
	//Get the vertex positions
	s32 x0 = vdp1cmd->XA;
	s32 y0 = vdp1cmd->YA;
	s32 x1 = vdp1cmd->XB;
	s32 y1 = vdp1cmd->YB;
	s32 x2 = vdp1cmd->XC;
	s32 y2 = vdp1cmd->YC;
	s32 x3 = vdp1cmd->XD;
	s32 y3 = vdp1cmd->YD;
	//TODO: Extend to leftmost edge
	s32 cx = x0 + ((x1 - x0) >> 1) + ((x3 - x0) >> 1) + ((x0 - x1 + x2 - x3) >> 2);
	s32 cy = y0 + ((y1 - y0) >> 1) + ((y3 - y0) >> 1) + ((y0 - y1 + y2 - y3) >> 2);

	x0 += (((u32)(x0 - cx) >> 31) ^ 1);
	y0 += (((u32)(y0 - cy) >> 31) ^ 1);
	x1 += (((u32)(x1 - cx) >> 31) ^ 1);
	y1 += (((u32)(y1 - cy) >> 31) ^ 1);
	x2 += (((u32)(x2 - cx) >> 31) ^ 1);
	y2 += (((u32)(y2 - cy) >> 31) ^ 1);
	x3 += (((u32)(x3 - cx) >> 31) ^ 1);
	y3 += (((u32)(y3 - cy) >> 31) ^ 1);

	//Set up the texture processing depending on mode.
	GX_SetNumTexGens(0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_KONST, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
	u32 colr = vdp1cmd->COLR;
	__SGX_Vdp1SetConstantPart(colr & 0x8000);
	GXColor konst = {(colr & 0x7C00) >> 7, (colr & 0x3E0) >> 2, (colr & 0x1F) << 3, 0xFF};
	GX_SetTevKColor(GX_KCOLOR0, konst);
	u32 colors[4] = {0};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}
	//Draw the sprite
	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(x0, y0);
		GX_Color1u16(colors[0]);
		GX_TexCoord1u16(0);
		GX_Position2s16(x1, y1);
		GX_Color1u16(colors[1]);
		GX_TexCoord1u16(0);
		GX_Position2s16(x2, y2);
		GX_Color1u16(colors[2]);
		GX_TexCoord1u16(0);
		GX_Position2s16(x3, y3);
		GX_Color1u16(colors[3]);
		GX_TexCoord1u16(0);
	GX_End();
}

void SGX_Vdp1DrawPolyline(void)
{
	//Set up the texture processing depending on mode.
	GX_SetNumTexGens(0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_KONST, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
	u32 colr = vdp1cmd->COLR;
	__SGX_Vdp1SetConstantPart(colr & 0x8000);
	GXColor konst = {(colr & 0x7C00) >> 7, (colr & 0x3E0) >> 2, (colr & 0x1F) << 3, 0xFF};
	GX_SetTevKColor(GX_KCOLOR0, konst);
	u32 colors[4] = {0};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}

	//Draw the sprite
	GX_Begin(GX_LINESTRIP, GX_VTXFMT2, 5);
		GX_Position2s16(vdp1cmd->XA, vdp1cmd->YA);
		GX_Color1u16(colors[0]);
		GX_TexCoord1u16(0);
		GX_Position2s16(vdp1cmd->XB, vdp1cmd->YB);
		GX_Color1u16(colors[1]);
		GX_TexCoord1u16(0);
		GX_Position2s16(vdp1cmd->XC, vdp1cmd->YC);
		GX_Color1u16(colors[2]);
		GX_TexCoord1u16(0);
		GX_Position2s16(vdp1cmd->XD, vdp1cmd->YD);
		GX_Color1u16(colors[3]);
		GX_TexCoord1u16(0);
		GX_Position2s16(vdp1cmd->XA, vdp1cmd->YA);
		GX_Color1u16(colors[0]);
		GX_TexCoord1u16(0);
	GX_End();
}

void SGX_Vdp1DrawLine(void)
{
	//TODO: Extend to leftmost edge??

	//Set up the texture processing depending on mode.
	GX_SetNumTexGens(0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_KONST, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
	u32 colr = vdp1cmd->COLR;
	__SGX_Vdp1SetConstantPart(colr & 0x8000);
	GXColor konst = {(colr & 0x7C00) >> 7, (colr & 0x3E0) >> 2, (colr & 0x1F) << 3, 0xFF};
	GX_SetTevKColor(GX_KCOLOR0, konst);
	u32 colors[4] = {0};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}

	//Draw the sprite
	GX_Begin(GX_LINES, GX_VTXFMT2, 2);
		GX_Position2s16(vdp1cmd->XA, vdp1cmd->YA);
		GX_Color1u16(colors[0]);
		GX_TexCoord1u16(0);
		GX_Position2s16(vdp1cmd->XB, vdp1cmd->YB);
		GX_Color1u16(colors[1]);
		GX_TexCoord1u16(0);
	GX_End();
}


//Other commands
//==============================================
void SGX_Vdp1UserClip(void)
{
	//TODO
}

void SGX_Vdp1SysClip(void)
{
	//TODO: should clamp value.
	sys_clipx = vdp1cmd->XC + 1;
	sys_clipy = vdp1cmd->YC + 1;
	GX_SetScissor(0, 0, sys_clipx, sys_clipy);
}

void SGX_Vdp1LocalCoord(void)
{
	vdp1mtx[0][3] = (f32) ((s16)vdp1cmd->XA);
	vdp1mtx[1][3] = (f32) ((s16)vdp1cmd->YA);
	GX_LoadPosMtxImm(vdp1mtx, GXMTX_VDP1);
}
