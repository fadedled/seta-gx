

#include "emit_ppc.inc"


void sh2_jit_ADD(rn, rm)
{
	PPCC_ADD(rn, rn, rm);
}

void sh2_jit_ADDI(rn, imm)
{
	PPCC_ADDI(rn, rn, imm);		//NOTE: Must sign extend immidiate
}

void sh2_jit_ADDC(rn, rm)
{
	PPCC_RLWINM(GP_TMP, GP_SR, 29, 2, 2); 	/*Extract T*/ \
	PPCC_MTXER(GP_TMP);  					/*Store T in XER[CA]*/ \
	PPCC_ADDEO(rn, rn, rm); 				/*Add rn + rm + XER[CA]*/ \
	PPCC_MFXER(GP_TMP); 					/*Load XER[CA] to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store T*/
}

void sh2_jit_ADDV(rn, rm)
{
	PPCC_ADDO(rn, rn, rm); 					/*Add rn + rm*/
	PPCC_MFXER(GP_TMP); 					/*Load XER[CA] to TMP*/
	PPCC_RLWIMI(GP_SR, GP_TMP, 2, 31, 31); 	/*Store T*/
}


void sh2_jit_SUB(rn, rm)
{
	PPCC_SUBF(rn, rm, rn);
}

void sh2_jit_SUBC(rn, rm)
{
	//XXX
}

void sh2_jit_SUBV(rn, rm)
{
	//XXX
}


void sh2_jit_AND(rn, rm)
{
	PPCC_AND(rn, rn, rm);
}

void sh2_jit_ANDI(imm)
{
	PPCC_ANDI(GP_R0, GP_R0, imm);
}

void sh2_jit_ANDM(imm)
{
	//XXX
}

void sh2_jit_OR(rn, rm)
{
	PPCC_OR(rn, rn, rm);
}

void sh2_jit_ORI(imm)
{
	PPCC_ORI(GP_R0, GP_R0, imm);
}

void sh2_jit_ORM(imm)
{
	//XXX
}

void sh2_jit_XOR(rn, rm)
{
	PPCC_XOR(rn, rn, rm);
}

void sh2_jit_XORI(imm)
{
	PPCC_XORI(GP_R0, GP_R0, imm);
}

void sh2_jit_XORM(imm)
{
	//XXX
}


void sh2_jit_ROTCL(rn)
{
	//XXX
}

void sh2_jit_ROTCR(rn)
{
	//XXX
}

void sh2_jit_ROTL(rn)
{
	PPCC_RLWIMI(GP_SR, rn, 1, 31, 31);	/*Store the T bit*/
	PPCC_RLWINM(rn, rn, 1, 0, 31); 		/*Rotate Left by 1*/
}

void sh2_jit_ROTR(rn)
{
	PPCC_RLWIMI(GP_SR, rn, 0, 31, 31);	/*Store the T bit*/
	PPCC_RLWINM(rn, rn, 32-1, 0, 31); 	/*Rotate Right by 1*/
}

void sh2_jit_SHAL(rn)
{
	PPCC_RLWIMI(GP_SR, rn, 1, 31, 31);	/*Store the T bit*/
	PPCC_RLWINM(rn, rn, 1, 0, 30); 		/*Shift Algebraic Left by 1*/
}

void sh2_jit_SHAR(rn)
{
	PPCC_RLWIMI(GP_SR, rn, 0, 31, 31);	/*Store the T bit*/
	PPCC_SRAWI(rn, rn, 1); 				/*Shift Algebraic Right by 1*/
}

void sh2_jit_SHLL(rn)
{
	PPCC_RLWIMI(GP_SR, rn, 1, 31, 31);	/*Store the T bit*/
	PPCC_RLWINM(rn, rn, 1, 0, 30); 		/*Shift Logical Left by 1*/
}

void sh2_jit_SHLL2(rn)
{
	PPCC_RLWINM(rn, rn, 2, 0, 31-2);
}

void sh2_jit_SHLL8(rn)
{
	PPCC_RLWINM(rn, rn, 8, 0, 31-8);
}

void sh2_jit_SHLL16(rn)
{
	PPCC_RLWINM(rn, rn, 16, 0, 31-16);
}

