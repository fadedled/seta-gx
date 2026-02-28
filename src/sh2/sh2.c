


#include "sh2.h"
#include "drc/compiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define ADDR_8(addr)	*((u8*)(addr))
#define ADDR_16(addr)	*((u16*)(addr))
#define ADDR_32(addr)	*((u32*)(addr))

SH2 msh2;
SH2 ssh2;

SH2 *sh_ctx;

#define write8(ptr, x)		(*((u8*)(ptr)) = (x))
#define write16(ptr, x)		(*((u16*)(ptr)) = (x))
#define write32(ptr, x)		(*((u32*)(ptr)) = (x))

static void sh2_HandleInterrupt(SH2 *sh);
static void sh2_FRTExec(SH2 *sh, u32 cycles);
static void sh2_WDTExec(SH2 *sh, u32 cycles);
void sh2_OnchipReset(SH2 *sh);


void sh2_Init()
{
	//Initialize the main sh2
	sh2_PowerOn(&msh2);
	//Initialize the secondary sh2
	SH2_FLAG_SET(&ssh2, SH2_FLAG_SLAVE);
	sh2_PowerOn(&ssh2);
}

void sh2_Deinit()
{
	//Does nothing? (yet?)
}


void sh2_PowerOn(SH2 *sh)
{
	sh->pc = sh2_Read32(0x0);	//Get from vector address table
	sh->r[15] = sh2_Read32(0x4); //Get from vector address table
	sh->vbr = 0x0;
	sh->sr |= 0xF0;
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
	sh2_DrcReset();
}


void sh2_OnchipReset(SH2 *sh)
{
	u8 *oc = sh->on_chip;
	write8(oc + OC_SMR, 0x00);
	write8(oc + OC_BRR, 0xFF);
	write8(oc + OC_SCR, 0x00);
	write8(oc + OC_TDR, 0xFF);
	write8(oc + OC_SSR, 0x84);
	write8(oc + OC_RDR, 0x00);
	write8(oc + OC_TIER, 0x01);
	write8(oc + OC_FTCSR, 0x00);
	write16(oc + OC_FRC, 0x0000);
	write16(oc + OC_OCRA, 0xFFFF);
	write16(oc + OC_OCRB + 0x10, 0xFFFF);		//Add 0x10
	write8(oc + OC_TRC, 0x00);
	write8(oc + OC_TOCR, 0xE0);
	write16(oc + OC_ICR, 0x0000);
	write16(oc + OC_IPRB, 0x0000);
	write16(oc + OC_VCRA, 0x0000);
	write16(oc + OC_VCRB, 0x0000);
	write16(oc + OC_VCRC, 0x0000);
	write16(oc + OC_VCRD, 0x0000);
	write8(oc + OC_DRCR0, 0x00);
	write8(oc + OC_DRCR1, 0x00);
	write16(oc + OC_WTCSR, 0x0018);
	write16(oc + OC_WTCNT, 0x0000);
	write16(oc + OC_RSTCSR, 0x001F);
	write8(oc + OC_SBYCR, 0x60);
	write16(oc + OC_ICR, 0x0000);
	write16(oc + OC_IPRA, 0x0000);
	write16(oc + OC_VCRWDT, 0x0000);
	write32(oc + OC_DVCR, 0x0000);
	write16(oc + OC_BARAH, 0x0000);
	write16(oc + OC_BARAL, 0x0000);
	write16(oc + OC_BAMRAH, 0x0000);
	write16(oc + OC_BAMRAL, 0x0000);
	write16(oc + OC_BBRA, 0x0000);
	write16(oc + OC_BARBH, 0x0000);
	write16(oc + OC_BARBL, 0x0000);
	write16(oc + OC_BAMRBH, 0x0000);
	write16(oc + OC_BAMRBL, 0x0000);
	write16(oc + OC_BBRB, 0x0000);
	write16(oc + OC_BDRBH, 0x0000);
	write16(oc + OC_BDRBL, 0x0000);
	write16(oc + OC_BDMRBH, 0x0000);
	write16(oc + OC_BDMRBL, 0x0000);
	write16(oc + OC_BRCR, 0x0000);
	write32(oc + OC_CHCR0, 0x0000);
	write32(oc + OC_CHCR1, 0x0000);
	write32(oc + OC_DMAOR, 0x0000);
	write32(oc + OC_BCR1, 0x03F0 | (msh2.flags & SH2_FLAG_SLAVE));
	write32(oc + OC_BCR2, 0x00FC);
	write32(oc + OC_WCR, 0xAAFF);
	write32(oc + OC_MCR, 0x0000);
	write32(oc + OC_RTCSR, 0x0000);
	write32(oc + OC_RTCNT, 0x0000);
	write32(oc + OC_RTCOR, 0x0000);
}

