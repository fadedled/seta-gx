


#include "sh2.h"
#include "drc/compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../cart.h"
#include "../memory.h"

#define __rlwimi(d, v, shift, ms, me) __asm__("rlwimi %0, %1, %2, %3, %4" : "+r"(d) : "r"(v), "I"(shift), "I"(31-me), "I"(31-ms))


#define ADDR_8(addr)	*((u8*)(addr))
#define ADDR_16(addr)	*((u16*)(addr))
#define ADDR_32(addr)	*((u32*)(addr))

SH2 msh2;
SH2 ssh2;

SH2 *sh_ctx;

#define write8(ptr, x)		(*((u8*)(ptr)) = (x))
#define write16(ptr, x)		(*((u16*)(ptr)) = (x))
#define write32(ptr, x)		(*((u32*)(ptr)) = (x))

static void sh2_FRTExec(SH2 *sh, u32 cycles);
static void sh2_WDTExec(SH2 *sh, u32 cycles);
void sh2_OnchipReset(SH2 *sh);


void sh2_Init(void)
{
	//Initialize the main sh2
	SH2_FLAG_SET(&msh2, 0);
	msh2.on_chip[OC_BCR1+2] = 0x00;
	SH2_FLAG_SET(&ssh2, SH2_FLAG_SLAVE);
	ssh2.on_chip[OC_BCR1+2] = 0x80;
	//Init dynarec
	sh2_DrcInit();
}

void sh2_Deinit(void)
{
	//Does nothing? (yet?)
	HashClearAll();
}


void sh2_PowerOn(SH2 *sh)
{
	sh->pc = sh2_Read32(0x0);	//Get from vector address table
	sh->r[15] = sh2_Read32(0x4); //Get from vector address table
	sh->vbr = 0x0;
	sh->sr |= 0xF0;
	sh->cycles = 0;
}


void sh2_Reset(SH2 *sh)
{
	sh->pc = sh2_Read32(0x8);	//Get from vector address table
	sh->r[15] = sh2_Read32(0xC); //Get from vector address table
	sh->vbr = 0x0;
	sh->gbr = 0x0;
	sh->mach = 0x0;
	sh->macl = 0x0;
	sh->pr = 0x0;
	sh->sr = 0xF0;

	sh->delay_slot = 0;
	sh->cycles = 0;

	sh->frc_leftover = 0;
	sh->wdt_leftover = 0;
	sh->wdt_shift = 1;


	//TODO: REset interrupts
	memset(sh->iqr, 0, sizeof(sh->iqr));
	sh->iqr_count = 0;

	sh2_OnchipReset(sh);
	//Do not reset if slave sh2
	if (!(sh->flags & SH2_FLAG_SLAVE)) {
		sh2_DrcReset();
	}
}


void sh2_OnchipReset(SH2 *sh)
{
	OCR_SMR = 0x00;
	OCR_BRR = 0xFF;
	OCR_SCR = 0x00;
	OCR_TDR = 0xFF;
	OCR_SSR = 0x84;
	OCR_RDR = 0x00;
	OCR_TIER = 0x01;
	OCR_FTCSR = 0x00;
	OCR_FRC = 0x0000;
	OCR_OCRA = 0xFFFF;
	OCR_OCRB = 0xFFFF;		//Add 0x10
	OCR_TCR = 0x00;
	OCR_TOCR = 0xE0;
	OCR_FICR = 0x0000;
	OCR_IPRB = 0x0000;
	OCR_VCRA = 0x0000;
	OCR_VCRB = 0x0000;
	OCR_VCRC = 0x0000;
	OCR_VCRD = 0x0000;
	OCR_DRCR0 = 0x00;
	OCR_DRCR1 = 0x00;
	OCR_WTCSR = 0x18;
	OCR_WTCNT = 0x00;
	OCR_RSTCSR = 0x1F;
	OCR_SBYCR = 0x60;
	OCR_ICR = 0x0000;
	OCR_IPRA = 0x0000;
	OCR_VCRWDT = 0x0000;
	OCR_DVCR = 0x00000000;
	OCR_BARAH = 0x0000;
	OCR_BARAL = 0x0000;
	OCR_BAMRAH = 0x0000;
	OCR_BAMRAL = 0x0000;
	OCR_BBRA = 0x0000;
	OCR_BARBH = 0x0000;
	OCR_BARBL = 0x0000;
	OCR_BAMRBH = 0x0000;
	OCR_BAMRBL = 0x0000;
	OCR_BBRB = 0x0000;
	OCR_BDRBH = 0x0000;
	OCR_BDRBL = 0x0000;
	OCR_BDMRBH = 0x0000;
	OCR_BDMRBL = 0x0000;
	OCR_BRCR = 0x0000;
	OCR_CHCR0 = 0x00000000;
	OCR_CHCR1 = 0x00000000;
	OCR_DMAOR = 0x00000000;
	OCR_BCR1 = 0x03F0 | (msh2.flags & SH2_FLAG_SLAVE);
	OCR_BCR2 = 0x00FC;
	OCR_WCR = 0xAAFF;
	OCR_MCR = 0x0000;
	OCR_RTCSR = 0x0000;
	OCR_RTCNT = 0x0000;
	OCR_RTCOR = 0x0000;
}