void sh2_jit_SHLR(rn)
{
	PPCC_RLWIMI(GP_SR, rn, 0, 31, 31);	/*Store the T bit*/
	PPCC_RLWINM(rn, rn, 32-1, 1, 31); 	/*Shift Logical Right by 1*/
}

void sh2_jit_SHLR2(rn)
{
	PPCC_RLWINM(rn, rn, 32-2, 2, 31);
}

void sh2_jit_SHLR8(rn)
{
	PPCC_RLWINM(rn, rn, 32-8, 8, 31);
}
void sh2_jit_SHLR16(rn)
{
	PPCC_RLWINM(rn, rn, 32-16, 16, 31);
}

void sh2_jit_NOT(rn, rm)
{
	PPCC_NOR(rn, rm, rm);
}

void sh2_jit_NEG(rn, rm)
{
	PPCC_NEG(rn, rm);
}

void sh2_jit_NEGC(rn, rm)
{
	//XXX
}

void sh2_jit_DT(rn)
{
	//XXX
}

void sh2_jit_EXTSB(rn, rm)
{
	PPCC_EXTSB(rn, rm);
}

void sh2_jit_EXTSW(rn, rm)
{
	PPCC_EXTSH(rn, rm);
}

void sh2_jit_EXTUB(rn, rm)
{
	PPCC_ANDI(rn, rm, 0x00FF);
}

void sh2_jit_EXTUW(rn, rm)
{
	PPCC_ANDI(rn, rm, 0xFFFF);
}


/*Mult and Division*/
void sh2_jit_DIV0S(rn, rm)
{
	//XXX
}

void sh2_jit_DIV0U()
{
	PPCC_ANDI(GP_SR, GP_SR, 0xFCFE);
}

void sh2_jit_DIV1(rn, rm)
{
	//XXX
}

void sh2_jit_DMULS(rn, rm)
{
	PPCC_MULHW(GP_MACH, rn, rm); 	/*Multiply for signed high word*/
	PPCC_MULLW(GP_MACL, rn, rm); 	/*Multiply for low word*/
}

void sh2_jit_DMULU(rn, rm)
{
	PPCC_MULHWU(GP_MACH, rn, rm); 	/*Multiply for unsigned high word*/
	PPCC_MULLW(GP_MACL, rn, rm); 	/*Multiply for low word*/
}

void sh2_jit_MACL(rn, rm)
{
	//XXX
}

void sh2_jit_MACW(rn, rm)
{
	//XXX
}

void sh2_jit_MULL(rn, rm)
{
	PPCC_MULLW(GP_MACL, rn, rm);
}

void sh2_jit_MULS(rn, rm)
{
	//XXX
}

void sh2_jit_MULU(rn, rm)
{
	//XXX
}


/*Set and Clear*/
void sh2_jit_CLRMAC()
{
	PPCC_ANDI(GP_MACH, GP_TMP, 0x0); 	/*Clear MACH*/
	PPCC_ANDI(GP_MACL, GP_TMP, 0x0); 	/*Clear MACL*/
}

void sh2_jit_CLRT()
{
	PPCC_ANDI(GP_SR, GP_SR, 0xFFFE);
}

void sh2_jit_SETT()
{
	PPCC_ORI(GP_SR, GP_SR, 0x0001);
}


/*Compare*/
void sh2_jit_CMPEQ(rn, rm)
{
	PPCC_CMP(0, rn, rm); 					/*Compare rn and rm*/
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store EQ in T*/
}

void sh2_jit_CMPGE(rn, rm)
{
	//XXX
}

void sh2_jit_CMPGT(rn, rm)
{
	PPCC_CMP(0, rn, rm); 					/*Compare rn and rm*/
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/
	PPCC_RLWIMI(GP_SR, GP_TMP, 2, 31, 31); 	/*Store GT in T*/
}

void sh2_jit_CMPHI(rn, rm)
{
	PPCC_CMPL(0, rn, rm); 					/*Compare Unsigned rn and rm*/
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/
	PPCC_RLWIMI(GP_SR, GP_TMP, 2, 31, 31); 	/*Store GT in T*/
}

void sh2_jit_CMPHS(rn, rm)
{
	//XXX
}

