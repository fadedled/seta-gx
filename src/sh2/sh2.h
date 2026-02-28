

#ifndef __SH2_H__
#define __SH2_H__

#include <gccore.h>
#include "../memory.h"

//===================================
// SH2 Flags
//===================================
#define SH2_FLAG_SLAVE				0x8000
#define SH2_FLAG_IDLE				0x02
#define SH2_FLAG_SLEEPING			0x04
#define SH2_FLAG_SET(sh, flag)		((sh)->flags |=  (flag))
#define SH2_FLAG_CLR(sh, flag)		((sh)->flags &= ~(flag))


//===================================
// SH2 SR register
//===================================
#define SH2_SR_T(sr)	((sr) & 1)
#define SH2_SR_S(sr)	(((sr) >> 1) & 1)
#define SH2_SR_I(sr)	(((sr) >> 4) & 0xF)
#define SH2_SR_M(sr)	(((sr) >> 5) & 1)
#define SH2_SR_Q(sr)	(((sr) >> 6) & 1)

#define SH2_SR_T_BIT	(0x01)
#define SH2_SR_S_BIT	(0x02)
#define SH2_SR_I_BITS	(0xF0)
#define SH2_SR_M_BIT	(0x100)
#define SH2_SR_Q_BIT	(0x200)

#define SH2_SR_SET_T(sr, t)		(((sr) & ~SH2_SR_T_BIT) | (t))
#define SH2_SR_SET_I(sr, i)		(((sr) & ~SH2_SR_I_BITS) | (((i) << 4) & SH2_SR_I_BITS))
#define SH2_SR_SET_Q(sr, q)		(((sr) & ~SH2_SR_Q_BIT) | ((q) << 5))
#define SH2_SR_SET_M(sr, m)		(((sr) & ~SH2_SR_M_BIT) | ((m) << 6))

//===================================
// SH2 Interrupt
//===================================
#define SH2_INTERRUPT_NMI				(((0x1F) << 8) | (0xB))
#define SH2_INTERRUPT(vec, level)		(((level) << 8) | (vec))
#define SH2_INTERRUPT_VEC(ir)			((ir) & 0x7F)
#define SH2_INTERRUPT_LEVEL(ir)			((ir) >> 8)


//===================================
// On-Chip peripheral modules (Addresses 0xFFFFFE00 - 0xFFFFFFFF)
//===================================
#define OC_SMR			0x000
#define OC_BRR			0x001
#define OC_SCR			0x002
#define OC_TDR			0x003
#define OC_SSR			0x004
#define OC_RDR			0x005
#define OC_TIER			0x010
#define OC_FTCSR		0x011
#define OC_FRC			0x012
#define OC_OCRA			0x014
#define OC_OCRB			0x014	//WTF.. Add 0x10
#define OC_TCR			0x016
#define OC_TOCR			0x017
#define OC_FICR			0x018
#define OC_IPRB			0x060
#define OC_VCRA			0x062
#define OC_VCRB			0x064
#define OC_VCRC			0x066
#define OC_VCRD			0x068
#define OC_DRCR0		0x071
#define OC_DRCR1		0x072
#define OC_WTCSR		0x080
#define OC_WTCNT		0x081
#define OC_RSTCSR		0x082
#define OC_SBYCR		0x091
#define OC_CCR			0x092
#define OC_ICR			0x0E0
#define OC_IPRA			0x0E2
#define OC_VCRWDT		0x0E4
#define OC_DVSR			0x100
#define OC_DVDNT		0x104
#define OC_DVCR			0x108
#define OC_VCRDIV		0x10C
#define OC_DVDNTH		0x110
#define OC_DVDNTL		0x114
#define OC_DVDNTUH		0x118
#define OC_DVDNTUL		0x11C
#define OC_BARAH		0x140
#define OC_BARAL		0x142
#define OC_BAMRAH		0x144
#define OC_BAMRAL		0x146
#define OC_BBRA			0x148
#define OC_BARBH		0x160
#define OC_BARBL		0x162
#define OC_BAMRBH		0x164
#define OC_BAMRBL		0x166
#define OC_BBRB			0x168
#define OC_BDRBH		0x170
#define OC_BDRBL		0x172
#define OC_BDMRBH		0x174
#define OC_BDMRBL		0x176
#define OC_BRCR			0x178
#define OC_SAR0			0x180
#define OC_DAR0			0x184
#define OC_TCR0			0x188
#define OC_CHCR0		0x18C
#define OC_SAR1			0x190
#define OC_DAR1			0x194
#define OC_TCR1			0x198
#define OC_CHCR1		0x19C
#define OC_VCRDMA0		0x1A0
#define OC_VCRDMA1		0x1A8
#define OC_DMAOR		0x1B0
#define OC_BCR1			0x1E0 //Real BCR1 is in 0x1E2-0x1E3
#define OC_BCR2			0x1E4
#define OC_WCR			0x1E8
#define OC_MCR			0x1EC
#define OC_RTCSR		0x1F0
#define OC_RTCNT		0x1F4
#define OC_RTCOR		0x1F8