void sh2_Exec(SH2 *sh, u32 cycles)
{
	//Update context
	sh_ctx = sh;
	sh2_HandleInterrupt(sh);
	//if (!(sh->flags & SH2_FLAG_SLEEPING)) {
	sh2_DrcExec(sh, cycles);
	cycles += sh->cycles;
	sh2_FRTExec(sh, cycles);
	sh2_WDTExec(sh, cycles);


#if USE_SH2_DMA_TIMING
	//sh2_DMAExec(sh)
#endif

}


void sh2_SetInterrupt(SH2 *sh, u32 vec, u32 level)
{
	u32 ir_pos = 0;
	//Check for repeated interrupts
	for (u32 i = 0; i < sh->iqr_count; ++i) {
		if (SH2_INTERRUPT_VEC(sh->iqr[i]) == vec) {
			return;
		} else if (SH2_INTERRUPT_LEVEL(sh->iqr[i]) < level) {
			ir_pos = i+1;
		}
	}

	//Interrupts are sorted so we just insert in the sorted position
	u32 irq = SH2_INTERRUPT(vec, level);
	for (u32 i = ir_pos; i < sh->iqr_count+1; ++i) {
		u32 tmp = sh->iqr[i];
		sh->iqr[i] = irq;
		irq = tmp;
	}
	sh->iqr_count++;
}

void sh2_HandleInterrupt(SH2 *sh)
{
	if (sh->iqr_count) {
		u32 level = SH2_INTERRUPT_LEVEL(sh->iqr[sh->iqr_count-1]);
		if (level > SH2_SR_I(sh->sr)) {
			u32 vec = SH2_INTERRUPT_VEC(sh->iqr[sh->iqr_count-1]);
			sh2_Write32(sh->r[15] - 4, sh->sr);
			sh2_Write32(sh->r[15] - 8, sh->pc);
			sh->r[15] -= 8;
			sh->sr = SH2_SR_SET_I(sh->sr, level);
			sh->pc = sh2_Read32(sh->vbr + (vec << 2));
			--sh->iqr_count;
			SH2_FLAG_CLR(sh, SH2_FLAG_IDLE | SH2_FLAG_SLEEPING);
		}
	}
}


static void sh2_FRTExec(SH2 *sh, u32 cycles)
{
	const u32 frcold = OCR_FRC;
	const u32 shift = ((OCR_TCR & 3) << 1) + 3;
	const u32 mask = (1 << shift) - 1;
	u32 frc = frcold;

	frc += (cycles + sh->frc_leftover) >> shift;
	sh->frc_leftover = (cycles + sh->frc_leftover) & mask;
	//Check for a match with Output Compare A
	if (frc >= OCR_OCRA && frcold < OCR_OCRA) {
		if (OCR_TIER & 0x8) {
			sh2_SetInterrupt(sh, OCR_VCRC & 0x7F, (OCR_IPRB >> 8) & 0xF);
		}
		if (OCR_FTCSR & 0x1) {
			frc = 0;
			sh->frc_leftover = 0;
		}
		OCR_FTCSR |= 0x8;
	}
	//Check for a match with Output Compare B
	if (frc >= OCR_OCRB && frcold < OCR_OCRB) {
		if (OCR_TIER & 0x4) {
			sh2_SetInterrupt(sh, OCR_VCRC & 0x7F, (OCR_IPRB >> 8) & 0xF);
		}
		OCR_FTCSR |= 0x4;
	}
	//Check for an overflow
	if (frc > 0xFFFF) {
		if (OCR_TIER & 0x2) {
			sh2_SetInterrupt(sh, (OCR_VCRD >> 8) & 0x7F, (OCR_IPRB >> 8) & 0xF);
		}
		OCR_FTCSR |= 0x2;
	}
	OCR_FRC = frc;
}

