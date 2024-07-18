




typedef struct SH2_tag
{
	u32 regs[16];
	u32 pc;
	u32 pr;
	u32 gbr;
	u32 vbr;
	u32 sr;
	u32 mach;
	u32 macl;

	u32 delay_slot;
	u32 cycles;
	u32 flags;

	//XXX: Interrupt stuff
	//Interrupt iqr_arr[MAX_INTERRUPTS];

	u32 address_arr[0x100];		/*Address Array*/
	u8 on_chip[0x200];			/*On-chip peripheral modules*/
	u8 cache[0x1000];			/*Data Cache Array*/
} SH2;

SH2 msh2;
SH2 ssh2;

#define write8(ptr, x)		(*((u8*)(ptr)) = (x))
#define write16(ptr, x)		(*((u16*)(ptr)) = (x))
#define write32(ptr, x)		(*((u32*)(ptr)) = (x))


void sh2_Init(SH2 *sh, u32 is_slave_flag)
{
	//Initialize the main sh2
	msh2.flags |= is_slave_flag;
	sh2_PowerOn(sh);
}


void sh2_PowerOn(SH2 *sh)
{
	sh->pc = mem_Read32(0x0);	//Get from vector address table
	sh->regs[15] = mem_Read32(0x4); //Get from vector address table
	sh->vbr = 0x0;
	sh->sr |= 0xF0;
}


void sh2_Reset(SH2 *sh)
{
	sh->pc = mem_Read32(0x8);	//Get from vector address table
	sh->regs[15] = mem_Read32(0xC); //Get from vector address table
	sh->vbr = 0x0;
	sh->sr |= 0xF0;
}


void sh2_HandleInterrupts(SH2 *sh)
{

}


void sh2_HandleBreakpoints(SH2 *sh)
{

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
	write32(oc + OC_BCR1, 0x03F0);
	write32(oc + OC_BCR2, 0x00FC);
	write32(oc + OC_WCR, 0xAAFF);
	write32(oc + OC_MCR, 0x0000);
	write32(oc + OC_RTCSR, 0x0000);
	write32(oc + OC_RTCNT, 0x0000);
	write32(oc + OC_RTCOR, 0x0000);
}


void sh2_Exec(SH2 *sh, u32 cycles)
{
	sh2_HandleInterrupts(sh);
	sh2_HandleBreakpoints(sh);
	u32* iblock;
	while(sh->cycles < cycles) {
		iblock = jit_LookupITable[sh->pc];
		if (!iblock) {
			iblock = jit_Recompile();
			jit_LookupITable[sh->pc] = iblock;
		}
		//XXX: Load the used values
		__asm call iblock;
		//XXX: Load the previous values´
	}
}