#define MAX_INTERRUPTS 	64 	//TODO: CHECK THIS

#define EXT_IMM8(imm)		((imm & 0xFF) | -(imm & 0x80u))
#define EXT_IMM12(imm)		((imm & 0xFFF) | -(imm & 0x800u))


typedef struct SH2_tag
{
	//
	u32 r[16];
	u32 pc;
	u32 pr;
	u32 gbr;
	u32 vbr;
	u32 mach;
	u32 macl;
	u32 sr;

	u32 delay_slot;
	s32 cycles; //There are no negative cycles, this is for underflow reasons
	s32 cycles_leftover;
	u32 flags;


	u32 frc_leftover;
	u32 wdt_leftover;
	u32 wdt_shift;
	//u32 frc_shift; // ((OC_TRC & 3) << 1) + 3

	//u32 wdt_cntl; // isenable (OC_WTCSR & 0x20), isinterval (~OC_WTCSR & 0x40)
	//XXX: Interrupt stuff
	u16 iqr[MAX_INTERRUPTS];
	u32 iqr_count;

	u32 address_arr[0x100];		/*Address Array*/
	u8 on_chip[0x200];			/*On-chip peripheral modules*/
	u8 cache[0x1000];			/*Data Cache Array*/
} SH2;


extern SH2 msh2;
extern SH2 ssh2;

void sh2_Init(void);
void sh2_Deinit(void);
void sh2_Reset(SH2 *sh);
void sh2_PowerOn(SH2 *sh);
void sh2_Exec(SH2 *sh, u32 cycles);
void sh2_Step(SH2 *sh);
void sh2_SetInterrupt(SH2 *sh, u32 vec, u32 level);
void sh2_NMI(SH2 *sh);
void sh2_SetRegs(SH2 *sh, u32 *regs[32]);
void sh2_GetRegs(SH2 *sh, u32 *regs[32]);
void sh2_WriteNotify(u32 start, u32 len); //Notifies a write for dynarec code cache

void sh2_DMAExec(SH2 *sh);
void sh2_DMATransfer(SH2 *sh, u32 dma_slot); //?

//Read/Write functions
u8   sh2_Read8(u32 addr);
u16  sh2_Read16(u32 addr);
u32  sh2_Read32(u32 addr);
void sh2_Write8(u32 addr, u8 val);
void sh2_Write16(u32 addr, u16 val);
void sh2_Write32(u32 addr, u32 val);
u32* sh2_GetPCAddr(u32 pc);

//Input Capture
void sh2_MSH2InputCaptureWrite16(u32 addr, u16 data);
void sh2_SSH2InputCaptureWrite16(u32 addr, u16 data);


#endif /* __SH2_H__ */