#ifndef __VID_GX_H__
#define __VID_GX_H__

#include <gccore.h>
#include "../memory.h"

#define	VTXFMT_FLAT_TEX			GX_VTXFMT2
#define	VTXFMT_GOUR_TEX			GX_VTXFMT3
#define	VTXFMT_COLOR			GX_VTXFMT5

#define TLUT_INDX(type, pos)	((GX_TLUT_2K << 10) | (((pos) + (type) + 0x200) & 0x3ff))


#define TLUT_SIZE_16			(GX_TLUT_16 << 10)
#define TLUT_SIZE_256			(GX_TLUT_256 << 10)
#define TLUT_SIZE_2K			(GX_TLUT_2K << 10)

#define TLUT_FMT_IA8			(GX_TL_IA8 << 10)
#define TLUT_FMT_RGB5A3			(GX_TL_RGB5A3 << 10)
#define TLUT_FMT_RGB565			(GX_TL_RGB565 << 10)

#define TLUT_INDX_CRAM			(0x200)
#define TLUT_INDX_PPCC			(0x210)
#define TLUT_INDX_CLRBANK		(0x2F0)
#define TLUT_INDX_CRAM0			(0x300)
#define TLUT_INDX_CRAM1			(0x380)
#define TLUT_INDX_IMM4			(0x310)
#define TLUT_INDX_IMM			((GX_TLUT_16 << 10) | (0x380 & 0x3ff))
#define TLUT_INDX_IMM8			(((GX_TLUT_256) << 10) | (0x3E0 & 0x3ff))


#define GXMTX_IDENTITY			GX_PNMTX0
#define GXMTX_IDENTITY_2X		GX_PNMTX1
#define GXMTX_VDP1				GX_PNMTX2
#define GXMTX_VDP2				GX_PNMTX3
#define GXMTX_VDP2_BG			GX_PNMTX3

#define TLUT_TYPE_FULL			0x0
#define TLUT_TYPE_4BPP			0x80
#define TLUT_TYPE_8BPP			0x100

#define TEX_FMT(fmt, w, h)			((fmt << 20) | (((h-1) & 0x3FFu) << 10) | ((w-1) & 0x3FFu))
#define TEX_ATTR(wrap_s, wrap_t)	((((wrap_t) & 0x3u) << 2) | ((wrap_s) & 0x3u))
#define TEXREG(addr, size)		((addr >> 5) | size)
#define TEXREG_SIZE_NONE		0x0
#define TEXREG_SIZE_128K		0x120000
#define TEXREG_SIZE_32K			0xD8000

#define SPRITE_NONE			0
#define SPRITE_4BPP			0
#define SPRITE_8BPP			1
#define SPRITE_16BPP		2


#define TEXPRE_TYPE_4BPP	(1<<15)
#define TEXPRE_TYPE_8BPP	(2<<15)
#define TEXPRE_TYPE_16BPP	(2<<15)
#define TEXPRE_TYPE_32BPP	(3<<15)


#define PRI_SPR			0x07
#define PRI_RGB0		0x06
#define PRI_NGB0		0x05
#define PRI_NGB1		0x04
#define PRI_NGB2		0x03
#define PRI_NGB3		0x02
#define PRI_LINECOLOR	0x01
#define PRI_BACKCOLOR	0x10	//This Z value should be the lowest possible

#define	USE_NEW_VDP1		1

typedef struct SGXTexPre_t {
	u32 addr; 	//Address region in TMEM
	u32 fmt;	//Format for loading texture to TEXMAP
	u32 attr;
} SGXTexPre;

static inline void GX_Color1u24(u32 clr)
{
	wgPipe->U8 = clr >> 16;
	wgPipe->U16 = clr;
}


void SGX_Init(void);
void SGX_BeginVdp1(void);
void SGX_InitTex(u32 mapid, u32 even, u32 odd);
void SGX_SetTex(void *img_addr, u32 fmt, u32 w, u32 h, u32 tlut);
void SGX_PreloadTex(void *tex_addr, u32 tmem_addr, u32 tile_cnt_fmt);
void SGX_SetTexPreloaded(u32 mapid, SGXTexPre *tex);
void SGX_SetOtherTex(u32 mapid, void *img_addr, u32 fmt, u32 w, u32 h, u32 tlut);
void SGX_SpriteConverterSet(u32 width, u32 bpp_id, u32 align);
void SGX_EndVdp1(void);


void SGX_BeginVdp2Scroll(u32 fmt, u32 sz);
void SGX_SetVdp2Texture(void *img_addr, u32 tlut);
void SGX_LoadTlut(void *data_addr, u32 tlut);
void SGX_SetZOffset(u32 offset);
void SGX_CellConverterSet(u32 cellsize, u32 bpp_id);

void SGX_DrawScroll(void);
void SGX_DrawBitmap(void);

void SGX_Vdp2ColorRamLoad(void);
void SGX_InvalidateVRAM(void);
void SGX_TlutLoadCRAMImm(u32 pos, u32 trn_code, u32 size);
void SGX_ColorRamDirty(u32 pos);

//Functions for Vdp1 Drawing
void SGX_Vdp1Init(void);
void SGX_Vdp1Deinit(void);
void SGX_Vdp1Begin(void);
void SGX_Vdp1End(void);
void SGX_Vdp1DrawFramebuffer(void);
void SGX_Vdp1ProcessFramebuffer(void);
void SGX_Vdp1DrawNormalSpr(void);
void SGX_Vdp1DrawScaledSpr(void);
void SGX_Vdp1DrawDistortedSpr(void);
void SGX_Vdp1DrawPolygon(void);
void SGX_Vdp1DrawPolyline(void);
void SGX_Vdp1DrawLine(void);
void SGX_Vdp1UserClip(void);
void SGX_Vdp1SysClip(void);
void SGX_Vdp1LocalCoord(void);

void SGX_Vdp2Draw(void);


#endif //__VID_GX_H__
