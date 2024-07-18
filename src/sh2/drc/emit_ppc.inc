/*
 *
 *
 *
 *
 */

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
 * PC			| GPR6
 * SR			| GPR7
 * GBR			| GPR8
 * VBR			| GPR9
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
 */

#define GP_R0		(16)
#define GP_R(x)		(x + GP_R0)

#define GP_TMP		(5)		//temp variable for mutli-instruction
#define GP_PC		(7)
#define GP_PR		(8)
#define GP_GBR		(9)
#define GP_VBR		(10)
#define GP_SR		(13)
#define GP_MACH		(14)
#define GP_MACL		(15)


#define EMIT(x) \
	do { \
		*icache_ptr = (x); \
		icache_ptr++; \
	} while (0)

/*Power PC opcodes*/
#define PPCC_ADD(rD, rA, rB)				EMIT(0x7C000214 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_ADDO(rD, rA, rB)				EMIT(0x7C000614 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_ADDI(rD, rA, imm)				EMIT(0x38000000 | ((rD) << 21) | ((rA) << 16) | (imm & 0xFFFF))
#define PPCC_ADDEO(rD, rA, rB)				EMIT(0x7C000514 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_SUBF(rD, rA, rB)				EMIT(0x7C000050 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_MULLW(rD, rA, rB)				EMIT(0x7C0001D6 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_MULHW(rD, rA, rB)				EMIT(0x7C000096 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_MULHWU(rD, rA, rB)				EMIT(0x7C000016 | ((rD) << 21) | ((rA) << 16) | ((rB) << 11))
#define PPCC_AND(rD, rA, rB)				EMIT(0x7C000038 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_ANDp(rD, rA, rB)				EMIT(0x7C000039 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_ANDI(rD, rA, imm)				EMIT(0x70000000 | ((rA) << 21) | ((rD) << 16) | (imm & 0xFFFF))
#define PPCC_OR(rD, rA, rB)					EMIT(0x7C000378 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_ORI(rD, rA, imm)				EMIT(0x60000000 | ((rA) << 21) | ((rD) << 16) | (imm & 0xFFFF))
#define PPCC_XOR(rD, rA, rB)				EMIT(0x7C000278 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_XORI(rD, rA, imm)				EMIT(0x68000000 | ((rA) << 21) | ((rD) << 16) | (imm & 0xFFFF))
#define PPCC_NOR(rD, rA, rB)				EMIT(0x7C0000F8 | ((rA) << 21) | ((rD) << 16) | ((rB) << 11))
#define PPCC_NEG(rD, rA)					EMIT(0x7C0000D0 | ((rD) << 21) | ((rA) << 16))
#define PPCC_EXTSB(rD, rA)					EMIT(0x7C000774 | ((rA) << 21) | ((rD) << 16))
#define PPCC_EXTSH(rD, rA)					EMIT(0x7C000734 | ((rA) << 21) | ((rD) << 16))
#define PPCC_RLWINM(rD, rA, shf, mb, me)	EMIT(0x54000000 | ((rA) << 21) | ((rD) << 16) | ((shf) << 11) | ((mb) << 6) | ((me) << 1))
#define PPCC_RLWIMI(rD, rA, shf, mb, me)	EMIT(0x50000000 | ((rA) << 21) | ((rD) << 16) | ((shf) << 11) | ((mb) << 6) | ((me) << 1))
#define PPCC_SRAWI(rD, rA, shf)				EMIT(0x7C000670 | ((rA) << 21) | ((rD) << 16) | ((shf) << 11))
#define PPCC_CMP(cD, rA, rB)				EMIT(0x7C000000 | ((cD) << 23) | ((rA) << 16) | ((rB) << 11))
#define PPCC_CMPL(cD, rA, rB)				EMIT(0x7C000040 | ((cD) << 23) | ((rA) << 16) | ((rB) << 11))
#define PPCC_CMPI(cD, rA, imm)				EMIT(0x2C000000 | ((cD) << 23) | ((rA) << 16) | (imm & 0xFFFF))

#define PPCC_MTXER(rS)						EMIT(0x7C0003A6 | ((rS) << 21) | ((0x20) << 11))
#define PPCC_MFXER(rD)						EMIT(0x7C0002A6 | ((rD) << 21) | ((0x20) << 11))
#define PPCC_MFCR(rD)						EMIT(0x7C000026 | ((rD) << 21))












