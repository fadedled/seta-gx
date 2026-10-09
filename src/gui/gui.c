/********************************************************************************
 * Copyright (C) 2026 Evoca
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************/

#include "gui.h"
#include "osd.h"
#include "../peripheral.h"
#include "../sgx/sgx.h"
#include "../config.h"
#include <dirent.h>
#include <spng.h>


GXTexObj gui_tobj;
GXTexObj gui_img_tobj;
GXTexRegion gui_treg;
GXTexRegion gui_img_treg;
u32 *gui_img_data;

u8 block_4kb[4096] ATTRIBUTE_ALIGN(4096) = {200};

extern u8 menu_tex_4bpp_data[];

/*System texture dimensions*/
#define GUI_SYSTEX_W	256
#define GUI_SYSTEX_H	64

#define CURSOR_COLOR_A	0x72ad84ff
#define CURSOR_COLOR_B	0xb56d8cff


#define GUI_HBAR_X		216
#define GUI_HBAR_Y		0
#define GUI_HBAR_W		320
#define GUI_HBAR_H		16

//Only buttons necesary for gui nav
#define GUI_PAD_A		0x0001
#define GUI_PAD_B		0x0002
#define GUI_PAD_R		PAD_DI_R
#define GUI_PAD_STR		PAD_DI_STR
#define GUI_PAD_UP		PAD_DI_UP
#define GUI_PAD_DOWN	PAD_DI_DOWN
#define GUI_PAD_LEFT	PAD_DI_LEFT
#define GUI_PAD_RIGHT	PAD_DI_RIGHT

#define GUI_COLOR_LIGHT		0x4259BDFF
#define GUI_COLOR_MEDIUM	0x273176FF
#define GUI_COLOR_DARK		0x070934FF


#define GUI_FILENAMES_MAX		16

static u32 cursor_longname = 0;
static s32 cursor_idle = 128;
static u32 cursor_time = 128;
static u32 cursor_inc = 2;
static char *selected_file = NULL;


const String gui_messages[8] = {
	GUI_STRING(" Could not open games   folder"),
	{18,"No BIOS.bin"
		"  found"},
	{19,"No games   "
		" found"},
	{11,"Game saved!"},
	{20,"Could not  "
		"save game"},
};


GuiOption gui_opts[CONF_OPT_MAX] = {
	[CONF_OPT_SHOW_FPS] = {
		.name  = GUI_STRING("Show FPS"),
		.opt_names = {
			[0] = GUI_STRING("No"),
			[1] = GUI_STRING("Yes")
		},
		.count = 2
	},
	[CONF_OPT_NATIVE_VIDEO] = {
		.name  = GUI_STRING("Native Video"),
		.opt_names = {
			[0] = GUI_STRING("No"),
			[1] = GUI_STRING("Yes")
		},
		.count = 2
	},
	[CONF_OPT_USE_MULTITAP] = {
		.name  = GUI_STRING("Connect Multitap"),
		.opt_names = {
			[0] = GUI_STRING("Off"),
			[1] = GUI_STRING("1 Port"),
			[2] = GUI_STRING("2 Ports")
		},
		.count = 3
	}
//	[CONF_OPT_INPUT_TYPE] = {
//		.name  = GUI_STRING("Input Type"),
//#if 0
//		.opt_names = {
//			[0] = GUI_STRING("Standard"),
//			[1] = GUI_STRING("Analogue"),
//			[2] = GUI_STRING("Virtua Gun"),
//			[3] = GUI_STRING("Mouse")
//		},
//		.count = 4
//#else
//		.opt_names = {
//			[0] = GUI_STRING("Standard"),
//			[1] = GUI_STRING("Virtua Gun")
//		},
//		.count = 2
//	}
//#endif
};

u32 menu_select_index = 0;
u32 axis_button = 0;
u32 axis_button_prev = 0;
u32 menu_num_items = 4;

static u32 __menu_LerpRGBA8888(u32 a, u32 b, u32 t) {
	u32 rg_a = ((a >> 8) & 0x00FF00FF);
	u32 ba_a = (a & 0x00FF00FF);

	u32 rg_b = ((b >> 8) & 0x00FF00FF);
	u32 ba_b = (b & 0x00FF00FF);

	t = t & 0xFF;
	u32 nt = 0xFF - t;

	return ( ((nt * rg_a) + (t * rg_b))       & 0xFF00FF00) |
	       ((((nt * ba_a) + (t * ba_b)) >> 8) & 0x00FF00FF);
}



const u32 gui_palette[] = {
	0x404040ff, 0xa0b0a0ff, 0x00d0d090, 0xFFFFFFFF, 0x0,
};


typedef struct GuiButton_t {
	String item;

} GuiButton;

