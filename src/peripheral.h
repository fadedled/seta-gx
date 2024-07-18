/*  Copyright 2005 Guillaume Duhamel
    Copyright 2005-2006 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef PERIPHERAL_H
#define PERIPHERAL_H

#include "core.h"
#include "smpc.h"
#include "yabause.h"

/** @defgroup peripheral Peripheral
 *
 * @brief This module provides two kind of functions
 * - peripheral core management functions
 * - controller ports management functions
 *
 * @{
 */

#define PERPAD   0x02
#define PERMOUSE 0xE3

#define PERCORE_DEFAULT -1
#define PERCORE_DUMMY 0

extern PortData_struct PORTDATA1;
extern PortData_struct PORTDATA2;

typedef struct
{
   int id;
   const char * Name;
   int (*Init)(void);
   void (*DeInit)(void);
   int (*HandleEvents)(void);
   void (*PerSetButtonMapping)(void);
   u32 (*Scan)(void);
   int canScan;
   void (*Flush)(void);
#ifdef PERKEYNAME
   void (*KeyName)(u32 key, char * name, int size);
#endif
} PerInterface_struct;

/** @brief Pointer to the current peripheral core.
 *
 * You should not set this manually but use
 * PerInit() and PerDeInit() instead. */
extern PerInterface_struct * PERCore;

extern PerInterface_struct PERDummy;

/**
 * @brief Init a peripheral core
 *
 * Searches through the PERCoreList array for the given coreid.
 * If found, PERCore is set to the address of that core and
 * the core's Init function is called.
 *
 * @param coreid the peripheral core to be used
 * @return 0 if core has been inited, -1 otherwise
 */
int PerInit(int coreid);
/**
 * @brief De-init a peripheral core
 *
 * Calls the core's DeInit callback and set PERCore to NULL.
 */
void PerDeInit(void);

/** @brief Adds a peripheral
 *
 * You shouldn't directly use this function but
 * PerPadAdd() or PerMouseAdd() instead.
 */
void * PerAddPeripheral(PortData_struct *port, int perid);
int PerGetId(void * peripheral);
void PerRemovePeripheral(PortData_struct *port, int removeoffset);
void PerPortReset(void);
/**
 * Iterate the list of peripherals connected to a port
 * and flush them if necesseray. This is needed for mouses.
 */
void PerFlush(PortData_struct * port);

void PerKeyDown(u32 key);
void PerKeyUp(u32 key);
void PerSetKey(u32 key, u8 name, void * controller);

/** @defgroup pad Pad
 *
 * @{
 */
#define PERPAD_UP					0
#define PERPAD_RIGHT				1
#define PERPAD_DOWN					2
#define PERPAD_LEFT					3
#define PERPAD_RIGHT_TRIGGER 		4
#define PERPAD_LEFT_TRIGGER 		5
#define PERPAD_START				6
#define PERPAD_A					7
#define PERPAD_B					8
#define PERPAD_C					9
#define PERPAD_X					10
#define PERPAD_Y					11
#define PERPAD_Z					12


extern const char * PerPadNames[14];

typedef struct
{
   u8 perid;
   u8 padbits[2];
} PerPad_struct;

/** @brief Adds a pad to one of the controller ports.
 *
 * @param port can be either &PORTDATA1 or &PORTDATA2
 * @return pointer to a PerPad_struct or NULL if it fails
 * */
PerPad_struct * PerPadAdd(PortData_struct * port);

void PerPadUpPressed(PerPad_struct * pad);
void PerPadUpReleased(PerPad_struct * pad);

void PerPadDownPressed(PerPad_struct * pad);
void PerPadDownReleased(PerPad_struct * pad);

void PerPadRightPressed(PerPad_struct * pad);
void PerPadRightReleased(PerPad_struct * pad);

void PerPadLeftPressed(PerPad_struct * pad);
void PerPadLeftReleased(PerPad_struct * pad);

void PerPadStartPressed(PerPad_struct * pad);
void PerPadStartReleased(PerPad_struct * pad);

void PerPadAPressed(PerPad_struct * pad);
void PerPadAReleased(PerPad_struct * pad);

void PerPadBPressed(PerPad_struct * pad);
void PerPadBReleased(PerPad_struct * pad);

void PerPadCPressed(PerPad_struct * pad);
void PerPadCReleased(PerPad_struct * pad);

void PerPadXPressed(PerPad_struct * pad);
void PerPadXReleased(PerPad_struct * pad);

void PerPadYPressed(PerPad_struct * pad);
void PerPadYReleased(PerPad_struct * pad);