static void sh2_WDTExec(SH2 *sh, u32 cycles)
{
	//XXX: Optimize conditional
	if ((~(OCR_WTCSR) & 0x20) || (OCR_WTCSR & 0x80) || (OCR_RSTCSR & 0x80)) {
		return;
	}
	const u32 mask = (1 << sh->wdt_shift) - 1;
	u32 wdt = ((cycles + sh->wdt_leftover) >> sh->wdt_shift) + (u32) OCR_WTCNT;
	sh->wdt_leftover = (cycles + sh->wdt_leftover) & mask;

	if (wdt > 0xFF) {
		if (~(OCR_WTCSR) & 0x40) { // if in Interval Timer Mode set overflow flag and send interrupt
			OCR_WTCSR |= 0x80;
			sh2_SetInterrupt(sh, (OCR_VCRWDT >> 8) & 0x7F, (OCR_IPRA >> 4) & 0xF);
		} else {
			 //XXX: Not implemented
		}
	}
	OCR_WTCNT = wdt;
}

void sh2_Step(SH2 *sh)
{
	//XXX.
}

void sh2_NMI(SH2 *sh)
{
   OCR_ICR |= 0x800; // real value is 0x8000
   sh2_SetInterrupt(sh, 0xB, 0x10);
}

void sh2_SetRegs(SH2 *sh, u32 *regs[32])
{

}

void sh2_GetRegs(SH2 *sh, u32 *regs[32])
{
	//XXX: DO this
}

void sh2_WriteNotify(u32 start, u32 len)
{
	//XXX: DO this
	//Only record 4kb page writes
	HashClearRange(start, start + len);
}


void sh2_DMATransfer(SH2 *sh, u32 dma_slot)
{

	u32 *chcr = &ADDR_32(sh->on_chip + (OC_CHCR0 + (dma_slot << 4)));
	u32 *sar = &ADDR_32(sh->on_chip + (OC_SAR0 + (dma_slot << 4)));
	u32 *dar = &ADDR_32(sh->on_chip + (OC_DAR0 + (dma_slot << 4)));
	u32 *tcr = &ADDR_32(sh->on_chip + (OC_TCR0 + (dma_slot << 4)));
	const u32 *vcrdma = &ADDR_32(sh->on_chip + (OC_VCRDMA0 + (dma_slot << 3)));
	int size;
	u32 i, i2;
	if (!(*chcr & 0x2)) { // TE is not set
		int srcInc;
		int destInc;

		switch((*chcr >> 12) & 0x3) {
			case 0x0: srcInc = 0; break;
			case 0x1: srcInc = 1; break;
			case 0x2: srcInc = -1; break;
			default: srcInc = 0; break;
		}

		switch((*chcr >> 14) & 0x3) {
			case 0x0: destInc = 0; break;
			case 0x1: destInc = 1; break;
			case 0x2: destInc = -1; break;
			default: destInc = 0; break;
		}

		switch (size = ((*chcr & 0x0C00) >> 10)) {
			case 0: {
				for (i = 0; i < *tcr; i++) {
					sh2_Write8(*dar, sh2_Read8(*sar));
					*sar += srcInc;
					*dar += destInc;
				}
			} break;
			case 1: {
				destInc *= 2;
				srcInc *= 2;

				for (i = 0; i < *tcr; i++) {
					sh2_Write16(*dar, sh2_Read16(*sar));
					*sar += srcInc;
					*dar += destInc;
				}
			} break;
			case 2: {
				destInc *= 4;
				srcInc *= 4;

				for (i = 0; i < *tcr; i++) {
					sh2_Write32(*dar, sh2_Read32(*sar));
					*dar += destInc;
					*sar += srcInc;
				}
			} break;
			case 3: { // 32 bit write
				destInc *= 4;
				srcInc *= 4;

				for (i = 0; i < *tcr; i+=4) {
					for(i2 = 0; i2 < 4; i2++) {
						sh2_Write32(*dar, sh2_Read32(*sar));
						*dar += destInc;
						*sar += srcInc;
					}
				}
			}
			break;
		}

		*tcr = 0;
		sh2_WriteNotify(destInc < 0 ? *dar : *dar - i * destInc, i * abs(destInc));
	}

	if (*chcr & 0x4) {
		sh2_SetInterrupt(sh, *vcrdma & 0x7F, (OCR_IPRA >> 8) & 0xF);
	}
	// Set Transfer End bit
	*chcr |= 0x2;
}