void sh2_Exec(SH2 *sh, u32 cycles)
{
	sh2_HandleInterrupt(sh);
	sh2_DrcExec(sh, cycles);

	sh2_FRTExec(sh, cycles);
	sh2_WDTExec(sh, cycles);
#if USE_SH2_DMA_TIMING
	sh2_DMAExec(sh)
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
			ir_pos = i;
		}
	}

	//Interrupts are sorted so we just insert in the sorted position
	for (u32 i = sh->iqr_count-1; i >= ir_pos; --i) {
		sh->iqr[i+1] = sh->iqr[i];
	}
	sh->iqr[ir_pos] = SH2_INTERRUPT(vec, level);
}

static void sh2_HandleInterrupt(SH2 *sh)
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
	const u32 frcold = ADDR_16(sh->on_chip + OC_FRC);
	const u32 shift = ((sh->on_chip[OC_FRC] & 3) << 1) + 3;
	const u32 mask = (1 << shift) - 1;
	u32 frc = frcold;

	frc += (cycles + sh->frc_leftover) >> shift;
	sh->frc_leftover = (cycles + sh->frc_leftover) & mask;
	//Check for a match with Output Compare A
	if (frc >= sh->on_chip[OC_OCRA] && frcold < sh->on_chip[OC_OCRA]) {
		if (sh->on_chip[OC_TIER] & 0x80) {
			sh2_SetInterrupt(sh, sh->on_chip[OC_VCRC+1] & 0x7F, sh->on_chip[OC_IPRB] & 0x7);
		}
		if (sh->on_chip[OC_FTCSR] & 0x1) {
			frc = 0;
			sh->frc_leftover = 0;
		}
		sh->on_chip[OC_FTCSR] |= 0x8;
	}
	//Check for a match with Output Compare B
	if (frc >= sh->on_chip[OC_OCRB] && frcold < sh->on_chip[OC_OCRB]) {
		if (sh->on_chip[OC_TIER] & 0x40) {
			sh2_SetInterrupt(sh, sh->on_chip[OC_VCRC+1] & 0x7F, sh->on_chip[OC_IPRB] & 0x7);
		}
		sh->on_chip[OC_FTCSR] |= 0x4;
	}
	//Check for an overflow
	if (frc > 0xFFFF) {
		if (sh->on_chip[OC_TIER] & 0x2) {
			sh2_SetInterrupt(sh, sh->on_chip[OC_VCRD] & 0x7F, sh->on_chip[OC_IPRB] & 0x7);
		}
		sh->on_chip[OC_FTCSR] |= 0x2;
	}
	write16(sh->on_chip + OC_FRC, frc);
}

static void sh2_WDTExec(SH2 *sh, u32 cycles)
{
	//XXX: Optimize conditional
	if ((~sh->on_chip[OC_WTCSR] & 0x20) || (sh->on_chip[OC_WTCSR] & 0x80) || (sh->on_chip[OC_RSTCSR] & 0x80)) {
		return;
	}
	const u32 mask = (1 << sh->wdt_shift) - 1;
	u32 wdt = ((cycles + sh->wdt_leftover) >> sh->wdt_shift) + sh->on_chip[OC_WTCNT];
	sh->wdt_leftover = (cycles + sh->wdt_leftover) & mask;

	if (wdt > 0xFF) {
		if (~OC_WTCSR & 0x40) { // if in Interval Timer Mode set overflow flag and send interrupt
			sh->on_chip[OC_WTCSR] |= 0x80;
			sh2_SetInterrupt(sh, sh->on_chip[OC_VCRWDT] & 0x7F, (sh->on_chip[OC_IPRA+1] >> 4) & 0x7);
		} else {
			 //XXX: Not implemented
		}
	}
	sh->on_chip[OC_WTCNT] = wdt;
}

void sh2_Step(SH2 *sh)
{
	//XXX.
}

