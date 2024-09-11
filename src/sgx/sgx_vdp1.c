
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

Mtx vdp1mtx ATTRIBUTE_ALIGN(32);

//About 1 Meg of data combined
u16 *color_tex ATTRIBUTE_ALIGN(32);
u16 *output_tex ATTRIBUTE_ALIGN(32);
u16 *alpha_tex ATTRIBUTE_ALIGN(32);
u8 *z_tex ATTRIBUTE_ALIGN(32);



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

extern u32 *tlut_data;
void SGX_Vdp1Init(void)
{
	//Set initial matrix
	guMtxIdentity(vdp1mtx);
	SGX_InitTex(GX_TEXMAP0, 0);
	SGX_InitTex(GX_TEXMAP1, 0);
	SGX_InitTex(GX_TEXMAP2, 0);
	SGX_InitTex(GX_TEXMAP3, 0);
	GX_LoadPosMtxImm(vdp1mtx, GXMTX_VDP1);
	color_tex = (u16*) memalign(32, 704*512);
	output_tex = (u16*) memalign(32, 704*512);
	alpha_tex = (u16*) memalign(32, 704*512*2);
	z_tex = (u8*) memalign(32, 704*256);
	GX_TexModeSync();
}

void SGX_Vdp1Deinit(void)
{
	free(color_tex);
	free(alpha_tex);
	free(z_tex);
}


//Begins the Vdp1 Drawing Process
void SGX_Vdp1Begin(void)
{
	//TODO: Should be used?
	VIDSoftVdp1EraseFrameBuffer();

	//GX_LoadPosMtxIdx(0, GX_PNMTX0);
	GX_SetPixelFmt(GX_PF_RGBA6_Z24, GX_ZC_LINEAR);
	GX_SetDither(GX_FALSE);
	//Load vdp1 matrix... should we clear the values?

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
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_DIVIDE_2, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

	//TODO: This blending does not work always
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetDstAlpha(GX_TRUE, 0x00);

	//ONLY FOR CONSTATNS
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_1_8);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);

	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_TRUE);

	//Set how the textures are set up
	//TODO: do this in another function
	SGX_InitTex(GX_TEXMAP0, 0);
	GX_TexModeSync();
	GX_LOAD_BP_REG(0x80000005);	//Repeat

	SGX_LoadTlut(tlut_data, TLUT_INDX_CLRBANK);

	//Draw the mesh z-texture
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	SGX_SetTex(mesh_tex, GX_TF_I4, 8, 8, 0);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 8, 8);
	GX_SetTexCoordBias(GX_TEXCOORD0, GX_ENABLE, GX_ENABLE);
	GX_SetZTexture(GX_ZT_REPLACE, GX_TF_Z8, 0);
	GX_SetCurrentMtx(GXMTX_IDENTITY);
	GX_SetColorUpdate(GX_FALSE);
	GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
	GX_Position2s16(0, 0);
	GX_Color1u16(0);
	GX_TexCoord1u16(0x0000);
	GX_Position2s16(352, 0);
	GX_Color1u16(0);
	GX_TexCoord1u16(0x2C00);
	GX_Position2s16(352, 240);
	GX_Color1u16(0);
	GX_TexCoord1u16(0x2C1E);
	GX_Position2s16(0, 240);
	GX_Color1u16(0);
	GX_TexCoord1u16(0x001E);
	GX_End();

	GX_SetZTexture(GX_ZT_DISABLE, GX_TF_Z8, 0);
	GX_SetColorUpdate(GX_TRUE);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 8, 1);
	GX_SetTexCoordBias(GX_TEXCOORD0, GX_DISABLE, GX_ENABLE);
	GX_LOAD_BP_REG(0x80000000);	//Clamp Texure
	GX_SetCurrentMtx(GXMTX_VDP1);
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_DISABLE);
	is_processed = 0;
}


static u16 clut_4bpp[16] = {

};

static const uint8_t priority_shift[16] =
	{ 14, 13, 14, 13,  13, 12, 12, 12,  7, 7, 6, 0,  7, 7, 6, 0 };
static const uint8_t priority_mask[16] =
	{  3,  7,  1,  3,   3,  7,  7,  7,  1, 1, 3, 0,  1, 1, 3, 0 };
static const uint8_t alpha_shift[16] =
	{ 11, 11, 11, 11,  10, 11, 10,  9,  0, 6, 0, 6,  0, 6, 0, 6 };
