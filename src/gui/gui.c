

#include "gui.h"
#include "../peripheral.h"
#include "../sgx/sgx.h"
#include <gccore.h>

GXTexObj gui_tobj;
GXTexRegion gui_treg;

u8 block_4kb[4096] ATTRIBUTE_ALIGN(4096) = {200};

extern u8 menu_tex_4bpp_data[];

/*System texture dimensions*/
#define GUI_SYSTEX_W	256
#define GUI_SYSTEX_H	64

#if 0


#define CURSOR_COLOR_A	0xa3d9
#define CURSOR_COLOR_B	0x4c0b


#define GUI_HBAR_X		216
#define GUI_HBAR_Y		0
#define GUI_HBAR_W		320
#define GUI_HBAR_H		16


static u32 cursor_longname = 0;
static s32 cursor_idle = 128;
static f32 cursor_time = 128.0f;
static f32 cursor_inc = 2.0f;
static String gui_msg;
static u32 gui_msg_color;
static u32 gui_msg_cleared;

static u32 __menu_LerpBGR565(u32 a, u32 b, u8 t) {
	u32 r_a = (a & 0x001F);
	u32 g_a = (a & 0x07E0);
	u32 b_a = (a & 0xF800);

	u32 r_b = (b & 0x001F);
	u32 g_b = (b & 0x07E0);
	u32 b_b = (b & 0xF800);

	u32 lr = (r_a + ((r_b - r_a)*t >> 8)) & 0x001F;
	u32 lg = (g_a + ((g_b - g_a)*t >> 8)) & 0x07E0;
	u32 lb = (b_a + ((b_b - b_a)*t >> 8)) & 0xF800;
	return lr | lg | lb;
}



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
}

void gui_DrawString(u32 x, u32 y, u32 w, String str)
{
	u32 len = str.len;
	u8 *data = (u8*) str.data;
	if (!data) {
		return;
	}
	u32 begin_x = x;
	u32 line_fill = 0;
	GX_Begin(GX_QUADS, GX_VTXFMT4, 4 * len);
	/*not while m_ptr, must use index for this because of circular buffering*/
	while (len) {
		//XXX: Use a texCoordGen for this.
		u32 chr_x = (*data & 0x1F) << 1;
		u32 chr_y = (*data >> 5) & 0x3;
		GX_Position2u16(x, y);					// Top Left
		GX_Color1u16(0xFFFF);
		GX_TexCoord2u8(chr_x, chr_y);
		GX_Position2u16(x + 8, y);			// Top Right
		GX_Color1u16(0xFFFF);
		GX_TexCoord2u8(chr_x + 2, chr_y);
		GX_Position2u16(x + 8, y + 8);	// Bottom Right
		GX_Color1u16(0xFFFF);
		GX_TexCoord2u8(chr_x + 2, chr_y + 1);
		GX_Position2u16(x, y + 8);			// Bottom Left
		GX_Color1u16(0xFFFF);
		GX_TexCoord2u8(chr_x, chr_y + 1);
		x += 6;
		line_fill += 6;
		if (line_fill > w) {
			line_fill = 0;
			y += 8;
			x = begin_x;
		}
		++data;
		--len;
	}
	GX_End();
}

void gui_SetMessage(String msg, u32 color)
{
	gui_msg = msg;
	gui_msg_color = color;
	//gui_msg_set = 0;
}

void gui_ShowMessage(void)
{
	// Clear to solid colors
	//GX_SetDispCopyFrame2Field(GX_COPY_NONE);
	//GX_SetDispCopySrc(0, 0, efbWidth, 2);
	//GX_SetDispCopyDst(0, 0);
	//GX_CopyDisp(&xfb[y][x], GX_TRUE);
	//
	//GX_SetDispCopyFrame2Field(GX_COPY_PROGRESSIVE);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, MTX_IDENTITY);
	GX_InitTexObj(&gui_tobj, menu_tex_4bpp_data, GUI_SYSTEX_W, GUI_SYSTEX_H, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&gui_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_LoadTexObj(&gui_tobj, GX_TEXMAP0);

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 4, 8);
	gui_DrawString(244, 48, 64, gui_msg);
}

