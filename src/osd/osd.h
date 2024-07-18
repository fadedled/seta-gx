

#ifndef __OSD_H__
#define __OSD_H__


/*
 * osd.h
 *--------------------
 * OSD for printing data
 */

#include <gccore.h>


#define MAX_MESSAGES        64
#define OSD_MSG_BG			0x01
#define OSD_MSG_MONO		0x02


void osd_MsgAdd(u32 x, u32 y, u32 color, char *str);
void osd_MsgShow(void);
void osd_CyclesSet(u32 indx, u64 cycles);


#endif /*__OSD_H__*/
