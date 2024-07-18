

#include "gui.h"


GXTexObj gui_tobj;
GXTexRegion gui_treg;

u8 block_4kb[4096] ATTRIBUTE_ALIGN(4096) = {200};

extern u8 osd_texture_4bpp[];

/*System texture dimensions*/
#define GUI_SYSTEX_W	256
#define GUI_SYSTEX_H	64



const u32 gui_palette[] = {
	0x404040ff, 0xa0b0a0ff, 0x00d0d090, 0xFFFFFFFF, 0x0,
};

static inline void gui_DrawOptions(GuiElem *elem)
{


}


static inline void gui_DrawLabel(GuiLabel *elem)
{


}


static inline void gui_DrawImage(GuiElem *elems)
{
	//GX_InitTexObj(&osd_tobj, osd_texture_4bpp, OSD_TEX_W, OSD_TEX_H, GX_TF_I4, GX_REPEAT, GX_REPEAT, GX_FALSE);
	//GX_InitTexObjLOD(&osd_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	//GX_LoadTexObj(&osd_tobj, GX_TEXMAP0);
}


void gui_Init(void)
{
	GX_InitTexObj(&gui_tobj, osd_texture_4bpp, GUI_SYSTEX_W, GUI_SYSTEX_H, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&gui_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexPreloadRegion(&gui_treg, 0x90000 + 0x16000, (GUI_SYSTEX_W * GUI_SYSTEX_H) >> 1, 0, GX_TEXCACHE_NONE);
	GX_PreloadEntireTexture(&gui_tobj, &gui_treg);
}


static void gui_DrawItems(GuiItems *items, u32 width)
{
	u32 ofs_x = items->x, ofs_y = items->y;

	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_COMP_A8_GT, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_KONST, GX_CA_ZERO);

	GX_InitTexObj(&gui_tobj, osd_texture_4bpp, GUI_SYSTEX_W, GUI_SYSTEX_H, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&gui_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_LoadTexObj(&gui_tobj, GX_TEXMAP0);

	u32 shown_entries = (items->count > items->disp_count ? items->disp_count : items->count);
	u32 cursor_pos = items->cursor - items->disp_offset;

	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) &gui_palette[3]));
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	for (int i = 0; i < shown_entries; ++i) {
		u32 x = ofs_x + 8, y = (i * 10) + ofs_y;
		u32 len = items->item[i + items->disp_offset].len;
		u8 *str = items->item[i + items->disp_offset].data;

		GX_Begin(GX_QUADS, GX_VTXFMT7, 4 * len);
			/*not while m_ptr, must use index for this because of circular buffering*/
			while (len) {
				//XXX: Use a texCoordGen for this.
				f32 chr_x = (f32) ((*str) & 0x1F) * 0.03125;
				f32 chr_y = (f32) (((*str) >> 5) & 0x3) * 0.125;
				u32 spacing = 8;
				x -= (8 - spacing);
				GX_Position2u16(x <<1, y<<1);					// Top Left
				GX_TexCoord2f32(chr_x, chr_y);
				GX_Position2u16((x + 8)<<1, y<<1);			// Top Right
				GX_TexCoord2f32(chr_x + 0.03125, chr_y);
				GX_Position2u16((x + 8)<<1, (y + 8)<<1);	// Bottom Right
				GX_TexCoord2f32(chr_x + 0.03125, chr_y + 0.125);
				GX_Position2u16(x<<1, (y + 8)<<1);			// Bottom Left
				GX_TexCoord2f32(chr_x, chr_y + 0.125);
				x += 8;
				++str;
				--len;
			}
		GX_End();
	}

	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) &gui_palette[2]));
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CC_ZERO, GX_CA_KONST);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);

	u32 cursor_y = ofs_y + (cursor_pos * 10) - 1;
	GX_Begin(GX_QUADS, GX_VTXFMT7, 4);
		GX_Position2u16(ofs_x <<1, cursor_y<<1);			// Top Left
		GX_Position2u16((ofs_x + width)<<1, cursor_y<<1);		// Top Right
		GX_Position2u16((ofs_x + width)<<1, (cursor_y + 10)<<1);	// Bottom Right
		GX_Position2u16(ofs_x<<1, (cursor_y + 10)<<1);		// Bottom Left
	GX_End();
}

void gui_Draw(GuiItems *items)
{
	/*Reserve GX_VTXFMT7 for OSD & GUI*/
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetVtxAttrFmt(GX_VTXFMT7, GX_VA_POS, GX_POS_XY, GX_U16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT7, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetNumChans(0);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_KONST, GX_CA_TEXA, GX_CA_ZERO);

	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_K0_A);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_LoadTexObjPreloaded(&gui_tobj, &gui_treg, GX_TEXMAP0);

	//Only if alpha is checked
	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) &gui_palette[0]));
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CC_ZERO, GX_CA_KONST);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);

	GX_SetCurrentMtx(GX_PNMTX0);

	GX_Begin(GX_QUADS, GX_VTXFMT7, 4);
		GX_Position2u16(0 , 0);			// Top Left
		GX_Position2u16(320<<1, 0);		// Top Right
		GX_Position2u16(320<<1, 240<<1);	// Bottom Right
		GX_Position2u16(0, 240<<1);		// Bottom Left
	GX_End();

	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) &gui_palette[1]));

	GX_Begin(GX_LINES, GX_VTXFMT7, 6);
		//Horizontal Strip
		GX_Position2u16(0, 226<<1);
		GX_Position2u16(320<<1, 226<<1);
		GX_Position2u16(0, 236<<1);	// Bottom Right
		GX_Position2u16(320<<1, 236<<1);		// Bottom Left
		//Vertical Strip
		GX_Position2u16(block_4kb[0]<<1, 0);	// Bottom Right
		GX_Position2u16(200<<1, 240<<1);		// Bottom Left
	GX_End();

	gui_DrawItems(items, 200);
	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) &gui_palette[4]));
}