void sh2_DMAExec(SH2 *sh)
{
	// If AE and NMIF bits are set, we can't continue
	if (OCR_DMAOR & 0x6) {
		return;
	}
	u32 num_chan = ((OCR_CHCR0 & 0x1) << 0) | ((OCR_CHCR1 & 0x1) << 1);
	//u32 cycles = 200;
	switch (num_chan) {
		case 1: { // Chanel 0 DMA
		//	cycles <<= (~sh->on_chip[OC_CHCR0] >> 3) & 1; //Dual channel
			sh2_DMATransfer(sh, 0);
		} break;
		case 2: { // Chanel 1DMA
		//	cycles <<= (~sh->on_chip[OC_CHCR1] >> 3) & 1; //Dual channel
			sh2_DMATransfer(sh, 1);
		} break;
		case 3: { // Chanel 0 and 1 DMA
			if (OCR_DMAOR & 0x8) { //Round robin priority
			//	cycles <<= (~sh->on_chip[OC_CHCR0] >> 3) & 1; //Dual channel
				sh2_DMATransfer(sh, 0);
				sh2_DMATransfer(sh, 1);
			} else { // Channel 0 > Channel 1 priority
			//	cycles <<= (~sh->on_chip[OC_CHCR0] >> 3) & 1; //Dual channel
				sh2_DMATransfer(sh, 0);
			//	cycles <<= (~sh->on_chip[OC_CHCR0] >> 3) & 1; //Dual channel
				sh2_DMATransfer(sh, 1);
			}
		} break;
	}
}


//Read/Write functions

//On-chip
u8 sh2_OnchipRead8(u32 addr)
{
	if ((addr & ~0x1) == 0x14) {
		addr = OC_OCRA + (sh_ctx->on_chip[OC_TOCR] & 0x10) + (addr & 0x1);
	}
	return ADDR_8(sh_ctx->on_chip + addr);
}

u16 sh2_OnchipRead16(u32 addr)
{
	return ADDR_16(sh_ctx->on_chip + addr);
}

u32 sh2_OnchipRead32(u32 addr)
{
	//Repeated registers
	if ((addr >= 0x120) && (addr <= 0x13C)) {
		addr ^= 0x20;
	}
	return ADDR_32(sh_ctx->on_chip + addr);
}

void sh2_OnchipWrite8(u32 addr, u8 val)
{
	SH2 * const sh = sh_ctx;
	switch(addr) {
		//Is this needed? v
		case 0x000: OCR_SMR = val; break;
		case 0x001: OCR_SMR = val; break;
		case 0x002:
			if (!(val & 0x20)) {//If Transmitter is getting disabled, set TDRE
				OCR_SSR |= 0x80;
			}
			OCR_SCR = val; break;
		case 0x003: OCR_TDR = val; break;
		case 0x004: break;
		//Is this needed? ^

		case 0x010: OCR_TIER = (val & 0x8E) | 0x1; break;
		case 0x011: OCR_FTCSR = (OCR_FTCSR & (val & 0xFE)) | (val & 0x1); break;
		case 0x012: sh->on_chip[OC_FRC] = val; break;
		case 0x013: sh->on_chip[OC_FRC + 1] = val; break;
		case 0x014: sh->on_chip[OC_OCRA + (OCR_TOCR & 0x10) ] = val; break;
		case 0x015: sh->on_chip[OC_OCRA + (OCR_TOCR & 0x10) + 1] = val; break;
		case 0x016: OCR_TCR = val & 0x83; break;
		case 0x017: OCR_TOCR = 0xE0 | (val & 0x13); break;

		case 0x060: sh->on_chip[OC_IPRB] = val; break;
		//case 0x061: sh->on_chip[OC_IPRB+1] = val; break;
		case 0x062: sh->on_chip[OC_VCRA]   = val & 0x7F; break;
		case 0x063: sh->on_chip[OC_VCRA+1] = val & 0x7F; break;
		case 0x064: sh->on_chip[OC_VCRB]   = val & 0x7F; break;
		case 0x065: sh->on_chip[OC_VCRB+1] = val & 0x7F; break;
		case 0x066: sh->on_chip[OC_VCRC]   = val & 0x7F; break;
		case 0x067: sh->on_chip[OC_VCRC+1] = val & 0x7F; break;
		case 0x068: sh->on_chip[OC_VCRD]   = val & 0x7F; break;
		//case 0x069: sh->on_chip[OC_] = val; break;
		case 0x071: OCR_DRCR0 = val & 0x3; break;
		case 0x072: OCR_DRCR1 = val & 0x3; break;
		case 0x091: OCR_SBYCR = val & 0xDF; break;
		case 0x092: OCR_CCR   = val & 0xCF; break;
		case 0x0E0: __rlwimi(OCR_ICR, val, 8, 8, 8); break;
		case 0x0E1: __rlwimi(OCR_ICR, val, 0, 0, 0); break;
		case 0x0E2: sh->on_chip[OC_IPRA]   = val; break;
		case 0x0E3: sh->on_chip[OC_IPRA+1] = val & 0xF0; break;
		case 0x0E4: sh->on_chip[OC_VCRWDT]   = val & 0x7F; break;
		case 0x0E5: sh->on_chip[OC_VCRWDT+1] = val & 0x7F; break;
		default:
			//Unhandled On chip write byte
	}
}

