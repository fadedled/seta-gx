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

/*
 * emit 32-bit PPC from SH2 instructions
 *
 * SH2 has 142 instructions
 *
 * Since the PPC has more regs than the SH2 we
 * can store them in all the GPRs as so:
 *
 * SH2 reg			PPC reg
 * =========================
 * PC			| GPR6 //Not set in stone
 * SR			| GPR7 //Not set in stone
 * GBR			| GPR8 //Not set in stone
 * VBR			| GPR9 //Not set in stone
 * PR			| GPR10
 * MACH			| GPR11
 * MACL			| GPR12
 * R0 to R15	| GPR16 to GPR31
 *
 * Also, CR2-CR4 must be preserved during function calls
 * and CR0 is not
 * XER register is also important
 * T can be checked as needed, for all opcodes the T bit
 * will be what it is used for afterward
 *
 * For calling memory read/write functions you must:
 * 1) Read
 * 		- put address in r3
 * 		- bl to read function
 * 2) Write
 * 		- put address in r3
 * 		- put value in r4
 * 		- bl to write function
 *
 * CR0 [LT][GT][EQ][SO]
 * XER [SO][OV][CA][--] [28 - BITS]
 * WE CAN USE FPR
 */

#define GP_R0		(14)	/* This reg is perserverd */
#define GP_R(x)		(14+(x)) 	/* These regs are perserverd */

#define GP_TMP		(5)		/* temp variable for mutli-instruction */
#define GP_TMP2		(6)		/* temp variable for mutli-instruction */
#define GP_PC		(7)		/* Volatile reg, must be perserved if modified */
#define GP_PR		(8)		/* Volatile reg, must be perserved if modified */
#define GP_GBR		(8)		/* Volatile reg, must be perserved if modified */
#define GP_VBR		(8)		/* Volatile reg, must be perserved if modified */
#define GP_MACL		(9)		/* Volatile reg, must be perserved if modified */
#define GP_MACH		(10)	/* Volatile reg, must be perserved if modified */
#define GP_STMP		(15)	/* This reg is perserverd (Temp preserved value) */
#define GP_SR		(30)	/* This reg is perserverd */
#define GP_CTX		(31)	/* This reg is perserverd, holds address of the sh2 struct */

#define EMIT(x) \
	do { \
		*icache_ptr = (x); \
		icache_ptr++; \
	} while (0);

/*Power PC opcodes*/
//Aritmethic/logic ins
#define PPCC_ADD(rD, rA, rB)				EMIT(0x7C000214 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_ADDCp(rD, rA, rB)				EMIT(0x7C000414 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_ADDO(rD, rA, rB)				EMIT(0x7C000614 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_ADDI(rD, rA, imm)				EMIT(0x38000000 | ((rD) << 21) | ((rA) << 16) | ((imm) & 0xFFFF))
#define PPCC_ADDIS(rD, rA, imm)				EMIT(0x3C000000 | ((rD) << 21) | ((rA) << 16) | ((imm) & 0xFFFF))
#define PPCC_ADDICR(rD, rA, imm)			EMIT(0x34000000 | ((rD) << 21) | ((rA) << 16) | ((imm) & 0xFFFF))
#define PPCC_ADDEO(rD, rA, rB)				EMIT(0x7C000514 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_ADDZE(rD, rA)					EMIT(0x7C000194 | ((rD) << 21) | ((rA) << 16))
#define PPCC_SUBF(rD, rA, rB)				EMIT(0x7C000050 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_SUBFCO(rD, rA, rB)				EMIT(0x7C000410 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_SUBFOp(rD, rA, rB)				EMIT(0x7C000451 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_SUBFEO(rD, rA, rB)				EMIT(0x7C000510 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_SUBFZE(rD, rA)					EMIT(0x7C000190 | ((rD) << 21) | ((rA) << 16))
#define PPCC_MULLW(rD, rA, rB)				EMIT(0x7C0001D6 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_MULHW(rD, rA, rB)				EMIT(0x7C000096 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_MULHWU(rD, rA, rB)				EMIT(0x7C000016 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_DIVW(rD, rA, rB)				EMIT(0x7C0003D6 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_DIVWU(rD, rA, rB)				EMIT(0x7C000396 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_AND(rD, rA, rB)				EMIT(0x7C000038 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_ANDC(rD, rA, rB)				EMIT(0x7C000078 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_ANDp(rD, rA, rB)				EMIT(0x7C000039 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_ANDI(rD, rA, imm)				EMIT(0x70000000 | ((rA) << 21) | ((rD) << 16) | ((imm) & 0xFFFF))
#define PPCC_OR(rD, rA, rB)					EMIT(0x7C000378 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_ORI(rD, rA, imm)				EMIT(0x60000000 | ((rA) << 21) | ((rD) << 16) | ((imm) & 0xFFFF))
#define PPCC_XOR(rD, rA, rB)				EMIT(0x7C000278 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_XORI(rD, rA, imm)				EMIT(0x68000000 | ((rA) << 21) | ((rD) << 16) | ((imm) & 0xFFFF))
#define PPCC_NOR(rD, rA, rB)				EMIT(0x7C0000F8 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_NEG(rD, rA)					EMIT(0x7C0000D0 | ((rD) << 21) | ((rA) << 16))
#define PPCC_EXTSB(rD, rA)					EMIT(0x7C000774 | ((rA) << 21) | ((rD) << 16))
#define PPCC_EXTSH(rD, rA)					EMIT(0x7C000734 | ((rA) << 21) | ((rD) << 16))
#define PPCC_RLWINM(rD, rA, shf, mb, me)	EMIT(0x54000000 | ((rA) << 21) | ((rD) << 16) | ((shf) << 11) | ((mb) << 6) | ((me) << 1))
#define PPCC_RLWIMI(rD, rA, shf, mb, me)	EMIT(0x50000000 | ((rA) << 21) | ((rD) << 16) | ((shf) << 11) | ((mb) << 6) | ((me) << 1))
#define PPCC_SRAWI(rD, rA, shf)				EMIT(0x7C000670 | ((rA) << 21) | ((rD) << 16) | ((shf) << 11))
#define PPCC_CNTLZW(rD, rA)					EMIT(0x7C000034 | ((rA) << 21) | ((rD) << 16))

