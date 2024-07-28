/*  src/scsp2.h: Header for new SCSP implementation
 *    Copyright 2010 Andrew Church
 *
 *    This file is part of Yabause.
 *
 *    Yabause is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    Yabause is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Yabause; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef SCSP_H  // Not SCSP2_H (we substitute for the original scsp.h)
#define SCSP_H

#include "core.h"  // For sized integer types
#include "snd.h"
///////////////////////////////////////////////////////////////////////////
// Breakpoint data structure (currently just an address)
typedef struct {
	u32 addr;
} M68KBreakpointInfo;

#ifdef SCSP_PLUGIN
#define SCSCORE_DEFAULT -1
#define SCSCORE_DUMMY   0
#define SCSCORE_SCSP1   1
#define SCSCORE_SCSP2   2

typedef struct
{
	int id;
	const char *Name;
	int (*Init)(int coreid, void (*interrupt_handler)(void));
	void (*DeInit)(void);
	void (*Reset)(void);
	int (*ChangeVideoFormat)(int type);
	void (*Exec)(int decilines);
	void (*MuteAudio)(int flags);
	void (*UnMuteAudio)(int flags);
	void (*SetVolume)(int volume);
	void (*M68KStart)(void);
	void (*M68KStop)(void);
	u8 FASTCALL (*SoundRamReadByte)(u32 addr);
	void FASTCALL (*SoundRamWriteByte)(u32 addr, u8 val);
	u16 FASTCALL (*SoundRamReadWord)(u32 addr);
	void FASTCALL (*SoundRamWriteWord)(u32 addr, u16 val);
	u32 FASTCALL (*SoundRamReadLong)(u32 addr);
	void FASTCALL (*SoundRamWriteLong)(u32 addr, u32 val);
	void (*ReceiveCDDA)(const u8 *sector);
	int (*SoundSaveState)(FILE *fp);
	int (*SoundLoadState)(FILE *fp, int version, int size);
	u32 FASTCALL (*M68KReadWord)(const u32 adr);
	u8 FASTCALL (*ReadByte)(u32 address);
	u16 FASTCALL (*ReadWord)(u32 address);
	u32 FASTCALL (*ReadLong)(u32 address);
	void FASTCALL (*WriteByte)(u32 address, u8 data);
	void FASTCALL (*WriteWord)(u32 address, u16 data);
	void FASTCALL (*WriteLong)(u32 address, u32 data);
	M68KBreakpointInfo *(*M68KGetBreakpointList)(void);
} SCSPInterface_struct;

extern SCSPInterface_struct *SCSCore;
extern SCSPInterface_struct SCSDummy;
extern SCSPInterface_struct SCSScsp1;
extern SCSPInterface_struct SCSScsp2;
extern SCSPInterface_struct *SCSCoreList[];  // Defined by each port
#endif

///////////////////////////////////////////////////////////////////////////

// Module interface declaration

#define SNDCORE_DEFAULT -1
#define SNDCORE_DUMMY   0
#define SNDCORE_WAV     10  // Should be 1, but left as is for backward compat

#define SCSP_MUTE_SYSTEM    1
#define SCSP_MUTE_USER      2


///////////////////////////////////////////////////////////////////////////

// Parameter block for M68K{Get,Set}Registers()
typedef struct {
	u32 D[8];
	u32 A[8];
	u32 SR;
	u32 PC;
} M68KRegs;

// Maximum number of M68K breakpoints that can be set simultaneously
#define M68K_MAX_BREAKPOINTS 10

///////////////////////////////////////////////////////////////////////////

// Data/function declarations

extern u8 *SoundRam;

extern int ScspInit(int coreid, void (*interrupt_handler)(void));
extern void ScspReset(void);
#ifdef SCSP_PLUGIN
extern int ScspChangeCore(int coreid);
#endif
extern int ScspChangeSoundCore(int coreid);
extern int ScspChangeVideoFormat(int type);
extern void ScspSetFrameAccurate(int on);
extern void ScspMuteAudio(int flags);
extern void ScspUnMuteAudio(int flags);
extern void ScspSetVolume(int volume);
extern void ScspDeInit(void);

extern void ScspExec(int decilines);

extern u8 FASTCALL SoundRamReadByte(u32 address);
extern u16 FASTCALL SoundRamReadWord(u32 address);
extern u32 FASTCALL SoundRamReadLong(u32 address);
extern void FASTCALL SoundRamWriteByte(u32 address, u8 data);
extern void FASTCALL SoundRamWriteWord(u32 address, u16 data);
extern void FASTCALL SoundRamWriteLong(u32 address, u32 data);
extern u8 FASTCALL ScspReadByte(u32 address);
extern u16 FASTCALL ScspReadWord(u32 address);
extern u32 FASTCALL ScspReadLong(u32 address);
extern void FASTCALL ScspWriteByte(u32 address, u8 data);
extern void FASTCALL ScspWriteWord(u32 address, u16 data);
extern void FASTCALL ScspWriteLong(u32 address, u32 data);
extern void ScspReceiveCDDA(const u8 *sector);

extern int SoundSaveState(FILE *fp);
extern int SoundLoadState(FILE *fp, int version, int size);
extern void ScspSlotDebugStats(u8 slotnum, char *outstring);
extern void ScspCommonControlRegisterDebugStats(char *outstring);
extern int ScspSlotDebugSaveRegisters(u8 slotnum, const char *filename);
extern int ScspSlotDebugAudioSaveWav(u8 slotnum, const char *filename);
#ifndef SCSP_PLUGIN
extern void ScspConvert32uto16s(s32 *srcL, s32 *srcR, s16 *dest, u32 len);
#else
extern void Scsp2Convert32uto16s(s32 *srcL, s32 *srcR, s16 *dest, u32 len);
#endif

extern void M68KStart(void);
extern void M68KStop(void);
extern void M68KStep(void);
extern void M68KWriteNotify(u32 address, u32 size);
extern void M68KGetRegisters(M68KRegs *regs);
extern void M68KSetRegisters(const M68KRegs *regs);
extern void M68KSetBreakpointCallBack(void (*func)(u32 address));
extern int M68KAddCodeBreakpoint(u32 address);
extern int M68KDelCodeBreakpoint(u32 address);
#ifndef SCSP_PLUGIN
extern const M68KBreakpointInfo *M68KGetBreakpointList(void);
#else
extern M68KBreakpointInfo *M68KGetBreakpointList(void);
#endif
extern void M68KClearCodeBreakpoints(void);
extern u32 FASTCALL M68KReadByte(u32 address);
extern u32 FASTCALL M68KReadWord(u32 address);
extern void FASTCALL M68KWriteByte(u32 address, u32 data);
extern void FASTCALL M68KWriteWord(u32 address, u32 data);

///////////////////////////////////////////////////////////////////////////

// Compatibility macros to match scsp.h interface

#define m68kregs_struct M68KRegs
#define m68kcodebreakpoint_struct M68KBreakpointInfo

#include "scu.h"
#ifndef SCSP_PLUGIN
#define ScspInit(coreid)  ScspInit((coreid), ScuSendSoundRequest)
#else
//#define SCSScsp1Init(coreid)  SCSScsp1Init((coreid), ScuSendSoundRequest)
#endif

#ifndef SCSP_PLUGIN
#define scsp_r_b  ScspReadByte
#define scsp_r_w  ScspReadWord
#define scsp_r_d  ScspReadLong
#define scsp_w_b  ScspWriteByte
#define scsp_w_w  ScspWriteWord
#define scsp_w_d  ScspWriteLong

#define c68k_word_read  M68KReadWord
#else
#define scsp_r_b  SCSCore->ReadByte
#define scsp_r_w  SCSCore->ReadWord
#define scsp_r_d  SCSCore->ReadLong
#define scsp_w_b  SCSCore->WriteByte
#define scsp_w_w  SCSCore->WriteWord
#define scsp_w_d  SCSCore->WriteLong
#define ScspReadByte SCSCore->ReadByte
#define ScspReadWord SCSCore->ReadWord
#define ScspReadLong SCSCore->ReadLong
#define ScspWriteByte SCSCore->WriteByte
#define ScspWriteWord SCSCore->WriteWord
#define ScspWriteLong SCSCore->WriteLong

#define c68k_word_read M68KReadWord
#define M68KReadWord SCSCore->M68KReadWord
#define SoundSaveState SCSCore->SoundSaveState
#define ScspMuteAudio SCSCore->MuteAudio
#define ScspUnMuteAudio SCSCore->UnMuteAudio
#define SoundLoadState SCSCore->SoundLoadState
//#define ScspInit SCSCore->Init
#define ScspDeInit SCSCore->DeInit
#define ScspReset SCSCore->Reset
#define ScspChangeVideoFormat SCSCore->ChangeVideoFormat
#define ScspReceiveCDDA SCSCore->ReceiveCDDA
#define M68KStart SCSCore->M68KStart
#define M68KStop SCSCore->M68KStop
#endif

///////////////////////////////////////////////////////////////////////////

typedef struct
{
	u32 scsptiming1;
	u32 scsptiming2;  // 16.16 fixed point

	m68kcodebreakpoint_struct codebreakpoint[MAX_BREAKPOINTS];
	int numcodebreakpoints;
	void (*BreakpointCallBack)(u32);
	int inbreakpoint;
} ScspInternal;

#ifndef SCSP_PLUGIN
void FASTCALL scsp_w_b(u32, u8);
void FASTCALL scsp_w_w(u32, u16);
void FASTCALL scsp_w_d(u32, u32);
u8 FASTCALL scsp_r_b(u32);
u16 FASTCALL scsp_r_w(u32);
u32 FASTCALL scsp_r_d(u32);
#endif

void scsp_init(u8 *scsp_ram, void (*sint_hand)(u32), void (*mint_hand)(void));
void scsp_shutdown(void);
void scsp_reset(void);

void scsp_midi_in_send(u8 data);
void scsp_midi_out_send(u8 data);
u8 scsp_midi_in_read(void);
u8 scsp_midi_out_read(void);
void scsp_update(s32 *bufL, s32 *bufR, u32 len);
void scsp_update_monitor(void);
void scsp_update_timer(u32 len);

void M68KExec(s32 cycles);
void M68KSync(void);
void M68KSortCodeBreakpoints(void);

#ifndef SCSP_PLUGIN
u32 FASTCALL c68k_word_read(const u32 adr);
#endif

#endif  // SCSP_H
