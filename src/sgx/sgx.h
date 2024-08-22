#ifndef __VID_GX_H__
#define __VID_GX_H__

#include <gccore.h>
#include "../memory.h"

#define	VTXFMT_FLAT_TEX			GX_VTXFMT2
#define	VTXFMT_GOUR_TEX			GX_VTXFMT3
#define	VTXFMT_COLOR			GX_VTXFMT5

#define TLUT_INDX(type, pos)	((GX_TLUT_2K << 10) | (((pos) + (type) + 0x200) & 0x3ff))
#define TLUT_INDX_IMM			((GX_TLUT_16 << 10) | (0x380 & 0x3ff))
#define TLUT_INDX_IMM4			(((GX_TLUT_16) << 10) | (0x380 & 0x3ff))
#define TLUT_INDX_IMM8			(((GX_TLUT_256) << 10) | (0x381 & 0x3ff))
#define TLUT_INDX_CLRBANK		(((GX_TLUT_256) << 10) | 0x3F0)

#define GXMTX_IDENTITY			GX_PNMTX0
#define GXMTX_VDP1				GX_PNMTX1
#define GXMTX_VDP2_BG			GX_PNMTX2

#define TLUT_TYPE_FULL			0x0
#define TLUT_TYPE_4BPP			0x80
#define TLUT_TYPE_8BPP			0x100


#define SPRITE_4BPP			0
#define SPRITE_8BPP			1
#define SPRITE_16BPP		2


#define PRI_SPR(x)					(((f32)(x)) - (8.0f - (0.125f * 5.0f)))
#define PRI_RBG0(x)					(((f32)(x)) - (8.0f - (0.125f * 4.0f)))
#define PRI_NBG0(x)					(((f32)(x)) - (8.0f - (0.125f * 3.0f)))
#define PRI_NBG1(x)					(((f32)(x)) - (8.0f - (0.125f * 2.0f)))
#define PRI_NBG2(x)					(((f32)(x)) - (8.0f - (0.125f * 1.0f)))
#define PRI_NBG3(x)					(((f32)(x)) - (8.0f - (0.125f * 0.0f)))

#define	USE_NEW_VDP1		0


void SGX_Init(void);

void SGX_BeginVdp1(void);
void SGX_SetTex(void *img_addr, u32 fmt, u32 w, u32 h, u32 tlut);
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
void SGX_SpriteConverterSet(u32 wsize, u32 bpp_id, u32 align);
void SGX_TlutLoadCRAMImm(u32 pos, u32 trn_code, u32 size);
void SGX_TlutCRAMUpdate(void);
void SGX_ColorRamDirty(u32 pos);

//Functions for Vdp1 Drawing
void SGX_Vdp1Init(void);
void SGX_Vdp1Deinit(void);
void SGX_Vdp1Begin(void);
void SGX_Vdp1End(void);
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

#endif //__VID_GX_H__