typedef struct GuiButtonArray_t {
	u32 cursor;				//position of cursor
	u32 h_cells;			//number of horizontal spaces
	u32 v_cells;			//number of vertical spaces
	u32 button_count;		//number of buttons
	//Position on screen
	u16 x;
	u16 y;
	//Array of buttons
	GuiButton *buttons;
} GuiButtonArray;

typedef struct GuiDial_t {
	u32 selected_value;			//position of cursor
	u32 value_count;			//number of entries displayed
	//Array of values
	String *values;
	String name;
} GuiDial;

typedef struct GuiDialArray_t {
	u32 cursor;			//position of cursor
	u32 dial_count;		//offset of list displayed
	//Position on screen
	u16 x;
	u16 y;
	//Array of strings
	GuiDial *dials;
} GuiDialrray;


typedef struct GuiFilelist_t {
	u32 cursor;				//position of cursor
	u32 page;				//page of cursor
	u32 page_item_count;	//number of items on page
	u32 count;				//number of entries
	u32 disp_offset;		//offset of list displayed
	u32 disp_count;			//number of entries displayed
	//Position on screen
	u16 x;
	u16 y;
	//Array of strings
	String *item;
} GuiFileList;

GuiFileList filelist = {
	.cursor = 0,
	.count = 0,
	.disp_offset = 0,
	.disp_count = GUI_FILENAMES_MAX,
	.page = 0,
	.page_item_count = 0,
	.x = 0,
	.y = 10,
	.item = NULL
};

#define GUI_OPTIONS_MIN 0
#define GUI_OPTIONS_MAX 200

u32 in_options = 0;
s32 options_panel = 0;
s32 option_selected = 0;


#define GUI_IMGTEX_ADDRESS (0x40000)
#define COVER_MAX_WIDTH 	(128)
#define COVER_MAX_HEIGHT 	(216)
#define COVER_SIZE 			(COVER_MAX_WIDTH*COVER_MAX_HEIGHT*2)

u32 gui_img_w;
u32 gui_img_h;
u8 *img_final;

void __convertPNGtoRGB5A3(u32 w, u32 h, u32 *src, u16* dst)
{
	u32 *src0 = src;
	u32 *src1 = src+(w);
	u32 *src2 = src+(w*2);
	u32 *src3 = src+(w*3);

	u16 *dst0 = dst;
	u16 *dst1 = dst+4;
	u16 *dst2 = dst+8;
	u16 *dst3 = dst+12;
	h >>= 2;
	u32 tile = 0;
	for (u32 i = 0; i < h; ++i) {
		//TEST
		for (u32 j = 0; j < w; ++j) {
			u32 p0 = *src0;
			u32 p1 = *src1;
			u32 p2 = *src2;
			u32 p3 = *src3;
			*dst0 = 0x8000 | ((p0 >> (9+8)) & 0x7C00) | ((p0 >> (6+8)) & 0x03E0) | ((p0 >> (3+8)) & 0x001F);
			*dst1 = 0x8000 | ((p1 >> (9+8)) & 0x7C00) | ((p1 >> (6+8)) & 0x03E0) | ((p1 >> (3+8)) & 0x001F);
			*dst2 = 0x8000 | ((p2 >> (9+8)) & 0x7C00) | ((p2 >> (6+8)) & 0x03E0) | ((p2 >> (3+8)) & 0x001F);
			*dst3 = 0x8000 | ((p3 >> (9+8)) & 0x7C00) | ((p3 >> (6+8)) & 0x03E0) | ((p3 >> (3+8)) & 0x001F);
			src0++; dst0++;
			src1++; dst1++;
			src2++; dst2++;
			src3++; dst3++;
			tile++;
			if (tile & 4) {
				dst0 += 12;
				dst1 += 12;
				dst2 += 12;
				dst3 += 12;
				tile = 0;
			}
		}
		src0 += w*3;
		src1 += w*3;
		src2 += w*3;
		src3 += w*3;
	}
	DCStoreRange(img_final, COVER_SIZE);
}