void sh2_jit_CMPPL(rn)
{
	PPCC_CMPI(0, rn, 0x0); 					/*Compare rn to 0*/
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/
	PPCC_RLWIMI(GP_SR, GP_TMP, 2, 31, 31); 	/*Store GT in T*/
}

void sh2_jit_CMPPZ(rn)
{
	//XXX
}

void sh2_jit_CMPSTR(rn, rm)
{
	//XXX
}

void sh2_jit_CMPIM(imm)
{
	//NOTE: Must sign extend immidiate
	PPCC_CMPI(0, rn, imm); 					/*Compare rn to 0*/
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store EQ in T*/
}

/*Load and Stores*/
void sh2_jit_LDCSR(rm)
{
	PPCC_ANDI(GP_SR, rm, 0x03F3);
}

void sh2_jit_LDCGBR(rm)
{
	PPCC_ORI(GP_GBR, rm, 0x0);
}

void sh2_jit_LDCVBR(rm)
{
	PPCC_ORI(GP_VBR, rm, 0x0);
}

void sh2_jit_LDCMSR(rm)
{
	//XXX
}

void sh2_jit_LDCMGBR(rm)
{
	//XXX
}

void sh2_jit_LDCMVBR(rm)
{
	//XXX
}

void sh2_jit_LDSMACH(rm)
{
	PPCC_ORI(GP_MACH, rm, 0x0);
}

void sh2_jit_LDSMACL(rm)
{
	PPCC_ORI(GP_MACL, rm, 0x0);
}

void sh2_jit_LDSPR(rm)
{
	PPCC_ORI(GP_PR, rm, 0x0);
}

void sh2_jit_LDSMMACH(rm)
{
	//XXX
}

void sh2_jit_LDSMMACL(rm)
{
	//XXX
}

void sh2_jit_LDSMPR(rm)
{
	//XXX
}


void sh2_jit_STCSR(rn)
{
	PPCC_ORI(rn, GP_SR, 0x0);
}

void sh2_jit_STCGBR(rn)
{
	PPCC_ORI(rn, GP_GBR, 0x0);
}

void sh2_jit_STCVBR(rn)
{
	PPCC_ORI(rn, GP_VBR, 0x0);
}

void sh2_jit_STCMSR(rn)
{
	//XXX
}

void sh2_jit_STCMGBR(rn)
{
	//XXX
}

void sh2_jit_STCMVBR(rn)
{
	//XXX
}

void sh2_jit_STSMACH(rn)
{
	PPCC_ORI(rn, GP_MACH, 0x0);
}

void sh2_jit_STSMACL(rn)
{
	PPCC_ORI(rn, GP_MACL, 0x0);
}

void sh2_jit_STSPR(rn)
{
	PPCC_ORI(rn, GP_PR, 0x0);
}

void sh2_jit_STSMMACH(rn)
{
	//XXX
}

void sh2_jit_STSMMACL(rn)
{
	//XXX
}

void sh2_jit_STSMPR(rn)
{
	//XXX
}

/*Move Data*/
void sh2_jit_MOV(rn, rm)
{
	PPCC_ORI(rn, rm, 0x0);
}

void sh2_jit_MOVBS(rn, rm)
{
	//XXX
}

void sh2_jit_MOVWS(rn, rm)
{
	//XXX
}

void sh2_jit_MOVLS(rn, rm)
{
	//XXX
}

void sh2_jit_MOVBL(rn, rm)
{
	//XXX
}

void sh2_jit_MOVWL(rn, rm)
{
	//XXX
}

void sh2_jit_MOVLL(rn, rm)
{
	//XXX
}

void sh2_jit_MOVBM(rn, rm)
{
	//XXX
}

void sh2_jit_MOVWM(rn, rm)
{
	//XXX
}

void sh2_jit_MOVLM(rn, rm)
{
	//XXX
}

void sh2_jit_MOVBP(rn, rm)
{
	//XXX
}

void sh2_jit_MOVWP(rn, rm)
{
	//XXX
}

void sh2_jit_MOVLP(rn, rm)
{
	//XXX
}

void sh2_jit_MOVBS0(rn, rm)
{
	//XXX
}

void sh2_jit_MOVWS0(rn, rm)
{
	//XXX
}

