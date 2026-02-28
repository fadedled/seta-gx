

#ifndef __TGUI_H__
#define __TGUI_H__


/*
 * gui.h
 *--------------------
 * Simple graphical user interface for the Wii
 */

#include <gccore.h>


#define MAX_MESSAGES        64
#define OSD_MSG_BG			0x01

#define GUI_ELEM_BOX		0x01
#define GUI_ELEM_LABEL		0x02
#define GUI_ELEM_IMAGE		0x03

#define GUI_BOX_BORDER		0x10

#define GUI_LABEL_BG		0x10
#define GUI_LABEL_MONO		0x20

#define GUI_XY(x, y)		((x & 0xFFFF) << 16 | (y & 0xFFFF))

#define TEST_GUI_

enum ReturnValue {
	GUI_RET_NONE,
	GUI_RET_SELECT,
	GUI_RET_EXIT
};


extern GXTexObj gui_tobj;
extern GXTexRegion gui_treg;

typedef struct String_t {
	u32 len;
	char *data;
} String;

typedef struct GuiLabel_t {
	u32 color;
	u16 x;
	u16 y;
	u32 len;
	void *data;	//NECESARY?
} GuiLabel;

typedef struct GuiItems_t {
	u32 cursor;				//position of cursor
	u32 count;				//number of entries
	u32 disp_offset;		//offset of list displayed
	u32 disp_count;			//number of entries displayed
	//Position on screen
	u16 x;
	u16 y;
	//Array of strings
	String *item;
} GuiItems;

typedef struct GuiAnim {
	u32 type;
	u32 color_fg;
	u32 value;
	u32 time;
} GuiAnim;


typedef struct GuiAnim {
	u32 type;
	u32 color_fg;
	u32 value;
	u32 time;
} GuiItem;

typedef struct GuiImage {
	u32 x;
	u32 y;


} GuiImage;

typedef struct GuiList_t {
	GuiItem *items;
	u32 count;
	u32 padding;
} GuiList;

enum GuiElemTypes {
	GUIELEM_RECT,
	GUIELEM_LABEL,
	GUIELEM_TOGGLE,
	GUIELEM_LIST,
	GUIELEM_TABLE,
	GUIELEM_IMAGE
};

#define GUI_RGBA32(r, g, b, a)	((((r) & 0xFF) << 24) | (((g) & 0xFF) << 16) | (((b) & 0xFF) << 8) | ((a) & 0xFF))
#define GUI_ALPHA(color)		(color & 0xFF)
//General GUI element that renders diferently depending on type
typedef struct GuiElem_t GuiElem;

struct GuiElem_t {
	u32 type;     /* Element type  */
	u32 bg_color; /* Background color */
	u32 fg_color; /* Foreground color */
	u16 x;
	u16 y;
	u16 w;
	u16 h;

	struct { /* Rectangle section that describes a */
		GuiElem *elem;
		u32 padding;
	} rect;

	struct { /* Label value */
		String *text;
	} label;

	struct { /* Toggle */
		String *text;
		u32 value;
	} toggle;

	struct { /* List of elements */
		GuiElem *elems;
		u16 selected;
		u16 count;
	} list;

	struct { /* Table of elements */
		GuiElem *elems;
		u16 selected;
		u8 cell_w;
		u8 cell_h;
	} table;

	struct { /* Image element */
		u8 *img_data;
		u16 w;
		u16 h;
	} image;
};

typedef GuiElem *GuiMenu;



//===================================
// Functions
//===================================
void gui_SetMenu(GuiMenu menu);
void gui_DrawMenu(void);
void gui_Error(String msg);

//===================================

void gui_Test(void);


void gui_Init(void);
void gui_Draw(GuiItems *items);
void gui_SetMessage(String msg, u32 color);



#endif /*__TGUI_H__*/