//This should b
static void __gui_LoadPNG(u32 texmap, char *filename)
{
	char tmpstr[256];
	struct spng_ihdr ihdr;
	FILE *png_fp;

	u32 filename_len = strlen(filename);

	sprintf(tmpstr, "%s/%.*s.png", conf.dir[CONF_COVERS], filename_len - 4, filename);
	png_fp = fopen(tmpstr, "rb");
	if (!png_fp) {
		gui_img_w = 0;
		return;
	}

	spng_ctx *png_ctx;
	png_ctx = spng_ctx_new(0);
	spng_set_chunk_limits(png_ctx, 1024*64, 1024*64);
	spng_set_png_file(png_ctx, png_fp);
	u32 ret = spng_get_ihdr(png_ctx, &ihdr);

	if (!ret && ihdr.width == COVER_MAX_WIDTH && ihdr.width <= COVER_MAX_HEIGHT) {
		if (!img_final) {
			img_final = memalign(32, COVER_SIZE);
		}
		u8 *buffer = malloc(COVER_SIZE*2);
		ret = spng_decode_image(png_ctx, buffer, COVER_SIZE*2, SPNG_FMT_RGBA8, 0);
		if (ret) {
			gui_img_w = 0; //Image was not loaded
			goto finish;
		}
		//Convert image data to RGB555A3 wii format
		__convertPNGtoRGB5A3(ihdr.width, COVER_MAX_HEIGHT, (u32*) buffer, (u16*) img_final);

		gui_img_w = ihdr.width;
		gui_img_h = ihdr.height;
		GX_InitTexObj(&gui_img_tobj, img_final, ihdr.width, ihdr.height, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObjLOD(&gui_img_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
		GX_InitTexPreloadRegion(&gui_img_treg, GUI_IMGTEX_ADDRESS, COVER_SIZE, 0, 0);
		GX_PreloadEntireTexture(&gui_img_tobj, &gui_img_treg);
		free(buffer);
	} else {
		gui_img_w = 0; //Image was not loaded
	}

finish:
	fclose(png_fp);
	spng_ctx_free(png_ctx);
}

u32 gui_texured = TRUE;

static void _gui_setupUntextured(void)
{
	if (gui_texured) {
		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);

		GX_SetNumChans(1);
		GX_SetNumTexGens(0);
		GX_SetNumTevStages(1);

		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP_NULL, GX_COLOR0);
		gui_texured = FALSE;
	}
}


static void _gui_setupTextured(void)
{
	if (!gui_texured) {
		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GX_SetNumChans(1);
		GX_SetNumTexGens(1);
		GX_SetNumTevStages(1);

		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_RASA, GX_CA_TEXA, GX_CA_ZERO);

		GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP7, GX_COLOR0A0);
		GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, MTX_IDENTITY);
		GX_LoadTexObjPreloaded(&gui_tobj, &gui_treg, GX_TEXMAP7);
		GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 4, 8);
		gui_texured = TRUE;
	}
}

static inline void gui_DrawRect(u32 x, u32 y, u32 w, u32 h, u32 color)
{
	_gui_setupUntextured();

	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);
	w += x;
	h += y;
	GX_Begin(GX_QUADS, GX_VTXFMT7, 4);
		GX_Position2u16(x, y);		// Top Left
		GX_Color1u32(color);
		GX_Position2u16(w, y);		// Top Right
		GX_Color1u32(color);
		GX_Position2u16(w, h);		// Bottom Right
		GX_Color1u32(color);
		GX_Position2u16(x, h);		// Bottom Left
		GX_Color1u32(color);
	GX_End();
}

static inline void gui_DrawTexRect(u32 x, u32 y, u32 w, u32 h, u32 tx, u32 ty, u32 color)
{
	_gui_setupTextured();
	s32 tw = tx + (w >> 2);
	s32 th = ty + (h >> 3);
	w += x;
	h += y;
	GX_Begin(GX_QUADS, GX_VTXFMT7, 4);
		GX_Position2u16(x, y);		// Top Left
		GX_Color1u32(color);
		GX_TexCoord2s16(tx, ty);
		GX_Position2u16(w, y);		// Top Right
		GX_Color1u32(color);
		GX_TexCoord2s16(tw, ty);
		GX_Position2u16(w, h);		// Bottom Right
		GX_Color1u32(color);
		GX_TexCoord2s16(tw, th);
		GX_Position2u16(x, h);		// Bottom Left
		GX_Color1u32(color);
		GX_TexCoord2s16(tx, th);
	GX_End();
}


void gui_DrawString(u32 x, u32 y, u32 w, String str)
{
	_gui_setupTextured();
	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);

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

#define GUI_SYSTEX_ADDRESS 0x58000