void sh2_NMI(SH2 *sh)
{
   sh->on_chip[OC_ICR] |= 0x80; // real value is 0x8000
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
}

void sh2_DMAExec(SH2 *sh)
{
	// If AE and NMIF bits are set, we can't continue
	if (sh->on_chip[OC_DMAOR] & 0x6)
		return;
	u32 num_chan = ((sh->on_chip[OC_CHCR0] & 0x1) << 0) | ((sh->on_chip[OC_CHCR1] & 0x1) << 1);
	u32 cycles = 200;
	switch (num_chan) {
		case 1: { // Chanel 0 DMA
			cycles <<= (~sh->on_chip[OC_CHCR0] >> 3) & 1; //Dual channel
			//sh2_DMATransfer(sh, int *chcr, int *sar, int *dar, int *tcr, int *vcrdma);
		} break;
		case 2: { // Chanel 1DMA
			cycles <<= (~sh->on_chip[OC_CHCR1] >> 3) & 1; //Dual channel
			//sh2_DMATransfer(sh, int *chcr, int *sar, int *dar, int *tcr, int *vcrdma);
		} break;
		case 3: { // Chanel 0 and 1 DMA
			if (sh->on_chip[OC_DMAOR] & 0x8) { //Round robin priority
				cycles <<= (~sh->on_chip[OC_CHCR0] >> 3) & 1; //Dual channel
				//sh2_DMATransfer(sh, int *chcr, int *sar, int *dar, int *tcr, int *vcrdma);
				//sh2_DMATransfer(sh, int *chcr, int *sar, int *dar, int *tcr, int *vcrdma);
			} else { // Channel 0 > Channel 1 priority
				//XXX: Makes no sense really since num_chan is == 3
				if (num_chan & 0x1) { //XXX: Only this one happens
					cycles <<= (~sh->on_chip[OC_CHCR0] >> 3) & 1; //Dual channel
					//sh2_DMATransfer(sh, int *chcr, int *sar, int *dar, int *tcr, int *vcrdma);
				} else if (num_chan & 0x2) {
					cycles <<= (~sh->on_chip[OC_CHCR0] >> 3) & 1; //Dual channel
					//sh2_DMATransfer(sh, int *chcr, int *sar, int *dar, int *tcr, int *vcrdma);
				}
			}
		} break;
	}
}

void sh2_DMATransfer(SH2 *sh, u32 *chcr, u32 *sar, u32 *dar, u32 *tcr, u32 *vcrdma)
{
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

				*tcr = 0;
			} break;
			case 1: {
				destInc *= 2;
				srcInc *= 2;

				for (i = 0; i < *tcr; i++) {
					sh2_Write16(*dar, sh2_Read16(*sar));
					*sar += srcInc;
					*dar += destInc;
				}

				*tcr = 0;
			} break;
			case 2: {
				destInc *= 4;
				srcInc *= 4;

				for (i = 0; i < *tcr; i++) {
					sh2_Write32(*dar, sh2_Read32(*sar));
					*dar += destInc;
					*sar += srcInc;
				}

				*tcr = 0;
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

				*tcr = 0;
			}
			break;
		}
		sh2_WriteNotify(destInc < 0 ? *dar : *dar - i * destInc, i * abs(destInc));
	}

	if (*chcr & 0x4) {
		sh2_SetInterrupt(sh, *vcrdma, sh->on_chip[OC_IPRA] & 0xF);
	}

	// Set Transfer End bit
	*chcr |= 0x2;
}


//Read/Write functions

//On-chip
u8   sh2_OnchipRead8(u32 addr)
{
	if ((addr & ~0x1) == 0x14) {
		addr = OC_OCRA + (sh_ctx->on_chip[OC_TOCR] & 0x10);
	}
	return ADDR_8(sh_ctx->on_chip + addr);
}

u16  sh2_OnchipRead16(u32 addr)
{
	return ADDR_16(sh_ctx->on_chip + addr);
}

u32  sh2_OnchipRead32(u32 addr)
{
	//TODO: finish this one
	return ADDR_32(sh_ctx->on_chip + addr);
}

void sh2_OnchipWrite8(u32 addr, u8 val)
{

}

void sh2_OnchipWrite16(u32 addr, u16 val)
{

}

void sh2_OnchipWrite32(u32 addr, u32 val)
{

}