static const uint8_t alpha_mask[16] =
	{  7,  3,  7,  3,   7,  1,  3,  7,  0, 1, 0, 3,  0, 1, 0, 3 };
static const uint16_t color_mask[16] =
	{  0x7FF, 0x7FF, 0x7FF, 0x7FF,  0x3FF, 0x7FF, 0x3FF, 0x1FF,
		0x7F,  0x3F,  0x3F,  0x3F,   0xFF,  0xFF,  0xFF,  0xFF };


void SGX_Vdp1ProcessFramebuffer(void)
{
	if (!is_processed) {
		u32 type = Vdp2Regs->SPCTL & 0xF;
		u32 pri_shf = priority_shift[type];
		u32 pri_msk = priority_mask[type];
		u32 alp_shf = alpha_shift[type];
		u32 alp_msk = alpha_mask[type];
		u32 col_msk = color_mask[type];
		u32 col_offset = (Vdp2Regs->CRAOFB << 4) & 0x700;
		u32 priority_arr[8] = {14, 14, 14, 14, 14, 14, 14, 14};
		//XXX: we can make this faster
		priority_arr[0] += (Vdp2Regs->PRISA & 0x7) << 4;
		priority_arr[1] += ((Vdp2Regs->PRISA >> 8) & 0x7) << 4;
		priority_arr[2] += (Vdp2Regs->PRISB & 0x7) << 4;
		priority_arr[3] += ((Vdp2Regs->PRISB >> 8) & 0x7) << 4;
		priority_arr[4] += (Vdp2Regs->PRISC & 0x7) << 4;
		priority_arr[5] += ((Vdp2Regs->PRISC >> 8) & 0x7) << 4;
		priority_arr[6] += (Vdp2Regs->PRISD & 0x7) << 4;
		priority_arr[7] += ((Vdp2Regs->PRISD >> 8) & 0x7) << 4;

		//TODO: Check this value for setting CC, RGB mode or
		//Vdp2Regs->SPCTL

		//Process VDP1 Framebuffer data
		u16 *dst = output_tex;//(u16*) GX_RedirectWriteGatherPipe(output_tex);
		u16 *src = color_tex;
		u16 *alpha = alpha_tex;
		u16 *cram = (u16*) Vdp2ColorRam;
		//TODO: Correctly handle other resmodes
		for (u32 i = 0; i < 352*240; ++i) {
			//Get values
			u32 pix = *src;
			//Check if msb == 0
			if (!(*alpha & 0xFF00)) {
				//TODO: Check MSB of cram first.
				if (pix & 0x7FFF) {
					//Note that pri = 0 does not draw
					*alpha = priority_arr[((pix >> pri_shf) & pri_msk)];
					pix = cram[((pix & col_msk) + col_offset) & 0x7FF] | 0x8000;
					//TODO: Get other data (MSB shadow, colorcalc, normalshadow)
				} else {
					*alpha = 0;
				}
			} else {
				*alpha = priority_arr[0];
			}
			*dst = pix;
			++src;
			++dst;
			++alpha;
		}
		//GX_RestoreWriteGatherPipe();
		//TODO: Currently the alpha texture is rewritten, should be in another texture
		DCFlushRange(output_tex, 352*240*2);
		DCFlushRange(alpha_tex, 352*240*2);
		is_processed = 1;
		//TODO: Only invalidate used tex
		GX_InvalidateTexAll();
	}
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS,  GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	//Set up general TEV
	GX_SetNumTevStages(2);
	GX_SetNumTexGens(1);
	GX_SetNumChans(0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 352, 240);
	GX_SetTexCoordBias(GX_TEXCOORD0, GX_DISABLE, GX_DISABLE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP2, GX_COLORNULL);

	SGX_SetTex(output_tex, GX_TF_RGB5A3, 352, 240, 0);
	SGX_SetOtherTex(GX_TEXMAP2, alpha_tex, GX_TF_IA8, 352, 240, 0);

	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

	GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_CPREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);

	GX_SetZTexture(GX_ZT_REPLACE, GX_TF_Z16, 0);
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_TRUE);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_CLEAR);

	//Set how the textures are set up
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP1, GX_TEV_SWAP1);
	GX_SetCurrentMtx(GXMTX_IDENTITY);

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
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 8, 8);
	GX_SetTexCoordBias(GX_TEXCOORD0, GX_DISABLE, GX_DISABLE);
	GX_SetZTexture(GX_ZT_DISABLE, GX_TF_Z16, 0);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
	GX_SetScissor(0, 0, 640, 480);
}