void sh2_OnchipWrite16(u32 addr, u16 val)
{
	SH2 * const sh = sh_ctx;
	switch(addr) {
		case 0x060: {OCR_IPRB = val & 0xFF00;} break;
		case 0x062: {OCR_VCRA = val & 0x7F7F;} break;
		case 0x063: {OCR_VCRB = val & 0x7F7F;} break;
		case 0x066: {OCR_VCRC = val & 0x7F7F;} break;
		case 0x068: {OCR_VCRD = val & 0x7F7F;} break;
		case 0x080: {
			switch (val >> 8) {
				case 0xA5: {
					sh_ctx->wdt_shift = (0xDCA98761 >> ((val & 7) << 2)) & 0xF;
					OCR_WTCSR = val | 0x18;} break;
				case 0x5A: {OCR_WTCNT = val;} break;
			}
		} break;
		case 0x082: {
			if (val == 0xA500) {
				OCR_RSTCSR &= 0x7F;
			} else if (val >> 8 == 0x5A) {
				OCR_RSTCSR = (OCR_RSTCSR & 0x80) | (val & 0x60) | 0x1F;
			}
		} break;
		case 0x092: {OCR_CCR = val & 0xCF;} break;
		case 0x0E0: {OCR_ICR = val & 0x0101;} break;
		case 0x0E2: {OCR_IPRA = val & 0xFFF0;} break;
		case 0x0E4: {OCR_VCRWDT = val & 0x7F7F;} break;
		case 0x108:
		case 0x128: {OCR_DVCR = val & 0x3;} break;
		case 0x140: {OCR_BARAH = val;} break;
		case 0x142: {OCR_BARAL = val;} break;
		case 0x144: {OCR_BAMRAH = val;} break;
		case 0x146: {OCR_BAMRAL = val;} break;
		case 0x148: {OCR_BBRA = val & 0xFF;} break;
		case 0x160: {OCR_BARBH = val;} break;
		case 0x162: {OCR_BARBL = val;} break;
		case 0x164: {OCR_BAMRBH = val;} break;
		case 0x166: {OCR_BAMRBL = val;} break;
		case 0x168: {OCR_BBRB = val & 0xFF;} break;
		case 0x170: {OCR_BDRBH = val;} break;
		case 0x172: {OCR_BDRBL = val;} break;
		case 0x174: {OCR_BDMRBH = val;} break;
		case 0x176: {OCR_BDMRBL = val;} break;
		case 0x178: {OCR_BRCR = val & 0xF4DC;} break;
		default:
			//Unhandled On chip write word
	}
}

static inline void _calcDiv32(SH2 *sh, s32 val)
{
    const s32 dividend = val;
    const s32 divisor = (s32) OCR_DVSR;
	s32 res_l = 0x80000000;
	s32 res_h = 0;
    if (divisor) {
        if (!(dividend == 0x80000000 && divisor == -1)) {
            res_l = dividend / divisor;
            res_h = dividend % divisor;
        }
    } else {
        // Overflow
        res_h = dividend >> 29;
        if (OCR_DVCR & 0x2) {
            res_l = (dividend << 3) | ((~dividend >> 31) & 7);
        } else {
            // DVDNT/DVDNTL is saturated if the interrupt signal is disabled
            res_l = dividend < 0 ? 0x80000000 : 0x7FFFFFFF;
        }

        // Signal overflow
        OCR_DVCR |= 0x1;
    }

	OCR_DVDNTUH = OCR_DVDNTH = res_h;
	OCR_DVDNTUL = OCR_DVDNTL = OCR_DVDNT = res_l;
    if ((OCR_DVCR & 0x3) == 0x3) {
		sh2_SetInterrupt(sh_ctx, OCR_VCRDIV & 0x7F, (OCR_IPRA >> 12) & 0xF);
    }
}