void gui_Init(void)
{
	//initial setup
	axis_button = 0;
	axis_button_prev = 0;

	//Initialize all GX objects
	GX_InitTexObj(&gui_tobj, menu_tex_4bpp_data, GUI_SYSTEX_W, GUI_SYSTEX_H, GX_TF_I4, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_InitTexObjLOD(&gui_tobj, GX_NEAR, GX_NEAR, 0, 0, 0, GX_DISABLE, GX_DISABLE, GX_ANISO_1);
	GX_InitTexPreloadRegion(&gui_treg, GUI_SYSTEX_ADDRESS, (GUI_SYSTEX_W * GUI_SYSTEX_H) >> 1, 0, GX_TEXCACHE_NONE);
	GX_PreloadEntireTexture(&gui_tobj, &gui_treg);

	osd_Init();
}

static void gui_DrawControllers(u32 x, u32 y)
{
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_RASA, GX_CA_TEXA, GX_CA_ZERO);

	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP7, GX_COLOR0A0);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, MTX_IDENTITY);
	GX_LoadTexObjPreloaded(&gui_tobj, &gui_treg, GX_TEXMAP7);

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 4, 8);
	//u8 pad[8] = {1, 0, 2, 1, 0, 0, 0, 0};
	const u32 num_pads = PER_PADMAX;
	GX_Begin(GX_QUADS, GX_VTXFMT7, num_pads*4*2);
	u32 act_ports = 2;
	if (gui_opts[CONF_OPT_USE_MULTITAP].selected == 1) {
		act_ports = 7;
	} else if (gui_opts[CONF_OPT_USE_MULTITAP].selected == 2) {
		act_ports = 8;
	}

	// Only display 2 controllers for now
	for (u32 i = 0; i < num_pads; ++i) {
		u32 dx = x + ((i & 1) * 32);
		u32 dy = y + ((i & ~1) * 11);
		u32 num = i << 1;
		u32 pad_tex = perpad[i].type << 3;
		u32 color = (perpad[i].type && i < act_ports ? 0xffeceeCC : 0xbdbdfb88);
		GX_Position2s16(dx, dy);		// Top Left
		GX_Color1u32(color);
		GX_TexCoord2s16(pad_tex+16, 4);
		GX_Position2s16(dx+32, dy);	// Top Right
		GX_Color1u32(color);
		GX_TexCoord2s16(pad_tex+24, 4);
		GX_Position2s16(dx+32, dy+24);	// Bottom Right
		GX_Color1u32(color);
		GX_TexCoord2s16(pad_tex+24, 7);
		GX_Position2s16(dx, dy+24);	// Bottom Left
		GX_Color1u32(color);
		GX_TexCoord2s16(pad_tex+16, 7);

		GX_Position2s16(dx+24, dy+16);		// Top Left
		GX_Color1u32(color);
		GX_TexCoord2s16(num+32, 7);
		GX_Position2s16(dx+32, dy+16);	// Top Right
		GX_Color1u32(color);
		GX_TexCoord2s16(num+34, 7);
		GX_Position2s16(dx+32, dy+24);	// Bottom Right
		GX_Color1u32(color);
		GX_TexCoord2s16(num+34, 8);
		GX_Position2s16(dx+24, dy+24);	// Bottom Left
		GX_Color1u32(color);
		GX_TexCoord2s16(num+32, 8);
	}
	GX_End();
}

static void gui_DrawItems(GuiFileList *items, u32 width, u32 height)
{

	GX_SetScissor(0, 0, width * 2, height * 2);

	if (items->count == 0) {
		return;
	}

	u32 ofs_x = items->x, ofs_y = items->y;
	u32 shown_entries = (items->page + 1) * items->disp_count;
	shown_entries = (shown_entries > items->count ? items->disp_count - (shown_entries - items->count) : items->disp_count);
	if (!in_options) {
		//Draw cursor
		u32 cursor_y = ofs_y + (items->cursor * 12) - 2 + 12;
		u32 cursor_color = __menu_LerpRGBA8888(CURSOR_COLOR_A, CURSOR_COLOR_B, cursor_time);
		if (items->cursor < items->count) {
			cursor_longname = (items->item[items->cursor].len*6) > width;
		} else {
			cursor_longname = 0;
		}
		if(!cursor_longname) {
			cursor_idle = 32;
		} else if ( ((s32)items->item[items->cursor].len * 6) - ((s32)(width - 24) - (cursor_idle >> 1)) > 0) {
			--cursor_idle;
		}
		GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
		gui_DrawRect(ofs_x, cursor_y, width, 12, cursor_color);
	}

	_gui_setupTextured();
	GX_SetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);

	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, MTX_IDENTITY);
	GX_LoadTexObjPreloaded(&gui_tobj, &gui_treg, GX_TEXMAP7);

	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 4, 8);


	const u32 INDEX_COLOR = 0xdfffe1FF;
	for (int i = 0; i < shown_entries; ++i) {
		GX_Begin(GX_QUADS, GX_VTXFMT7, 4 * 4);
		u32 x = ofs_x + 24, y = (i * 12) + ofs_y + 12;
		u32 num = i + (items->page * items->disp_count);
		u32 xnum = x - 8;
			GX_Position2s16(xnum, y);					// Top Left
			GX_Color1u32(INDEX_COLOR);
			GX_TexCoord2s16(10, 6);
			GX_Position2s16(xnum + 4, y);			// Top Right
			GX_Color1u32(INDEX_COLOR);
			GX_TexCoord2s16(11, 6);
			GX_Position2s16(xnum + 4, y + 8);	// Bottom Right
			GX_Color1u32(INDEX_COLOR);
			GX_TexCoord2s16(11, 7);
			GX_Position2s16(xnum, y + 8);			// Bottom Left
			GX_Color1u32(INDEX_COLOR);
			GX_TexCoord2s16(10, 7);
		for (u32 i = 0; i < 3; ++i) {
			u32 chr_x = (num % 10);
			xnum -= 4;
			//Draw the file number
			GX_Position2s16(xnum, y);					// Top Left
			GX_Color1u32(INDEX_COLOR);
			GX_TexCoord2s16(chr_x, 6);
			GX_Position2s16(xnum + 4, y);			// Top Right
			GX_Color1u32(INDEX_COLOR);
			GX_TexCoord2s16(chr_x + 1, 6);
			GX_Position2s16(xnum + 4, y + 8);	// Bottom Right
			GX_Color1u32(INDEX_COLOR);
			GX_TexCoord2s16(chr_x + 1, 7);
			GX_Position2s16(xnum, y + 8);			// Bottom Left
			GX_Color1u32(INDEX_COLOR);
			GX_TexCoord2s16(chr_x, 7);
			num /= 10;
		}
		GX_End();
	}

	GX_SetScissor((ofs_x + 24) * 2, 0, (width - (ofs_x + 24)) * 2, 480);

	// Draw text
	char tmpstr_data[16];
	String tmpstr = {.data = tmpstr_data};
	tmpstr.len = sprintf(tmpstr_data, "%2u/%2u", items->page+1, (items->count / items->disp_count) + 1);

	gui_DrawString(204, 222, tmpstr.len * 6, tmpstr);

	for (int i = 0; i < shown_entries; ++i) {
		s32 x = ofs_x + 24, y = (i * 12) + ofs_y + 12;
		u32 num = i + (items->page * items->disp_count);
		if (items->cursor == num && cursor_idle < 0) {
			x += cursor_idle >> 1;
		}
		gui_DrawString(x, y, items->item[num].len * 6, items->item[num]);
	}

	GX_SetScissor(0, 0, 640, 480);
}


