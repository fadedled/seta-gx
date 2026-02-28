

#if 0



#include <stdio.h>



extern SH2 *sh_ctx;
static u32 *Rn;
static u32 *Rm;
static u32 imm;
#define disp 	(imm)
#define R0	 	(sh_ctx->r)

#define SR 		(sh_ctx->sr)
#define PC 		(sh_ctx->pc)
#define CYCLES 	(sh_ctx->cycles)


void sh2_int_Unimpl(char *str, u32 n, u32 m)
{
	print("Error Umplimented: %s %d %d\n", str, n, m);
	//TODO: Stop exec
}

void sh2_int_ADD()	/*ADD Rm,Rn*/
{
	*Rn += *Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_ADDI() /*ADD #imm,Rn*/
{
	*Rn += EXT_IMM8(imm);
	PC += 2;
	++CYCLES;
}

void sh2_int_ADDC() /*ADDC Rm,Rn*/
{
	u32 tmp0 = *Rn;
	u32 tmp1 = tmp0 + *Rm;
	*Rn = *Rn + SH2_SR_T(SR);
	SH2_SR_SET_T(SR, tmp0 > tmp1 || tmp1 > *Rn);
	PC += 2;
	++CYCLES;
}

void sh2_int_ADDV() /*ADDV Rm,Rn*/
{
	u32 tmp;
	asm("addo %1, %1, %3\n\t"			/*Add rn + rm*/
		"mfxer %0\n\t"					/*Load XER[CA] to TMP*/
		"rlwimi %2, %0, 2, 31, 31"		/*Store T*/
		: "=r"(tmp), "+r"(*Rn), "+r"(SR)
		: "r" (*Rm)
	);
	PC += 2;
	++CYCLES;
}


void sh2_int_SUB(rn, rm)
{
	sh2_int_Unimpl("SUB", rn, rm);
	//TODO: Cycles
}

void sh2_int_SUBC(rn, rm)
{
	sh2_int_Unimpl("SUBC", rn, rm);
	//TODO: Cycles
}

void sh2_int_SUBV(rn, rm)
{
	sh2_int_Unimpl("SUBV", rn, rm);
	//TODO: Cycles
}


void sh2_int_AND(rn, rm) /*AND Rm,Rn*/
{
	*Rn &= *Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_ANDI()	/*AND #imm,R0*/
{
	*R0 &= imm;
	PC += 2;
	++CYCLES;
}

void sh2_int_ANDM() /*AND.B #imm,@(R0,GBR)*/
{
	u8 *ptr = mem_GetAddr8(sh_ctx->gbr + *R0);
	*ptr &= imm;
	PC += 2;
	//TODO: Cycles
}

void sh2_int_OR() /*OR Rm,Rn*/
{
	*Rn |= *Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_ORI() /*OR #imm,R0*/
{
	*R0 |= imm;
	PC += 2;
	++CYCLES;
}

void sh2_int_ORM() /*OR.B #imm,@(R0,GBR)*/
{
	u8 *ptr = mem_GetAddr8(sh_ctx->gbr + *R0);
	*ptr |= imm;
	PC += 2;
	//TODO: Cycles
}

void sh2_int_XOR() /*XOR Rm,Rn*/
{
	*Rn ^= *Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_XORI() /*XOR #imm,R0*/
{
	*R0 ^= imm;
	PC += 2;
	++CYCLES;
}

void sh2_int_XORM() /*XOR.B #imm,@(R0,GBR)*/
{
	u8 *ptr = mem_GetAddr8(sh_ctx->gbr + *R0);
	*ptr ^= imm;
	PC += 2;
	//TODO: Cycles (3 + (access cycles)*2)
}


void sh2_int_ROTCL() /*ROTCL Rn*/
{
	u32 tmp = *Rn >> 31;
	*Rn = (*Rn << 1) | SH2_SR_T(SR);
	SH2_SR_SET_T(SR, tmp);
	PC += 2;
	++CYCLES;
}

void sh2_int_ROTCR() /*ROTCR Rn*/
{
	u32 tmp = *Rn & 1;
	*Rn = (*Rn >> 1) | (SH2_SR_T(SR) << 31);
	SH2_SR_SET_T(SR, tmp);
	PC += 2;
	++CYCLES;
}

void sh2_int_ROTL() /*ROTL Rn*/
{
	u32 tmp = *Rn >> 31;
	*Rn = (*Rn << 1) | tmp;
	SH2_SR_SET_T(SR, tmp);
	PC += 2;
	++CYCLES;
}

void sh2_int_ROTR() /*ROTR Rn*/
{
	u32 tmp = *Rn & 1;
	*Rn = (*Rn >> 1) | (tmp << 31);
	SH2_SR_SET_T(SR, tmp);
	PC += 2;
	++CYCLES;
}

void sh2_int_SHAL() /*SHAL Rn*/
{
	SH2_SR_SET_T(SR, *Rn >> 31);
	*Rn <<= 1;
	PC += 2;
	++CYCLES;
}

void sh2_int_SHAR() /*SHAR Rn*/
{
	SH2_SR_SET_T(SR, *Rn & 1);
	*Rn = (u32)(((s32)*Rn) >> 1);
	PC += 2;
	++CYCLES;
}

//Does the same thing
#define sh2_int_SHLL	sh2_int_SHAL


void sh2_int_SHLL2() /*SHLL2 Rn*/
{
	*Rn <<= 2;
	PC += 2;
	++CYCLES;
}

void sh2_int_SHLL8() /*SHLL8 Rn*/
{
	*Rn <<= 8;
	PC += 2;
	++CYCLES;
}

void sh2_int_SHLL16() /*SHLL16 Rn*/
{
	*Rn <<= 16;
	PC += 2;
	++CYCLES;
}

void sh2_int_SHLR(rn) /*SHLR Rn*/
{
	SH2_SR_SET_T(SR, *Rn & 1);
	*Rn >>= 1;
	PC += 2;
	++CYCLES;
}

void sh2_int_SHLR2() /*SHLR2 Rn*/
{
	*Rn >>= 2;
	PC += 2;
	++CYCLES;
}

void sh2_int_SHLR8() /*SHLR8 Rn*/
{
	*Rn >>= 8;
	PC += 2;
	++CYCLES;
}

void sh2_int_SHLR16() /*SHLR16 Rn*/
{
	*Rn >>= 16;
	PC += 2;
	++CYCLES;
}

void sh2_int_NOT() /*NOT Rm,Rn*/
{
	*Rn = ~*Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_NEG(rn, rm) /*NEG Rm,Rn*/
{
	*Rn = -*Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_NEGC(rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_DT() /*DT Rn*/
{
	--*Rn;
	SH2_SR_SET_T(SR, !*Rn);
	PC += 2;
	++CYCLES;
}

void sh2_int_EXTSB() /*EXTS.B Rm,Rn*/
{
	*Rn = (u32) ((((s32)*Rm) << 24) >> 24);
	PC += 2;
	++CYCLES;
}

void sh2_int_EXTSW() /*EXTS.W Rm,Rn*/
{
	*Rn = (u32) ((((s32)*Rm) << 16) >> 16);
	PC += 2;
	++CYCLES;
}

void sh2_int_EXTUB() /*EXTU.B Rm,Rn*/
{
	*Rn = *Rm & 0xFF;
	PC += 2;
	++CYCLES;
}

void sh2_int_EXTUW() /*EXTU.W Rm,Rn*/
{
	*Rn = *Rm & 0xFFFF;
	PC += 2;
	++CYCLES;
}


/*Mult and Division*/
void sh2_int_DIV0S(rn, rm)
{
	SH2_SR_SET_Q(SR, *Rn >> 31);
	SH2_SR_SET_M(SR, *Rm >> 31);
	SH2_SR_SET_T(SR, ((*Rm >> 31) ^ (*Rn >> 31)));
	PC += 2;
	++CYCLES;
}

void sh2_int_DIV0U() /*DIV0U*/
{
	SR &= ~(SH2_SR_T_BIT | SH2_SR_M_BIT | SH2_SR_Q_BIT)
	PC += 2;
	++CYCLES;
}

void sh2_int_DIV1(rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_DMULS(rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_DMULU(rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MACL(rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MACW(rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MULL(rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MULS(rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MULU(rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}


/*Set and Clear*/
void sh2_int_CLRMAC() /*CLRMAC*/
{
	sh_ctx->mach = 0;
	sh_ctx->macl = 0;
	PC += 2;
	++CYCLES;
}

void sh2_int_CLRT() /*CLRT*/
{
	SH2_SR_SET_T(SR, 0)
	PC += 2;
	++CYCLES;
}

void sh2_int_SETT() /*SETT*/
{
	SH2_SR_SET_T(SR, 1)
	PC += 2;
	++CYCLES;
}


/*Compare*/
void sh2_int_CMPEQ() /*CMP_EQ Rm,Rn*/
{
	SH2_SR_SET_T(SR, *Rn == *Rm);
	PC += 2;
	++CYCLES;
}

void sh2_int_CMPGE() /*CMP_GE Rm,Rn*/
{
	SH2_SR_SET_T(SR, ((s32)*Rn) >= ((s32)*Rm));
	PC += 2;
	++CYCLES;
}

void sh2_int_CMPGT() /*CMP_GT Rm,Rn*/
{
	SH2_SR_SET_T(SR, ((s32)*Rn) > ((s32)*Rm));
	PC += 2;
	++CYCLES;
}

void sh2_int_CMPHI() /*CMP_HI Rm,Rn*/
{
	SH2_SR_SET_T(SR, *Rn > *Rm);
	PC += 2;
	++CYCLES;
}

void sh2_int_CMPHS() /*CMP_HS Rm,Rn*/
{
	SH2_SR_SET_T(SR, *Rn >= *Rm);
	PC += 2;
	++CYCLES;
}

void sh2_int_CMPPL() /*CMP_PL Rn*/
{
	SH2_SR_SET_T(SR, ((s32)*Rn) > 0);
	PC += 2;
	++CYCLES;
}

void sh2_int_CMPPZ() /*CMP_PZ Rn*/
{
	SH2_SR_SET_T(SR, ((s32)*Rn) >= 0);
	PC += 2;
	++CYCLES;
}

void sh2_int_CMPSTR() /*CMP_STR Rm,Rn*/
{
	u32 tmp = *Rn ^ *Rm;
	tmp &= (tmp >> 16);
	tmp &= (tmp >> 8);
	SH2_SR_SET_T(SR, !tmp);
	PC += 2;
	++CYCLES;
}

void sh2_int_CMPIM() /*CMP_EQ #imm,R0*/
{
	SH2_SR_SET_T(SR, *R0 == EXT_IMM8(imm));
	PC += 2;
	++CYCLES;
}

/*Load and Stores*/
void sh2_int_LDCSR() /*LDC Rm,SR*/
{
	SR = *Rm & 0x3F3;
	PC += 2;
	++CYCLES;
}

void sh2_int_LDCGBR() /*LDC Rm,GBR*/
{
	sh_ctx->gbr = *Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_LDCVBR() /*LDC Rm,VBR*/
{
	sh_ctx->vbr = *Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_LDCMSR() /*LDC.L @Rm+,SR*/
{
	SR = mem_Read32(*Rm) & 0x3F3;
	*Rm += 4;
	PC += 2;
	//TODO: Cycles
}

void sh2_int_LDCMGBR() /*LDC.L @Rm+,GBR*/
{
	sh_ctx->gbr = mem_Read32(*Rm);
	*Rm += 4;
	PC += 2;
	//TODO: Cycles
}

void sh2_int_LDCMVBR() /*LDC.L @Rm+,VBR*/
{
	sh_ctx->vbr = mem_Read32(*Rm);
	*Rm += 4;
	PC += 2;
	//TODO: Cycles
}

void sh2_int_LDSMACH() /*LDS Rm,MACH*/
{
	sh_ctx->mach = *Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_LDSMACL() /*LDS Rm,MACL*/
{
	sh_ctx->macl = *Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_LDSPR() /*LDS Rm,PR*/
{
	sh_ctx->pr = *Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_LDSMMACH() /*LDS.L @Rm+,MACH*/
{
	sh_ctx->mach = mem_Read32(*Rm);
	*Rm += 4;
	PC += 2;
	//TODO: Cycles
}

void sh2_int_LDSMMACL() /*LDS.L @Rm+,MACL*/
{
	sh_ctx->macl = mem_Read32(*Rm);
	*Rm += 4;
	PC += 2;
	//TODO: Cycles
}

void sh2_int_LDSMPR() /*LDS.L @Rm+,PR*/
{
	sh_ctx->pr = mem_Read32(*Rm);
	*Rm += 4;
	PC += 2;
	//TODO: Cycles
}


void sh2_int_STCSR() /* STC SR,Rn */
{
	*Rn = SR;
	PC += 2;
	++CYCLES;
}

void sh2_int_STCGBR() /* STC GBR,Rn */
{
	*Rn = sh_ctx->gbr;
	PC += 2;
	++CYCLES;
}

void sh2_int_STCVBR() /* STC VBR,Rn */
{
	*Rn = sh_ctx->vbr;
	PC += 2;
	++CYCLES;
}

void sh2_int_STCMSR() /* STC.L SR,@-Rn */
{
	*Rn -= 4;
	mem_Write32(*Rn, SR);
	PC += 2;
	//TODO: Cycles
}

void sh2_int_STCMGBR() /* STC.L GBR,@-Rn */
{
	*Rn -= 4;
	mem_Write32(*Rn, sh_ctx->gbr);
	PC += 2;
	//TODO: Cycles
}

void sh2_int_STCMVBR() /* STC.L VBR,@-Rn */
{
	*Rn -= 4;
	mem_Write32(*Rn, sh_ctx->vbr);
	PC += 2;
	//TODO: Cycles
}

void sh2_int_STSMACH() /* STS MACH,Rn */
{
	*Rn = sh_ctx->mach;
	PC += 2;
	++CYCLES;
}

void sh2_int_STSMACL() /* STS MACL,Rn */
{
	*Rn = sh_ctx->macl;
	PC += 2;
	++CYCLES;
}

void sh2_int_STSPR() /* STS PR,Rn */
{
	*Rn = sh_ctx->pr;
	PC += 2;
	++CYCLES;
}

void sh2_int_STSMMACH() /* STS.L MACH,@–Rn */
{
	*Rn -= 4;
	mem_Write32(*Rn, sh_ctx->mach);
	PC += 2;
	//TODO: Cycles
}

void sh2_int_STSMMACL() /* STS.L MACL,@–Rn */
{
	*Rn -= 4;
	mem_Write32(*Rn, sh_ctx->macl);
	PC += 2;
	//TODO: Cycles
}

void sh2_int_STSMPR() /* STS.L PR,@–Rn */
{
	*Rn -= 4;
	mem_Write32(*Rn, sh_ctx->pr);
	PC += 2;
	//TODO: Cycles
}

/*Move Data*/
void sh2_int_MOV(rn, rm) /* MOV Rm,Rn */
{
	*Rn = Rm;
	PC += 2;
	++CYCLES;
}

void sh2_int_MOVBS() /* MOV.B Rm,@Rn */
{
	mem_Write8(*Rn, *Rm);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVWS() /* MOV.W Rm,@Rn */
{
	mem_Write16(*Rn, *Rm);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVLS() /* MOV.L Rm,@Rn */
{
	mem_Write32(*Rn, *Rm);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVBL() /* MOV.B @Rm,Rn */
{
	*Rn = (s32)(s8) mem_Read8(*Rm); //NOTE: check this
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVWL() /* MOV.W @Rm,Rn */
{
	*Rn = (s32)(s16) mem_Read16(*Rm); //NOTE: check this
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVLL() /* MOV.L @Rm,Rn */
{
	*Rn = mem_Read32(*Rm); //NOTE: check this
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVBM() /* MOV.B Rm,@–Rn */
{
	*Rn -= 1;
	mem_Write8(*Rn, *Rm);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVWM() /* MOV.W Rm,@–Rn */
{
	*Rn -= 2;
	mem_Write16(*Rn, *Rm);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVLM() /* MOV.L Rm,@–Rn */
{
	*Rn -= 4;
	mem_Write32(*Rn, *Rm);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVBP() /* MOV.W @Rm+,Rn */
{
	*Rn = (s32)(s8) mem_Read8(*Rm); //NOTE: check this
	*Rm += (n!=m);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVWP() /* MOV.L @Rm+,Rn */
{
	*Rn = (s32)(s16) mem_Read16(*Rm); //NOTE: check this
	*Rm += (n!=m) << 1;
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVLP() /* MOV.B Rm,@(R0,Rn) */
{
	*Rn = mem_Read32(*Rm); //NOTE: check this
	*Rm += (n!=m) << 2;
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVBS0() /* MOV.B Rm,@(R0,Rn) */
{
	mem_Write8(*Rn + *R0, *Rm);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVWS0() /* MOV.W Rm,@(R0,Rn) */
{
	mem_Write16(*Rn + *R0, *Rm);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVLS0() /* MOV.L Rm,@(R0,Rn) */
{
	mem_Write32(*Rn + *R0, *Rm);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVBL0() /* MOV.B @(R0,Rm),Rn */
{
	*Rn = (s32)(s8) mem_Read8(*Rm + *R0); //NOTE: check this
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVWL0() /* MOV.W @(R0,Rm),Rn */
{
	*Rn = (s32)(s16) mem_Read16(*Rm + *R0); //NOTE: check this
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVLL0() /* MOV.L @(R0,Rm),Rn */
{
	*Rn = mem_Read32(*Rm + *R0);
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVI() /* MOV #imm,Rn */
{
	*Rn = EXT_IMM8(imm);
	PC += 2;
	++CYCLES;
}

void sh2_int_MOVWI() /* MOV.W @(disp,PC),Rn */
{
	*Rn = (s32)(s16) mem_Read16(PC + (disp << 1)); //NOTE: check this
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVLI() /* MOV.L @(disp,PC),Rn */
{
	*Rn = mem_Read32((PC & 0xFFFFFFFC) + (disp << 2));
	PC += 2;
	CYCLES += 1; //TODO: Add write cycles
}

void sh2_int_MOVBLG(d)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVWLG(d)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVLLG(d)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVBSG(d)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVWSG(d)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVLSG(d)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVBS4(d, rn)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVWS4(d, rn)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVLS4(d, rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVBL4(d, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVWL4(d, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}

void sh2_int_MOVLL4(d, rn, rm)
{
	sh2_int_Unimpl("ORM", imm, -1);
	//TODO: Cycles
}


void sh2_int_MOVA() /* MOVA @(disp,PC),R0 */
{
	*R0 = (PC & 0xFFFFFFFC) + (disp << 2);
	PC += 2;
	++CYCLES;
}

void sh2_int_MOVT() /* MOVT Rn */
{
	*Rn = (SR & 1);
	PC += 2;
	++CYCLES;
}

/*Branch and Jumps*/
void sh2_int_BF(d)
{
	sh2_int_Unimpl("BF", d, -1);
	//TODO: Cycles
}

void sh2_int_BFS(d)
{
	sh2_int_Unimpl("BFS", d, -1);
	//TODO: Cycles
}

void sh2_int_BRA(d)
{
	sh2_int_Unimpl("BRA", d, -1);
	//TODO: Cycles
}

void sh2_int_BRAF(rm)
{
	sh2_int_Unimpl("BRAF", rm, -1);
	//TODO: Cycles
}

void sh2_int_BSR(d)
{
	sh2_int_Unimpl("BSR", d, -1);
	//TODO: Cycles
}

void sh2_int_BSRF(rm)
{
	sh2_int_Unimpl("BSRF", rm, -1);
	//TODO: Cycles
}

void sh2_int_BT(d)
{
	sh2_int_Unimpl("BT", d, -1);
	//TODO: Cycles
}

void sh2_int_BTS(d)
{
	sh2_int_Unimpl("BTS", d, -1);
	//TODO: Cycles
}

void sh2_int_JMP(rm)
{
	sh2_int_Unimpl("JMP", rm, -1);
	//TODO: Cycles
}

void sh2_int_JSR(rm)
{
	sh2_int_Unimpl("JSR", rm, -1);
	//TODO: Cycles
}

void sh2_int_RTE()
{
	sh2_int_Unimpl("RTE", -1, -1);
	//TODO: Cycles
}

void sh2_int_RTS()
{
	sh2_int_Unimpl("RTS", -1, -1);
	//TODO: Cycles
}



/*Other*/
void sh2_int_NOP()
{
	/*Does nothing..*/
	sh2_int_Unimpl("NOP", -1, -1);
	//TODO: Cycles
}

void sh2_int_SLEEP()
{
	sh2_int_Unimpl("SLEEP", -1, -1);
	//TODO: Cycles
}

void sh2_int_SWAPB(rn, rm)
{
	sh2_int_Unimpl("SWAPB", rn, rm);
	//TODO: Cycles
}

void sh2_int_SWAPW(rn, rm)
{
	sh2_int_Unimpl("SWAPW", rn, rm);
	//TODO: Cycles
}

void sh2_int_TAS(rn)
{
	sh2_int_Unimpl("TAS", rn, -1);
	//TODO: Cycles
}

void sh2_int_TRAPA(imm)
{
	sh2_int_Unimpl("TRAPA", imm, -1);
	//TODO: Cycles
}

void sh2_int_TST(rn, rm)
{
	sh2_int_Unimpl("TST", rn, rm);
	//TODO: Cycles
}

void sh2_int_TSTI(imm)
{
	sh2_int_Unimpl("TSTI", imm, -1);
	//TODO: Cycles
}

void sh2_int_TSTM(imm)
{
	sh2_int_Unimpl("TSTM", imm, -1);
	//TODO: Cycles
}

void sh2_int_XTRCT(rn, rm)
{
	sh2_int_Unimpl("XTRCT", rn, rm);
	//TODO: Cycles
}


u32 sh2_ExecInt(u32 cycles)
{
	//TODO: implement the interpreter
	while(sh_ctx->cycles < cycles) {
		iblock = jit_LookupITable[sh_ctx->pc];
		if (!iblock) {
			iblock = jit_Recompile();
			jit_LookupITable[sh_ctx->pc] = iblock;
		}
	}
}
#endif