static inline void _calcDiv64(SH2 *sh, u32 val)
{
#if 1
	const s32 kMinValue32 = 0x80000000;
	const s32 kMaxValue32 = 0x7FFFFFFF;

	s64 dividend = OCR_DVDNTH;
	dividend <<= 32;
	//s64 dividend = *((s64 *)(sh->on_chip + OC_DVDNTH));
	dividend |= val;
	const s32 divisor = (s32) OCR_DVSR;

	s32 res_l = 0x80000000;
	s32 res_h = 0;
	u32 overflow = 0;
	if (divisor == 0) {
		overflow = 1;
	} else if (OCR_DVDNTH == 0x80000000 && val == 0x00000000 && divisor == -1) {
		// Handle extreme case
		overflow = 1;
	} else {
		const s32 quotient = dividend / divisor;
		//const s32 remainder = dividend % divisor;
		//f64 df = (f64) dividend;
		//f64 dvf = (f64) divisor;
		//const s32 quotient = (s32) (s64) (df / dvf);
		const s32 remainder = dividend - (quotient * (s64) divisor);
		//TODO:
		res_l = quotient;
		res_h = remainder;

	}

	if (overflow) {
		const s64 origDividend = dividend;
		bool Q = dividend < 0;
        const bool M = divisor < 0;
		for (int i = 0; i < 3; i++) {
			if (Q == M) {
				dividend -= ((u64)divisor) << 32ull;
			} else {
				dividend += ((u64)divisor) << 32ull;
			}

			Q = dividend < 0;
			dividend = (dividend << 1ll) | (Q == M);
		}

		// Update output registers
		if (OCR_DVCR & 0x2) {
			res_l = dividend;
		} else {
			// DVDNT/DVDNTL is saturated if the interrupt signal is disabled
			res_l = (s32)((origDividend >> 32) ^ divisor) < 0 ? kMinValue32 : kMaxValue32;
		}
		res_h = dividend >> 32ll;

		// Signal overflow
		OCR_DVCR |= 0x1;
	}

	OCR_DVDNTUH = OCR_DVDNTH = res_h;
	OCR_DVDNTUL = OCR_DVDNTL = OCR_DVDNT = res_l;

	if ((OCR_DVCR & 0x3) == 0x3) {
		sh2_SetInterrupt(sh_ctx, OCR_VCRDIV & 0x7F, (OCR_IPRA >> 12) & 0xF);
	}
#else
	s32 divisor = (s32) OCR_DVSR;
	s64 dividend = OCR_DVDNTH;
	dividend <<= 32;
	dividend |= val;
w
	if (divisor == 0) {
		if (OCR_DVDNTH & 0x80000000)
		{
			OCR_DVDNTL = 0x80000000;
			OCR_DVDNTH = OCR_DVDNTH << 3; // fix me
		}
		else
		{
			OCR_DVDNTL = 0x7FFFFFFF;
			OCR_DVDNTH = OCR_DVDNTH << 3; // fix me
		}

		OCR_DVDNTUL = OCR_DVDNTL;
		OCR_DVDNTUH = OCR_DVDNTH;
		OCR_DVCR |= 1;
		if (OCR_DVCR & 0x2) {
			sh2_SetInterrupt(sh_ctx, OCR_VCRDIV & 0x7F, (OCR_IPRA >> 12) & 0xF);
		}
	} else {
		s64 quotient = dividend / divisor;
		s32 remainder = dividend % divisor;

		if (quotient > 0x7FFFFFFF)
		{
			OCR_DVCR |= 1;
			OCR_DVDNTL = 0x7FFFFFFF;
			OCR_DVDNTH = 0xFFFFFFFE; // fix me
			if (OCR_DVCR & 0x2) {
				sh2_SetInterrupt(sh_ctx, OCR_VCRDIV & 0x7F, (OCR_IPRA >> 12) & 0xF);
			}
		}
		else if ((s32)(quotient >> 32) < -1)
		{
			OCR_DVCR |= 1;
			OCR_DVDNTL = 0x80000000;
			OCR_DVDNTH = 0xFFFFFFFE; // fix me
			if (OCR_DVCR & 0x2) {
				sh2_SetInterrupt(sh_ctx, OCR_VCRDIV & 0x7F, (OCR_IPRA >> 12) & 0xF);
			}
		} else {
			OCR_DVDNTL = quotient;
			OCR_DVDNTH = remainder;
		}

		OCR_DVDNTUL = OCR_DVDNTL;
		OCR_DVDNTUH = OCR_DVDNTH;
	}
#endif

}