void PerPadZPressed(PerPad_struct * pad);
void PerPadZReleased(PerPad_struct * pad);

void PerPadRTriggerPressed(PerPad_struct * pad);
void PerPadRTriggerReleased(PerPad_struct * pad);

void PerPadLTriggerPressed(PerPad_struct * pad);
void PerPadLTriggerReleased(PerPad_struct * pad);
/** @} */

/** @defgroup mouse Mouse
 *
 * @{
 * */
#define PERMOUSE_LEFT	13
#define PERMOUSE_MIDDLE	14
#define PERMOUSE_RIGHT	15
#define PERMOUSE_START	16

extern const char * PerMouseNames[5];

typedef struct
{
   u8 perid;
   u8 mousebits[3];
} PerMouse_struct;

/** @brief Adds a mouse to one of the controller ports.
 *
 * @param port can be either &PORTDATA1 or &PORTDATA2
 * @return pointer to a PerMouse_struct or NULL if it fails
 * */
PerMouse_struct * PerMouseAdd(PortData_struct * port);

void PerMouseLeftPressed(PerMouse_struct * mouse);
void PerMouseLeftReleased(PerMouse_struct * mouse);

void PerMouseMiddlePressed(PerMouse_struct * mouse);
void PerMouseMiddleReleased(PerMouse_struct * mouse);

void PerMouseRightPressed(PerMouse_struct * mouse);
void PerMouseRightReleased(PerMouse_struct * mouse);

void PerMouseStartPressed(PerMouse_struct * mouse);
void PerMouseStartReleased(PerMouse_struct * mouse);

void PerMouseMove(PerMouse_struct * mouse, s32 dispx, s32 dispy);
/** @} */





//NEW
typedef struct PerPad_t {
	u32 v0;
	u32 v1;
	u32 v2;
	u32 v3;
} PerPad;

#define PER_PADMAX					8
#define PER_DATASIZE				64

typedef struct PerData_t {
	u32 port2_offset;
	u32 data_size;
	u32 data_sent;
	u8 ids[PER_PADMAX];
	u8 data[PER_DATASIZE];
} PerData;

extern PerData per_data;


  /* Port Status:
  0x04 - Sega-tap is connected
  0x16 - Multi-tap is connected
  0x21-0x2F - Clock serial peripheral is connected
  0xF0 - Not Connected or Unknown Device
  0xF1 - Peripheral is directly connected */

  // PeripheralID:
#define PER_ID_DIGITAL		0x02
#define PER_ID_ANALOGUE		0x15
#define PER_ID_GUN			0x23
#define PER_ID_MOUSE		0xE3
#define PER_ID_NONE			0xFF

//Standard Sega Saturn Controller
#define PAD_DI_UP					0x0001
#define PAD_DI_RIGHT				0x0002
#define PAD_DI_DOWN					0x0004
#define PAD_DI_LEFT					0x0008
#define PAD_DI_R		 			0x0010
#define PAD_DI_L 					0x0020
#define PAD_DI_STR					0x0040
#define PAD_DI_A					0x0080
#define PAD_DI_B					0x0100
#define PAD_DI_C					0x0200
#define PAD_DI_X					0x0400
#define PAD_DI_Y					0x0800
#define PAD_DI_Z					0x1000

#define PAD_DI_BIT_B				0
#define PAD_DI_BIT_C				1
#define PAD_DI_BIT_A				2
#define PAD_DI_BIT_STR				3
#define PAD_DI_BIT_UP				4
#define PAD_DI_BIT_DOWN				5
#define PAD_DI_BIT_LEFT				6
#define PAD_DI_BIT_RIGHT			7
#define PAD_DI_BIT_L 				3
#define PAD_DI_BIT_Z				4
#define PAD_DI_BIT_X				5
#define PAD_DI_BIT_Y				6
#define PAD_DI_BIT_R		 		7


#define GC_BIT_LEFT				0
#define GC_BIT_RIGHT			1
#define GC_BIT_DOWN				2
#define GC_BIT_UP				3
#define GC_BIT_Z				4
#define GC_BIT_R		 		5
#define GC_BIT_L 				6
#define GC_BIT_Z2				7
#define GC_BIT_A				8
#define GC_BIT_B				9
#define GC_BIT_X				10
#define GC_BIT_Y				11
#define GC_BIT_STR				12

void per_initPad(u32 indx, u32 id);
u32 per_updatePads(void);
void per_closePad(u32 indx);

#define GC_AXIS_TO_DIGITAL(x, y)	((x < -46) | ((x > 46) << 1) | (((y < -54) << 2) | ((y > 54) << 3)))

/** @} */

#endif