void gui_Init(void)
{
	GX_InitTexObj(&gui_tobj, menu_tex_4bpp_data, GUI_SYSTEX_W, GUI_SYSTEX_H, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&gui_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexPreloadRegion(&gui_treg, 0x90000 + 0x16000, (GUI_SYSTEX_W * GUI_SYSTEX_H) >> 1, 0, GX_TEXCACHE_NONE);
	GX_PreloadEntireTexture(&gui_tobj, &gui_treg);
}


static void gui_DrawControllers()
{
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, MTX_IDENTITY);
	GX_InitTexObj(&gui_tobj, menu_tex_4bpp_data, GUI_SYSTEX_W, GUI_SYSTEX_H, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&gui_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_LoadTexObj(&gui_tobj, GX_TEXMAP0);

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 4, 8);
	//u8 pad[8] = {1, 0, 2, 1, 0, 0, 0, 0};
	const u32 num_pads = PER_PADMAX;
	u32 x = 244, y = 126;
	GX_Begin(GX_QUADS, GX_VTXFMT4, num_pads*4*2);
	// Only display 2 controllers for now
	for (u32 i = 0; i < num_pads; ++i) {
		u32 dx = x + ((i & 1) * 30);
		u32 dy = y + ((i & ~1) * 11);
		u32 num = i << 1;
		u32 pad_tex = perpad[i].type << 3;
		u32 color = (perpad[i].type ? 0xFFFF : 0xbdd7);
		GX_Position2u16(dx, dy);		// Top Left
		GX_Color1u16(color);
		GX_TexCoord2u8(pad_tex+24, 4);
		GX_Position2u16(dx+32, dy);	// Top Right
		GX_Color1u16(color);
		GX_TexCoord2u8(pad_tex+32, 4);
		GX_Position2u16(dx+32, dy+24);	// Bottom Right
		GX_Color1u16(color);
		GX_TexCoord2u8(pad_tex+32, 7);
		GX_Position2u16(dx, dy+24);	// Bottom Left
		GX_Color1u16(color);
		GX_TexCoord2u8(pad_tex+24, 7);

		GX_Position2u16(dx+24, dy+16);		// Top Left
		GX_Color1u16(color);
		GX_TexCoord2u8(num+32, 7);
		GX_Position2u16(dx+32, dy+16);	// Top Right
		GX_Color1u16(color);
		GX_TexCoord2u8(num+34, 7);
		GX_Position2u16(dx+32, dy+24);	// Bottom Right
		GX_Color1u16(color);
		GX_TexCoord2u8(num+34, 8);
		GX_Position2u16(dx+24, dy+24);	// Bottom Left
		GX_Color1u16(color);
		GX_TexCoord2u8(num+32, 8);
	}
	GX_End();
}

static void gui_DrawItems(GuiItems *items, u32 width, u32 height)
{

	GX_SetScissor(0, 0, width * 2, height * 2);

	if (items->count == 0) {
		return;
	}

	u32 ofs_x = items->x, ofs_y = items->y;
	u32 shown_entries = (items->count > items->disp_count ? items->disp_count : items->count);
	u32 cursor_pos = items->cursor - items->disp_offset;
	u32 cursor_y = ofs_y + (cursor_pos * 12) - 2;
	u16 cursor_color = __menu_LerpBGR565(CURSOR_COLOR_A, CURSOR_COLOR_B, cursor_time);

	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	if (items->cursor < items->count) {
		cursor_longname = (items->item[items->cursor].len - 5) > 37;
	} else {
		cursor_longname = 0;
	}

	if(!cursor_longname) {
		cursor_idle = 128;
	} else {
		--cursor_idle;
	}

	GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
		GX_Position2u16(ofs_x, cursor_y);			// Top Left
		GX_Color1u16(cursor_color);
		GX_Position2u16((ofs_x + width), cursor_y);		// Top Right
		GX_Color1u16(cursor_color);
		GX_Position2u16((ofs_x + width), (cursor_y + 12));	// Bottom Right
		GX_Color1u16(cursor_color);
		GX_Position2u16(ofs_x, (cursor_y + 12));		// Bottom Left
		GX_Color1u16(cursor_color);
	GX_End();

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, MTX_IDENTITY);
	GX_InitTexObj(&gui_tobj, menu_tex_4bpp_data, GUI_SYSTEX_W, GUI_SYSTEX_H, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&gui_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_LoadTexObj(&gui_tobj, GX_TEXMAP0);

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 4, 8);

	for (int i = 0; i < shown_entries; ++i) {
		u32 x = ofs_x + 24, y = (i * 12) + ofs_y;
		u32 num = i + items->disp_offset;
		u32 padding = 0;
		if (items->cursor == num && cursor_idle < 0) {
			padding = cursor_idle >> 3;
		}
		x += padding;
		gui_DrawString(x, y, 1024, items->item[num]);

		GX_Begin(GX_QUADS, GX_VTXFMT4, 4 * 4);

		u32 xnum = x - 8;
			GX_Position2u16(xnum, y);					// Top Left
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(22, 4);
			GX_Position2u16(xnum + 4, y);			// Top Right
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(23, 4);
			GX_Position2u16(xnum + 4, y + 8);	// Bottom Right
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(23, 5);
			GX_Position2u16(xnum, y + 8);			// Bottom Left
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(22, 5);
		for (u32 i = 0; i < 3; ++i) {
			u32 chr_x = (num % 10) + 12;
			xnum -= 4;
			//Draw the file number
			GX_Position2u16(xnum, y);					// Top Left
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(chr_x, 4);
			GX_Position2u16(xnum + 4, y);			// Top Right
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(chr_x + 1, 4);
			GX_Position2u16(xnum + 4, y + 8);	// Bottom Right
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(chr_x + 1, 5);
			GX_Position2u16(xnum, y + 8);			// Bottom Left
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(chr_x, 5);
			num /= 10;
		}
		GX_End();
	}

}