void sh2_OnchipWrite32(u32 addr, u32 val)
{
	const SH2 *sh = sh_ctx;
	switch(addr) {
		case 0x100:
		case 0x120: {OCR_DVSR = val;} break;
		case 0x104: // 32bit / 32bit divide
		case 0x124: {_calcDiv32(sh_ctx, val); } break;
		case 0x108:
		case 0x128: {OCR_DVCR = val & 0x3;} break;
		case 0x10C:
		case 0x12C: {OCR_VCRDIV = val & 0xFFFF;} break;
		case 0x110:
		case 0x130: {OCR_DVDNTH = val;} break;
		case 0x114: // 64bit / 32bit divide
		case 0x134: {_calcDiv64(sh_ctx, val);} break;
		case 0x118:
		case 0x138: {OCR_DVDNTUH = val;} break;
		case 0x11C:
		case 0x13C: {OCR_DVDNTUL = val;} break;
		case 0x140: {ADDR_32(sh->on_chip + OC_BARAH) = val;} break;
		case 0x144: {ADDR_32(sh->on_chip + OC_BAMRAH) = val;} break;
		case 0x180: {OCR_SAR0 = val;} break;
		case 0x184: {OCR_DAR0 = val;} break;
		case 0x188: {OCR_TCR0 = val & 0xFFFFFF;} break;

		case 0x18C: {
			OCR_CHCR0 = val & 0xFFFF;
			if ((OCR_DMAOR & 7) == 1 && (val & 3) == 1) {
				sh2_DMAExec(sh_ctx);
			} } break;
		case 0x190: {OCR_SAR1 = val;} break;
		case 0x194: {OCR_DAR1 = val;} break;
		case 0x198: {OCR_TCR1 = val & 0xFFFFFF;} break;
		case 0x19C: {
			OCR_CHCR1 = val & 0xFFFF;
			if ((OCR_DMAOR & 7) == 1 && (val & 3) == 1) {
				sh2_DMAExec(sh_ctx);
			} } break;
		case 0x1A0: {OCR_VCRDMA0 = val & 0xFFFF;} break;
		case 0x1A8: {OCR_VCRDMA1 = val & 0xFFFF;} break;
		case 0x1B0: {
			OCR_DMAOR = val & 0xF;
			if ((val & 7) == 1) {
				sh2_DMAExec(sh_ctx);
			} } break;
		case 0x1E0: {OCR_BCR1 = (sh->flags & SH2_FLAG_SLAVE) | (val & 0x1FF7);} break;
		case 0x1E4: {OCR_BCR2 = val & 0xFC;} break;
		case 0x1E8: {OCR_WCR = val;} break;
		case 0x1EC: {OCR_MCR = val & 0xFEFC;} break;
		case 0x1F0: {OCR_RTCSR = val & 0xF8;} break;
		case 0x1F8: {OCR_RTCOR = val & 0xFF;} break;
		default:
			//Unhandled On chip write long
	}
}

//Access timings for cycles


u8 sh2_Read8(u32 addr)
{
	switch(addr >> 29) {
		case 0x1:	//Cache-through area
		case 0x5:	sh_ctx->cycles += mem_CyclesR(addr);
		case 0x0:	//Cache area
			return mem_Read8(addr & 0x0FFFFFFF);
			//addr &= 0x0FFFFFFF;
			//return mem_read8_arr[(addr >> 19) & 0xFF](addr);
		case 0x3:	//Adress Array, read/write space
			return 0;
		case 0x2:	//Associative purge space
			return 0x23;
		case 0x4:	//Data Array, read/write space	(DataCache)
		case 0x6:
			return ADDR_8(sh_ctx->cache + (addr & 0xFFF));
		case 0x7:	//On-chip peripheral modules
			if (addr >= 0xFFFFFE00) {
				return sh2_OnchipRead8(addr & 0x1FF);
			}
	}
	return 0xFF;
}


u16 sh2_Read16(u32 addr)
{
	switch(addr >> 29) {
		case 0x1:	//Cache-through area
		case 0x5:	sh_ctx->cycles += mem_CyclesR(addr);
		case 0x0:	//Cache area
			return mem_Read16(addr & 0x0FFFFFFF);
			//return mem_read16_arr[(addr >> 19) & 0xFF](addr);
		case 0x2:	//Associative purge space
			return 0x2312;
		case 0x3:	//Adress Array, read/write space
			return 0;
		case 0x4:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			return ADDR_16(sh_ctx->cache + (addr & 0xFFF));
		case 0x7:	//On-chip peripheral modules
			if (addr >= 0xFFFFFE00) {
				return sh2_OnchipRead16(addr & 0x1FF);
			}
	}
	return 0xFFFF;
}


u32 sh2_Read32(u32 addr)
{
	switch(addr >> 29) {
		case 0x1:	//Cache-through area
		case 0x5:	sh_ctx->cycles += mem_CyclesR(addr);
		case 0x0:	//Cache area
			return mem_Read32(addr & 0x0FFFFFFF);
			//return mem_read32_arr[(addr >> 19) & 0xFF](addr);
		case 0x2:	//Associative purge space
			return 0x2312;
		case 0x3:	//Adress Array, read/write space //TODO: Add real cache handling
			return sh_ctx->address_arr[(addr & 0x3FC) >> 2];
		case 0x4:	//Data Array, read/write space	(DataCache)
		case 0x6:
			return ADDR_32(sh_ctx->cache + (addr & 0xFFF));
		case 0x7:	//On-chip peripheral modules
			if (addr >= 0xFFFFFE00) {
				return sh2_OnchipRead32(addr & 0x1FF);
			}
	}
	return 0xFFFFFFFF;
}