static void gui_DrawOptions(u32 x, u32 y, u32 w)
{
	u32 x_right = x + w;
	if (in_options) {
		u32 cursor_color = __menu_LerpRGBA8888(CURSOR_COLOR_A, CURSOR_COLOR_B, cursor_time);
		u32 max_strlen = 0;
		for (u32 j = 0; j < gui_opts[option_selected].count; ++j) {
			max_strlen = MAX(max_strlen, gui_opts[option_selected].opt_names[j].len);
		}

		GX_SetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
		gui_DrawRect(x_right - ((max_strlen*6)+2), y + (option_selected * 14)-3, max_strlen*6 + 5, 13, cursor_color);
	}
	for (u32 i = 0; i < CONF_OPT_MAX; ++i) {
		if (gui_opts[i].count == 0) {
			continue;
		}
		u32 max_strlen = 0;
		for (u32 j = 0; j < gui_opts[i].count; ++j) {
			max_strlen = MAX(max_strlen, gui_opts[i].opt_names[j].len);
		}

		gui_DrawString(x, y, 640, gui_opts[i].name);
		gui_DrawString(x_right - (max_strlen*6), y, 640, gui_opts[i].opt_names[gui_opts[i].selected]);

		if (i == option_selected) {
			gui_DrawTexRect(x_right - ((max_strlen*6)+10), y, 8, 8, 48, 7, 0xFFFFFFFF);
			gui_DrawTexRect(x_right + 3, y, 8, 8, 50, 7, 0xFFFFFFFF);
		}
		y += 14;
	}

}