void gui_Draw(GuiItems *items)
{
	GX_SetLineWidth(2 << 2, 0);
	/*Reserve GX_VTXFMT7 for OSD & GUI*/
	GX_SetScissor(0, 0, 640, 480);
	GX_SetCurrentMtx(MTX_IDENTITY_2X);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);

	GX_SetNumChans(1);
	GX_SetNumTexGens(0);
	GX_SetNumTevStages(1);

	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP_NULL, GX_COLOR0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, MTX_IDENTITY);
	GX_LoadTexObjPreloaded(&gui_tobj, &gui_treg, GX_TEXMAP0);

	//Only if alpha is checked
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);

	GX_Begin(GX_QUADS, GX_VTXFMT4, 4 << 2);
		GX_Position2u16(20 , 0);			// Top Left
		GX_Color1u16(0x3274);
		GX_Position2u16(240, 0);		// Top Right
		GX_Color1u16(0x3274);
		GX_Position2u16(240, 223);	// Bottom Right
		GX_Color1u16(0x3274);
		GX_Position2u16(20 , 223);		// Bottom Left
		GX_Color1u16(0x3274);

		GX_Position2u16(20 , 233);			// Top Left
		GX_Color1u16(0x3274);
		GX_Position2u16(240, 233);		// Top Right
		GX_Color1u16(0x3274);
		GX_Position2u16(240, 240);	// Bottom Right
		GX_Color1u16(0x3274);
		GX_Position2u16(20 , 240);		// Bottom Left
		GX_Color1u16(0x3274);

		GX_Position2u16(241, 0);			// Top Left
		GX_Color1u16(0x296d);
		GX_Position2u16(320, 0);		// Top Right
		GX_Color1u16(0x296d);
		GX_Position2u16(320, 240);	// Bottom Right
		GX_Color1u16(0x296d);
		GX_Position2u16(241, 240);		// Bottom Left
		GX_Color1u16(0x296d);

		GX_Position2u16(0, 218);
		GX_Color1u16(0x296d);
		GX_Position2u16(320, 218);
		GX_Color1u16(0x296d);
		GX_Position2u16(320, 230);	// Bottom Right
		GX_Color1u16(0x296d);
		GX_Position2u16(0, 230);		// Bottom Left
		GX_Color1u16(0x296d);
	GX_End();

	gui_DrawItems(items, 240, 216);

	GX_SetScissor(0, 0, 640, 480);
	gui_DrawControllers();

	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_RASC, GX_CC_TEXA, GX_CC_ZERO);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);

	GX_Begin(GX_QUADS, GX_VTXFMT4, 4*4);
	GX_Position2u16(32, 216);					// Top Left
	GX_Color1u16(0x9773);
	GX_TexCoord2u8(0, 4);
	GX_Position2u16(48, 216);			// Top Right
	GX_Color1u16(0x9773);
	GX_TexCoord2u8(4, 4);
	GX_Position2u16(48, 216+ 16);	// Bottom Right
	GX_Color1u16(0x9773);
	GX_TexCoord2u8(4, 6);
	GX_Position2u16(32, 216+ 16);			// Bottom Left
	GX_Color1u16(0x9773);
	GX_TexCoord2u8(0, 6);

	GX_Position2u16(50, 216 + 4); 					// Top Left
	GX_Color1u16(0xFFFF);
	GX_TexCoord2u8(0, 7);
	GX_Position2u16(86, 216 + 4);			// Top Right
	GX_Color1u16(0xFFFF);
	GX_TexCoord2u8(9, 7);
	GX_Position2u16(86, 216+ 8 + 4);	// Bottom Right
	GX_Color1u16(0xFFFF);
	GX_TexCoord2u8(9, 8);
	GX_Position2u16(50, 216+ 8 + 4);			// Bottom Left
	GX_Color1u16(0xFFFF);
	GX_TexCoord2u8(0, 8);

	GX_Position2u16(32 + 60, 216);					// Top Left
	GX_Color1u16(0xfc92);
	GX_TexCoord2u8(4, 4);
	GX_Position2u16(48 + 60, 216);			// Top Right
	GX_Color1u16(0xfc92);
	GX_TexCoord2u8(8, 4);
	GX_Position2u16(48 + 60, 216+ 16);	// Bottom Right
	GX_Color1u16(0xfc92);
	GX_TexCoord2u8(8, 6);
	GX_Position2u16(32 + 60, 216+ 16);			// Bottom Left
	GX_Color1u16(0xfc92);
	GX_TexCoord2u8(4, 6);

	GX_Position2u16(50 + 60, 216 + 4); 					// Top Left
	GX_Color1u16(0xFFFF);
	GX_TexCoord2u8(10, 7);
	GX_Position2u16(82 + 60, 216 + 4);			// Top Right
	GX_Color1u16(0xFFFF);
	GX_TexCoord2u8(17, 7);
	GX_Position2u16(82 + 60, 216+ 8 + 4);	// Bottom Right
	GX_Color1u16(0xFFFF);
	GX_TexCoord2u8(17, 8);
	GX_Position2u16(50 + 60, 216+ 8 + 4);			// Bottom Left
	GX_Color1u16(0xFFFF);
	GX_TexCoord2u8(10, 8);
	GX_End();

	gui_ShowMessage();

	cursor_time += cursor_inc;
	if (cursor_time <= 2.0f || cursor_time >= 254.0f) {
		cursor_inc = -cursor_inc;
	}

	GX_SetLineWidth(1 << 2, 0);
}


