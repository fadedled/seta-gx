




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



	u32 is_slave;
} SH2;

SH2 msh2;
SH2 ssh2;


void sh2_Init(void)
{
	//Initialize the main sh2
	msh2.is_slave = 0;


	//Initialize the main sh2
	msh2.is_slave = 1;
}

void sh2_Reset(SH2 *sh)
{


}

void sh2_HandleInterrupts(SH2 *sh)
{

}

void sh2_HandleBreakpoints(SH2 *sh)
{

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