//Ends the VDP1 Drawing, copies the FB to memory
//and then proceses it depending on the sprite type
void SGX_Vdp1End(void)
{
	GXColor cc = {0x00, 0x00, 0x00, 0xFF};
	GXColor cc_res = {0x00, 0x00, 0x00, 0x00};
	GX_DrawDone();
	//Copy alpha
	GX_SetCopyClear(cc, 0);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetColorUpdate(GX_FALSE);
	GX_SetTexCopySrc(0, 0, 352, 240);
	GX_SetTexCopyDst(352, 240, GX_TF_IA8, GX_FALSE);
	GX_CopyTex(alpha_tex, GX_TRUE);
	//Copy colors
	GX_SetCopyClear(cc_res, 0);
	GX_SetAlphaUpdate(GX_TRUE);
	GX_SetColorUpdate(GX_TRUE);
	GX_SetTexCopySrc(0, 0, 352, 240);
	GX_SetTexCopyDst(352, 240, GX_TF_RGB5A3, GX_FALSE);
	GX_CopyTex(color_tex, GX_TRUE);
	GX_DrawDone();
	//Copy z
	//GX_SetTexCopySrc(0, 0, 640, 480);
	//GX_SetTexCopyDst(640, 480, GX_TF_RGB565, GX_FALSE);
	//GX_CopyTex(win_tex, GX_TRUE);

	//frame must be processed now
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
	//TODO: CHECK if it is not 8bpp
	//if MSB on then only set alpha
	if (vdp1cmd->PMOD & 0x8000) {
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_ZERO, GX_BL_ONE, GX_LO_CLEAR);
		GX_SetAlphaUpdate(GX_TRUE);
		GX_SetDstAlpha(GX_TRUE, 0x7F); //Only when RGB or window
	}
	//Set mesh processing
	if (vdp1cmd->PMOD & 0x100) {
		GX_SetZMode(GX_ENABLE, GX_NEQUAL, GX_DISABLE);
	} else {
		GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_DISABLE);
	}
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
	u32 constant_color = SGX_TORGB565(colr);

	//Check transparent code
	u32 trn_code = ((vdp1cmd->PMOD & 0x40) ^ 0x40) << 1;

	if(vdp1cmd->PMOD & 0x40) {
		//TODO: use alpha compare function?
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	} else {
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
	}

	switch (tex_mode) {
		case 0: // Colorbank 4-bit
			__SGX_Vdp1SetConstantPart(0);
			SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_INDX_CLRBANK);
			SGX_SpriteConverterSet(w, SPRITE_4BPP, vdp1cmd->SRCA & 3);
			return constant_color & 0xFFF0;
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
				SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_INDX_CLRBANK);
				SGX_SpriteConverterSet(w, SPRITE_4BPP, vdp1cmd->SRCA & 3);
				return SGX_TORGB565(cb) & 0xFFF0;
			}
			//Its RGB code.
			if (trn_code) {
				pal = MEM_K0_TO_K1(Vdp1Ram + colorlut);
				*pal &= 0xFFFFu;
			}
			SGX_LoadTlut(Vdp1Ram + colorlut, TLUT_INDX_IMM4);
			__SGX_Vdp1SetConstantPart(1);
			SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_INDX_IMM4);
			SGX_SpriteConverterSet(w, SPRITE_4BPP, vdp1cmd->SRCA & 3);
			return 0;
		case 2: // Colorbank 6-bit
		case 3: // Colorbank 7-bit
		case 4: // Colorbank 8-bit
			__SGX_Vdp1SetConstantPart(0);
			SGX_SetTex(chr_addr, GX_TF_CI8, spr_w, spr_h, TLUT_INDX_CLRBANK);
			SGX_SpriteConverterSet(w, SPRITE_8BPP, vdp1cmd->SRCA & 3);
			return constant_color & 0xFFC0;	 //TODO: This is wrong
		case 5: // RGB
			__SGX_Vdp1SetConstantPart(is_rgb | 1);
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
	GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
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
	GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
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
	s32 x0 = vdp1cmd->XA << 1;
	s32 y0 = vdp1cmd->YA << 1;
	s32 x1 = vdp1cmd->XB << 1;
	s32 y1 = vdp1cmd->YB << 1;
	s32 x2 = vdp1cmd->XC << 1;
	s32 y2 = vdp1cmd->YC << 1;
	s32 x3 = vdp1cmd->XD << 1;
	s32 y3 = vdp1cmd->YD << 1;
	//TODO: Extend to leftmost edge
	s32 cx = x0 + ((x1 - x0) >> 1) + ((x3 - x0) >> 1) + ((x0 - x1 + x2 - x3) >> 2);
	s32 cy = y0 + ((y1 - y0) >> 1) + ((y3 - y0) >> 1) + ((y0 - y1 + y2 - y3) >> 2);

	x0 += (((x0 - cx) >> 31) & ~1);
	y0 += (((y0 - cy) >> 31) & ~1);
	x1 += (((x1 - cx) >> 31) & ~1);
	y1 += (((y1 - cy) >> 31) & ~1);
	x2 += (((x2 - cx) >> 31) & ~1);
	y2 += (((y2 - cy) >> 31) & ~1);
	x3 += (((x3 - cx) >> 31) & ~1);
	y3 += (((y3 - cy) >> 31) & ~1);


	//Set up the texture processing depending on mode.
	GX_SetNumTexGens(1);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	u32 konst = __SGX_Vdp1SetMode(w, h);
	u32 colors[4] = {konst, konst, konst, konst};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}

	//Draw the sprite
	GX_Begin(GX_QUADS, GX_VTXFMT3, 4);
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
	s32 x0 = vdp1cmd->XA << 1;
	s32 y0 = vdp1cmd->YA << 1;
	s32 x1 = vdp1cmd->XB << 1;
	s32 y1 = vdp1cmd->YB << 1;
	s32 x2 = vdp1cmd->XC << 1;
	s32 y2 = vdp1cmd->YC << 1;
	s32 x3 = vdp1cmd->XD << 1;
	s32 y3 = vdp1cmd->YD << 1;
	//TODO: Extend to leftmost edge
	s32 cx = x0 + ((x1 - x0) >> 1) + ((x3 - x0) >> 1) + ((x0 - x1 + x2 - x3) >> 2);
	s32 cy = y0 + ((y1 - y0) >> 1) + ((y3 - y0) >> 1) + ((y0 - y1 + y2 - y3) >> 2);

	x0 += (((x0 - cx) >> 31) & ~1);
	y0 += (((y0 - cy) >> 31) & ~1);
	x1 += (((x1 - cx) >> 31) & ~1);
	y1 += (((y1 - cy) >> 31) & ~1);
	x2 += (((x2 - cx) >> 31) & ~1);
	y2 += (((y2 - cy) >> 31) & ~1);
	x3 += (((x3 - cx) >> 31) & ~1);
	y3 += (((y3 - cy) >> 31) & ~1);

	//Set up the texture processing depending on mode.
	GX_SetNumTexGens(0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	u32 colr = vdp1cmd->COLR;
	__SGX_Vdp1SetConstantPart(colr & 0x8000);
	u32 konst = SGX_TORGB565(colr);
	u32 colors[4] = {konst, konst, konst, konst};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}

	//Draw the sprite
	GX_Begin(GX_QUADS, GX_VTXFMT3, 4);
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
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	u32 colr = vdp1cmd->COLR;
	__SGX_Vdp1SetConstantPart(colr & 0x8000);
	u32 konst = SGX_TORGB565(colr);
	u32 colors[4] = {konst, konst, konst, konst};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}

	//Draw the sprite
	GX_Begin(GX_LINESTRIP, GX_VTXFMT4, 5);
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
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	u32 colr = vdp1cmd->COLR;
	__SGX_Vdp1SetConstantPart(colr & 0x8000);
	u32 konst = SGX_TORGB565(colr);
	u32 colors[4] = {konst, konst, konst, konst};
	if (vdp1cmd->PMOD & 0x4) {
		__SGX_GetGouraud(colors);
	}

	//Draw the sprite
	GX_Begin(GX_LINES, GX_VTXFMT4, 2);
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
	GX_SetScissor(0, 0, vdp1cmd->XC + 1, vdp1cmd->YC + 1);
}

void SGX_Vdp1LocalCoord(void)
{
	vdp1mtx[0][3] = (f32) (vdp1cmd->XA);
	vdp1mtx[1][3] = (f32) (vdp1cmd->YA);
	GX_LoadPosMtxImm(vdp1mtx, GXMTX_VDP1);
}
