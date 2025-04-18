

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

enum {
	PROF_SH2M,
	PROF_SH2S,
	PROF_M68K,
	PROF_SCSP,
	PROF_SCU,
	PROF_SMPC,
	PROF_VDP1,
	PROF_VDP2,
	PROF_CDB
};

void osd_ProfInit(u32 sys_cycles);
void osd_ProfAddCounter(u32 indx, char *name);
void osd_ProfAddTime(u32 indx, u32 ticks);
void osd_ProfDraw(void);	//All counters reset to 0
void osd_FPSDraw(u32 fps);


#endif /*__OSD_H__*/
