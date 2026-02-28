

#ifndef __GUI_H__
#define __GUI_H__


/*
 * gui.h
 *--------------------
 * Simple graphical user interface for the wii
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

void gui_Init(void);
void gui_Draw(GuiItems *items);
void gui_SetMessage(String msg, u32 color);

//u32 gui_AnimSet(GuiAnim *anim, GuiElem *elems);




#endif /*__GUI_H__*/
