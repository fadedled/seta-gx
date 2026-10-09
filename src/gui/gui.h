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

#ifndef __GUI_H__
#define __GUI_H__


/*
 * gui.h
 *--------------------
 * Simple graphical user interface for the wii
 */

#include <gccore.h>
#include <malloc.h>

#define MAX_MESSAGES        64
#define OSD_MSG_BG			0x01

#define GUI_ELEM_BOX		0x01
#define GUI_ELEM_LABEL		0x02
#define GUI_ELEM_IMAGE		0x03

#define GUI_BOX_BORDER		0x10

#define GUI_LABEL_BG		0x10
#define GUI_LABEL_MONO		0x20

#define GUI_XY(x, y)		((x & 0xFFFF) << 16 | (y & 0xFFFF))

#define GUI_QUIT			0x1
#define GUI_RETRY			0x2
#define GUI_RESUME			0x4
#define GUI_OK				0x8

enum ReturnValue {
	GUI_RET_NONE,
	GUI_RET_SELECT,
	GUI_RET_EXIT
};

typedef struct String_t {
	u32 len;
	char *data;
} String;

#define GUI_STRING(str)			(String){.data = str, .len = (sizeof(str)-1)}

typedef struct GuiLabel_t {
	u32 color;
	u16 x;
	u16 y;
	u32 len;
	void *data;	//NECESARY?
} GuiLabel;

typedef struct GuiItems_t {
	u32 cursor;				//position of cursor
	u32 page;				//page of cursor
	u32 page_count;			//number of entries on cursor
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

typedef struct GuiOption_t {
	String name;
	String opt_names[4];
	u32    count;
	u32    selected;
} GuiOption;


void  gui_Init(void);
u32   gui_LoadGameList(void);
u32   gui_Exec(void);
void  gui_SetMessage(String msg, u32 color);
char* gui_GetSelectedFile(void);
void  gui_SetSelectedFile(char* filename);
u32   gui_MsgBox(String *btn_labels, u32 btns_count, char *msg_format, ...);


#endif /*__GUI_H__*/