void gui_Draw(GuiFileList *items)
{
	/*Reserve GX_VTXFMT7 for OSD & GUI*/
	GX_SetScissor(0, 0, 640, 480);
	GX_SetCurrentMtx(MTX_IDENTITY_2X);

	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_KONST);

	//Only if alpha is checked
	GX_SetNumIndStages(0);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);

	gui_DrawRect(0, 0, 320, 240, GUI_COLOR_DARK);
	gui_DrawRect(20, 17, 220 - options_panel, 198, GUI_COLOR_LIGHT);
	gui_DrawRect(242 - options_panel, 15, 78+GUI_OPTIONS_MAX, 202, GUI_COLOR_MEDIUM);
	gui_DrawRect(0, 12, 320, 2, GUI_COLOR_LIGHT);
	gui_DrawRect(0, 218, 320, 2, GUI_COLOR_LIGHT);

	if (gui_img_w == 128) {
		u32 img_x = 248 - options_panel;
		u32 img_y = 120 + ((COVER_MAX_HEIGHT - gui_img_h) >> 2);
		u32 img_w = gui_img_w >> 1;
		u32 img_h = gui_img_h >> 1;
		gui_DrawRect(img_x-2, img_y-2, img_w+4, img_h+4, GUI_COLOR_LIGHT);
	}

	GX_LoadTexObjPreloaded(&gui_tobj, &gui_treg, GX_TEXMAP7);
	gui_DrawItems(items, 240 - options_panel, 216);
	gui_DrawControllers(245 - options_panel, 18);

	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_RASC, GX_CC_TEXA, GX_CC_ZERO);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);

	//A button
	if (!in_options) {
		gui_DrawTexRect( 26, 213, 16, 16, 0, 4, 0xEE2222FF);
		gui_DrawTexRect( 37, 222, 36, 8,  0, 7, 0xFFFFFFFF);
		//B button
		gui_DrawTexRect( 78, 213, 16, 16, 4, 4, 0x22EE22FF);
		gui_DrawTexRect( 89, 222, 28, 8, 10, 7, 0xFFFFFFFF);
		//S button
		gui_DrawTexRect(121, 213, 16, 16, 8, 4, 0xEEEE22FF);
		gui_DrawTexRect(132, 222, 40, 8, 17, 7, 0xFFFFFFFF);
	}

	gui_DrawOptions(242 + 88 - options_panel, 32, 168);

	//If we have a cover loaded
	if (gui_img_w == 128) {
		GX_ClearVtxDesc();
		GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GX_SetNumChans(1);
		GX_SetNumTexGens(1);
		GX_SetNumTevStages(1);
		GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
		GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
		GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
		GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_TEXA);
		GX_LoadTexObjPreloaded(&gui_img_tobj, &gui_img_treg, GX_TEXMAP7);

		GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_TRUE, 1, 1);

		u32 img_x = 248 - options_panel;
		u32 img_y = 120 + ((COVER_MAX_HEIGHT - gui_img_h) >> 2);
		u32 img_w = gui_img_w >> 1;
		u32 img_h = gui_img_h >> 1;

		GX_Begin(GX_QUADS, GX_VTXFMT4, 4);
			GX_Position2u16(img_x, img_y);					// Top Left
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(0, 0);
			GX_Position2u16(img_x + img_w, img_y);			// Top Right
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(gui_img_w, 0);
			GX_Position2u16(img_x + img_w, img_y + img_h);	// Bottom Right
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(gui_img_w, gui_img_h);
			GX_Position2u16(img_x, img_y + img_h);			// Bottom Left
			GX_Color1u16(0xbdd7);
			GX_TexCoord2u8(0, gui_img_h);
		GX_End();
	}

	cursor_time += cursor_inc;
	if (cursor_time <= 2 || cursor_time >= 254) {
		cursor_inc = -cursor_inc;
	}

	GX_SetLineWidth(1 << 2, 0);
}

char* game_name_strings;
u32 games_filecount;
u32 games_cursor;
u32 games_page;
//u32 games_cursor;

static int fn_compare(const void *x, const void *y)
{
	return strcasecmp(((String *)x)->data, ((String *)y)->data);
}

u32 gui_LoadGameList(void)
{

	u32 str_pos = 0;
	struct dirent *entry;
	DIR *dp = opendir(conf.dir[CONF_GAMES]);
	if (!dp) {
		//gui_SetMessage(gui_messages[MESSAGE_NO_FOLDER], 0xffff);
		return 0;
	}
	//Allocate memory for filename storing
	game_name_strings = (char*) calloc(0x10000, sizeof(*game_name_strings));	//64KB for now
	filelist.item = (String*) calloc(1024, sizeof(*filelist.item));	//1024 entries
	games_filecount = 0;

	while ((entry = readdir(dp))) {
		//Copy the string to memory
		u32 len = 0;
		char *str_src = entry->d_name;
		char *str_dst = &game_name_strings[str_pos];
		while (*str_src != 0) {
			*str_dst = *str_src;
			++len;
			++str_dst;
			++str_src;
		}
		++len;
		*str_dst = '\0';
		//XXX: Be careful with this implementation, can have problems
		if (len > 4) {
			if (!strcmp(".cue", str_dst - 4)) {
				filelist.item[games_filecount].len = len;
				filelist.item[games_filecount].data = &game_name_strings[str_pos];
				games_filecount++;
				str_pos += len;
			} else if (!strcmp(".chd", str_dst - 4)) {
				filelist.item[games_filecount].len = len;
				filelist.item[games_filecount].data = &game_name_strings[str_pos];
				games_filecount++;
				str_pos += len;
			}
		}
	}

	filelist.count = games_filecount;
	u32 shown_entries = (filelist.page + 1) * filelist.disp_count;
	filelist.page_item_count = filelist.disp_count;
	filelist.page_item_count -= (shown_entries > filelist.count ? shown_entries - filelist.count : 0);
	if (!filelist.count) {
		closedir(dp);
		//gui_SetMessage(gui_messages[MESSAGE_NO_GAMES], 0xffff);
		return 0;
	}

	qsort(filelist.item, games_filecount, sizeof(*filelist.item), fn_compare);
	closedir(dp);
	return filelist.count;
}