void sh2_Write8(u32 addr, u8 val)
{
	switch(addr >> 29) {
		case 0x1:	//Cache-through area
		case 0x5:	sh_ctx->cycles += mem_CyclesW(addr);
		case 0x0:	//Cache area
			mem_Write8(addr & 0x0FFFFFFF, val); return;
			//mem_write8_arr[(addr >> 19) & 0xFF](addr, val); return;
		case 0x2:	//Associative purge space
		case 0x3:	//Adress Array, read/write space
			break;
		case 0x4:	//Data Array, read/write space	(DataCache)
		case 0x6:
			ADDR_8(sh_ctx->cache + (addr & 0xFFF)) = val; return;
		case 0x7:	//On-chip peripheral modules
			if (addr >= 0xFFFFFE00) {
				sh2_OnchipWrite8(addr & 0x1FF, val);
			}
	}
}


void sh2_Write16(u32 addr, u16 val)
{
	switch(addr >> 29) {
		case 0x1:	//Cache-through area
		case 0x5:	sh_ctx->cycles += mem_CyclesW(addr);
		case 0x0:	//Cache area
			mem_Write16(addr & 0x0FFFFFFF, val); return;
			//mem_write16_arr[(addr >> 19) & 0xFF](addr, val); return;
		case 0x2:	//Associative purge space
		case 0x3:	//Adress Array, read/write space
			break;
		case 0x4:	//Data Array, read/write space	(DataCache)
		case 0x6:
			ADDR_16(sh_ctx->cache + (addr & 0xFFF)) = val; return;
		case 0x7:	//On-chip peripheral modules
			if (addr >= 0xFFFFFE00) {
				sh2_OnchipWrite16(addr & 0x1FF, val);
			}
	}
}


void sh2_Write32(u32 addr, u32 val)
{
	switch(addr >> 29) {
		case 0x1:	//Cache-through area
		case 0x5:	sh_ctx->cycles += mem_CyclesW(addr);
		case 0x0:	//Cache area
			mem_Write32(addr & 0x0FFFFFFF, val); return;
			//mem_write32_arr[(addr >> 19) & 0xFF](addr, val); return;
		case 0x2:	//Associative purge space
			return;
		case 0x3:	//Adress Array, read/write space  //TODO: Add real cache handling
			sh_ctx->address_arr[(addr & 0x3FC) >> 2] = val; return;
		case 0x4:	//Data Array, read/write space	(DataCache)
		case 0x6:
			ADDR_32(sh_ctx->cache + (addr & 0xFFF)) = val; return;
		case 0x7:	//On-chip peripheral modules
			if (addr >= 0xFFFFFE00) {
				sh2_OnchipWrite32(addr & 0x1FF, val);
			}
	}
}

u32* sh2_GetPCAddr(u32 pc)
{
	switch((pc >> 19) & 0xFF) {
		case 0x000: // Bios
			return (u32*) (bios_rom + (pc & (BIOS_SIZE - 1)));
		case 0x040: // CS0
			return cs0_getPCAddr(pc);
		default: // Assume WRAM... could lead to terrible sideeffects
			pc |= ((pc >> 6) & HIGH_WRAM_SIZE); // High WRAM bit
			return (u32*) (wram + (pc & (WRAM_SIZE - 1)));
	}
	return 0;
}


//Input Capture
void sh2_MSH2InputCaptureWrite16(u32 addr, u16 data)
{
	SH2 *sh = &msh2;
	OCR_FTCSR |= 0x80; // Set Input Capture Flag
	OCR_FICR = OCR_FRC; // Copy FRC register to FICR
 	// Time for an Interrupt?
	if (OCR_TIER & 0x80) {
		sh2_SetInterrupt(&ssh2, (OCR_VCRC >> 8) & 0x7F, (OCR_IPRB >> 8) & 0xF);
	}
}

void sh2_SSH2InputCaptureWrite16(u32 addr, u16 data)
{
	SH2 *sh = &ssh2;
	OCR_FTCSR |= 0x80; // Set Input Capture Flag
	OCR_FICR = OCR_FRC; // Copy FRC register to FICR
 	// Time for an Interrupt?
	if (OCR_TIER & 0x80) {
		sh2_SetInterrupt(&ssh2, (OCR_VCRC >> 8) & 0x7F, (OCR_IPRB >> 8) & 0xF);
	}
}
