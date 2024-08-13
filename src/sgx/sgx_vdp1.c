
#include "sgx.h"
#include "../vidshared.h"
#include "../vdp1.h"
#include "../vidsoft.h"
#include "../vdp2.h"


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


#define SGX_TORGBA32(col) ((col & 0x1F) << 11 | ((col & 0x3E0) << 14) | ((col & 0x7C00) << 17))

Mtx vdp1mtx ATTRIBUTE_ALIGN(32);

//Begins the Vdp1 Drawing Process
void SGX_Vdp1Begin(void)
{
	//TODO: Should be used?
	VIDSoftVdp1EraseFrameBuffer();

	//GX_LoadPosMtxIdx(0, GX_PNMTX0);
	//Load vdp1 matrix... should we clear the values?
	guMtxIdentity(vdp1mtx);
	GX_LoadPosMtxImm(vdp1mtx, GXMTX_VDP1);
	GX_SetCurrentMtx(GXMTX_VDP1);

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
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 8, 1);

	//XXX: this is for paletted sprites, Konst is for transparency, gouraud is always active and half
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	//ONLY FOR CONSTATNS
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
	GX_SetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP1, GX_TEV_SWAP1);

	//Set how the textures are set up
	u32 tmem_even = 0x8C000000 | 0x100000 | 0x20000;	//128K even cache
	u32 tmem_odd = 0x90000000;	//No odd tmem cache
	u32 tex_filt = 0x80000000;	//No filter
	u32 tex_lod = 0x84000000;	//No LOD
	u32 tex_maddr = 0x94000000;	//This will be modified per command
	u32 tex_size = 0x88000000;	//This will be modified per command

	GX_LOAD_BP_REG(tex_filt);
	GX_LOAD_BP_REG(tex_lod);
	GX_LOAD_BP_REG(tex_size);
	GX_LOAD_BP_REG(tmem_even);
	GX_LOAD_BP_REG(tmem_odd);
	GX_LOAD_BP_REG(tex_maddr);
}

//Ends the VDP1 Drawing, copies the FB to memory
//and then proceses it depending on the sprite type
void SGX_Vdp1End(void)
{

}


static void SGX_Vdp1SetMode(u32 w, u32 h, u32 *clr)
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

	// This changes the color format
	if (colr & 0x8000) {
		//TODO: Change the format or just change Z offset?
		//*clr = ((colr & 7) << 5) | ((colr & 0x78) << 9) | ((colr & 0x780) << 13) | ((colr & 0x7800) << 17);
		*clr = SGX_TORGBA32(colr);
	} else {
		*clr = SGX_TORGBA32(colr);
	}

	//Check transparent code
	u32 trn_code = ((vdp1cmd->PMOD & 0x40) ^ 0x40) << 1;

	if(vdp1cmd->PMOD & 0x40) {
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	} else {
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);

		//GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_BGR24_GT, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		//GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST, GX_CA_ZERO);
	}

	switch (tex_mode) {
		case 0: // Colorbank 4-bit
			GX_SetTevKColor(GX_KCOLOR0, *((GXColor*)(clr)));
			SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_INDX_CLRBANK);
			SGX_SpriteConverterSet(w, SPRITE_4BPP, vdp1cmd->SRCA & 3);
			break;
		case 1: // LUT 4-bit
#if 0
			u32 colorlut = (colr << 3) & 0x7FFFF;
			//Upload palette
			//TODO: palette uploading should be done once per frame (for color bank, the other will be done per sprite)
			if (trn_code) {
				u32 *pal = MEM_K0_TO_K1(Vdp1Ram + colorlut);
				*pal &= 0xFFFFu;
			}
			GX_InitTlutObj(&tlut_obj, Vdp1Ram + colorlut, GX_TL_RGB5A3, 16);
			GX_LoadTlut(&tlut_obj, TLUT_INDX_IMM4);
			//Change address and size
			SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_INDX_IMM4);
			SGX_SpriteConverterSet(w, SPRITE_4BPP, cmd.CMDSRCA & 3);
			break;