char* gui_GetSelectedFile(void)
{
	return selected_file;
}

void gui_SetSelectedFile(char* filename)
{
	selected_file = filename;
}

//gui input handles only buttons B, A, Start, R and Dpad
static u32 __gui_GetInput(void) {
	per_updatePads();
	u32 buttons = PER_BUTTONS_DOWN(0);
	u32 gui_btns = buttons & ~3;
	//Remap A/B buttons depending on type
	if (perpad[0].type == PAD_TYPE_GCPAD) {
		gui_btns |=  (buttons >> PAD_DI_BIT_B) & GUI_PAD_A;
		gui_btns |= ((buttons >> PAD_DI_BIT_A) << 1) & GUI_PAD_B;
	} else if (perpad[0].type == PAD_TYPE_N64PAD){
		gui_btns |=  (buttons >> PAD_DI_BIT_A) & GUI_PAD_A;
		gui_btns |= ((buttons >> PAD_DI_BIT_X) << 1) & GUI_PAD_B;
	} else {
		gui_btns |= (buttons >> PAD_DI_BIT_C) & GUI_PAD_A;
		gui_btns |= ((buttons >> PAD_DI_BIT_B) << 1) & GUI_PAD_B;
	}
	return gui_btns;
}

u32 gui_Exec(void)
{
	if (gui_img_w == 128) { // Cover art is loaded
		GX_PreloadEntireTexture(&gui_img_tobj, &gui_img_treg);
	} else {
		__gui_LoadPNG(GX_TEXMAP1, filelist.item[(filelist.page * filelist.disp_count) + filelist.cursor].data);
	}

	while (1) {
		u32 buttons = __gui_GetInput();

		if (!in_options) {
			//Previous selected game
			u32 prev_selected_file = (filelist.page * filelist.disp_count) + filelist.cursor;
			if(buttons & GUI_PAD_DOWN) {
				if (filelist.cursor == (filelist.page_item_count-1)) {
					filelist.cursor = 0;
				}
				else {
					filelist.cursor++;
				}
			}
			else if(buttons & GUI_PAD_UP) {
				if (filelist.cursor == 0) {
					filelist.cursor = filelist.page_item_count - 1;
				}
				else {
					filelist.cursor--;
				}
			} else if(buttons & GUI_PAD_LEFT) {
				if (filelist.page == 0) {
					filelist.page = filelist.count / GUI_FILENAMES_MAX;
				} else {
					filelist.page--;
				}

				//Set number of shown items
				u32 shown_entries = (filelist.page + 1) * filelist.disp_count;
				filelist.page_item_count = filelist.disp_count;
				filelist.page_item_count -= (shown_entries > filelist.count ? shown_entries - filelist.count : 0);
				filelist.cursor = MIN(filelist.page_item_count - 1, filelist.cursor);
			} else if(buttons & GUI_PAD_RIGHT) {
				if (filelist.page == (filelist.count / GUI_FILENAMES_MAX)) {
					filelist.page = 0;
				} else {
					filelist.page++;
				}

				//Set number of shown items
				u32 shown_entries = (filelist.page + 1) * filelist.disp_count;
				filelist.page_item_count = filelist.disp_count;
				filelist.page_item_count -= (shown_entries > filelist.count ? shown_entries - filelist.count : 0);
				filelist.cursor = MIN(filelist.page_item_count - 1, filelist.cursor);
			} else if (buttons & GUI_PAD_A) {	//Uses the C button as A button
				//Set all options to configuration
				for(u32 i = 0; i < CONF_OPT_MAX; ++i) {
					config_SetOption(i, gui_opts[i].selected);
				}

#ifdef DO_PROFILE
				//Hold R to turn on profiler
				config_SetOption(CONF_OPT_SHOW_PROFILE, (PER_BUTTONS_HELD(0) & PAD_DI_R) != 0);
#endif /* DO_PROFILE */

				//Set up file name
				u32 selected_file_num = (filelist.page * filelist.disp_count) + filelist.cursor;
				selected_file = filelist.item[selected_file_num].data;
				SVI_ClearAllFrames();
				return 1;
			} else if (buttons & GUI_PAD_STR) {	//Go into options menu
				in_options ^= 1;
			} else if (buttons & GUI_PAD_B) {	//Uses the B button as B button
				mem_Deinit();
				return 0;
			}

			//item changed, load new image
			if (prev_selected_file != ((filelist.page * filelist.disp_count) + filelist.cursor)) {
				__gui_LoadPNG(GX_TEXMAP1, filelist.item[(filelist.page * filelist.disp_count) + filelist.cursor].data);
			}
		} else { // In options panel
			//Previous selected game
			if(buttons & GUI_PAD_DOWN) {
				if (option_selected == (CONF_OPT_MAX-1)) {
					option_selected = 0;
				}
				else {
					option_selected++;
				}
			}
			else if(buttons & GUI_PAD_UP) {
				if (option_selected == 0) {
					option_selected = CONF_OPT_MAX - 1;
				}
				else {
					option_selected--;
				}
			} else if(buttons & GUI_PAD_LEFT) {
				if (gui_opts[option_selected].selected == 0) {
					gui_opts[option_selected].selected = gui_opts[option_selected].count - 1;
				} else {
					gui_opts[option_selected].selected--;
				}
			} else if(buttons & GUI_PAD_RIGHT) {
				if (gui_opts[option_selected].selected == gui_opts[option_selected].count - 1) {
					gui_opts[option_selected].selected = 0;
				} else {
					gui_opts[option_selected].selected++;
				}
			} else if (buttons & GUI_PAD_STR) {	//Exit options menu
				in_options ^= 1;
			}
		}

		if (in_options) {
			if (options_panel < GUI_OPTIONS_MAX) {
				options_panel += 20;
			} else if (options_panel > GUI_OPTIONS_MAX) {
				options_panel = GUI_OPTIONS_MAX;
			}
		} else {
			if (options_panel > GUI_OPTIONS_MIN) {
				options_panel -= 20;
			} else if (options_panel < GUI_OPTIONS_MIN) {
				options_panel = GUI_OPTIONS_MIN;
			}
		}

		gui_Draw(&filelist);
		SVI_CopyFrame();
		SVI_SwapBuffers(1);
	}
}

