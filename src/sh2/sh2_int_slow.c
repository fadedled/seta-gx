/********************************************************************************
 * Copyright (C) 2026 Evoca
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 *******************************************************************************/
#if 0
#include <gccore.h>
#include "sh2.h"


extern u8  sh2_Read8(u32);
extern u16 sh2_Read16(u32);
extern u32 sh2_Read32(u32);
extern void sh2_Write8(u32,u8);
extern void sh2_Write16(u32,u16);
extern void sh2_Write32(u32,u32);



#define RN(inst)		((inst >> 8) & 0xF)
#define RM(inst)		((inst >> 4) & 0xF)

typedef void (*SH2InstFunc)(SH2*, u32);
void sh2_int_DelaySlot(SH2* sh);




void sh2_int_ADD(SH2 *sh, u32 inst) { /* ADD Rm,Rn  0011nnnnmmmm1100 */
	sh->r[RN(inst)] += sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ADDI(SH2 *sh, u32 inst) { /* ADD #imm,Rn  0111nnnniiiiiiii */
	sh->r[RN(inst)] += EXT_IMM8(inst);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ADDC(SH2 *sh, u32 inst) { /* ADDC Rm,Rn  0011nnnnmmmm1110 */
	u32 tmp;
	asm("rlwinm %[tmp], %[sr], 29, 2, 2\n\t"
		"mtxer %[tmp]\n\t"
		"adde %[rn], %[rn], %[rm]\n\t"
		"mfxer %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 3, 31, 31"
		: [tmp] "=r"(tmp), [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
		: [rm] "r" (sh->r[RM(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ADDV(SH2 *sh, u32 inst) { /* ADDV Rm,Rn  0011nnnnmmmm1111 */
	u32 tmp;
	asm("addo %[rn], %[rn], %[rm]\n\t"			/*Add rn + rm*/
		"mfxer %[tmp]\n\t"						/*Load XER[CA] to TMP*/
		"rlwimi %[sr], %[tmp], 2, 31, 31"		/*Store T*/
		: [tmp] "=r"(tmp), [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
		: [rm] "r" (sh->r[RM(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SUB(SH2 *sh, u32 inst) { /* SUB Rm,Rn  0011nnnnmmmm1111 */
	sh->r[RN(inst)] -= sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SUBC(SH2 *sh, u32 inst) { /* SUBC Rm,Rn  0011nnnnmmmm1010*/
	u32 tmp;
	asm("xori %[sr], %[sr], 1\n\t"
		"rlwinm %[tmp], %[sr], 29, 2, 2\n\t"
		"mtxer %[tmp]\n\t"
		"subfeo %[rn], %[rm], %[rn]\n\t"
		"mfxer %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 3, 31, 31\n\t"
		"xori %[sr], %[sr], 1"
		: [tmp] "=r"(tmp), [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
		: [rm] "r" (sh->r[RM(inst)])
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SUBV(SH2 *sh, u32 inst) { /* SUBV Rm,Rn  0011nnnnmmmm1011*/
	u32 tmp;
	asm("subfo %[rn], %[rm], %[rn]\n\t"
		"mfxer %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 2, 31, 31\n\t"
		: [tmp] "=r"(tmp), [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
		: [rm] "r" (sh->r[RM(inst)])
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_AND(SH2 *sh, u32 inst) { /* AND Rm,Rn  0010nnnnmmmm1001 */
	sh->r[RN(inst)] &= sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ANDI(SH2 *sh, u32 inst) { /* AND #imm,R0  11001001iiiiiiii */
	sh->r[RN(inst)] &= (inst & 0xFF);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ANDM(SH2 *sh, u32 inst) { /* AND.B #imm,@(R0,GBR)  11001101iiiiiiii*/
	u32 addr = sh->r[0] + sh->gbr;
	u32 val = sh2_Read8(sh->r[0] + sh->gbr) & (inst & 0xFF);
	sh2_Write8(addr, val);
	sh->pc += 2;
	sh->cycles += 3;
}

void sh2_int_OR(SH2 *sh, u32 inst) { /* OR Rm,Rn  0010nnnnmmmm1011 */
	sh->r[RN(inst)] |= sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ORI(SH2 *sh, u32 inst) { /* OR #imm,R0  11001011iiiiiiii */
	sh->r[RN(inst)] |= (inst & 0xFF);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ORM(SH2 *sh, u32 inst) { /* OR.B #imm,@(R0,GBR)  11001111iiiiiiii*/
	u32 addr = sh->r[0] + sh->gbr;
	u32 val = sh2_Read8(sh->r[0] + sh->gbr) | (inst & 0xFF);
	sh2_Write8(addr, val);
	sh->pc += 2;
	sh->cycles += 3;
}

void sh2_int_XOR(SH2 *sh, u32 inst) { /* XOR Rm,Rn  0010nnnnmmmm1010 */
	sh->r[RN(inst)] ^= sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_XORI(SH2 *sh, u32 inst) { /* XOR #imm,R0  11001010iiiiiiii */
	sh->r[RN(inst)] ^= (inst & 0xFF);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_XORM(SH2 *sh, u32 inst) { /* XOR.B #imm,@(R0,GBR)  11001110iiiiiiii*/
	u32 addr = sh->r[0] + sh->gbr;
	u32 val = sh2_Read8(sh->r[0] + sh->gbr) ^ (inst & 0xFF);
	sh2_Write8(addr, val);
	sh->pc += 2;
	sh->cycles += 3;
}

void sh2_int_ROTCL(SH2 *sh, u32 inst) { /* ROTCL Rn  0100nnnn00100100 */
	u32 tmp;
	asm("rlwinm %[rn], %[rn], 29, 2, 2\n\t"
		"andi. %[tmp], %[rn], 1\n\t"
		"rlwimi %[rn], %[sr], 0, 31, 31\n\t"
		"rlwimi %[sr], %[tmp], 0, 31, 31\n\t"
		: [tmp] "=r"(tmp), [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ROTCR(SH2 *sh, u32 inst) { /* ROTCR Rn  0100nnnn00100101 */
	u32 tmp;
	asm("andi. %[tmp], %[rn], 1\n\t"
		"rlwimi %[rn], %[sr], 0, 31, 31\n\t"
		"rlwinm %[rn], %[rn], 32-1, 0, 31\n\t"
		"rlwimi %[sr], %[tmp], 0, 31, 31\n\t"
		: [tmp] "=r"(tmp), [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ROTL(SH2 *sh, u32 inst) { /* ROTL Rn  0100nnnn00000100 */
	asm("rlwimi %[sr], %[rn], 1, 31, 31\n\t"
		"rlwinm %[rn], %[rn], 1, 0, 31\n\t"
		: [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ROTR(SH2 *sh, u32 inst) {
	asm("rlwimi %[sr], %[rn], 0, 31, 31\n\t"
		"rlwinm %[rn], %[rn], 32-1, 0, 31\n\t"
		: [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHAL(SH2 *sh, u32 inst) {
	asm("rlwimi %[sr], %[rn], 1, 31, 31\n\t"
		"rlwinm %[rn], %[rn], 1, 0, 30\n\t"
		: [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHAR(SH2 *sh, u32 inst) {
	asm("rlwimi %[sr], %[rn], 0, 31, 31\n\t"
		"srawi %[rn], %[rn], 1\n\t"
		: [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHLL(SH2 *sh, u32 inst) {
	asm("rlwimi %[sr], %[rn], 1, 31, 31\n\t"
		"rlwinm %[rn], %[rn], 1, 0, 30\n\t"
		: [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHLL2(SH2 *sh, u32 inst) {
	sh->r[RN(inst)] <<= 2;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHLL8(SH2 *sh, u32 inst) {
	sh->r[RN(inst)] <<= 8;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHLL16(SH2 *sh, u32 inst) {
	sh->r[RN(inst)] <<= 16;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHLR(SH2 *sh, u32 inst) {
	asm("rlwimi %[sr], %[rn], 0, 31, 31\n\t"
		"rlwinm %[rn], %[rn], 32-1, 1, 31\n\t"
		: [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHLR2(SH2 *sh, u32 inst) {
	sh->r[RN(inst)] >>= 2;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHLR8(SH2 *sh, u32 inst) {
	sh->r[RN(inst)] >>= 8;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SHLR16(SH2 *sh, u32 inst) {
	sh->r[RN(inst)] >>= 16;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_NOT(SH2 *sh, u32 inst) { /* NOT Rm,Rn  0110nnnnmmmm0111 */
	sh->r[RN(inst)] = ~sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_NEG(SH2 *sh, u32 inst) { /* NEG Rm,Rn  0110nnnnmmmm1011 */
	sh->r[RN(inst)] = -sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_NEGC(SH2 *sh, u32 inst) { /* NEGC Rm,Rn  0110nnnnmmmm1010 */
	u32 tmp;
	asm("xori %[tmp], %[sr], 1\n\t"
		"rlwinm %[tmp], %[tmp], 29, 2, 2\n\t"
		"mtxer %[tmp]\n\t"
		"subfze %[rn], %[rm]\n\t"
		"mfxer %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 3, 31, 31\n\t"
		"xori %[sr], %[sr], 1\n\t"
		: [tmp] "=r"(tmp), [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
		: [rm] "r"(sh->r[RM(inst)])
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_DT(SH2 *sh, u32 inst) { /* DT Rn  0100nnnn00010000 */
	u32 tmp;
	asm("addic. %[rn], %[rn], -1\n\t"
		"mfcr   %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 3, 31, 31\n\t"
		: [tmp] "=r"(tmp), [rn] "+r"(sh->r[RN(inst)]), [sr] "+r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_EXTSB(SH2 *sh, u32 inst) { /* EXTS.B Rm,Rn  0110nnnnmmmm1110 */
	sh->r[RN(inst)] = (s8) sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_EXTSW(SH2 *sh, u32 inst) { /* EXTS.W Rm,Rn  0110nnnnmmmm1111 */
	sh->r[RN(inst)] = (s16) sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_EXTUB(SH2 *sh, u32 inst) { /* EXTU.B Rm,Rn  0110nnnnmmmm1100 */
	sh->r[RN(inst)] = sh->r[RM(inst)] & 0xFF;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_EXTUW(SH2 *sh, u32 inst) { /* EXTU.W Rm,Rn  0110nnnnmmmm1101 */
	sh->r[RN(inst)] = sh->r[RM(inst)] & 0xFFFF;
	sh->pc += 2;
	sh->cycles += 1;
}


/*Mult and Division*/

void sh2_int_DIV0S(SH2 *sh, u32 inst) {
	u32 tmp;
	asm("rlwimi %[sr], %[rn], 8+1, 31-8, 31-8\n\t"
		"rlwimi %[sr], %[rm], 9+1, 31-9, 31-9\n\t"
		"xor %[tmp], %[rn], %[rm]\n\t"
		"rlwimi %[sr], %[tmp], 1, 31, 31\n\t"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [rn] "r"(sh->r[RN(inst)]), [rm] "r"(sh->r[RM(inst)])
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_DIV0U(SH2 *sh, u32 inst) {
	sh->sr &= 0xFCFE;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_DIV1(SH2 *sh, u32 inst) {
	u32 tmp;
	asm("mtcrf	0x07, %[sr]\n\t"
		"creqv	3, 31-9, 31-8\n\t"					//(M == oldQ)
		"bc	    0b00100, 3,	div1_skip_rm_neg\n\t" 	// do not negate if false
		"neg    %[rm], %[rm]\n\t"					//Rm = -Rm
	"div1_skip_rm_neg:\n\t"
		"mtcrf	0x80, %[rn]\n\t"					//Q = MSB(Rn)
		"crxor  31-8, 0, 31-8\n\t"					//Q ^= oldQ
		"rlwimi %[sr], %[rn], 1, 0, 30\n\t"		// Rn = (Rn << 1) | T
		"addc   %[rn], %[sr], %[rm]\n\t"			// Rn += Rm
		"mcrxr  0\n\t"								// Q ^= carry(Rn += Rm)
		"creqv  31-8, 2, 31-8\n\t"
		"creqv  31, 31-8, 31-9\n\t"				//T = (Q == M)
		"mfcr   %[tmp]\n\t"
		"andi.	%[sr], %[tmp], 0xFFF\n\t"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr), [rn] "+r"(sh->r[RN(inst)])
		: [rm] "r"(sh->r[RM(inst)])
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_DMULS(SH2 *sh, u32 inst) {
	asm("mulhw  %[mach], %[rn], %[rm]\n\t"
		"mullw	%[macl], %[rn], %[rm]\n\t"
		: [mach] "=r"(sh->mach), [macl] "=r"(sh->macl)
		: [rn] "r"(sh->r[RN(inst)]), [rm] "r"(sh->r[RM(inst)])
	);
	sh->pc += 2;
	sh->cycles += 2;
}

void sh2_int_DMULU(SH2 *sh, u32 inst) {
	asm("mulhwu %[mach], %[rn], %[rm]\n\t"
		"mullw	%[macl], %[rn], %[rm]\n\t"
		: [mach] "=r"(sh->mach), [macl] "=r"(sh->macl)
		: [rn] "r"(sh->r[RN(inst)]), [rm] "r"(sh->r[RM(inst)])
	);
	sh->pc += 2;
	sh->cycles += 2;
}

void sh2_int_MACL(SH2 *sh, u32 inst) {
	u32 t0, t1, sat;
	u32 rn = (sh->r[RN(inst)] += 4) - 4;
	rn = sh2_Read32(rn);
	u32 rm = (sh->r[RM(inst)] += 4) - 4;
	rm = sh2_Read32(rm);
	asm(
		"mullw  %[t0], %[rn], %[rm]\n\t"				// calc low(rn*rm)
		"xor    %[sat], %[rn], %[rm]\n\t"				// xor rn * rm and MSB for flipping saturation
		"srawi  %[sat], %[sat], 31\n\t"					// extend MSB
		"mulhw  %[t1], %[rn], %[rm]\n\t"				// calc high(rn*rm)
		"addc   %[macl], %[macl], %[t0]\n\t"			// result low macl + low(rn*rm)
		"mtcrf  0x01, %[sr]\n\t"		 				// is S set? move S to CR
		"adde   %[mach], %[mach], %[t1]\n\t"			// result high macl + high(rn*rm)
		"addic. %[t0], %[mach], -0x8000\n\t"			// add 0xFFFF8000_00000000 to result
		"crandc 0, 30, 0\n\t"							// is S set and is it negative?
		"bc	    0b00100, 0,	macl_skip_saturation\n\t" 	// branch to end if false
		"xori   %[mach], %[sat], 0x7FFF\n\t"
		"not    %[macl], %[sat]\n\t"
"macl_skip_saturation:\n\t"
		: [t0] "=r"(t0), [t1] "=r"(t1), [sat] "=r"(sat),
		  [mach] "+r"(sh->mach), [macl] "+r"(sh->macl)
		: [rn] "r"(rn), [rm] "r"(rm), [sr] "r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 2;
}

void sh2_int_MACW(SH2 *sh, u32 inst) {
	u32 t0;
	u32 rn = (sh->r[RN(inst)] += 2) - 2;
	rn = (s16) sh2_Read16(rn);
	u32 rm = (sh->r[RM(inst)] += 2) - 2;
	rm = (s16) sh2_Read16(rm);
	asm("mullw %[rn], %[rn], %[rm]\n\t"
		"andi. %[t0], %[sr], 0x2\n\t"			//Check for saturation (S bit)
		"bc 0b01100, 1, macw_saturate\n\t"
		"addc %[macl], %[macl], %[rn]\n\t" 		//TODO: Add lower, this can be optimized with addco
		"addze %[mach], %[mach]\n\t"
		"b finish\n\t"
	"macw_saturate:\n\t"
		"addo %[rn], %[macl], %[rn]\n\t"
		"mfxer %[t0]\n\t"
		"rlwinm %[t0], %[t0], 2, 31, 31\n\t"
		"neg %[t0], %[t0]\n\t"
		"andc %[macl], %[rn], %[t0]\n\t"
		"srawi %[rn], %[rn], 31\n\t"
		"addis %[rn], %[rn], 0x8000\n\t"
		"and %[rn], %[rn], %[t0]\n\t"
		"or %[macl], %[macl], %[t0]\n\t"
		"ori %[mach], %[t0], 0x1\n\t"
	"finish:\n\t"
		: [t0] "=r"(t0), [mach] "+r"(sh->mach), [macl] "+r"(sh->macl)
		: [rn] "r"(rn), [rm] "r"(rm), [sr] "r"(sh->sr)
	);
	sh->pc += 2;
	sh->cycles += 2;
}


void sh2_int_MULL(SH2 *sh, u32 inst) {
	sh->macl = sh->r[RN(inst)] * sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 2;
}

void sh2_int_MULS(SH2 *sh, u32 inst) {
	sh->macl = ((s32)((s16)sh->r[RN(inst)])) * ((s32)((s16)sh->r[RM(inst)]));
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MULU(SH2 *sh, u32 inst) {
	sh->macl = (sh->r[RN(inst)] & 0xFFFF) * (sh->r[RM(inst)] & 0xFFFF);
	sh->pc += 2;
	sh->cycles += 1;
}


/*Set and Clear*/
void sh2_int_CLRMAC(SH2 *sh, u32 inst) { /* CLRMAC  0000000000101000 */
	sh->macl = sh->mach = 0;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_CLRT(SH2 *sh, u32 inst) { /* CLRT  0000000000001000 */
	sh->sr &= 0xFFFE;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SETT(SH2 *sh, u32 inst) { /* SETT  0000000000011000 */
	sh->sr |= 0x1;
	sh->pc += 2;
	sh->cycles += 1;
}


/*Compares*/
void sh2_int_CMPEQ(SH2 *sh, u32 inst) { /* CMP_EQ Rm,Rn  0011nnnnmmmm0000 */
	u32 tmp;
	asm("cmpw 0, %[rn], %[rm]\n\t"			/*compare rn, rm*/
		"mfcr %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 3, 31, 31"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [rn] "r"(sh->r[RN(inst)]), [rm] "r" (sh->r[RM(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_CMPGE(SH2 *sh, u32 inst) { /* CMP_GE Rm,Rn  0011nnnnmmmm0011 */
	u32 tmp;
	asm("cmpw 0, %[rn], %[rm]\n\t"			/*compare rn, rm*/
		"crnor 0, 0, 0\n\t"
		"mfcr %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 1, 31, 31"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [rn] "r"(sh->r[RN(inst)]), [rm] "r" (sh->r[RM(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_CMPGT(SH2 *sh, u32 inst) { /* CMP_GT Rm,Rn  0011nnnnmmmm0111 */
	u32 tmp;
	asm("cmpw 0, %[rn], %[rm]\n\t"			/*compare rn, rm*/
		"mfcr %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 2, 31, 31"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [rn] "r"(sh->r[RN(inst)]), [rm] "r" (sh->r[RM(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_CMPHI(SH2 *sh, u32 inst) { /* CMP_HI Rm,Rn  0011nnnnmmmm0110 */
	u32 tmp;
	asm("cmplw 0, %[rn], %[rm]\n\t"			/*compare rn, rm*/
		"mfcr %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 2, 31, 31"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [rn] "r"(sh->r[RN(inst)]), [rm] "r" (sh->r[RM(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_CMPHS(SH2 *sh, u32 inst) { /* CMP_HS Rm,Rn  0011nnnnmmmm0010 */
	u32 tmp;
	asm("cmplw 0, %[rn], %[rm]\n\t"			/*compare rn, rm*/
		"crnor 0, 0, 0\n\t"
		"mfcr %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 2, 31, 31"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [rn] "r"(sh->r[RN(inst)]), [rm] "r" (sh->r[RM(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_CMPPL(SH2 *sh, u32 inst) { /* CMP_PL Rn  0100nnnn00010101 */
	asm("neg %[rn], %[rn]\n\t"
		"rlwimi %[sr], %[rn], 1, 31, 31"
		: [sr] "+r"(sh->sr)
		: [rn] "r"(sh->r[RN(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_CMPPZ(SH2 *sh, u32 inst) { /* CMP_PZ Rn  0100nnnn00010001 */
	asm("rlwimi %[sr], %[rn], 1, 31, 31\n\t"
		"xori   %[sr], %[sr], 1"
		: [sr] "+r"(sh->sr)
		: [rn] "r"(sh->r[RN(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_CMPSTR(SH2 *sh, u32 inst) { /* CMP_STR Rm,Rn  0010nnnnmmmm1100 */
	u32 cm = sh->r[RN(inst)] ^ sh->r[RM(inst)], t0, t1;
	asm(
		"lis    %[t1], 0x0101\n\t"
		"ori    %[t1], %[t1], 0x0101\n\t"
		"subf   %[t0], %[t1], %[cm]\n\t"
		"andc   %[cm], %[t0], %[cm]\n\t"
		"rlwinm %[cm], %[cm], 1, 0, 31\n\t"
		"and.   %[cm], %[cm], %[t1]\n\t"
		"mfcr   %[cm]\n\t"
		"rlwimi %[sr], %[cm], 2, 31, 31"
		: [t0] "=r"(t0), [t1] "=r"(t1), [sr] "+r"(sh->sr)
		: [cm] "r"(cm)
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_CMPIM(SH2 *sh, u32 inst) { /* CMP_EQ #imm,R0  10001000iiiiiiii */
	u32 tmp;
	asm(
		"cmpi 0, %[rn], %[imm]\n\t"
		"mfcr %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 3, 31, 31\n\t"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [rn] "r"(sh->r[RN(inst)]), [imm] "r" (EXT_IMM8(inst))
	);

	sh->pc += 2;
	sh->cycles += 1;
}

/*Load and Stores*/
void sh2_int_LDCSR(SH2 *sh, u32 inst) { /* LDC Rm,SR  0100mmmm00001110 */
	sh->sr = sh->r[RN(inst)] & 0x03F3;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDCGBR(SH2 *sh, u32 inst) { /* LDC Rm,GBR  0100mmmm00011110 */
	sh->gbr = sh->r[RN(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDCVBR(SH2 *sh, u32 inst) { /* LDC Rm,VBR  0100mmmm00101110 */
	sh->vbr = sh->r[RN(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDCMSR(SH2 *sh, u32 inst) { /* LDC.L @Rm+,SR  0100mmmm00000111 */
	u32 addr = (sh->r[RN(inst)] += 4) - 4;
	sh->sr = sh2_Read32(addr) & 0x3F3;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDCMGBR(SH2 *sh, u32 inst) { /* LDC.L @Rm+,GBR  0100mmmm00010111 */
	u32 addr = (sh->r[RN(inst)] += 4) - 4;
	sh->gbr = sh2_Read32(addr);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDCMVBR(SH2 *sh, u32 inst) { /* LDC.L @Rm+,VBR  0100mmmm00100111 */
	u32 addr = (sh->r[RN(inst)] += 4) - 4;
	sh->vbr = sh2_Read32(addr);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDSMACH(SH2 *sh, u32 inst) { /* LDS Rm,MACH  0100mmmm00001010 */
	sh->mach = sh->r[RN(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDSMACL(SH2 *sh, u32 inst) { /* LDS Rm,MACL  0100mmmm00011010 */
	sh->macl = sh->r[RN(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDSPR(SH2 *sh, u32 inst) { /* LDS Rm,PR  0100mmmm00101010 */
	sh->pr = sh->r[RN(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDSMMACH(SH2 *sh, u32 inst) { /* LDS.L @Rm+,MACH  0100mmmm00000110 */
	u32 addr = (sh->r[RN(inst)] += 4) - 4;
	sh->mach = sh2_Read32(addr);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDSMMACL(SH2 *sh, u32 inst) { /* LDS.L @Rm+,MACL  0100mmmm00010110 */
	u32 addr = (sh->r[RN(inst)] += 4) - 4;
	sh->macl = sh2_Read32(addr);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_LDSMPR(SH2 *sh, u32 inst) { /* LDS.L @Rm+,PR  0100mmmm00100110 */
	u32 addr = (sh->r[RN(inst)] += 4) - 4;
	sh->pr = sh2_Read32(addr);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STCSR(SH2 *sh, u32 inst) { /* STC SR,Rn  0000nnnn00000010 */
	sh->r[RN(inst)] = sh->sr;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STCGBR(SH2 *sh, u32 inst) { /* STC GBR,Rn  0000nnnn00010010 */
	sh->r[RN(inst)] = sh->gbr;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STCVBR(SH2 *sh, u32 inst) { /* STC VBR,Rn  0000nnnn00100010 */
	sh->r[RN(inst)] = sh->vbr;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STCMSR(SH2 *sh, u32 inst) { /* STC.L SR,@-Rn  0100nnnn00000011 */
	u32 addr = (sh->r[RN(inst)] -= 4);
	sh2_Write32(addr, sh->sr);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STCMGBR(SH2 *sh, u32 inst) { /* STC.L GBR,@-Rn  0100nnnn00010011 */
	u32 addr = (sh->r[RN(inst)] -= 4);
	sh2_Write32(addr, sh->gbr);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STCMVBR(SH2 *sh, u32 inst) { /* STC.L VBR,@-Rn  0100nnnn00100011 */
	u32 addr = (sh->r[RN(inst)] -= 4);
	sh2_Write32(addr, sh->vbr);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STSMACH(SH2 *sh, u32 inst) { /* STS MACH,Rn  0000nnnn00001010 */
	sh->r[RN(inst)] = sh->mach;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STSMACL(SH2 *sh, u32 inst) { /* STS MACL,Rn  0000nnnn00011010 */
	sh->r[RN(inst)] = sh->macl;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STSPR(SH2 *sh, u32 inst) { /* STS PR,Rn  0000nnnn00101010 */
	sh->r[RN(inst)] = sh->pr;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STSMMACH(SH2 *sh, u32 inst) { /* STS.L MACH,@–Rn  0100nnnn00000010 */
	u32 addr = (sh->r[RN(inst)] -= 4);
	sh2_Write32(addr, sh->mach);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STSMMACL(SH2 *sh, u32 inst) { /* STS.L MACL,@–Rn  0100nnnn00010010 */
	u32 addr = (sh->r[RN(inst)] -= 4);
	sh2_Write32(addr, sh->macl);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_STSMPR(SH2 *sh, u32 inst) { /* STS.L PR,@–Rn  0100nnnn00100010 */
	u32 addr = (sh->r[RN(inst)] -= 4);
	sh2_Write32(addr, sh->pr);
	sh->pc += 2;
	sh->cycles += 1;
}


/*Move Data*/
void sh2_int_MOV(SH2 *sh, u32 inst) { /* MOV Rm,Rn  0110nnnnmmmm0011 */
	sh->r[RN(inst)] = sh->r[RM(inst)];
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBS(SH2 *sh, u32 inst) { /* MOV.B Rm,@Rn  0010nnnnmmmm0000 */
	sh2_Write8(sh->r[RN(inst)], sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWS(SH2 *sh, u32 inst) { /* MOV.W Rm,@Rn  0010nnnnmmmm0001 */
	sh2_Write16(sh->r[RN(inst)], sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLS(SH2 *sh, u32 inst) { /* MOV.L Rm,@Rn  0010nnnnmmmm0010 */
	sh2_Write32(sh->r[RN(inst)], sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBL(SH2 *sh, u32 inst) { /* MOV.B @Rm,Rn  0110nnnnmmmm0000 */
	sh->r[RN(inst)] = sh2_Read8(sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWL(SH2 *sh, u32 inst) { /* MOV.W @Rm,Rn  0110nnnnmmmm0001 */
	sh->r[RN(inst)] = sh2_Read16(sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLL(SH2 *sh, u32 inst) { /* MOV.L @Rm,Rn  0110nnnnmmmm0010 */
	sh->r[RN(inst)] = sh2_Read32(sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBM(SH2 *sh, u32 inst) { /* MOV.B Rm,@–Rn  0010nnnnmmmm0100 */
	u32 addr = (sh->r[RN(inst)] -= 1);
	sh2_Write8(addr, sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWM(SH2 *sh, u32 inst) { /* MOV.W Rm,@–Rn  0010nnnnmmmm0101 */
	u32 addr = (sh->r[RN(inst)] -= 2);
	sh2_Write16(addr, sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLM(SH2 *sh, u32 inst) { /* MOV.L Rm,@–Rn  0010nnnnmmmm0110 */
	u32 addr = (sh->r[RN(inst)] -= 4);
	sh2_Write32(addr, sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBP(SH2 *sh, u32 inst) { /* MOV.B @Rm+,Rn  0110nnnnmmmm0100 */
	u32 n = RN(inst), m = RM(inst);
	sh->r[n] = (s8) sh2_Read8(sh->r[m]);
	sh->r[m] += (n!=m);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWP(SH2 *sh, u32 inst) { /* MOV.W @Rm+,Rn  0110nnnnmmmm0101 */
	u32 n = RN(inst), m = RM(inst);
	sh->r[n] = (s16) sh2_Read16(sh->r[m]);
	sh->r[m] += (n!=m) << 1;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLP(SH2 *sh, u32 inst) { /* MOV.L @Rm+,Rn  0110nnnnmmmm0110 */
	u32 n = RN(inst), m = RM(inst);
	sh->r[n] = sh2_Read32(sh->r[m]);
	sh->r[m] += (n!=m) << 2;
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBS0(SH2 *sh, u32 inst) { /* MOV.B Rm,@(R0,Rn)  0000nnnnmmmm0100 */
	sh2_Write8(sh->r[0] + sh->r[RN(inst)], sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWS0(SH2 *sh, u32 inst) { /* MOV.W Rm,@(R0,Rn)  0000nnnnmmmm0101 */
	sh2_Write16(sh->r[0] + sh->r[RN(inst)], sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLS0(SH2 *sh, u32 inst) { /* MOV.L Rm,@(R0,Rn)  0000nnnnmmmm0110 */
	sh2_Write32(sh->r[0] + sh->r[RN(inst)], sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBL0(SH2 *sh, u32 inst) { /* MOV.B @(R0,Rm),Rn  0000nnnnmmmm1100 */
	sh->r[RN(inst)] = (s8) sh2_Read8(sh->r[0] + sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWL0(SH2 *sh, u32 inst) { /* MOV.W @(R0,Rm),Rn  0000nnnnmmmm1101 */
	sh->r[RN(inst)] = (s16) sh2_Read16(sh->r[0] + sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLL0(SH2 *sh, u32 inst) { /* MOV.L @(R0,Rm),Rn  0000nnnnmmmm1110 */
	sh->r[RN(inst)] = sh2_Read32(sh->r[0] + sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVI(SH2 *sh, u32 inst) { /* MOV #imm,Rn  1110nnnniiiiiiii */
	sh->r[RN(inst)] = EXT_IMM8(inst);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWI(SH2 *sh, u32 inst) { /* MOV.W @(disp,PC),Rn  1001nnnndddddddd */
	sh->r[RN(inst)] = (s16) sh2_Read16(sh->pc + ((inst & 0xFF) << 1));
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLI(SH2 *sh, u32 inst) { /* MOV.L @(disp,PC),Rn  1101nnnndddddddd */
	sh->r[RN(inst)] = sh2_Read32(sh->pc + ((inst & 0xFF) << 2));
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBLG(SH2 *sh, u32 inst) { /* MOV.B @(disp,GBR),R0  11000100dddddddd */
	sh->r[0] = (s8) sh2_Read8(sh->gbr + ((inst & 0xFF)));
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWLG(SH2 *sh, u32 inst) { /* MOV.W @(disp,GBR),R0  11000101dddddddd */
	sh->r[0] = (s16) sh2_Read16(sh->gbr + ((inst & 0xFF) << 1));
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLLG(SH2 *sh, u32 inst) { /* MOV.L @(disp,GBR),R0  11000110dddddddd */
	sh->r[0] = sh2_Read32(sh->gbr + ((inst & 0xFF) << 2));
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBSG(SH2 *sh, u32 inst) { /* MOV.B R0,@(disp,GBR)  11000000dddddddd */
	sh2_Write8(sh->gbr + (inst & 0xFF), sh->r[0]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWSG(SH2 *sh, u32 inst) { /* MOV.W R0,@(disp,GBR)  11000001dddddddd */
	sh2_Write16(sh->gbr + ((inst & 0xFF) << 1), sh->r[0]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLSG(SH2 *sh, u32 inst) { /* MOV.L R0,@(disp,GBR)  11000010dddddddd */
	sh2_Write32(sh->gbr + ((inst & 0xFF) << 2), sh->r[0]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBS4(SH2 *sh, u32 inst) { /* MOV.B R0,@(disp,Rn)  10000000nnnndddd */
	sh2_Write8(sh->r[RM(inst)] + (inst & 0xF), sh->r[0]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWS4(SH2 *sh, u32 inst) { /* MOV.W R0,@(disp,Rn)  10000001nnnndddd */
	sh2_Write16(sh->r[RM(inst)] + ((inst & 0xF) << 1), sh->r[0]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLS4(SH2 *sh, u32 inst) { /* MOV.L Rm,@(disp,Rn)  0001nnnnmmmmdddd */
	sh2_Write32(sh->r[RN(inst)] + ((inst & 0xF) << 2), sh->r[RM(inst)]);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVBL4(SH2 *sh, u32 inst) { /* MOV.B @(disp,Rm),R0  10000100mmmmdddd */
	sh->r[0] = (s8) sh2_Read8(sh->r[RM(inst)] + ((inst & 0xF)));
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVWL4(SH2 *sh, u32 inst) { /* MOV.W @(disp,Rm),R0  10000101mmmmdddd */
	sh->r[0] = (s16) sh2_Read16(sh->r[RM(inst)] + ((inst & 0xF) << 1));
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVLL4(SH2 *sh, u32 inst) { /* MOV.L @(disp,Rm),Rn  0101nnnnmmmmdddd */
	sh->r[RN(inst)] = sh2_Read32(sh->r[RM(inst)] + ((inst & 0xF) << 2));
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVA(SH2 *sh, u32 inst) { /* MOVA @(disp,PC),R0  11000111dddddddd */
	sh->r[0] = (sh->pc & 0xFFFFFFFC) + ((inst & 0xFF) << 2);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_MOVT(SH2 *sh, u32 inst) { /* MOVT Rn  0000nnnn00101001 */
	sh->r[RN(inst)] = sh->sr & 0x1;
	sh->pc += 2;
	sh->cycles += 1;
}


/*Branch and Jumps*/

void sh2_int_BF(SH2 *sh, u32 inst) { /* BF disp  10001011dddddddd */
	if (!(sh->sr & 0x1)) {
		sh->pc += (EXT_IMM8(inst) << 1) + 4;
		sh->inst_addr = sh2_GetPCAddr(sh->pc); //get new host address
		sh->cycles += 3;
	} else {
		sh->pc += 2;
		sh->cycles += 1;
	}
}

void sh2_int_BFS(SH2 *sh, u32 inst) { /* BFS disp  10001111dddddddd */
	if (!(sh->sr & 0x1)) {
		sh->pc += (EXT_IMM8(inst) << 1) + 4;
		sh->cycles += 2;
		sh2_int_DelaySlot(sh);
	} else {
		sh->pc += 2;
		sh->cycles += 1;
	}
}

void sh2_int_BRA(SH2 *sh, u32 inst) { /* BRA disp  1010dddddddddddd */
	sh->pc += (EXT_IMM12(inst) << 1) + 4;
	sh->cycles += 2;
	sh2_int_DelaySlot(sh);
}

void sh2_int_BRAF(SH2 *sh, u32 inst) { /* BRAF Rm  0000mmmm00100011 */
	sh->pc += (sh->r[RN(inst)]) + 4;
	sh->cycles += 2;
	sh2_int_DelaySlot(sh);
}

void sh2_int_BSR(SH2 *sh, u32 inst) { /* BSR disp  1011dddddddddddd */
	sh->pr = sh->pc + 4;
	sh->pc += (EXT_IMM12(inst) << 1) + 4;
	sh->cycles += 2;
	sh2_int_DelaySlot(sh);
}

void sh2_int_BSRF(SH2 *sh, u32 inst) { /* BSRF Rm  0000mmmm00000011 */
	sh->pr = sh->pc + 4;
	sh->pc += (sh->r[RN(inst)]) + 4;
	sh->cycles += 2;
	sh2_int_DelaySlot(sh);
}

void sh2_int_BT(SH2 *sh, u32 inst) { /* BT disp  10001001dddddddd */
	if (sh->sr & 0x1) {
		sh->pc += (EXT_IMM8(inst) << 1) + 4;
		sh->inst_addr = sh2_GetPCAddr(sh->pc); //get new host address
		sh->cycles += 3;
	} else {
		sh->pc += 2;
		sh->cycles += 1;
	}
}

void sh2_int_BTS(SH2 *sh, u32 inst) { /* BTS disp  10001101dddddddd */
	if (sh->sr & 0x1) {
		sh->pc += (EXT_IMM8(inst) << 1) + 4;
		sh->cycles += 2;
		sh2_int_DelaySlot(sh);
	} else {
		sh->pc += 2;
		sh->cycles += 1;
	}
}

void sh2_int_JMP(SH2 *sh, u32 inst) { /* JMP @Rm  0100mmmm00101011 */
	sh->pc = sh->r[RN(inst)];
	sh->cycles += 2;
	sh2_int_DelaySlot(sh);
}

void sh2_int_JSR(SH2 *sh, u32 inst) { /* JSR @Rm  0100mmmm00001011 */
	sh->pr = sh->pc + 4;
	sh->pc = sh->r[RN(inst)];
	sh->cycles += 2;
	sh2_int_DelaySlot(sh);
}

void sh2_int_RTE(SH2 *sh, u32 inst) { /* RTE  0000000000101011 */
	u32 sp = sh->r[15];
	sh->pc = sh2_Read32(sp);
	sh->sr = sh2_Read32(sp+4) & 0x03F3;
	sh->r[15] += 8;
	sh->cycles += 4;
	sh2_int_DelaySlot(sh);
}

void sh2_int_RTS(SH2 *sh, u32 inst) { /* RTS  0000000000001011 */
	sh->pc += sh->pr;
	sh->cycles += 2;
	sh2_int_DelaySlot(sh);
}


/*Other*/
void sh2_int_NOP(SH2 *sh, u32 inst) { /* NOP  0000000000001001 */
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SLEEP(SH2 *sh, u32 inst) { /* SLEEP  0000000000011011 */
	//?? SHOULD SLEEP STATE BE ADDED?
	sh->cycles += 3;
}

void sh2_int_SWAPB(SH2 *sh, u32 inst) { /* SWAP.B Rm,Rn  0110nnnnmmmm1000 */
	asm("mr     %[rn], %[rm]\n\t"				/*rm -> rn*/
		"rlwimi %[rn], %[rm], 8, 16, 24\n\t"	/*swap first 8 bits*/
		"rlwimi %[rn], %[rm], 32-8, 24, 31"		/*swap secont 8 bits*/
		: [rn] "+r"(sh->r[RN(inst)])
		: [rm] "r" (sh->r[RM(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_SWAPW(SH2 *sh, u32 inst) { /* SWAP.W Rm,Rn  0110nnnnmmmm1001 */
	asm("rlwinm %[rn], %[rm], 16, 0, 31\n\t"		/*swap 16 bits*/
		: [rn] "+r"(sh->r[RN(inst)])
		: [rm] "r" (sh->r[RM(inst)])
	);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_TAS(SH2 *sh, u32 inst) { /* TAS.B @Rn  0100nnnn00011011 */
	u32 tmp;
	const u32 addr = sh->r[RN(inst)];
	u8 val = sh2_Read8(addr);
	asm("cntlzw %[tmp], %[val]\n\t"					/*count zeroes*/
		"rlwimi %[sr], %[tmp], 32-5, 31, 31\n\t"	/*move 5th bit to T*/
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [val] "r" (val)
	);

	sh2_Read8(addr | 0x80);

	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_TRAPA(SH2 *sh, u32 inst) { /* TRAPA #imm  11000011iiiiiiii */
	const u32 sp = sh->r[15];
	sh2_Write32(sp-4, sh->sr);
	sh2_Write32(sp-8, sh->pc + 2);
	sh->r[15] -= 8;
	u32 vec = inst & 0xFF; //TODO: Should be 6 when illegal delay slot (branch)
	sh->pc = sh2_Read32(sh->vbr + (vec << 2));
	sh->cycles += 8;
}


void sh2_int_TST(SH2 *sh, u32 inst) { /* TST Rm,Rn  0010nnnnmmmm1000 */
	u32 tmp;
	asm("and. %[tmp], %[rn], %[rm]\n\t"			/*check if and is zero*/
		"mfcr %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 3, 31, 31\n\t"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [rn] "r" (sh->r[RN(inst)]), [rm] "r" (sh->r[RM(inst)])
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_TSTI(SH2 *sh, u32 inst) { /* TEST #imm,R0  11001000iiiiiiii */
	u32 tmp;
	u32 imm = inst & 0xFF;
	asm("and. %[tmp], %[rn], %[imm]\n\t"			/*check if and is zero*/
		"mfcr %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 3, 31, 31\n\t"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [rn] "r" (sh->r[RN(inst)]), [imm] "r" (imm)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_TSTM(SH2 *sh, u32 inst) { /* TST.B #imm,@(R0,GBR)  11001100iiiiiiii */
	u32 tmp;
	u32 imm = inst & 0xFF;
	u32 val = sh2_Read8(sh->r[0] + sh->gbr);
	asm("and. %[tmp], %[val], %[imm]\n\t"			/*check if and is zero*/
		"mfcr %[tmp]\n\t"
		"rlwimi %[sr], %[tmp], 3, 31, 31\n\t"
		: [tmp] "=r"(tmp), [sr] "+r"(sh->sr)
		: [val] "r" (val), [imm] "r" (imm)
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_XTRCT(SH2 *sh, u32 inst) { /* XTRCT Rm,Rn  0010nnnnmmmm1101 */
	asm("rlwinm %[rn], %[rn], 16, 16, 31\n\t"			/*check if and is zero*/
		"rlwimi %[rn], %[rm], 16, 0, 15\n\t"
		: [rn] "+r" (sh->r[RN(inst)])
		: [rm] "r" (sh->r[RM(inst)])
	);
	sh->pc += 2;
	sh->cycles += 1;
}

void sh2_int_ILLEGAL(SH2 *sh, u32 inst) { /* Illegal instruciton handler */
	const u32 sp = sh->r[15];
	sh2_Write32(sp-4, sh->sr);
	sh2_Write32(sp-8, sh->pc + 2);
	sh->r[15] -= 8;
	u32 vec = 4; //TODO: Should be 6 when illegal delay slot (branch)
	sh->pc = sh2_Read32(sh->vbr + (vec << 2));
	sh->cycles += 8;
}


SH2InstFunc decode_arr[] = {
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_STCSR, sh2_int_BSRF,
	sh2_int_MOVBS0, sh2_int_MOVWS0, sh2_int_MOVLS0, sh2_int_MULL,
	sh2_int_CLRT, sh2_int_NOP, sh2_int_STSMACH, sh2_int_RTS,
	sh2_int_MOVBL0, sh2_int_MOVWL0, sh2_int_MOVLL0, sh2_int_MACL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_STCGBR, sh2_int_ILLEGAL,
	sh2_int_MOVBS0, sh2_int_MOVWS0, sh2_int_MOVLS0, sh2_int_MULL,
	sh2_int_SETT, sh2_int_DIV0U, sh2_int_STSMACL, sh2_int_SLEEP,
	sh2_int_MOVBL0, sh2_int_MOVWL0, sh2_int_MOVLL0, sh2_int_MACL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_STCVBR, sh2_int_BRAF,
	sh2_int_MOVBS0, sh2_int_MOVWS0, sh2_int_MOVLS0, sh2_int_MULL,
	sh2_int_CLRMAC, sh2_int_MOVT, sh2_int_STSPR, sh2_int_RTE,
	sh2_int_MOVBL0, sh2_int_MOVWL0, sh2_int_MOVLL0, sh2_int_MACL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_MOVBS0, sh2_int_MOVWS0, sh2_int_MOVLS0, sh2_int_MULL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_MOVBL0, sh2_int_MOVWL0, sh2_int_MOVLL0, sh2_int_MACL,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4, sh2_int_MOVLS4,
	sh2_int_MOVBS, sh2_int_MOVWS, sh2_int_MOVLS, sh2_int_ILLEGAL,
	sh2_int_MOVBM, sh2_int_MOVWM, sh2_int_MOVLM, sh2_int_DIV0S,
	sh2_int_TST, sh2_int_AND, sh2_int_XOR, sh2_int_OR,
	sh2_int_CMPSTR, sh2_int_XTRCT, sh2_int_MULU, sh2_int_MULS,
	sh2_int_MOVBS, sh2_int_MOVWS, sh2_int_MOVLS, sh2_int_ILLEGAL,
	sh2_int_MOVBM, sh2_int_MOVWM, sh2_int_MOVLM, sh2_int_DIV0S,
	sh2_int_TST, sh2_int_AND, sh2_int_XOR, sh2_int_OR,
	sh2_int_CMPSTR, sh2_int_XTRCT, sh2_int_MULU, sh2_int_MULS,
	sh2_int_MOVBS, sh2_int_MOVWS, sh2_int_MOVLS, sh2_int_ILLEGAL,
	sh2_int_MOVBM, sh2_int_MOVWM, sh2_int_MOVLM, sh2_int_DIV0S,
	sh2_int_TST, sh2_int_AND, sh2_int_XOR, sh2_int_OR,
	sh2_int_CMPSTR, sh2_int_XTRCT, sh2_int_MULU, sh2_int_MULS,
	sh2_int_MOVBS, sh2_int_MOVWS, sh2_int_MOVLS, sh2_int_ILLEGAL,
	sh2_int_MOVBM, sh2_int_MOVWM, sh2_int_MOVLM, sh2_int_DIV0S,
	sh2_int_TST, sh2_int_AND, sh2_int_XOR, sh2_int_OR,
	sh2_int_CMPSTR, sh2_int_XTRCT, sh2_int_MULU, sh2_int_MULS,
	sh2_int_CMPEQ, sh2_int_ILLEGAL, sh2_int_CMPHS, sh2_int_CMPGE,
	sh2_int_DIV1, sh2_int_DMULU, sh2_int_CMPHI, sh2_int_CMPGT,
	sh2_int_SUB, sh2_int_ILLEGAL, sh2_int_SUBC, sh2_int_SUBV,
	sh2_int_ADD, sh2_int_DMULS, sh2_int_ADDC, sh2_int_ADDV,
	sh2_int_CMPEQ, sh2_int_ILLEGAL, sh2_int_CMPHS, sh2_int_CMPGE,
	sh2_int_DIV1, sh2_int_DMULU, sh2_int_CMPHI, sh2_int_CMPGT,
	sh2_int_SUB, sh2_int_ILLEGAL, sh2_int_SUBC, sh2_int_SUBV,
	sh2_int_ADD, sh2_int_DMULS, sh2_int_ADDC, sh2_int_ADDV,
	sh2_int_CMPEQ, sh2_int_ILLEGAL, sh2_int_CMPHS, sh2_int_CMPGE,
	sh2_int_DIV1, sh2_int_DMULU, sh2_int_CMPHI, sh2_int_CMPGT,
	sh2_int_SUB, sh2_int_ILLEGAL, sh2_int_SUBC, sh2_int_SUBV,
	sh2_int_ADD, sh2_int_DMULS, sh2_int_ADDC, sh2_int_ADDV,
	sh2_int_CMPEQ, sh2_int_ILLEGAL, sh2_int_CMPHS, sh2_int_CMPGE,
	sh2_int_DIV1, sh2_int_DMULU, sh2_int_CMPHI, sh2_int_CMPGT,
	sh2_int_SUB, sh2_int_ILLEGAL, sh2_int_SUBC, sh2_int_SUBV,
	sh2_int_ADD, sh2_int_DMULS, sh2_int_ADDC, sh2_int_ADDV,
	sh2_int_SHLL, sh2_int_SHLR, sh2_int_STSMMACH, sh2_int_STCMSR,
	sh2_int_ROTL, sh2_int_ROTR, sh2_int_LDSMMACH, sh2_int_LDCMSR,
	sh2_int_SHLL2, sh2_int_SHLR2, sh2_int_LDSMACH, sh2_int_JSR,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_LDCSR, sh2_int_MACW,
	sh2_int_DT, sh2_int_CMPPZ, sh2_int_STSMMACL, sh2_int_STCMGBR,
	sh2_int_ILLEGAL, sh2_int_CMPPL, sh2_int_LDSMMACL, sh2_int_LDCMGBR,
	sh2_int_SHLL8, sh2_int_SHLR8, sh2_int_LDSMACL, sh2_int_TAS,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_LDCGBR, sh2_int_MACW,
	sh2_int_SHAL, sh2_int_SHAR, sh2_int_STSMPR, sh2_int_STCMVBR,
	sh2_int_ROTCL, sh2_int_ROTCR, sh2_int_LDSMPR, sh2_int_LDCMVBR,
	sh2_int_SHLL16, sh2_int_SHLR16, sh2_int_LDSPR, sh2_int_JMP,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_LDCVBR, sh2_int_MACW,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_MACW,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4, sh2_int_MOVLL4,
	sh2_int_MOVBL, sh2_int_MOVWL, sh2_int_MOVLL, sh2_int_MOV,
	sh2_int_MOVBP, sh2_int_MOVWP, sh2_int_MOVLP, sh2_int_NOT,
	sh2_int_SWAPB, sh2_int_SWAPW, sh2_int_NEGC, sh2_int_NEG,
	sh2_int_EXTUB, sh2_int_EXTUW, sh2_int_EXTSB, sh2_int_EXTSW,
	sh2_int_MOVBL, sh2_int_MOVWL, sh2_int_MOVLL, sh2_int_MOV,
	sh2_int_MOVBP, sh2_int_MOVWP, sh2_int_MOVLP, sh2_int_NOT,
	sh2_int_SWAPB, sh2_int_SWAPW, sh2_int_NEGC, sh2_int_NEG,
	sh2_int_EXTUB, sh2_int_EXTUW, sh2_int_EXTSB, sh2_int_EXTSW,
	sh2_int_MOVBL, sh2_int_MOVWL, sh2_int_MOVLL, sh2_int_MOV,
	sh2_int_MOVBP, sh2_int_MOVWP, sh2_int_MOVLP, sh2_int_NOT,
	sh2_int_SWAPB, sh2_int_SWAPW, sh2_int_NEGC, sh2_int_NEG,
	sh2_int_EXTUB, sh2_int_EXTUW, sh2_int_EXTSB, sh2_int_EXTSW,
	sh2_int_MOVBL, sh2_int_MOVWL, sh2_int_MOVLL, sh2_int_MOV,
	sh2_int_MOVBP, sh2_int_MOVWP, sh2_int_MOVLP, sh2_int_NOT,
	sh2_int_SWAPB, sh2_int_SWAPW, sh2_int_NEGC, sh2_int_NEG,
	sh2_int_EXTUB, sh2_int_EXTUW, sh2_int_EXTSB, sh2_int_EXTSW,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI, sh2_int_ADDI,
	sh2_int_MOVBS4, sh2_int_MOVWS4, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_MOVBL4, sh2_int_MOVWL4, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_CMPIM, sh2_int_BT, sh2_int_ILLEGAL, sh2_int_BF,
	sh2_int_ILLEGAL, sh2_int_BTS, sh2_int_ILLEGAL, sh2_int_BFS,
	sh2_int_MOVBS4, sh2_int_MOVWS4, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_MOVBL4, sh2_int_MOVWL4, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_CMPIM, sh2_int_BT, sh2_int_ILLEGAL, sh2_int_BF,
	sh2_int_ILLEGAL, sh2_int_BTS, sh2_int_ILLEGAL, sh2_int_BFS,
	sh2_int_MOVBS4, sh2_int_MOVWS4, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_MOVBL4, sh2_int_MOVWL4, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_CMPIM, sh2_int_BT, sh2_int_ILLEGAL, sh2_int_BF,
	sh2_int_ILLEGAL, sh2_int_BTS, sh2_int_ILLEGAL, sh2_int_BFS,
	sh2_int_MOVBS4, sh2_int_MOVWS4, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_MOVBL4, sh2_int_MOVWL4, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_CMPIM, sh2_int_BT, sh2_int_ILLEGAL, sh2_int_BF,
	sh2_int_ILLEGAL, sh2_int_BTS, sh2_int_ILLEGAL, sh2_int_BFS,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI, sh2_int_MOVWI,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BRA, sh2_int_BRA, sh2_int_BRA, sh2_int_BRA,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_BSR, sh2_int_BSR, sh2_int_BSR, sh2_int_BSR,
	sh2_int_MOVBSG, sh2_int_MOVWSG, sh2_int_MOVLSG, sh2_int_TRAPA,
	sh2_int_MOVBLG, sh2_int_MOVWLG, sh2_int_MOVLLG, sh2_int_MOVA,
	sh2_int_TSTI, sh2_int_ANDI, sh2_int_XORI, sh2_int_ORI,
	sh2_int_TSTM, sh2_int_ANDM, sh2_int_XORM, sh2_int_ORM,
	sh2_int_MOVBSG, sh2_int_MOVWSG, sh2_int_MOVLSG, sh2_int_TRAPA,
	sh2_int_MOVBLG, sh2_int_MOVWLG, sh2_int_MOVLLG, sh2_int_MOVA,
	sh2_int_TSTI, sh2_int_ANDI, sh2_int_XORI, sh2_int_ORI,
	sh2_int_TSTM, sh2_int_ANDM, sh2_int_XORM, sh2_int_ORM,
	sh2_int_MOVBSG, sh2_int_MOVWSG, sh2_int_MOVLSG, sh2_int_TRAPA,
	sh2_int_MOVBLG, sh2_int_MOVWLG, sh2_int_MOVLLG, sh2_int_MOVA,
	sh2_int_TSTI, sh2_int_ANDI, sh2_int_XORI, sh2_int_ORI,
	sh2_int_TSTM, sh2_int_ANDM, sh2_int_XORM, sh2_int_ORM,
	sh2_int_MOVBSG, sh2_int_MOVWSG, sh2_int_MOVLSG, sh2_int_TRAPA,
	sh2_int_MOVBLG, sh2_int_MOVWLG, sh2_int_MOVLLG, sh2_int_MOVA,
	sh2_int_TSTI, sh2_int_ANDI, sh2_int_XORI, sh2_int_ORI,
	sh2_int_TSTM, sh2_int_ANDM, sh2_int_XORM, sh2_int_ORM,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI, sh2_int_MOVLI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI, sh2_int_MOVI,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL,
	sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL, sh2_int_ILLEGAL
};


//TODO
void sh2_int_DelaySlot(SH2* sh) {
	u16 opcode = *sh->inst_addr;
	u32 decode_pos = (opcode >> 4) & 0xF00;
	if (opcode & 0x8000){
		decode_pos |= (opcode >> 6) & 0x3C;
	} else {
		decode_pos |= (opcode << 2) & 0xFC;
	}
	//get address for new pc
	sh->inst_addr = sh2_GetPCAddr(sh->pc);
	//execute instruction (TODO: CHECK ILLEGAL JUMP)
	decode_arr[decode_pos >> 2](sh, opcode);
}

//TODO
u32 sh2_IntExec(SH2 *sh, s32 cycles)
{
	sh2_HandleInterrupt(sh);

	sh->inst_addr = sh2_GetPCAddr(sh->pc);
	sh->cycles -= cycles;
	while(sh->cycles < 0) {
		u16 opcode = *sh->inst_addr++;
		u32 decode_pos = (opcode >> 4) & 0xF00;
		if (opcode & 0x8000){
			decode_pos |= (opcode >> 6) & 0x3C;
		} else {
			decode_pos |= (opcode << 2) & 0xFC;
		}
		//execute instruction
		decode_arr[decode_pos >> 2](sh, opcode);
	}
	return sh->cycles;
}

#endif