#endif
			*clr = 0xFFFFFF00;
			GX_SetTevKColor(GX_KCOLOR0, *((GXColor*)(clr)));
			SGX_SetTex(chr_addr, GX_TF_CI4, spr_w, spr_h, TLUT_INDX_CLRBANK);
			SGX_SpriteConverterSet(w, SPRITE_4BPP, vdp1cmd->SRCA & 3);
			break;
		case 2: // Colorbank 6-bit
		case 3: // Colorbank 7-bit
		case 4: // Colorbank 8-
			GX_SetTevKColor(GX_KCOLOR0, *((GXColor*)(clr)));
			SGX_SetTex(chr_addr, GX_TF_CI8, spr_w, spr_h, TLUT_INDX_CLRBANK);
			SGX_SpriteConverterSet(w, SPRITE_8BPP, vdp1cmd->SRCA & 3);
			break;
		case 5: // RGB
			SGX_SetTex(chr_addr, GX_TF_RGB5A3, spr_w, spr_h, 0);
			SGX_SpriteConverterSet(w, SPRITE_16BPP, vdp1cmd->SRCA & 3);
			break;
	}
}

//Textured parts
//==============================================
void SGX_Vdp1DrawNormalSpr(void)
{
	//Check if it is a valid sprite (tex size is > 0 for w and h)
	u32 color = 0;
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
	SGX_Vdp1SetMode(w, h, &color);

	//Draw the sprite
	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(x0, y0);
		GX_Color1u32(0);
		GX_TexCoord1u16(spr_size & (0x0000 ^ tex_flip));
		GX_Position2s16(x1, y0);
		GX_Color1u32(0);
		GX_TexCoord1u16(spr_size & (0x3F00 ^ tex_flip));
		GX_Position2s16(x1, y1);
		GX_Color1u32(0);
		GX_TexCoord1u16(spr_size & (0x3FFF ^ tex_flip));
		GX_Position2s16(x0, y1);
		GX_Color1u32(0);
		GX_TexCoord1u16(spr_size & (0x00FF ^ tex_flip));
	GX_End();
}

void SGX_Vdp1DrawScaledSpr(void)
{
	//Check if it is a valid sprite (tex size is > 0 for w and h)
	u32 color = 0;
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
	SGX_Vdp1SetMode(w, h, &color);

	//Draw the sprite
	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
		GX_Position2s16(x0, y0);
		GX_Color1u32(0);
		GX_TexCoord1u16(spr_size & (0x0000 ^ tex_flip));
		GX_Position2s16(x1, y0);
		GX_Color1u32(0);
		GX_TexCoord1u16(spr_size & (0x3F00 ^ tex_flip));
		GX_Position2s16(x1, y1);
		GX_Color1u32(0);
		GX_TexCoord1u16(spr_size & (0x3FFF ^ tex_flip));
		GX_Position2s16(x0, y1);
		GX_Color1u32(0);
		GX_TexCoord1u16(spr_size & (0x00FF ^ tex_flip));
	GX_End();
}

void SGX_Vdp1DrawDistortedSpr(void)
{
	//Check if it is a valid sprite (tex size is > 0 for w and h)
	u32 color = 0;
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

	//Set up the texture processing depending on mode.
	SGX_Vdp1SetMode(w, h, &color);

	//Draw the sprite
	GX_Begin(GX_QUADS, GX_VTXFMT2, 4);
	GX_Position2s16(x0, y0);
	GX_Color1u32(0);
	GX_TexCoord1u16(spr_size & (0x0000 ^ tex_flip));
	GX_Position2s16(x1, y1);
	GX_Color1u32(0);
	GX_TexCoord1u16(spr_size & (0x3F00 ^ tex_flip));
	GX_Position2s16(x2, y2);
	GX_Color1u32(0);
	GX_TexCoord1u16(spr_size & (0x3FFF ^ tex_flip));
	GX_Position2s16(x3, y3);
	GX_Color1u32(0);
	GX_TexCoord1u16(spr_size & (0x00FF ^ tex_flip));
	GX_End();
}


//Non-textured parts
//==============================================
void SGX_Vdp1DrawPolygon(void)
{

}

void SGX_Vdp1DrawPolyline(void)
{

}

void SGX_Vdp1DrawLine(void)
{

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
	GX_SetScissor(0, 0, vdp1cmd->XC, vdp1cmd->YC);
}

void SGX_Vdp1LocalCoord(void)
{
	vdp1mtx[0][3] = (f32) (vdp1cmd->XA);
	vdp1mtx[1][3] = (f32) (vdp1cmd->YA);
	GX_LoadPosMtxImm(vdp1mtx, GXMTX_VDP1);
}
