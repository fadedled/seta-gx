


#include "osd.h"
#include "gui.h"
#include "../yabause.h"



struct MsgQueue {
	u32 count;
	struct Msg {
		u32 msg;
		u32 m_len;
		u16 x;
		u16 y;
		u32 color;
	} queue[MAX_MESSAGES];
} osd = {0};


GXTexObj osd_tobj;

#define OSD_TEX_W	256
#define OSD_TEX_H	64


u8 msg_buffer[0x800];
u32 msg_index = 0;
u64 cycle_data[8];

extern u8 osd_texture_4bpp[];
extern yabsys_struct yabsys;



void osd_CyclesSet(u32 indx, u64 cycles)
{
	cycle_data[indx] = (cycles * 100) / yabsys.OneFrameTime;
}


void osd_MsgAdd(u32 x, u32 y, u32 color, char *msg)
{
	u32 len = 0;

	if (!msg || osd.count >= MAX_MESSAGES) {
		return;
	}

	osd.queue[osd.count].x = x;
	osd.queue[osd.count].y = y;
	osd.queue[osd.count].color = color;
	osd.queue[osd.count].msg = msg_index;

	//Copy the string to the message buffer:
	while ((msg_buffer[msg_index] = (*msg))) {
		msg_index = (msg_index + 1) & 0x7FF;
		++len;
		++msg;
	}
	msg_index = (msg_index + 1) & 0x7FF;

	osd.queue[osd.count].m_len = len;
	++osd.count;
}


void osd_MsgShow(void)
{
	/*Show messages*/
	if (!osd.count) {
		return;
	}


	//GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	/*Reserve GX_VTXFMT7 for OSD*/
	GX_SetVtxAttrFmt(GX_VTXFMT7, GX_VA_POS, GX_POS_XY, GX_U16, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT7, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	GX_SetNumChans(0);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);

	GX_LoadTexObjPreloaded(&gui_tobj, &gui_treg, GX_TEXMAP0);

	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_KONST, GX_CC_TEXC, GX_CC_ZERO);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);

	for (u32 i = 0; i < osd.count; ++i) {
		u32 c = osd.queue[i].msg;
		u32 x = osd.queue[i].x;
		u32 y = osd.queue[i].y;

		GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) &osd.queue[i].color));

		GX_Begin(GX_QUADS, GX_VTXFMT7, 4 * osd.queue[i].m_len);
		/*not while m_ptr, must use index for this because of circular buffering*/
		for (u32 j = 0; j < osd.queue[i].m_len; ++j) {
			//XXX: Use a texCoordGen for this.
			f32 chr_x = (f32) ((msg_buffer[c]) & 0x1F) * 0.03125;
			f32 chr_y = (f32) (((msg_buffer[c]) >> 5) & 0x3) * 0.125;
			GX_Position2u16(x , y);					// Top Left
			GX_TexCoord2f32(chr_x, chr_y);
			GX_Position2u16(x + 8, y);			// Top Right
			GX_TexCoord2f32(chr_x + 0.03125, chr_y);
			GX_Position2u16(x + 8, y + 8);	// Bottom Right
			GX_TexCoord2f32(chr_x + 0.03125, chr_y + 0.125);
			GX_Position2u16(x, y + 8);			// Bottom Left
			GX_TexCoord2f32(chr_x, chr_y + 0.125);
			x += 8;
			c = (c + 1) & 0x7FF;
		}
		GX_End();
	}


	u32 colors[8] = {0xFF0000FF, 0x00FF00FF, 0x00FFFFFF, 0xFFFF00FF,
					0xFF00FFFF, 0xFFFF00FF, 0x0000FFFF,	0xFFFFFFFF};
	//Only if alpha is checked
	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) &colors[7]));
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_KONST);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CC_ZERO, GX_CA_KONST);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);

	u32 cycle_x = 8;
	u32 cycle_y = 440;
	for (u32 i = 0; i < 5; ++i, cycle_x += 4) {
		GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) &colors[i]));
		GX_Begin(GX_QUADS, GX_VTXFMT7, 4);
			GX_Position2u16(cycle_x + 4, cycle_y - cycle_data[i]);	// Top Left
			GX_Position2u16(cycle_x, cycle_y - cycle_data[i]);	// Top Right
			GX_Position2u16(cycle_x, cycle_y);	// Bottom Right
			GX_Position2u16(cycle_x + 4, cycle_y);	// Bottom Left
		GX_End();
	}

	GX_SetTevKColor(GX_KCOLOR0, *((GXColor*) &colors[7]));
	GX_Begin(GX_LINES, GX_VTXFMT7, 4);
		GX_Position2u16(4, cycle_y - 100);	// Top Left
		GX_Position2u16(24, cycle_y - 100);	// Top Right
		GX_Position2u16(4, cycle_y);	// Bottom Right
		GX_Position2u16(24, cycle_y);	// Bottom Left
	GX_End();

	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	osd.count = 0;
}