//Condition Register inst
#define PPCC_MCRXR(cD)						EMIT(0x7C000400 | ((cD) << 23))
#define PPCC_CMP(cD, rA, rB)				EMIT(0x7C000000 | ((cD) << 23) | ((rA) << 16) | ((rB) << 11))
#define PPCC_CMPL(cD, rA, rB)				EMIT(0x7C000040 | ((cD) << 23) | ((rA) << 16) | ((rB) << 11))
#define PPCC_CMPI(cD, rA, imm)				EMIT(0x2C000000 | ((cD) << 23) | ((rA) << 16) | ((imm) & 0xFFFF))
#define PPCC_CRNOR(cD, cA, cB)				EMIT(0x4C000042 | ((cD) << 21) | ((cA) << 16) | ((cB) << 11))
#define PPCC_CROR(cD, cA, cB)				EMIT(0x4C000382 | ((cD) << 21) | ((cA) << 16) | ((cB) << 11))

//Branch inst
#define PPCC_B(addr)						EMIT(0x48000000 | ((((u32) addr - (u32)icache_ptr) & 0x3FFFFFC)))
#define PPCC_BL(addr)						EMIT(0x48000001 | ((((u32) addr - (u32)icache_ptr) & 0x3FFFFFC)))
#define PPCC_BCT(cB, disp)					EMIT(0x40000000 | ((0b01100) << 21) | ((cB) << 16) | (((disp) << 2) & 0xFFFC))
#define PPCC_BCF(cB, disp)					EMIT(0x40000000 | ((0b00100) << 21) | ((cB) << 16) | (((disp) << 2) & 0xFFFC))
#define PPCC_BLR							EMIT(0x4E800020)


//Move inst
#define PPCC_MFCR(rD)						EMIT(0x7C000026 | ((rD) << 21))

#define PPCC_MTXER(rS)						EMIT(0x7C0003A6 | ((rS) << 21) | ((0x20) << 11))
#define PPCC_MFXER(rD)						EMIT(0x7C0102A6 | ((rD) << 21) | ((0x20) << 11))
#define PPCC_MTLR(rD)						EMIT(0x7C0803A6 | ((rD) << 21))
#define PPCC_MFLR(rD)						EMIT(0x7C0802A6 | ((rD) << 21))

//Store/Load inst
#define PPCC_STWU(rS, rA, d)				EMIT(0x94000000 | ((rS) << 21) | ((rA) << 16) | ((d) & 0xFFFF))
#define PPCC_STW(rS, rA, d)					EMIT(0x90000000 | ((rS) << 21) | ((rA) << 16) | ((d) & 0xFFFF))
#define PPCC_STMW(rS, rA, d)				EMIT(0xBC000000 | ((rS) << 21) | ((rA) << 16) | ((d) & 0xFFFF))
#define PPCC_LHA(rD, rA, d)					EMIT(0xA8000000 | ((rD) << 21) | ((rA) << 16) | ((d) & 0xFFFF))
#define PPCC_LWZ(rD, rA, d)					EMIT(0x80000000 | ((rD) << 21) | ((rA) << 16) | ((d) & 0xFFFF))
#define PPCC_LMW(rD, rA, d)					EMIT(0xB8000000 | ((rD) << 21) | ((rA) << 16) | ((d) & 0xFFFF))


//Helper inst
#define PPCC_MOV(rD, rA)					PPCC_ORI(rD, rA, 0x0)
#define PPCC_CLEAR(rD)						PPCC_ANDI(rD, rD, 0x0)

//Extend signed immediate
#define EXT_IMM8(imm)		(((s32)(imm << 24)) >> 24)
#define EXT_IMM12(imm)		(((s32)(imm << 20)) >> 20)