u32 gui_MsgBox(String *btn_labels, u32 btns_count, char *msg_format, ...)
{
	#if 1
	char tmpstr[256];
	va_list l;
	va_start(l, msg_format);
	String msg;
	msg.len = vsprintf(tmpstr, msg_format, l);
	msg.data = tmpstr;
	va_end(l);

	_gui_setupUntextured();
	_gui_setupTextured();

	const u32 msgbox_w = 200;
	const u32 msgbox_h = 90;
	const u32 msgbox_x = (320 - msgbox_w) >> 1;
	const u32 msgbox_y = (240 - msgbox_h) >> 1;
	const u32 margin = 16;
	const u32 btn_size = 32;
	const u32 btns_y = msgbox_y + msgbox_h - 16;

	u32 selected_btn = 0;
	//Basic Setup for gui
	while (1) {
		u32 buttons = __gui_GetInput();

		if (buttons & GUI_PAD_LEFT) {
			if (selected_btn > 0) {
				selected_btn--;
			}
		} else if (buttons & GUI_PAD_RIGHT) {
			if (selected_btn < btns_count-1) {
				selected_btn++;
			}
		} else if (buttons & GUI_PAD_A) {
			SVI_ClearFrame();
			SVI_SwapBuffers(1);
			SVI_ClearFrame();
			SVI_SwapBuffers(1);
			return selected_btn;
		}

		//Only if alpha is checked
		/*Reserve GX_VTXFMT7 for OSD & GUI*/
		GX_SetScissor(0, 0, 640, 480);
		GX_SetCurrentMtx(MTX_IDENTITY_2X);
		GX_SetNumIndStages(0);
		GX_SetTevDirect(GX_TEVSTAGE0);
		GX_SetTevKAlphaSel(GX_TEVSTAGE0, GX_TEV_KASEL_1);

		GX_LoadTexObjPreloaded(&gui_tobj, &gui_treg, GX_TEXMAP7);

		u32 btns_distance = (msgbox_w - btn_size) / btns_count;
		u32 btns_start = 160 - ((btns_distance*(btns_count-1) + btn_size)>> 1);

		gui_DrawRect(msgbox_x - 4, msgbox_y - 4, msgbox_w + 8, msgbox_h + 8, GUI_COLOR_LIGHT);
		gui_DrawRect(msgbox_x - 3, msgbox_y - 3, msgbox_w + 6, msgbox_h + 6, GUI_COLOR_DARK);
		gui_DrawRect(msgbox_x - 2, msgbox_y - 2, msgbox_w + 4, msgbox_h + 4, GUI_COLOR_LIGHT);
		gui_DrawRect(msgbox_x, msgbox_y, msgbox_w, msgbox_h, GUI_COLOR_MEDIUM);
		gui_DrawRect(btns_start + btns_distance*selected_btn - 2, btns_y - 2, btn_size, 14, 0x88000000);
		gui_DrawString(msgbox_x + margin, msgbox_y + margin, msgbox_w - (margin << 1), msg);


		for (u32 i = 0; i < btns_count; ++i) {
			gui_DrawString(btns_start + btns_distance*i, btns_y, btn_size, btn_labels[i]);
		}

		SVI_CopyFrame();
		SVI_SwapBuffers(1);
	}
		#endif
	return 0;
}