#endif

















// New Implementation
//===================================================00
#include "gui.h"

struct GuiState {
	GuiMenu menu;

	//Cursor of list
	u32 cursor;

	//Current viewport
	u32 x;
	u32 y;
	u32 w;
	u32 h;

} gui;



void gui_SetMessage(String msg, u32 color)
{
	//gui_msg = msg;
	//gui_msg_color = color;
	//gui_msg_set = 0;
}

void gui_Init(void)
{
	GX_InitTexObj(&gui_tobj, menu_tex_4bpp_data, GUI_SYSTEX_W, GUI_SYSTEX_H, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&gui_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexPreloadRegion(&gui_treg, 0x90000 + 0x16000, (GUI_SYSTEX_W * GUI_SYSTEX_H) >> 1, 0, GX_TEXCACHE_NONE);
	GX_PreloadEntireTexture(&gui_tobj, &gui_treg);
}

// Draws a String inside the width
static void __gui_DrawString(String str, u32 x, u32 y, u32 w)
{
	u32 len = str.len;
	u8 *data = (u8*) str.data;
	if (!data) {
		return;
	}
	u32 x_reset = x;
	GX_Begin(GX_POINTS, GX_VTXFMT4, len-1);
	/*not while m_ptr, must use index for this because of circular buffering*/
	while (len) {
		//Find the first blank space or untill the width is filled
		u32 line_fill = 0;
		u32 last_white = -1;
		while (data[line_fill] != '\0' && line_fill < w) {
			if (data[line_fill] != ' ') {
				last_white = line_fill;
			}
			line_fill++;
		}
		//Print up untill the last white value
		line_fill = (last_white == -1 ? last_white : w);

		while (--line_fill) {
			//XXX: Use a texCoordGen for this.
			u32 chr_x = (*data & 0x1F) << 1;
			u32 chr_y = (*data >> 5) & 0x3;
			GX_Position2u16(x, y);
			//XXX: Use the foreground color
			GX_Color1u16(0xFFFF);
			GX_TexCoord2u8(chr_x, chr_y);
			x += 6;
			++data;
			--len;
		}
		y += 8;
		x = x_reset;
	}
	GX_End();
}

static void __gui_DrawRect(u32 x, u32 y, u32 w, u32 h)
{
	GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
	GX_Position2u16(x, y); 					// Top Left
	GX_Color1u16(0x8888); GX_TexCoord2u8(0, 0);
	GX_Position2u16(x + w, y);			// Top Right
	GX_Color1u16(0x8888); GX_TexCoord2u8(0, 0);
	GX_Position2u16(x + w, y + h);	// Bottom Right
	GX_Color1u16(0x8888); GX_TexCoord2u8(0, 0);
	GX_Position2u16(x, y + h);			// Bottom Left
	GX_Color1u16(0x8888); GX_TexCoord2u8(0, 0);
	GX_End();
}

#if 0

static void __gui_DrawItemElem(GuiElem *elem, u32 count)
{
	for (u32 i = 0; i < count; ++i) {
		gui.x = elem->x; gui.y = elem->y;
		gui.w = elem->w; gui.h = elem->h;
		switch (elem->type) {
			case GUIELEM_LABEL: {
				__gui_DrawString(elem->label.text);
			} break;
			case GUIELEM_TOGGLE: {
				__gui_DrawString(elem->toggle.text);
			} break;
			case GUIELEM_IMAGE: {

			} break;
			case GUIELEM_CUSTOM: {

			} break;
		}
		++elem:
	}
}


static void __gui_DrawElem(GuiElem *elem)
{
	gui.x = elem->x; gui.y = elem->y;
	gui.w = elem->w; gui.h = elem->h;
	switch (elem->type) {
		case GUIELEM_RECT: {
			if (GUI_ALPHA(elem->bg_color)) {
				__gui_DrawRect(void);
			}
		} break;
		case GUIELEM_LABEL: {
			__gui_DrawString(elem->label.text);
		} break;
		case GUIELEM_TOGGLE: {
			__gui_DrawString(elem->toggle.text);
		} break;
		case GUIELEM_LIST: {
			u32 count = elem->list.count;
			u32 sel = elem->list.selected;
			GuiElem *elems = elem->list.elems;

			u32 y = 0;
			for (u32 i = 0; i < count; ++i) {
			}

			for (u32 i = 0; i < count; ++i) {
				__gui_DrawItemElem(elem->x, elem->y + y, elems, elem->flags);
				elems++;
			}
		} break;
		case GUIELEM_TABLE: {

		} break;
		case GUIELEM_IMAGE: {

		} break;

/* 		case GUIELEM_CUSTOM: {

		} break; */
	}
}
#endif
void gui_SetMenu(GuiMenu menu)
{
	gui.menu = menu;

	//TODO: Convert menu to an easy to render structure.
}


void gui_DrawMenu(void)
{
	//gui.menu
	//Implement a stack
	GuiElem stack[128];
	//__gui_UpdateMenu();
}

void gui_Error(String msg)
{
	/* Loop error and set up last selected menu */
	GuiElem menu = {.type = GUIELEM_RECT, .bg_color = 0xFFFFFFFF};

	gui_SetMenu(&menu);

	u32 done = 1;
	while(done) {

	}

}

void gui_Test(void)
{
	GX_SetLineWidth(2 << 2, 0);
	/*Reserve GX_VTXFMT7 for OSD & GUI*/
	GX_SetScissor(0, 0, 640, 480);
	GX_SetCurrentMtx(MTX_IDENTITY_2X);
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);

	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP_NULL, GX_COLOR0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, MTX_IDENTITY);
	GX_LoadTexObjPreloaded(&gui_tobj, &gui_treg, GX_TEXMAP0);

	//Only if alpha is checked
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);


	//__gui_DrawRect(12, 32, 24, 44);
	//__gui_DrawRect(120, 42, 60, 56);
	String str = {sizeof("hi this is a test string"), "hi this is a test string"};
	__gui_DrawString(str, 128, 42, 128);
}