u8 sh2_Read8(u32 addr)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			return mem_Read8(addr);
			//addr &= 0x0FFFFFFF;
			//return mem_read8_arr[(addr >> 19) & 0xFF](addr);
		case 0x3:	//Adress Array, read/write space
			return 0;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			return ADDR_8(sh_ctx->cache + (addr & 0xFFF));
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				return sh2_OnchipRead8(addr & 0x1FF);
			//}
	}
	return 0xFF;
}


u16 sh2_Read16(u32 addr)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			return mem_Read16(addr);
			//return mem_read16_arr[(addr >> 19) & 0xFF](addr);
		case 0x3:	//Adress Array, read/write space
			return 0;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			return ADDR_16(sh_ctx->cache + (addr & 0xFFF));
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				return sh2_OnchipRead16(addr & 0x1FF);
			//}
	}
	return 0xFFFF;
}


u32 sh2_Read32(u32 addr)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			return mem_Read32(addr);
			//return mem_read32_arr[(addr >> 19) & 0xFF](addr);
		case 0x3:	//Adress Array, read/write space //TODO: Add real cache handling
			return sh_ctx->address_arr[(addr & 0x3FC) >> 2];
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			return ADDR_32(sh_ctx->cache + (addr & 0xFFF));
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				return sh2_OnchipRead32(addr & 0x1FF);
			//}
	}
	return 0xFFFFFFFF;
}


void sh2_Write8(u32 addr, u8 val)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			mem_Write8(addr, val); return;
			//mem_write8_arr[(addr >> 19) & 0xFF](addr, val); return;
		case 0x3:	//Adress Array, read/write space
			break;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			ADDR_8(sh_ctx->cache + (addr & 0xFFF)) = val; return;
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				sh2_OnchipWrite8(addr & 0x1FF, val);
				return;
			//}
	}
}


void sh2_Write16(u32 addr, u16 val)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//dunno
			mem_Write16(addr, val); return;
			//mem_write16_arr[(addr >> 19) & 0xFF](addr, val); return;
		case 0x3:	//Adress Array, read/write space
			break;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			ADDR_16(sh_ctx->cache + (addr & 0xFFF)) = val; return;
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				sh2_OnchipWrite16(addr & 0x1FF, val);
				return;
			//}
	}
}


void sh2_Write32(u32 addr, u32 val)
{
	switch(addr >> 29) {
		case 0x0:	//Cache area
		case 0x1:	//Cache-through area
		case 0x5:	//Dunno
			mem_Write32(addr, val); return;
			//mem_write32_arr[(addr >> 19) & 0xFF](addr, val); return;
		case 0x3:	//Adress Array, read/write space  //TODO: Add real cache handling
			sh_ctx->address_arr[(addr & 0x3FC) >> 2] = val;
		case 0x2:	//Associative purge space
		case 0x6:	//Data Array, read/write space	(DataCache)
			ADDR_32(sh_ctx->cache + (addr & 0xFFF)) = val; return;
		case 0x7:	//On-chip peripheral modules
			//if (addr >= 0xFFFFFE00) {
				sh2_OnchipWrite32(addr & 0x1FF, val);
				return;
			//}
	}
}


//Input Capture
void sh2_MSH2InputCaptureWrite16(u32 addr, u16 data)
{
	msh2.on_chip[OC_FTCSR] |= 0x80; // Set Input Capture Flag
 	write16(msh2.on_chip + OC_FICR, ADDR_16(msh2.on_chip + OC_FRC)); // Copy FRC register to FICR
	// Time for an Interrupt?
	if (msh2.on_chip[OC_TIER] & 0x80) {
		sh2_SetInterrupt(&msh2, msh2.on_chip[OC_VCRC] & 0x7F, msh2.on_chip[OC_IPRB] & 0xF);
	}
}

void sh2_SSH2InputCaptureWrite16(u32 addr, u16 data)
{
	ssh2.on_chip[OC_FTCSR] |= 0x80; // Set Input Capture Flag
	write16(ssh2.on_chip + OC_FICR, ADDR_16(ssh2.on_chip + OC_FRC)); // Copy FRC register to FICR
 	// Time for an Interrupt?
	if (ssh2.on_chip[OC_TIER] & 0x80) {
		sh2_SetInterrupt(&ssh2, ssh2.on_chip[OC_VCRC] & 0x7F, ssh2.on_chip[OC_IPRB] & 0xF);
	}
}