void sh2_jit_MOVLS0(rn, rm)
{
	//XXX
}

void sh2_jit_MOVBL0(rn, rm)
{
	//XXX
}

void sh2_jit_MOVWL0(rn, rm)
{
	//XXX
}

void sh2_jit_MOVLL0(rn, rm)
{
	//XXX
}

void sh2_jit_MOVI(rn, imm)
{
	PPCC_ADDI(rn, 0, imm);	//NOTE: Must sign extend immidiate
}

void sh2_jit_MOVWI(d, rn)
{
	//XXX
}

void sh2_jit_MOVLI(d, rn)
{
	//XXX
}

void sh2_jit_MOVBLG(d)
{
	//XXX
}

void sh2_jit_MOVWLG(d)
{
	//XXX
}

void sh2_jit_MOVLLG(d)
{
	//XXX
}

void sh2_jit_MOVBSG(d)
{
	//XXX
}

void sh2_jit_MOVWSG(d)
{
	//XXX
}

void sh2_jit_MOVLSG(d)
{
	//XXX
}

void sh2_jit_MOVBS4(d, rn)
{
	//XXX
}

void sh2_jit_MOVWS4(d, rn)
{
	//XXX
}

void sh2_jit_MOVLS4(d, rn, rm)
{
	//XXX
}

void sh2_jit_MOVBL4(d, rm)
{
	//XXX
}

void sh2_jit_MOVWL4(d, rm)
{
	//XXX
}

void sh2_jit_MOVLL4(d, rn, rm)
{
	//XXX
}


void sh2_jit_MOVA(d)
{
	//XXX
}

void sh2_jit_MOVT(rn)
{
	PPCC_ANDI(rn, GP_SR, 0x0001);
}

/*Branch and Jumps*/
void sh2_jit_BF(d)
{
	//XXX
}

void sh2_jit_BFS(d)
{
	//XXX
}

void sh2_jit_BRA(d)
{
	//XXX
}

void sh2_jit_BRAF(rm)
{
	//XXX
}

void sh2_jit_BSR(d)
{
	//XXX
}

void sh2_jit_BSRF(rm)
{
	//XXX
}

void sh2_jit_BT(d)
{
	//XXX
}

void sh2_jit_BTS(d)
{
	//XXX
}

void sh2_jit_JMP(rm)
{
	//XXX
}

void sh2_jit_JSR(rm)
{
	//XXX
}

void sh2_jit_RTE()
{
	//XXX
}

void sh2_jit_RTS()
{
	//XXX
}



/*Other*/
void sh2_jit_NOP()
{
	/*Does nothing*/
}

void sh2_jit_SLEEP()
{
	//XXX
}

void sh2_jit_SWAPB(rn, rm)
{
	PPCC_ORI(rn, rm, 0x0);					/*RN <- RM*/
	PPCC_RLWINM(GP_TMP, rm, 8, 16, 31);		/*shift first 8 bits*/
	PPCC_RLWINM(GP_TMP, rm, 32-8, 24, 31);	/*shift second 8 bits*/
	PPCC_RLWINM(rn, GP_TMP, 0, 16, 31);		/*Store swap in rn*/
}

void sh2_jit_SWAPW(rn, rm)
{
	PPCC_RLWINM(rn, rm, 16, 0, 31);
}

void sh2_jit_TAS(rn)
{
	//XXX
}

void sh2_jit_TRAPA(imm)
{
	//XXX
}

void sh2_jit_TST(rn, rm)
{
	PPCC_ANDp(rn, rn, rm);					/*Rn AND Rm and check if zero*/
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store EQ (Zero) in T*/
}

void sh2_jit_TSTI(imm)
{
	PPCC_ANDI(GP_R0, GP_R0, imm);			/*R0 AND imm and check if zero*/
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store EQ (Zero) in T*/
}

void sh2_jit_TSTM(imm)
{
	//XXX
}

void sh2_jit_XTRCT(rn, rm)
{
	PPCC_RLWINM(rn, rn, 16, 16, 31); 	/*Shift Rn*/
	PPCC_RLWIMI(rn, rm, 16, 0, 15); 	/*Store shifted Rm in Rn*/
}


u32 sh2_jit_Recompile()
{

}

