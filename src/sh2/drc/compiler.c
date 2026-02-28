

#include "emit_ppc.inc"
#include "../sh2.h"
#include "ogc/pad.h"
#include "ogc/video.h"
#include <stdlib.h>


#define MAX_BLOCK_INST	1024			/* NOTE: change this value eventually */
#define BLOCK_ARR_SIZE	2048			//TODO:x16?
#define DRC_CODE_SIZE	1024*1024		/* 4Mb of instructions */

typedef void (*DrcCode)(void *shctx);

typedef struct Block_t {
	DrcCode code;		/*  */

	u32 ret_addr; 		/* If return is a constant address (hashed) */
	u32 start_addr; 	/* Address of original code */
	u32 sh2_len;		/* Number of original code instructions */
	u32 ppc_len;		/* Number of native code instructions */
	u32 cycle_count;	/* Number of base block cycles */
} Block;

extern SH2 *sh_ctx;

u32 drc_code[DRC_CODE_SIZE] ATTRIBUTE_ALIGN(32);
Block drc_blocks[BLOCK_ARR_SIZE] ATTRIBUTE_ALIGN(32);
u32 drc_blocks_size = 0;

u32 drc_code_pos = 0;
u32 _uses_t = 1;			//T bit check must not allways be done
u32 _jit_opcode = 0;
u32 rn = 0;
u32 rm = 0;


#define PPCE_LOAD(rA, name)		PPCC_LWZ(rA, GP_CTX, offsetof(SH2, name))
#define PPCE_SAVE(rA, name)		PPCC_STW(rA, GP_CTX, offsetof(SH2, name))

u32 __GetDrcHash(s32 key)
{
#if 0
	key = (~key) + (key << 5); // key = (key << 18) - key - 1;
	key = key ^ (key >> 8);
	key = key * 21; // key = (key + (key << 2)) + (key << 4);
	key = key ^ (key >> 3);
	key = key + (key << 1);
	key = key ^ (key >> 6);
#else
	key >>= 6; // Terrible implementation
#endif
	return (key >> 2);
}


Block* HashGet(u32 key, u32 *found)
{
	key &= 0x07FFFFFF;
	u32 i = __GetDrcHash(key) & (BLOCK_ARR_SIZE - 1);
	u32 count = 0;
	while (drc_blocks[i].start_addr != -1) {
		//No more blocks can be generated
		if (count == BLOCK_ARR_SIZE - 1) {
			return NULL;
		}
		// do linear probing
		if (drc_blocks[i].start_addr == key) {
			*found = 1;
			return &drc_blocks[i];
		}
		i = (i + 1) & (BLOCK_ARR_SIZE - 1);
		++count;
	}
	*found = 0;
	return &drc_blocks[i];
}

void HashClearRange(u32 start_addr, u32 end_addr)
{
	for (u32 i = 0; i < BLOCK_ARR_SIZE; ++i) {
		u32 addr = drc_blocks[i].start_addr;
		u32 len = drc_blocks[i].sh2_len;
		if ( ((addr >= start_addr) & (addr <= end_addr))
			| ((addr + len >= start_addr) & (addr + len <= end_addr))) {
			drc_blocks[i].start_addr = -1;
			--drc_blocks_size;
		}
	}
}

void HashClearAll(void)
{
	for (u32 i = 0; i < BLOCK_ARR_SIZE; ++i) {
		drc_blocks[i].start_addr = -1;
	}
	drc_blocks_size = 0;
}


#define OPCODE_ARG_REGL(i)		(((i) >> 8) & 0xF)
#define OPCODE_ARG_REGR(i)		(((i) >> 4) & 0xF)
#define imm 					(_jit_opcode)
#define disp 					(_jit_opcode)

#define PPCE_LDREG(i) \
	if (!GP_R(i)) { \
		GP_R(i) = reg_curr--; \
		PPCE_LOAD(GP_R(i), r[i]); \
	}

#define PPCE_STREG(i) \
	stregs = 1<<(i);



#define SH2JIT_ADD			/* ADD Rm,Rn  0011nnnnmmmm1100 */ \
	PPCC_ADD(rn, rn, rm);


#define SH2JIT_ADDI			/* ADD #imm,Rn  0111nnnniiiiiiii */ \
	PPCC_ADDI(rn, rn, EXT_IMM8(imm));


#define SH2JIT_ADDC			/* ADDC Rm,Rn  0011nnnnmmmm1110 */ \
	PPCC_RLWINM(GP_TMP, GP_SR, 29, 2, 2); 	/*Extract T*/ \
	PPCC_MTXER(GP_TMP);  					/*Store T in XER[CA]*/ \
	PPCC_ADDEO(rn, rn, rm); 				/*Add rn + rm + XER[CA]*/ \
	PPCC_MFXER(GP_TMP); 					/*Load XER[CA] to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store T*/


#define SH2JIT_ADDV			/* ADDV Rm,Rn  0011nnnnmmmm1111 */ \
	PPCC_ADDO(rn, rn, rm); 					/*Add rn + rm*/ \
	PPCC_MFXER(GP_TMP); 					/*Load XER[OV] to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 2, 31, 31); 	/*Store T*/


#define SH2JIT_SUB			/* SUB Rm,Rn  0011nnnnmmmm1000*/ \
	PPCC_SUBF(rn, rm, rn);


#define SH2JIT_SUBC			/* SUBC Rm,Rn  0011nnnnmmmm1010*/ \
	PPCC_RLWINM(GP_TMP, GP_SR, 29, 2, 2); 	/*Extract T*/ \
	PPCC_MTXER(GP_TMP);  					/*Store T in XER[CA]*/ \
	PPCC_SUBFEO(rn, rm, rn); 				/* rn - rm - !XER[CA]*/ \
	PPCC_MFXER(GP_TMP); 					/*Load XER[CA] to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store T*/


#define SH2JIT_SUBV			/* SUBV Rm,Rn  0011nnnnmmmm1011*/ \
	PPCC_SUBFO(rn, rm, rn); 				/*Rn - Rm */ \
	PPCC_MFXER(GP_TMP); 					/*Load XER[OV] to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 2, 31, 31); 	/*Store T*/



#define SH2JIT_AND			/* AND Rm,Rn  0010nnnnmmmm1001*/ \
	PPCC_AND(rn, rn, rm);


#define SH2JIT_ANDI			/* AND #imm,R0  11001001iiiiiiii*/ \
	PPCC_ANDI(GP_R0, GP_R0, imm & 0xFF);


#define SH2JIT_ANDM			/* AND.B #imm,@(R0,GBR)  11001101iiiiiiii*/ \
	PPCE_LOAD(GP_STMP, gbr);		\
	PPCC_ADD(3, GP_R0, GP_STMP);	/*(R0+GBR) -> address*/ \
	PPCC_BL(sh2_Read8);				/*Read addr*/ \
	PPCC_ANDI(4, 3, imm & 0xFF);	/*(R0+GBR) & imm -> value*/ \
	PPCC_ADD(3, GP_R0, GP_STMP);	/*(R0+GBR) -> address*/ \
	PPCC_BL(sh2_Write8);			/*Write (R0+GBR) & imm */


#define SH2JIT_OR			/* OR Rm,Rn  0010nnnnmmmm1011 */ \
	PPCC_OR(rn, rn, rm);


#define SH2JIT_ORI			/* OR #imm,R0  11001011iiiiiiii */ \
	PPCC_ORI(GP_R0, GP_R0, imm & 0xFF);


#define SH2JIT_ORM			/* OR.B #imm,@(R0,GBR)  11001111iiiiiiii */ \
	PPCE_LOAD(GP_STMP, gbr);		\
	PPCC_ADD(3, GP_R0, GP_STMP);	/*(R0+GBR) -> address*/ \
	PPCC_BL(sh2_Read8);				/*Read addr*/ \
	PPCC_ORI(4, 3, imm & 0xFF);		/*val(R0+GBR) | imm -> value*/ \
	PPCC_ADD(3, GP_R0, GP_STMP);	/*(R0+GBR) -> address*/ \
	PPCC_BL(sh2_Write8);			/*Write (R0+GBR) & imm */


#define SH2JIT_XOR			/* XOR Rm,Rn  0010nnnnmmmm1010 */ \
	PPCC_XOR(rn, rn, rm);


#define SH2JIT_XORI			/* XOR #imm,R0  11001010iiiiiiii */ \
	PPCC_XORI(GP_R0, GP_R0, imm & 0xFF);


#define SH2JIT_XORM			/* XOR.B #imm,@(R0,GBR)  11001110iiiiiiii */ \
	PPCE_LOAD(GP_STMP, gbr);		\
	PPCC_ADD(3, GP_R0, GP_STMP);	/*(R0+GBR) -> address*/ \
	PPCC_BL(sh2_Read8);				/*Read addr*/ \
	PPCC_XORI(4, 3, imm & 0xFF);	/*(R0+GBR) ^ imm -> value*/ \
	PPCC_ADD(3, GP_R0, GP_STMP);	/*(R0+GBR) -> address*/ \
	PPCC_BL(sh2_Write8);			/*Write (R0+GBR) & imm */


#define SH2JIT_ROTCL		/* ROTCL Rn  0100nnnn00100100 */ \
	PPCC_RLWINM(rn, rn, 1, 0, 31); 			/*Rotate Left by 1*/ \
	PPCC_ANDI(GP_TMP, rn, 1); 				/*Extract MSB Rn -> TMP*/ \
	PPCC_RLWIMI(rn, GP_SR, 0, 31, 31);		/*Insert T as LSB*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 0, 31, 31);	/*Store TMP as T bit*/


#define SH2JIT_ROTCR		/* ROTCR Rn  0100nnnn00100101 */ \
	PPCC_ANDI(GP_TMP, rn, 1); 				/*Extract LSB Rn -> TMP*/ \
	PPCC_RLWIMI(rn, GP_SR, 0, 31, 31);		/*Insert T as LSB*/ \
	PPCC_RLWINM(rn, rn, 32-1, 0, 31); 		/*Rotate Right by 1*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 0, 31, 31);	/*Store TMP as T bit*/


#define SH2JIT_ROTL			/* ROTL Rn  0100nnnn00000100 */ \
	PPCC_RLWIMI(GP_SR, rn, 1, 31, 31);	/*Store the T bit*/ \
	PPCC_RLWINM(rn, rn, 1, 0, 31); 		/*Rotate Left by 1*/


#define SH2JIT_ROTR			 \
	PPCC_RLWIMI(GP_SR, rn, 0, 31, 31);	/*Store the T bit*/\
	PPCC_RLWINM(rn, rn, 32-1, 0, 31); 	/*Rotate Right by 1*/


#define SH2JIT_SHAL			 \
	PPCC_RLWIMI(GP_SR, rn, 1, 31, 31);	/*Store the T bit*/\
	PPCC_RLWINM(rn, rn, 1, 0, 30); 		/*Shift Algebraic Left by 1*/


#define SH2JIT_SHAR \
	PPCC_RLWIMI(GP_SR, rn, 0, 31, 31);	/*Store the T bit*/\
	PPCC_SRAWI(rn, rn, 1); 				/*Shift Algebraic Right by 1*/


#define SH2JIT_SHLL \
	PPCC_RLWIMI(GP_SR, rn, 1, 31, 31);	/*Store the T bit*/ \
	PPCC_RLWINM(rn, rn, 1, 0, 30); 		/*Shift Logical Left by 1*/


#define SH2JIT_SHLL2 \
	PPCC_RLWINM(rn, rn, 2, 0, 31-2);


#define SH2JIT_SHLL8 \
	PPCC_RLWINM(rn, rn, 8, 0, 31-8);


#define SH2JIT_SHLL16 \
	PPCC_RLWINM(rn, rn, 16, 0, 31-16);


#define SH2JIT_SHLR \
	PPCC_RLWIMI(GP_SR, rn, 0, 31, 31);	/*Store the T bit*/ \
	PPCC_RLWINM(rn, rn, 32-1, 1, 31); 	/*Shift Logical Right by 1*/


#define SH2JIT_SHLR2 \
	PPCC_RLWINM(rn, rn, 32-2, 2, 31);


#define SH2JIT_SHLR8 \
	PPCC_RLWINM(rn, rn, 32-8, 8, 31);

#define SH2JIT_SHLR16 \
	PPCC_RLWINM(rn, rn, 32-16, 16, 31);


#define SH2JIT_NOT /* NOT Rm,Rn  0110nnnnmmmm0111 */ \
	PPCC_NOR(rn, rm, rm);


#define SH2JIT_NEG /* NEG Rm,Rn  0110nnnnmmmm1011 */ \
	PPCC_NEG(rn, rm);


#define SH2JIT_NEGC /* NEGC Rm,Rn  0110nnnnmmmm1010 */ \
	PPCC_ANDI(GP_TMP, GP_SR, 0x1);			/* Extract T */ \
	PPCC_NEG(GP_TMP, GP_TMP);				/* Negate T */ \
	PPCC_SUBFCO(rn, rm, GP_TMP);			/* 0 - Rm - T store in Rn */ \
	PPCC_MFXER(GP_TMP); 					/*Load XER[CA] to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store T*/


#define SH2JIT_DT /* DT Rn  0100nnnn00010000 */ \
	PPCC_ADDICR(rn, rn, -1);				/*Add -1 set CR0 reg NOTE: Check for overflow problems*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store CR[EQ] in T*/


#define SH2JIT_EXTSB /* EXTS.B Rm,Rn  0110nnnnmmmm1110 */ \
	PPCC_EXTSB(rn, rm);


#define SH2JIT_EXTSW /* EXTS.W Rm,Rn  0110nnnnmmmm1111 */ \
	PPCC_EXTSH(rn, rm);


#define SH2JIT_EXTUB /* EXTU.B Rm,Rn  0110nnnnmmmm1100 */ \
	PPCC_ANDI(rn, rm, 0x00FF);


#define SH2JIT_EXTUW /* EXTU.W Rm,Rn  0110nnnnmmmm1101 */ \
	PPCC_ANDI(rn, rm, 0xFFFF);



/*Mult and Division*/
#define SH2JIT_DIV0S /* DIV0S Rm,Rn  0010nnnnmmmm0111 */ \
	PPCC_XOR(GP_TMP, rn, rm);					/*Q ^ M*/ \
	PPCC_RLWIMI(GP_SR, rn, 8+1, 31-8, 31-8);	/*Store MSB of Rn as Q bit*/ \
	PPCC_RLWIMI(GP_SR, rm, 9+1, 31-9, 31-9);	/*Store MSB of Rm as M bit*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 1, 31, 31);		/*Store Q ^ M as T bit*/
//NOTE: Is this optimal?


#define SH2JIT_DIV0U /* DIV0U  0000000000011001 */ \
	PPCC_ANDI(GP_SR, GP_SR, 0xFCFE);

//NOTE: This accurate but can be optimized if no immediate calculation is needed
#define SH2JIT_DIV1 /* DIV1 Rm,Rn  0011nnnnmmmm0100 */ \
	PPCC_RLWINM(rn, rn, 1, 0, 31);				 \
	PPCC_ANDI(GP_TMP, rn, 0x1);					/* MSB = (Rn >> 31) & 1 */ \
	PPCC_RLWIMI(rn, GP_SR, 0, 31, 31);			/* Rn = (Rn << 1) | T */ \
	PPCC_RLWINM(3, GP_SR, 32-9, 31, 31);		/* tmp2 = -(Q ^ M) */ \
	PPCC_RLWINM(GP_TMP2, GP_SR, 32-8, 31, 31);	 \
	PPCC_XOR(GP_TMP2, GP_TMP2, 3);				 \
	PPCC_NEG(GP_TMP2, GP_TMP2);					 \
	PPCC_AND(GP_TMP2, GP_TMP2, rm);				/* tmp2 = ((Rm << 1) & tmp2) - Rm */ \
	PPCC_RLWINM(GP_TMP2, GP_TMP2, 1, 0, 31);	 \
	PPCC_SUBF(GP_TMP2, rm, GP_TMP2);			 \
	/* MSB ^= carry(Rn += tmp2) */				 \
	PPCC_ADDCp(rn, GP_TMP2, rn);				 \
	PPCC_MFXER(GP_TMP2); 						/*Load XER[CA] to TMP2 (Reuse) */ \
	PPCC_RLWIMI(GP_TMP2, GP_TMP2, 3, 31, 31);	 \
	PPCC_XOR(GP_TMP, GP_TMP, GP_TMP2);			 \
	/* Q = M ^ MSB */							 \
	PPCC_XOR(3, 3, GP_TMP);						 \
	PPCC_RLWIMI(GP_SR, 3, 8, 31-8, 31-8);		/*Store Q bit*/ \
	/* T = !MSB */								 \
	PPCC_NOR(GP_TMP, GP_TMP, GP_TMP);			 \
	PPCC_RLWIMI(GP_SR, GP_TMP, 0, 31, 31);		/*Store T bit*/


#define JIT_DMULS 					/* DMULS.L Rm,Rn  0011nnnnmmmm1101 */ \
	PPCC_MULHW(GP_MACH, rn, rm); 	/*Multiply for signed high word*/ \
	PPCC_MULLW(GP_MACL, rn, rm); 	/*Multiply for low word*/ \
	PPCE_SAVE(GP_MACH, mach);		\
	PPCE_SAVE(GP_MACL, macl);		\


#define SH2JIT_DMULS 				/* DMULS.L Rm,Rn  0011nnnnmmmm1101 */ \
	PPCC_MULHW(GP_MACH, rn, rm); 	/*Multiply for signed high word*/ \
	PPCC_MULLW(GP_MACL, rn, rm); 	/*Multiply for low word*/ \
	PPCE_SAVE(GP_MACH, mach);		\
	PPCE_SAVE(GP_MACL, macl);		\


#define SH2JIT_DMULU /* DMULU.L Rm,Rn  0011nnnnmmmm0101 */ \
	PPCC_MULHWU(GP_MACH, rn, rm); 	/*Multiply for unsigned high word*/ \
	PPCC_MULLW(GP_MACL, rn, rm); 	/*Multiply for low word*/ \
	PPCE_SAVE(GP_MACH, mach);		\
	PPCE_SAVE(GP_MACL, macl);		\


#define SH2JIT_MACL /* MAC.L @Rm+,@Rn+  0000nnnnmmmm1111 */ \
	PPCC_MOV(3, rn);							/*Rn -> address*/ \
	PPCC_BL(sh2_Read32);						/*Read addr*/ \
	PPCC_MOV(GP_STMP, 3);						/*value -> TMP*/ \
	PPCC_MOV(3, rm);							/*Rm -> address*/ \
	PPCC_BL(sh2_Read32);						/*Read addr*/ \
	PPCC_MOV(GP_TMP2, 3);						/*value -> TMP2*/ \
	PPCC_ADDI(rn, rn, 4);						 \
	PPCC_ADDI(rm, rm, 4);						 \
	PPCC_MULHW(4, GP_STMP, GP_TMP2);				/* tmp * tmp2 -> high word */ \
	PPCC_MULLW(3, GP_STMP, GP_TMP2); 			/* tmp * tmp2 -> low word */ \
	PPCE_LOAD(GP_MACH, mach);		\
	PPCE_LOAD(GP_MACL, macl);		\
	PPCC_ADDCp(GP_MACL, GP_MACL, 3); 			/* macl += lw */ \
	PPCC_ADDEO(GP_MACH, GP_MACH, 4); 			/* mach += hw + carry */ \
	PPCC_RLWINM(GP_STMP, GP_SR, 15, 15, 15); 	/* saturate if S bit == 1 */ \
	PPCC_AND(GP_STMP, GP_MACH, GP_STMP); 		/* MACH & -(MACH & 0x0s00) */ \
	PPCC_NEG(GP_STMP, GP_STMP);					\
	PPCC_AND(GP_MACH, GP_MACH, GP_STMP);		\
	PPCE_SAVE(GP_MACH, mach);		\
	PPCE_SAVE(GP_MACL, macl);		\


#define SH2JIT_MACW /* MAC.W @Rm+,@Rn+  0100nnnnmmmm1111 */ \
	PPCC_MOV(3, rn);							/*Rn -> address*/ \
	PPCC_BL(sh2_Read16);						/*Read addr*/ \
	PPCC_EXTSH(GP_STMP, 3);						/*value -> TMP*/ \
	PPCC_MOV(3, rm);							/*Rm -> address*/ \
	PPCC_BL(sh2_Read16);						/*Read addr*/ \
	PPCC_EXTSH(GP_TMP2, 3);						/*value -> TMP2*/ \
	PPCC_ADDI(rn, rn, 2);						 \
	PPCC_ADDI(rm, rm, 2);						 \
	PPCC_MULLW(GP_STMP, GP_STMP, GP_TMP2); 		/* tmp * tmp2 -> tmp (low word) */ \
	PPCC_RLWINM(GP_TMP2, GP_SR, 32-1, 31, 31); 	/* mask MACH if S bit == 0 */ \
	PPCC_ADDI(GP_TMP2, GP_TMP2, -1);			 \
	PPCE_LOAD(GP_MACH, mach);					 \
	PPCE_LOAD(GP_MACL, macl);					 \
	PPCC_AND(GP_MACH, GP_MACH, GP_TMP2);		 \
	PPCC_ADDCp(GP_MACL, GP_MACL, GP_STMP); 		/* macl += lw */ \
	PPCC_ADDZE(GP_MACH, GP_MACH); 				/* mach = mach + carry */ \
	PPCE_SAVE(GP_MACH, mach);					 \
	PPCE_SAVE(GP_MACL, macl);					 \


#define SH2JIT_MULL			/* MUL.L Rm,Rn  0000nnnnmmmm0111 */ \
	PPCC_MULLW(GP_MACL, rn, rm);				 \
	PPCE_SAVE(GP_MACL, macl);					 \


#define SH2JIT_MULS			/* MULS Rm,Rn  0010nnnnmmmm1111 */ \
	PPCC_EXTSH(GP_TMP, rn);				/* (s16) Rn  */ \
	PPCC_EXTSH(GP_TMP2, rm);					/* (s16) Rm  */ \
	PPCC_MULLW(GP_MACL, GP_TMP, GP_TMP2);	/* Rn * Rm -> MACL */ \
	PPCE_SAVE(GP_MACL, macl);					 \


#define SH2JIT_MULU			/* MULU Rm,Rn  0010nnnnmmmm1110 */ \
	PPCC_ANDI(GP_TMP, rn, 0xFFFF);			/* (u16) Rn  */ \
	PPCC_ANDI(GP_TMP2, rm, 0xFFFF);			/* (u16) Rm  */ \
	PPCC_MULLW(GP_MACL, GP_TMP, GP_TMP2);	/* Rn * Rm -> MACL */ \
	PPCE_SAVE(GP_MACL, macl);				 \



/*Set and Clear*/
#define SH2JIT_CLRMAC /* CLRMAC  0000000000101000 */ \
	PPCC_ANDI(GP_TMP, GP_TMP, 0x0); 	/*Clear MACH/MACL*/ \
	PPCE_SAVE(GP_TMP, mach);			 \
	PPCE_SAVE(GP_TMP, macl);			 \


#define SH2JIT_CLRT /* CLRT  0000000000001000 */ \
	PPCC_ANDI(GP_SR, GP_SR, 0xFFFE);


#define SH2JIT_SETT /* SETT  0000000000011000 */ \
	PPCC_ORI(GP_SR, GP_SR, 0x0001);



/*Compare*/
#define SH2JIT_CMPEQ		/* CMP_EQ Rm,Rn  0011nnnnmmmm0000 */ \
	PPCC_CMP(0, rn, rm); 					/*Compare rn and rm*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store EQ in T*/


#define SH2JIT_CMPGE		/* CMP_GE Rm,Rn  0011nnnnmmmm0011 */ \
	PPCC_CMP(0, rn, rm); 					/*Compare rn and rm*/ \
	PPCC_CRNOR(0, 0, 0);					/*Negate the CR*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 1, 31, 31); 	/*Store LT in T*/


#define SH2JIT_CMPGT		/* CMP_GT Rm,Rn  0011nnnnmmmm0111 */ \
	PPCC_CMP(0, rn, rm); 					/*Compare rn and rm*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 2, 31, 31); 	/*Store GT in T*/


#define SH2JIT_CMPHI		/* CMP_HI Rm,Rn  0011nnnnmmmm0110 */ \
	PPCC_CMPL(0, rn, rm); 					/*Compare Unsigned rn and rm*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 2, 31, 31); 	/*Store GT in T*/


#define SH2JIT_CMPHS		/* CMP_HS Rm,Rn  0011nnnnmmmm0010 */ \
	PPCC_CMPL(0, rn, rm); 					/*Compare Unsigned rn and rm*/ \
	PPCC_CRNOR(0, 0, 0);					/*Negate the CR*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 1, 31, 31); 	/*Store LT in T*/


#define SH2JIT_CMPPL		/* CMP_PL Rn  0100nnnn00010101 */ \
	PPCC_NEG(GP_TMP, rn);					/*Negate value*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 1, 31, 31); 	/*Store MSB of ~Rn in T*/


#define SH2JIT_CMPPZ		/* CMP_PZ Rn  0100nnnn00010001 */ \
	PPCC_RLWIMI(GP_SR, rn, 1, 31, 31); 		/*Store MSB in T*/ \
	PPCC_XORI(GP_SR, GP_SR, 1); 			/*negate T*/


#define SH2JIT_CMPSTR		/* CMP_STR Rm,Rn  0010nnnnmmmm1100 */ \
	PPCC_XOR(GP_TMP, rn, rm); 				/*Rn ^ Rm*/ \
	PPCC_RLWINM(3, GP_TMP, 16, 16, 31); 	/*Get second half*/ \
	PPCC_AND(GP_TMP, GP_TMP, 3); 			/*AND with first half*/ \
	PPCC_RLWINM(3, GP_TMP, 8, 24, 31); 		/*Get second byte*/ \
	PPCC_AND(GP_TMP, GP_TMP, 3); 			/*AND with first byte*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR0 to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store CR0[EQ] (is zero) in T*/


#define SH2JIT_CMPIM		/* CMP_EQ #imm,R0  10001000iiiiiiii */ \
	PPCC_CMPI(0, GP_R0, EXT_IMM8(imm)); 	/*Compare rn to EXT(imm)*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store EQ in T*/


/*Load and Stores*/
#define SH2JIT_LDCSR		/* LDC Rm,SR  0100mmmm00001110 */ \
	PPCC_ANDI(GP_SR, rn, 0x03F3);


#define SH2JIT_LDCGBR		/* LDC Rm,GBR  0100mmmm00011110 */ \
	PPCC_ORI(GP_GBR, rn, 0x0);		 \
	PPCE_SAVE(GP_GBR, gbr);			 \


#define SH2JIT_LDCVBR		/* LDC Rm,VBR  0100mmmm00101110 */ \
	PPCC_ORI(GP_VBR, rn, 0x0);		 \
	PPCE_SAVE(GP_VBR, vbr);			 \


#define SH2JIT_LDCMSR /* LDC.L @Rm+,SR  0100mmmm00000111 */ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read addr*/ \
	PPCC_ANDI(GP_SR, 3, 0x3F3);	/*value -> SR*/ \
	PPCC_ADDI(rn, rn, 4);		/*Move address four bytes*/


#define SH2JIT_LDCMGBR /* LDC.L @Rm+,GBR  0100mmmm00010111 */ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read addr*/ \
	/* PPCC_ORI(GP_GBR, 3, 0x0); */	/*value -> GBR*/ \
	PPCE_SAVE(3, gbr);			 \
	PPCC_ADDI(rn, rn, 4);		/*Move address four bytes*/


#define SH2JIT_LDCMVBR /* LDC.L @Rm+,VBR  0100mmmm00100111 */ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read addr*/ \
	/* PPCC_ORI(GP_VBR, 3, 0x0); */	/*value -> VBR*/ \
	PPCE_SAVE(3, vbr);			 \
	PPCC_ADDI(rn, rn, 4);		/*Move address four bytes*/


#define SH2JIT_LDSMACH /* LDS Rm,MACH  0100mmmm00001010 */ \
	/* PPCC_ORI(GP_MACH, rn, 0x0); */	 \
	PPCE_SAVE(rn, mach);				 \


#define SH2JIT_LDSMACL /* LDS Rm,MACL  0100mmmm00011010 */ \
	/* PPCC_ORI(GP_MACL, rn, 0x0); */	 \
	PPCE_SAVE(rn, macl);				 \


#define SH2JIT_LDSPR /* LDS Rm,PR  0100mmmm00101010 */ \
	/* PPCC_ORI(GP_PR, rn, 0x0); */		 \
	PPCE_SAVE(rn, pr);					 \


#define SH2JIT_LDSMMACH /* LDS.L @Rm+,MACH  0100mmmm00000110 */ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read addr*/ \
	/* PPCC_ORI(GP_MACH, 3, 0x0); */	/*value -> MACH*/ \
	PPCE_SAVE(3, mach);			 \
	PPCC_ADDI(rn, rn, 4);		/*Move address four bytes*/


#define SH2JIT_LDSMMACL /* LDS.L @Rm+,MACL  0100mmmm00010110 */ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read addr*/ \
	/* PPCC_ORI(GP_MACL, 3, 0x0); */	/*value -> MACL*/ \
	PPCE_SAVE(3, macl);			 \
	PPCC_ADDI(rn, rn, 4);		/*Move address four bytes*/


#define SH2JIT_LDSMPR /* LDS.L @Rm+,PR  0100mmmm00100110 */ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read addr*/ \
	/* PPCC_ORI(GP_PR, 3, 0x0); */	/*value -> PR*/ \
	PPCE_SAVE(3, pr);			 \
	PPCC_ADDI(rn, rn, 4);		/*Move address four bytes*/



#define SH2JIT_STCSR /* STC SR,Rn  0000nnnn00000010 */ \
	PPCC_ORI(rn, GP_SR, 0x0);


#define SH2JIT_STCGBR /* STC GBR,Rn  0000nnnn00010010 */ \
	/* PPCC_ORI(rn, GP_GBR, 0x0); */	 \
	PPCE_LOAD(rn, gbr);

#define SH2JIT_STCVBR /* STC VBR,Rn  0000nnnn00100010 */ \
	/* PPCC_ORI(rn, GP_VBR, 0x0); */	 \
	PPCE_LOAD(rn, vbr);

#define SH2JIT_STCMSR /* STC.L SR,@-Rn  0100nnnn00000011 */ \
	PPCC_ADDI(rn, rn, -4);		/*Move address four bytes*/ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_ORI(4, GP_SR, 0x0);	/*SR -> value*/ \
	PPCC_BL(sh2_Write32);		/*Write SR*/


#define SH2JIT_STCMGBR /* STC.L GBR,@-Rn  0100nnnn00010011 */ \
	PPCC_ADDI(rn, rn, -4);		/*Move address four bytes*/ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	/* PPCC_ORI(4, GP_GBR, 0x0); */	/*SR -> value*/ \
	PPCE_LOAD(4, gbr);			 \
	PPCC_BL(sh2_Write32);		/*Write GBR*/


#define SH2JIT_STCMVBR /* STC.L VBR,@-Rn  0100nnnn00100011 */ \
	PPCC_ADDI(rn, rn, -4);		/*Move address four bytes*/ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	/* PPCC_ORI(4, GP_VBR, 0x0); */	/*SR -> value*/ \
	PPCE_LOAD(4, vbr);			 \
	PPCC_BL(sh2_Write32);		/*Write VBR*/


#define SH2JIT_STSMACH /* STS MACH,Rn  0000nnnn00001010 */ \
	/* PPCC_ORI(rn, GP_MACH, 0x0); */	\
	PPCE_LOAD(rn, mach);


#define SH2JIT_STSMACL /* STS MACL,Rn  0000nnnn00011010 */ \
	/* PPCC_ORI(rn, GP_MACL, 0x0); */		\
	PPCE_LOAD(rn, macl);


#define SH2JIT_STSPR /* STS PR,Rn  0000nnnn00101010 */ \
	/* PPCC_ORI(rn, GP_PR, 0x0); */			\
	PPCE_LOAD(rn, pr);


#define SH2JIT_STSMMACH /* STS.L MACH,@–Rn  0100nnnn00000010 */ \
	PPCC_ADDI(rn, rn, -4);		/*Move address four bytes*/ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	/* PPCC_ORI(4, GP_MACH, 0x0); */	/*MACH -> value*/ \
	PPCE_LOAD(4, mach);			 \
	PPCC_BL(sh2_Write32);		/*Write MACH*/


#define SH2JIT_STSMMACL /* STS.L MACL,@–Rn  0100nnnn00010010 */ \
	PPCC_ADDI(rn, rn, -4);		/*Move address four bytes*/ \
	PPCC_MOV(3, rn);			/*Rn -> address*/ \
	/* PPCC_MOV(4, GP_MACL); */		/*MACL -> value*/ \
	PPCE_LOAD(4, mach);			 \
	PPCC_BL(sh2_Write32);		/*Write MACL*/


#define SH2JIT_STSMPR /* STS.L PR,@–Rn  0100nnnn00100010 */ \
	PPCC_ADDI(rn, rn, -4);		/*Move address four bytes*/ \
	PPCC_MOV(3, rn);			/*Rn -> address*/ \
	/* PPCC_MOV(4, GP_PR); */			/*PR -> value*/ \
	PPCE_LOAD(4, pr);			 \
	PPCC_BL(sh2_Write32);		/*Write PR*/


/*Move Data*/
#define SH2JIT_MOV \
	PPCC_ORI(rn, rm, 0x0);


#define SH2JIT_MOVBS \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_ORI(4, rm, 0x0);		/*Rm -> value*/ \
	PPCC_BL(sh2_Write8);		/*Write byte*/


#define SH2JIT_MOVWS \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_ORI(4, rm, 0x0);		/*Rm -> value*/ \
	PPCC_BL(sh2_Write16);		/*Write 16 bit value*/


#define SH2JIT_MOVLS \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_ORI(4, rm, 0x0);		/*Rm -> value*/ \
	PPCC_BL(sh2_Write32);		/*Write 32 bit value*/


#define SH2JIT_MOVBL \
	PPCC_ORI(3, rm, 0x0);		/*Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_EXTSB(rn, 3);			/*EXT8(read value) -> Rn*/


#define SH2JIT_MOVWL \
	PPCC_ORI(3, rm, 0x0);		/*Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_EXTSH(rn, 3);			/*EXT16(read value) -> Rn*/


#define SH2JIT_MOVLL \
	PPCC_ORI(3, rm, 0x0);		/*Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_ORI(rn, 3, 0x0);		/*read value -> Rn*/


#define SH2JIT_MOVBM \
	PPCC_ADDI(rn, rn, -1);		/*Move address a byte*/ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_ORI(4, rm, 0x0);		/*Rm -> value*/ \
	PPCC_BL(sh2_Write8);		/*Write byte*/


#define SH2JIT_MOVWM \
	PPCC_ADDI(rn, rn, -2);		/*Move address two bytes*/ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_ORI(4, rm, 0x0);		/*Rm -> value*/ \
	PPCC_BL(sh2_Write16);		/*Write 16 bit value*/


#define SH2JIT_MOVLM \
	PPCC_ADDI(rn, rn, -4);		/*Move address four bytes*/ \
	PPCC_ORI(3, rn, 0x0);		/*Rn -> address*/ \
	PPCC_ORI(4, rm, 0x0);		/*Rm -> value*/ \
	PPCC_BL(sh2_Write32);		/*Write 32 bit value*/


#define SH2JIT_MOVBP \
	PPCC_ORI(3, rm, 0x0);		/*Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_EXTSB(rn, 3);			/*EXT8(read value) -> Rn*/ \
	if (rn != rm) { PPCC_ADDI(rm, rm, 1); }		/*increment 4 to Rn*/


#define SH2JIT_MOVWP \
	PPCC_ORI(3, rm, 0x0);		/*Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_EXTSH(rn, 3);			/*EXT16(read value) -> Rn*/ \
	if (rn != rm) { PPCC_ADDI(rm, rm, 2); }		/*increment 4 to Rn*/


#define SH2JIT_MOVLP  \
	PPCC_ORI(3, rm, 0x0);		/*Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_MOV(rn, 3);			/*read value -> Rn*/ \
	if (rn != rm) { PPCC_ADDI(rm, rm, 4); }		/*increment 4 to Rn*/


#define SH2JIT_MOVBS0 \
	PPCC_ADD(3, rn, GP_R0);		/*R0+Rn -> address*/ \
	PPCC_MOV(4, rm);			/*Rm -> value*/ \
	PPCC_BL(sh2_Write8);		/*Write byte*/


#define SH2JIT_MOVWS0 \
	PPCC_ADD(3, rn, GP_R0);		/*R0+Rn -> address*/ \
	PPCC_MOV(4, rm);			/*Rm -> value*/ \
	PPCC_BL(sh2_Write16);		/*Write 16 bit value*/


#define SH2JIT_MOVLS0 \
	PPCC_ADD(3, rn, GP_R0);		/*R0+Rn -> address*/ \
	PPCC_MOV(4, rm);			/*Rm -> value*/ \
	PPCC_BL(sh2_Write32);		/*Write 32 bit value*/


#define SH2JIT_MOVBL0 \
	PPCC_ADD(3, rm, GP_R0);		/*R0+Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_EXTSB(rn, 3);			/*EXT8(read value) -> Rn*/


#define SH2JIT_MOVWL0 \
	PPCC_ADD(3, rm, GP_R0);		/*R0+Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_EXTSH(rn, 3);			/*EXT16(read value) -> Rn*/


#define SH2JIT_MOVLL0 \
	PPCC_ADD(3, rm, GP_R0);		/*R0+Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_MOV(rn, 3);			/*read value -> Rn*/


#define SH2JIT_MOVI /* MOV #imm,Rn  1110nnnniiiiiiii */ \
	PPCC_ADDI(rn, 0, EXT_IMM8(imm));	//NOTE: Must sign extend immidiate


#define SH2JIT_MOVWI /* MOV.W @(disp,PC),Rn  1001nnnndddddddd */ \
	u32 iaddr = curr_pc + ((disp & 0xFF) << 1); \
	PPCC_ADDIS(3, 0, (iaddr >> 16));		/*Set high imm 16 bits */ \
	PPCC_ORI(3, 3, iaddr);				/*Set low imm 16 bits */ \
	PPCC_BL(sh2_Read16);				/*Read 16 bit value*/ \
	PPCC_EXTSH(rn, 3);					/*Extend s16*/
	//XXX: the address is constant so we can ignore sh2_Read16 and go straigth to its handling function
	//TODO: Check for delay-slot

#define SH2JIT_MOVLI /* MOV.L @(disp,PC),Rn  1101nnnndddddddd */ \
	u32 iaddr = (curr_pc & 0xFFFFFFFC) + ((disp & 0xFF) << 2); \
	PPCC_ADDIS(3, 0, (iaddr >> 16));		/*Set high imm 16 bits */ \
	PPCC_ORI(3, 3, iaddr);				/*Set low imm 16 bits */ \
	PPCC_BL(sh2_Read32);				/*Read 32 bit value*/ \
	PPCC_MOV(rn, 3);
	//XXX: the address is constant so we can ignore sh2_Read16 and go straigth to its handling function
	//TODO: Check for delay-slot


#define SH2JIT_MOVBLG /* MOV.B @(disp,GBR),R0  11000100dddddddd */ \
	PPCE_LOAD(GP_GBR, gbr);						 \
	PPCC_ADDI(3, GP_GBR, disp & 0xFF);	/*Set GBR+disp -> addr */ \
	PPCC_BL(sh2_Read8);					/*Read 8 bit value*/ \
	PPCC_EXTSB(GP_R0, 3);				/* Extend s8  -> R0*/


#define SH2JIT_MOVWLG /* MOV.W @(disp,GBR),R0  11000101dddddddd */ \
	PPCE_LOAD(GP_GBR, gbr);						 \
	PPCC_ADDI(3, GP_GBR, (disp & 0xFF) << 1);	/*Set GBR+disp -> addr */ \
	PPCC_BL(sh2_Read16);						/*Read 16 bit value*/ \
	PPCC_EXTSH(GP_R0, 3);						/* Extend s16 -> R0*/


#define SH2JIT_MOVLLG /* MOV.L @(disp,GBR),R0  11000110dddddddd */ \
	PPCE_LOAD(GP_GBR, gbr);						 \
	PPCC_ADDI(3, GP_GBR, (disp & 0xFF) << 2);	/*Set GBR+disp -> addr */ \
	PPCC_BL(sh2_Read32);						/*Read 32 bit value*/ \
	PPCC_MOV(GP_R0, 3);							/* Move to R0 */


#define SH2JIT_MOVBSG /* MOV.B R0,@(disp,GBR)  11000000dddddddd */ \
	PPCE_LOAD(GP_GBR, gbr);				 \
	PPCC_ADDI(3, GP_GBR, disp & 0xFF);	/*GBR+disp -> address*/ \
	PPCC_MOV(4, GP_R0);					/*R0 -> value*/ \
	PPCC_BL(sh2_Write8);				/*Write 8 bit value*/


#define SH2JIT_MOVWSG /* MOV.W R0,@(disp,GBR)  11000001dddddddd */ \
	PPCE_LOAD(GP_GBR, gbr);						 \
	PPCC_ADDI(3, GP_GBR, (disp & 0xFF) << 1);	/*GBR+(disp*2) -> address*/ \
	PPCC_MOV(4, GP_R0);							/*R0 -> value*/ \
	PPCC_BL(sh2_Write16);						/*Write 16 bit value*/


#define SH2JIT_MOVLSG /* MOV.L R0,@(disp,GBR)  11000010dddddddd */ \
	PPCE_LOAD(GP_GBR, gbr);						 \
	PPCC_ADDI(3, GP_GBR, (disp & 0xFF) << 2);	/*GBR+(disp*4) -> address*/ \
	PPCC_MOV(4, GP_R0);							/*R0 -> value*/ \
	PPCC_BL(sh2_Write32);						/*Write 32 bit value*/


#define SH2JIT_MOVBS4 /* MOV.B R0,@(disp,Rn)  10000000nnnndddd */ \
	PPCC_ADDI(3, rm, disp & 0xF);	/*Rn+disp -> address*/ \
	PPCC_MOV(4, GP_R0);				/*R0 -> value*/ \
	PPCC_BL(sh2_Write8);			/*Write 8 bit value*/


#define SH2JIT_MOVWS4 /* MOV.W R0,@(disp,Rn)  10000001nnnndddd */ \
	PPCC_ADDI(3, rm, (disp & 0xF) << 1);	/*Rn+(disp*2) -> address*/ \
	PPCC_MOV(4, GP_R0);						/*R0 -> value*/ \
	PPCC_BL(sh2_Write16);					/*Write 16 bit value*/


#define SH2JIT_MOVLS4 /* MOV.L Rm,@(disp,Rn)  0001nnnnmmmmdddd */ \
	PPCC_ADDI(3, rn, (disp & 0xF) << 2);	/*Rn+(disp*4) -> address*/ \
	PPCC_MOV(4, rm);						/*Rm -> value*/ \
	PPCC_BL(sh2_Write32);					/*Write 32 bit value*/


#define SH2JIT_MOVBL4 /* MOV.B @(disp,Rm),R0  10000100mmmmdddd */ \
	PPCC_ADDI(3, rm, disp & 0xF);	/*Rm+disp -> address*/ \
	PPCC_BL(sh2_Read8);				/*Read 8 bit value*/ \
	PPCC_EXTSB(GP_R0, 3);			/*Extend s8 -> R0*/


#define SH2JIT_MOVWL4 /* MOV.W @(disp,Rm),R0  10000101mmmmdddd */ \
	PPCC_ADDI(3, rm, (disp & 0xF) << 1);	/*Rm+(disp*2) -> address*/ \
	PPCC_BL(sh2_Read16);					/*Read 16 bit value*/ \
	PPCC_EXTSH(GP_R0, 3);					/*Extend s16  -> R0*/


#define SH2JIT_MOVLL4 /* MOV.L @(disp,Rm),Rn  0101nnnnmmmmdddd */ \
	PPCC_ADDI(3, rm, (disp & 0xF) << 2);	/*Rm+(disp*4) -> address*/ \
	PPCC_BL(sh2_Read32);					/*Read 32 bit value*/ \
	PPCC_MOV(rn, 3);						/*Result -> R0*/



#define SH2JIT_MOVA /* MOVA @(disp,PC),R0  11000111dddddddd */ \
	u32 iaddr = (curr_pc & 0xFFFFFFFC) + ((disp & 0xFF) << 2); \
	PPCC_ADDIS(GP_R0, 0, (iaddr >> 16));			/*Set high imm 16 bits */ \
	PPCC_ORI(GP_R0, GP_R0, iaddr);				/*Set low imm 16 bits */
	//TODO: Check for delay-slot


#define SH2JIT_MOVT /* MOVT Rn  0000nnnn00101001 */ \
	PPCC_ANDI(rn, GP_SR, 0x0001);


/*Branch and Jumps*/
#define SH2JIT_BF			/* BF disp  10001011dddddddd */ \
	u32 offset = (EXT_IMM8(disp) << 1) + 4 - 2; \
	PPCC_ADDIS(GP_PC, 0, (curr_pc+2) >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, curr_pc+2);		/*Set low imm 16 bits */ \
	PPCC_ANDI(GP_TMP, GP_SR, 0x0001);		/*Get T bit */ \
	PPCC_ADDI(GP_TMP, GP_TMP, -1);			/*Mask from T bit (active when T=0)*/ \
	PPCC_ANDI(GP_TMP, GP_TMP, offset);		/*Mask off offset */ \
	PPCC_EXTSH(GP_TMP, GP_TMP);				/*Extend to 32bits */ \
	PPCC_ADD(GP_PC, GP_PC, GP_TMP);			/*Add offset and store in ret value */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \


#define SH2JIT_BFS	SH2JIT_BF		/* BFS disp  10001111dddddddd */


#define SH2JIT_BRA			/* BRA disp  1010dddddddddddd */ \
	u32 new_pc = curr_pc + (EXT_IMM12(disp) << 1) + 4; \
	PPCC_ADDIS(GP_PC, 0, new_pc >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, new_pc);		/*Set low imm 16 bits */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \

#define SH2JIT_BRAF						/* BRAF Rm  0000mmmm00100011 */ \
	PPCC_ADDIS(GP_PC, 0, curr_pc >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, curr_pc);		/*Set low imm 16 bits */ \
	PPCC_ADD(GP_PC, GP_PC, rn)				/*Add Rn to PC */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \

#define SH2JIT_BSR			/* BSR disp  1011dddddddddddd */ \
	u32 offset = (EXT_IMM12(disp) << 1) + 4; \
	PPCC_ADDIS(GP_PC, 0, curr_pc >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PR, GP_PC, curr_pc);		/*Set low imm 16 bits (PR <- PC) */ \
	PPCC_ADDI(GP_PC, GP_PR, offset)			/* PC <- PC + Ext(offset)*/ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \

#define SH2JIT_BSRF			/* BSRF Rm  0000mmmm00000011 */ \
	PPCC_ADDIS(GP_PC, 0, curr_pc >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PR, GP_PC, curr_pc);		/*Set low imm 16 bits (PR <- PC) */ \
	PPCC_ADD(GP_PC, GP_PR, rn)				/* PC <- Rn + PC */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \


#define SH2JIT_BT			/* BT disp  10001001dddddddd */ \
	u32 offset = (EXT_IMM8(disp) << 1) + 4 - 2; \
	PPCC_ADDIS(GP_PC, 0, (curr_pc+2) >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, curr_pc+2);		/*Set low imm 16 bits */ \
	PPCC_ANDI(GP_TMP, GP_SR, 0x0001);		/*Get T bit */ \
	PPCC_NEG(GP_TMP, GP_TMP);				/*Mask from T bit (active when T=1)*/ \
	PPCC_ANDI(GP_TMP, GP_TMP, offset);		/*Mask off offset */ \
	PPCC_EXTSH(GP_TMP, GP_TMP);				/*Extend to 32bits */ \
	PPCC_ADD(GP_PC, GP_PC, GP_TMP);			/*Add offset and store in ret value */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \


#define SH2JIT_BTS	SH2JIT_BT		/* BTS disp  10001101dddddddd */


#define SH2JIT_JMP			/* JMP @Rm  0100mmmm00101011 */ \
	PPCC_ADDI(GP_PC, rm, 0x0);		/*Rm + 4 -> New PC*/ \
	PPCE_SAVE(rn, pc);			/* Save PC */


#define SH2JIT_JSR			/* JSR @Rm  0100mmmm00001011 */ \
	PPCC_ADDIS(GP_PR, 0, curr_pc >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PR, GP_PR, curr_pc);		/*Set low imm 16 bits */ \
	PPCC_ADDI(GP_PC, rn, 0x4);				/*Rm + 4 -> New PC*/ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */


#define SH2JIT_RTE 			/* RTE  0000000000101011 */ \
	PPCC_MOV(3, GP_R(15));					/*R15 -> address*/ \
	PPCC_BL(sh2_Read32);					/*Read 32 bit value*/ \
	PPCC_ADDI(GP_STMP, 3, 0x4);				/*Result + 4 -> New PC*/ \
	PPCC_ADDI(3, GP_R(15), 0x4);			/*R15 + 4 -> address*/ \
	PPCC_BL(sh2_Read32);					/*Read 32 bit value*/ \
	PPCC_ANDI(GP_SR, 3, 0x3F3);				/*Result & 0x03F3 -> SR*/ \
	PPCC_ADDI(GP_R(15), GP_R(15), 0x8);		/*R15 + 4 -> address*/ \
	PPCE_SAVE(GP_STMP, pc);					/* Save PC */


#define SH2JIT_RTS 			/* RTS  0000000000001011 */ \
	PPCC_ADDI(GP_PC, GP_PR, 4);			/* SR + 4 -> PC*/ \
	PPCE_SAVE(GP_PC, pc);				/* Save PC */



/*Other*/
#define SH2JIT_NOP 			/* NOP  0000000000001001 */
	/*Does nothing*/


#define SH2JIT_SLEEP 		/* SLEEP  0000000000011011 */ \
	PPCC_ADDIS(GP_PC, 0, (curr_pc+2) >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, curr_pc+2);			/*Set low imm 16 bits */ \
	PPCE_SAVE(GP_PC, pc);						/* Save PC */
	/*TODO: Does nothing else ?...*/


#define SH2JIT_SWAPB 		/* SWAP.B Rm,Rn  0110nnnnmmmm1000 */ \
	PPCC_RLWINM(rn, rm, 8, 16, 31);		/*shift to left lower 8 bits*/ \
	PPCC_RLWINM(rn, rm, 32-8, 24, 31);	/*shift to right higher 8 bits*/ \
	PPCC_RLWINM(rn, rm, 0, 0, 15);		/*Store as is rest of upper 16 bits of Rm*/


#define SH2JIT_SWAPW 		/* SWAP.W Rm,Rn  0110nnnnmmmm1001 */ \
	PPCC_RLWINM(rn, rm, 16, 0, 31);


#define SH2JIT_TAS 			/* TAS.B @Rn  0100nnnn00011011 */ \
	PPCC_MOV(3, rn);						/* -> address*/ \
	PPCC_BL(sh2_Read8);						/*Read addr*/ \
	PPCC_CNTLZW(GP_TMP, 3);					/*Compare to zero (32 if is zero)*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 27, 31, 31); /*Rotate left to Store bit 5 in T*/ \
	PPCC_ORI(4, 3, 0x80);					/*(TMP | 0x80) -> value*/ \
	PPCC_MOV(3, rn);						/* -> address*/ \
	PPCC_BL(sh2_Write8);					/*Write 8bit value */


#define SH2JIT_TRAPA 		/* TRAPA #imm  11000011iiiiiiii */ \
	u32 dispimm = (imm & 0xFF) << 2;		\
	PPCC_ADDI(3, GP_R(15), -4);			/*R15 - 4 -> address*/ \
	PPCC_MOV(4, GP_SR);					/*SR -> value*/ \
	PPCC_BL(sh2_Write32);				/*Write 32bit value*/ \
	PPCC_ADDI(3, GP_R(15), -8);			/*R15 - 8 -> address*/ \
	PPCC_ADDIS(4, 0, (curr_pc+2) >> 16);	/*Load PC-2 immediate -> value*/ \
	PPCC_ORI(4, 4, curr_pc+2);		 	\
	PPCC_BL(sh2_Write32);				/*Write 32 bit value*/ \
	PPCC_ADDI(GP_R(15), GP_R(15), -8);	/*R15 - 8 -> address*/ \
	PPCE_LOAD(GP_VBR, vbr);				 \
	PPCC_ADDI(3, GP_VBR, dispimm);		/* VBR + dispimm -> address*/ \
	PPCC_BL(sh2_Read32);				/*Read addr*/ \
	/* PPCC_MOV(GP_PC, 3); */			/*Result -> PC*/ \
	PPCE_SAVE(3, pc);				/* Save PC */


#define SH2JIT_TST			/* TST Rm,Rn  0010nnnnmmmm1000 */ \
	PPCC_ANDp(rn, rn, rm);					/*Rn AND Rm and check if zero*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store CR0[EQ] (Zero) in T*/


#define SH2JIT_TSTI			/* TEST #imm,R0  11001000iiiiiiii */ \
	PPCC_ANDI(GP_R0, GP_R0, imm & 0xFF);	/*R0 AND imm and check if zero*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store EQ (Zero) in T*/


#define SH2JIT_TSTM			/* TST.B #imm,@(R0,GBR)  11001100iiiiiiii */ \
	PPCE_SAVE(GP_GBR, gbr);					/* Fetch GBR */ \
	PPCC_ADD(3, GP_R0, GP_GBR);				/*GBR+R0 -> addr (Reg3)*/ \
	PPCC_BL(sh2_Read8);						/*Read byte from addr (Reg3) -> byte in Reg3*/ \
	PPCC_ANDI(3, 3, imm & 0xFF);			/*Reg3 AND imm and check if zero*/ \
	PPCC_MFCR(3); 							/*Load CR0 to Reg3*/ \
	PPCC_RLWIMI(GP_SR, 3, 3, 31, 31); 		/*Store CR0[EQ] (Zero) in T*/
	//NOTE: Uses Reg3 for TMP

#define SH2JIT_XTRCT		/* XTRCT Rm,Rn  0010nnnnmmmm1101 */ \
	PPCC_RLWINM(rn, rn, 16, 16, 31); 	/*Shift Rn*/ \
	PPCC_RLWIMI(rn, rm, 16, 0, 15); 	/*Store shifted Rm in Rn*/



//========================================
// Constand address versions
//========================================
// TODO: We should have constant versions of
// every instruction that accesses memory in which
// the address is known, most of the time this should
// be pretty easy to determine and could happen often
// since there are a lot of accesses to predefined registers.
// Eventually, if fastmem is implemented, there will
// be new versions that do basically the same thing as these.

// TODO: T value: only the following 10 instructions use the T value:
// MOVT, ADDC, NEGC, SUBC, ROTCL, ROTCR, BF, BF/S, BT, BT/S
// Really we only need to set the T value in the previous instruction
// where it is set, else we can skip the T asigment without any problem.
// Not only this, but there are functions that only modify T, when this
// happens if the next instruction modifies T then the previous instruction
// is useless therefore it can be completely skipped.


#define sh2_jit_ca_ANDM(addr, imm)
	//CONST_XXX

#define sh2_jit_ca_ORM(addr, imm)
	//CONST_XXX

#define sh2_jit_ca_XORM(addr, imm)
	//CONST_XXX


/*Mult and Division*/
#define sh2_jit_ca_DIV0S(addr, rn, rm)
	//CONST_XXX

#define sh2_jit_ca_DIV1(addr, rn, rm)
	//CONST_XXX

#define sh2_jit_ca_MACL(addr, rn, rm)
	//CONST_XXX

#define sh2_jit_ca_MACW(addr, rn, rm)
	//CONST_XXX

#define sh2_jit_ca_MULS(addr, rn, rm)
	//CONST_XXX

#define sh2_jit_ca_MULU(addr, rn, rm)
	//CONST_XXX

/*Load and Stores*/
#define sh2_jit_ca_LDCMSR(addr, rm)
	//CONST_XXX

#define sh2_jit_ca_LDCMGBR(addr, rm)
	//CONST_XXX

#define sh2_jit_ca_LDCMVBR(addr, rm)
	//CONST_XXX

#define sh2_jit_ca_LDSMMACH(addr, rm)
	//CONST_XXX

#define sh2_jit_ca_LDSMMACL(addr, rm)
	//CONST_XXX

#define sh2_jit_ca_LDSMPR(addr, rm)
	//CONST_XXX

#define sh2_jit_ca_STCMSR(addr, rn)
	//CONST_XXX

#define sh2_jit_ca_STCMGBR(addr, rn)
	//CONST_XXX

#define sh2_jit_ca_STCMVBR(addr, rn)
	//CONST_XXX

#define sh2_jit_ca_STSMMACH(addr, rn)
	//CONST_XXX

#define sh2_jit_ca_STSMMACL(addr, rn)
	//CONST_XXX

#define sh2_jit_ca_STSMPR(addr, rn)
	//CONST_XXX

/*Move Data*/

#define sh2_jit_ca_MOVBS(addr, rn, rm)
	//CONST_XXX

#define sh2_jit_ca_MOVWS(addr, rn, rm)
	//CONST_XXX

#define sh2_jit_ca_MOVLS(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVBL(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVWL(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVLL(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVBM(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVWM(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVLM(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVBP(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVWP(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVLP(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVBS0(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVWS0(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVLS0(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVBL0(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVWL0(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVLL0(addr, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVWI(addr, d, rn)

	//CONST_XXX


#define sh2_jit_ca_MOVLI(addr, d, rn)

	//CONST_XXX


#define sh2_jit_ca_MOVBLG(addr, d)

	//CONST_XXX


#define sh2_jit_ca_MOVWLG(addr, d)

	//CONST_XXX


#define sh2_jit_ca_MOVLLG(addr, d)

	//CONST_XXX


#define sh2_jit_ca_MOVBSG(addr, d)

	//CONST_XXX


#define sh2_jit_ca_MOVWSG(addr, d)

	//CONST_XXX


#define sh2_jit_ca_MOVLSG(addr, d)

	//CONST_XXX


#define sh2_jit_ca_MOVBS4(addr, d, rn)

	//CONST_XXX


#define sh2_jit_ca_MOVWS4(addr, d, rn)

	//CONST_XXX


#define sh2_jit_ca_MOVLS4(addr, d, rn, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVBL4(addr, d, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVWL4(addr, d, rm)

	//CONST_XXX


#define sh2_jit_ca_MOVLL4(addr, d, rn, rm)

	//CONST_XXX



#define sh2_jit_ca_MOVA(addr, d)

	//CONST_XXX


/*Branch and Jumps*/
#define sh2_jit_ca_BF(addr, d)

	//CONST_XXX


#define sh2_jit_ca_BFS(addr, d)

	//CONST_XXX


#define sh2_jit_ca_BRA(addr, d)

	//CONST_XXX


#define sh2_jit_ca_BRAF(addr, rm)

	//CONST_XXX


#define sh2_jit_ca_BSR(addr, d)

	//CONST_XXX


#define sh2_jit_ca_BSRF(addr, rm)

	//CONST_XXX


#define sh2_jit_ca_BT(addr, d)

	//CONST_XXX


#define sh2_jit_ca_BTS(addr, d)

	//CONST_XXX


#define sh2_jit_ca_JMP(addr, rm)

	//CONST_XXX


#define sh2_jit_ca_JSR(addr, rm)

	//CONST_XXX


#define sh2_jit_ca_RTE(addr)

	//CONST_XXX


#define sh2_jit_ca_RTS(addr)

	//CONST_XXX




/*Other*/
#define sh2_jit_ca_TAS(addr, rn)

	//CONST_XXX


#define sh2_jit_ca_TRAPA(imm)
	//CONST_XXX

#define sh2_jit_ca_TSTM(addr, imm)
	//CONST_XXX

void SH2JIT_NoOpCode(void) {
	//Does nothing yet.
}

#define REG_NUM_R(n)		(1 << (n))
#define REG_NUM_PR			(1 << 17)
#define REG_NUM_GBR			(1 << 18)
#define REG_NUM_VBR			(1 << 19)
#define REG_NUM_MACH		(1 << 20)
#define REG_NUM_MACL		(1 << 21)
#define REG_NUM_SR			(1 << 22)

#include <stdio.h>

//This pass computes the following information:
//	Registers used
//	Registers stored
//  TODO: Instructions that set a T bit that is used
//  TODO: Delay slot rearanged instruction
//  TODO: Other
u32 _jit_BlockPass0(u32 addr, u32 *useregs, u32 *strregs)
{
	u32 continue_block = 1, delay_slot = 0;
	u32 curr_pc = addr;
	//Pass 0, check instructions and decode one by one:
	u16 *inst_ptr = (u16*) mem_GetPCAddr(curr_pc);
	u32 last_t_mod = 0; // Instruction
	u32 cycles = 0;
	//TODO: special registers are not accounted for right now, add them later
	u32 used_regs = REG_NUM_SR;		// Allways have SR loaded
	u32 stored_regs = REG_NUM_SR;	// Allways have SR loaded

	while (continue_block | delay_slot) {
		u32 inst = *(inst_ptr++);
		rn = REG_NUM_R((inst >> 8) & 0xF);
		rm = REG_NUM_R((inst >> 4) & 0xF);
		if (delay_slot) {
			delay_slot = 0;
		}

		switch ((inst >> 12) & 0xF) {
			case 0b0000: {
				switch(inst) { //Constant instructions
					case 0x0008: { } break;
					case 0x0018: { } break;
					case 0x0028: {used_regs |= REG_NUM_MACH | REG_NUM_MACL;} break;
					case 0x0009: { } break;
					case 0x0019: { } break;
					case 0x000B: { continue_block = 0; delay_slot = 1;} break;
					case 0x001B: { continue_block = 0;} break; //Waits for interrupt
					case 0x002B: {used_regs |= REG_NUM_R(15); stored_regs |= REG_NUM_R(15); continue_block = 0; delay_slot = 1;} break;
					default:
						switch (inst & 0xF) {
							case 0b0100: {used_regs |= rn | rm | REG_NUM_R(0); } break;
							case 0b0101: {used_regs |= rn | rm | REG_NUM_R(0); } break;
							case 0b0110: {used_regs |= rn | rm | REG_NUM_R(0); } break;
							case 0b0111: {used_regs |= rn | rm; } break;
							case 0b1100: {used_regs |= rn | rm | REG_NUM_R(0); stored_regs |= rn; } break; //MOVBL0
							case 0b1101: {used_regs |= rn | rm | REG_NUM_R(0); stored_regs |= rn; } break; //MOVWL0
							case 0b1110: {used_regs |= rn | rm | REG_NUM_R(0); stored_regs |= rn; } break; //MOVLL0
							case 0b1111: {used_regs |= rn; stored_regs |= rn; } break; //MACL
							default:
								switch (inst & 0x3F) {
									case 0b000010: {used_regs |= rn; stored_regs |= rn; } break; //STCSR
									case 0b010010: {used_regs |= rn; stored_regs |= rn; } break; //STCGBR
									case 0b000011: {used_regs |= rn; continue_block = 0; delay_slot = 1;} break; //BSRF
									case 0b100010: {used_regs |= rn; stored_regs |= rn; } break; //STCVBR
									case 0b100011: {used_regs |= rn; continue_block = 0; delay_slot = 1;} break; //BRAF
									case 0b101001: {used_regs |= rn; stored_regs |= rn; } break; //MOVT
									case 0b001010: {used_regs |= rn; stored_regs |= rn; } break; //STSMACH
									case 0b011010: {used_regs |= rn; stored_regs |= rn; } break; //STSMACL
									case 0b101010: {used_regs |= rn; stored_regs |= rn; } break; //STSPR
								} break;
						} break;
				}
			} break; //TODO
			case 0b0001: { // MOV.L Rn Rm disp
				used_regs |= rn | rm; //MOVLS4
			} break;
			case 0b0010: {
				switch (inst & 0xF) { // 0010_NNNN_MMMM_XXXX
					case 0b0000: {used_regs |= rn | rm; } break;  //MOVBS
					case 0b0001: {used_regs |= rn | rm; } break;  //MOVWS
					case 0b0010: {used_regs |= rn | rm; } break;  //MOVLS
					case 0b0011: { } break;  //NoOpCode
					case 0b0100: {used_regs |= rn | rm; stored_regs |= rn; } break;  //MOVBM
					case 0b0101: {used_regs |= rn | rm; stored_regs |= rn; } break;  //MOVWM
					case 0b0110: {used_regs |= rn | rm; stored_regs |= rn; } break;  //MOVLM
					case 0b0111: {used_regs |= rn | rm; } break;  //DIV0S
					case 0b1000: {used_regs |= rn | rm; } break;  //TST
					case 0b1001: {used_regs |= rn | rm; stored_regs |= rn; } break;  //AND
					case 0b1010: {used_regs |= rn | rm; stored_regs |= rn; } break;  //XOR
					case 0b1011: {used_regs |= rn | rm; stored_regs |= rn; } break;  //OR
					case 0b1100: {used_regs |= rn | rm; } break;  //CMPSTR
					case 0b1101: {used_regs |= rn | rm; stored_regs |= rn; } break;  //XTRCT
					case 0b1110: {used_regs |= rn | rm; } break;  //MULU
					case 0b1111: {used_regs |= rn | rm; } break;  //MULS
				}
			} break;
			case 0b0011: {
				switch (inst & 0xF) { // 0011_NNNN_MMMM_XXXX
					case 0b0000: {used_regs |= rn | rm; } break;  //CMPEQ
					case 0b0001: { } break;  //NoOpCode
					case 0b0010: {used_regs |= rn | rm; } break;  //CMPHS
					case 0b0011: {used_regs |= rn | rm; } break;  //CMPGE
					case 0b0100: {used_regs |= rn | rm; stored_regs |= rn; } break;  //DIV1
					case 0b0101: {used_regs |= rn | rm; } break;  //DMULU
					case 0b0110: {used_regs |= rn | rm; } break;  //CMPHI
					case 0b0111: {used_regs |= rn | rm; } break;  //CMPGT
					case 0b1000: {used_regs |= rn | rm; stored_regs |= rn; } break;  //SUB
					case 0b1001: { } break;  //NoOpCode
					case 0b1010: {used_regs |= rn | rm; stored_regs |= rn; } break;  //SUBC
					case 0b1011: {used_regs |= rn | rm; stored_regs |= rn; } break;  //SUBV
					case 0b1100: {used_regs |= rn | rm; stored_regs |= rn; } break;  //ADD
					case 0b1101: {used_regs |= rn | rm; } break;  //DMULS
					case 0b1110: {used_regs |= rn | rm; stored_regs |= rn; } break;  //ADDC
					case 0b1111: {used_regs |= rn | rm; stored_regs |= rn; } break;  //ADDV
				}
			} break;
			case 0b0100: {
				switch (inst & 0xF) { // 0100_NNNN_.FX._XXXX
					case 0b000000: {used_regs |= rn; stored_regs |= rn; } break;  //SHLL
					case 0b010000: {used_regs |= rn; stored_regs |= rn; } break;  //DT
					case 0b100000: {used_regs |= rn; stored_regs |= rn; } break;  //SHAL
					case 0b000001: {used_regs |= rn; stored_regs |= rn; } break;  //SHLR
					case 0b010001: {used_regs |= rn; } break;  //CMPPZ
					case 0b100001: {used_regs |= rn; stored_regs |= rn; } break;  //SHAR
					case 0b000010: {used_regs |= rn; stored_regs |= rn; } break;  //STSMMACH
					case 0b010010: {used_regs |= rn; stored_regs |= rn; } break;  //STSMMACL
					case 0b100010: {used_regs |= rn; stored_regs |= rn; } break;  //STSMPR
					case 0b000011: {used_regs |= rn; stored_regs |= rn; } break;  //STCMSR
					case 0b010011: {used_regs |= rn; stored_regs |= rn; } break;  //STCMGBR
					case 0b100011: {used_regs |= rn; stored_regs |= rn; } break;  //STCMVBR
					case 0b000100: {used_regs |= rn; stored_regs |= rn; } break;  //ROTL
					case 0b100100: {used_regs |= rn; stored_regs |= rn; } break;  //ROTCL
					case 0b000101: {used_regs |= rn; stored_regs |= rn; } break;  //ROTR
					case 0b010101: {used_regs |= rn; } break;  //CMPPL
					case 0b100101: {used_regs |= rn; stored_regs |= rn; } break;  //ROTCR
					case 0b000110: {used_regs |= rn; stored_regs |= rn; } break;  //LDSMMACH
					case 0b010110: {used_regs |= rn; stored_regs |= rn; } break;  //LDSMMACL
					case 0b100110: {used_regs |= rn; stored_regs |= rn; } break;  //LDSMPR
					case 0b000111: {used_regs |= rn; stored_regs |= rn; } break;  //LDCMSR
					case 0b010111: {used_regs |= rn; stored_regs |= rn; } break;  //LDCMGBR
					case 0b100111: {used_regs |= rn; stored_regs |= rn; } break;  //LDCMVBR
					case 0b001000: {used_regs |= rn; stored_regs |= rn; } break;  //SHLL2
					case 0b011000: {used_regs |= rn; stored_regs |= rn; } break;  //SHLL8
					case 0b101000: {used_regs |= rn; stored_regs |= rn; } break;  //SHLL16
					case 0b001001: {used_regs |= rn; stored_regs |= rn; } break;  //SHLR2
					case 0b011001: {used_regs |= rn; stored_regs |= rn; } break;  //SHLR8
					case 0b101001: {used_regs |= rn; stored_regs |= rn; } break;  //SHLR16
					case 0b001010: {used_regs |= rn; } break;  //LDSMACH
					case 0b011010: {used_regs |= rn; } break;  //LDSMACL
					case 0b101010: {used_regs |= rn; } break;  //LDSPR
					case 0b001011: {used_regs |= rn; delay_slot = 1; continue_block = 0;} break; //JSR
					case 0b011011: {used_regs |= rn; } break; //TAS
					case 0b101011: {used_regs |= rn; delay_slot = 1; continue_block = 0;} break; //JMP
					case 0b001110: {used_regs |= rn; } break; // LDCSR
					case 0b011110: {used_regs |= rn; } break; // LDCGBR
					case 0b101110: {used_regs |= rn; } break; // LDCVBR
					case 0b001111:
					case 0b011111:
					case 0b101111:
					case 0b111111: {used_regs |= rn | rm; } break; //MACW
					default: { } break; //Illegal Instr
				}
			} break;
			case 0b0101: { used_regs |= rn | rm; stored_regs |= rn; } break; //MOVLL4
			case 0b0110: {
				switch (inst & 0xF) { // 0100_NNNN_.FX._XXXX
					case 0b0000: {used_regs |= rn | rm; stored_regs |= rn; } break;  //MOVBL
					case 0b0001: {used_regs |= rn | rm; stored_regs |= rn; } break;  //MOVWL
					case 0b0010: {used_regs |= rn | rm; stored_regs |= rn; } break;  //MOVLL
					case 0b0011: {used_regs |= rn | rm; stored_regs |= rn; } break;  //MOV
					case 0b0100: {used_regs |= rn | rm; stored_regs |= rn | rm; } break;  //MOVBP
					case 0b0101: {used_regs |= rn | rm; stored_regs |= rn | rm; } break;  //MOVWP
					case 0b0110: {used_regs |= rn | rm; stored_regs |= rn | rm; } break;  //MOVLP
					case 0b0111: {used_regs |= rn | rm; stored_regs |= rn; } break;  //NOT
					case 0b1000: {used_regs |= rn | rm; stored_regs |= rn; } break;  //SWAPB
					case 0b1001: {used_regs |= rn | rm; stored_regs |= rn; } break;  //SWAPW
					case 0b1010: {used_regs |= rn | rm; stored_regs |= rn; } break;  //NEGC
					case 0b1011: {used_regs |= rn | rm; stored_regs |= rn; } break;  //NEG
					case 0b1100: {used_regs |= rn | rm; stored_regs |= rn; } break;  //EXTUB
					case 0b1101: {used_regs |= rn | rm; stored_regs |= rn; } break;  //EXTUW
					case 0b1110: {used_regs |= rn | rm; stored_regs |= rn; } break;  //EXTSB
					case 0b1111: {used_regs |= rn | rm; stored_regs |= rn; } break;  //EXTSW
				}
			} break;
			case 0b0111: { used_regs |= rn; stored_regs |= rn; } break; //ADDI
			case 0b1000: {
				switch ((inst >> 8) & 0xF) { // 0100_XXXX_YYYY_YYYY
					case 0b0000: {used_regs |= rm | REG_NUM_R(0); } break; //MOVBS4
					case 0b0001: {used_regs |= rm | REG_NUM_R(0); } break; //MOVWS4
					case 0b0010: { } break; //NoOpCode
					case 0b0011: { } break; //NoOpCode
					case 0b0100: {used_regs |= rm | REG_NUM_R(0); stored_regs |= REG_NUM_R(0); } break; //MOVBL4
					case 0b0101: {used_regs |= rm | REG_NUM_R(0); stored_regs |= REG_NUM_R(0); } break; //MOVWL4
					case 0b0110: { } break; //NoOpCode
					case 0b0111: { } break; //NoOpCode
					case 0b1000: {used_regs |= REG_NUM_R(0); } break; //CMPIM
					case 0b1001: {continue_block = 0; delay_slot = 1;} break; //BT
					case 0b1010: { } break;
					case 0b1011: {continue_block = 0; delay_slot = 1;} break; //BF
					case 0b1100: { } break;
					case 0b1101: {continue_block = 0; delay_slot = 1;} break; //BTS
					case 0b1110: { } break;
					case 0b1111: {continue_block = 0; delay_slot = 1;} break; //BFS
				}
			} break;
			case 0b1001: { used_regs |= rn; stored_regs |= rn; } break;	//MOVWI
			case 0b1010: { continue_block = 0; delay_slot = 1;} break; //BRA
			case 0b1011: { continue_block = 0; delay_slot = 1;} break; //BSR
			case 0b1100: {
				switch ((inst >> 8) & 0xF) { // 0100_XXXX_YYYY_YYYY
					case 0b0000: {used_regs |= REG_NUM_R(0); } break; //MOVBSG
					case 0b0001: {used_regs |= REG_NUM_R(0); } break; //MOVWSG
					case 0b0010: {used_regs |= REG_NUM_R(0); } break; //MOVLSG
					case 0b0011: {used_regs |= REG_NUM_R(15); stored_regs |= REG_NUM_R(15); continue_block = 0;} break; //TRAPA
					case 0b0100: {used_regs |= REG_NUM_R(0); stored_regs |= REG_NUM_R(0); } break; //MOVBLG
					case 0b0101: {used_regs |= REG_NUM_R(0); stored_regs |= REG_NUM_R(0); } break; //MOVWLG
					case 0b0110: {used_regs |= REG_NUM_R(0); stored_regs |= REG_NUM_R(0); } break; //MOVLLG
					case 0b0111: {used_regs |= REG_NUM_R(0); stored_regs |= REG_NUM_R(0); } break; //MOVA
					case 0b1000: {used_regs |= REG_NUM_R(0); } break; //TSTI
					case 0b1001: {used_regs |= REG_NUM_R(0); } break; //ANDI
					case 0b1010: {used_regs |= REG_NUM_R(0); } break; //XORI
					case 0b1011: {used_regs |= REG_NUM_R(0); } break; //ORI
					case 0b1100: {used_regs |= REG_NUM_R(0); } break; //TSTM
					case 0b1101: {used_regs |= REG_NUM_R(0); } break; //ANDM
					case 0b1110: {used_regs |= REG_NUM_R(0); } break; //XORM
					case 0b1111: {used_regs |= REG_NUM_R(0); } break; //ORM
				}
			} break;
			case 0b1101: { used_regs |= rn; stored_regs |= rn; } break; //MOVLI
			case 0b1110: { used_regs |= rn; stored_regs |= rn; } break; //MOVI
			case 0b1111: { } break;
		}
		++cycles; /* At least one cycle */
		curr_pc += 2;
	}

	*useregs = used_regs | stored_regs;
	*strregs = stored_regs;
	return 0;
}


u32 _jit_GenBlock(u32 addr, Block *iblock)
{
	u32 continue_block = 1, delay_slot = 0;
	u32 curr_pc = addr;
	//Pass 0, check instructions and decode one by one:
	u16 *inst_ptr = (u16*) mem_GetPCAddr(curr_pc);
	u32 last_t_mod = 0; // Instruction
	u32 instr_count = 0;
	u32 cycles = 0;
	u32 *icache_ptr = &drc_code[drc_code_pos];
	u32 reg_indx[32];
	u32 useregs = 0;
	u32 strregs = 0;
	_jit_BlockPass0(addr, &useregs, &strregs);
	printf("used: %X, saved: %X", useregs, strregs);
	PPCC_BEGIN_BLOCK();
	PPCC_MOV(GP_CTX, 3); //SH2 context is in r3
	PPCE_LOAD(GP_SR, sr);
	u32 reg_curr = 30;
	for (u32 i = 0; i < 16; ++i) {
		if ((useregs >> i) & 1) {
			reg_indx[i] = reg_curr;
			PPCE_LOAD(GP_R(i), r[i]);
			reg_curr--;
		}
	}
	//for (u32 i = 0; i < 16; ++i) {
	//	PPCE_LOAD(GP_R(i), r[i]);
	//}
	while (continue_block | delay_slot) {
		u32 inst = *(inst_ptr++); ++instr_count;
		_jit_opcode = inst;
		rn = GP_R((inst >> 8) & 0xF);
		rm = GP_R((inst >> 4) & 0xF);

		if (delay_slot) {
			delay_slot = 0;
		}

		switch ((inst >> 12) & 0xF) {
			case 0b0000: {
				switch(inst) { //Constant instructions
					case 0x0008: {SH2JIT_CLRT; } break;
					case 0x0018: {SH2JIT_SETT; } break;
					case 0x0028: {SH2JIT_CLRMAC; } break;
					case 0x0009: {SH2JIT_NOP; } break;
					case 0x0019: {SH2JIT_DIV0U; } break;
					case 0x000B: {SH2JIT_RTS; cycles+=1; continue_block = 0; delay_slot = 1;} break;
					case 0x001B: {SH2JIT_SLEEP; cycles+=2; continue_block = 0;} break; //Waits for interrupt
					case 0x002B: {SH2JIT_RTE; cycles+=3; continue_block = 0; delay_slot = 1;} break;
					default:
						switch (inst & 0xF) {
							case 0b0100: {SH2JIT_MOVBS0; } break;
							case 0b0101: {SH2JIT_MOVWS0; } break;
							case 0b0110: {SH2JIT_MOVLS0; } break;
							case 0b0111: {SH2JIT_MULL; cycles+=1; } break;
							case 0b1100: {SH2JIT_MOVBL0; } break;
							case 0b1101: {SH2JIT_MOVWL0; } break;
							case 0b1110: {SH2JIT_MOVLL0; } break;
							case 0b1111: {SH2JIT_MACL; cycles+=1; } break;
							default:
								switch (inst & 0x3F) {
									case 0b000010: {SH2JIT_STCSR; } break;
									case 0b010010: {SH2JIT_STCGBR; } break;
									case 0b000011: {SH2JIT_BSRF; cycles+=1; continue_block = 0; delay_slot = 1;} break;
									case 0b100010: {SH2JIT_STCVBR; } break;
									case 0b100011: {SH2JIT_BRAF; cycles+=1; continue_block = 0; delay_slot = 1;} break;
									case 0b101001: {SH2JIT_MOVT; } break;
									case 0b001010: {SH2JIT_STSMACH; } break;
									case 0b011010: {SH2JIT_STSMACL; } break;
									case 0b101010: {SH2JIT_STSPR; } break;
								} break;
						} break;
				}
			} break; //TODO
			case 0b0001: { // MOV.L Rn Rm disp
				SH2JIT_MOVLS4;
			} break;
			case 0b0010: {
				switch (inst & 0xF) { // 0010_NNNN_MMMM_XXXX
					case 0b0000: {SH2JIT_MOVBS; } break;
					case 0b0001: {SH2JIT_MOVWS; } break;
					case 0b0010: {SH2JIT_MOVLS; } break;
					case 0b0011: {SH2JIT_NoOpCode(); } break;
					case 0b0100: {SH2JIT_MOVBM; } break;
					case 0b0101: {SH2JIT_MOVWM; } break;
					case 0b0110: {SH2JIT_MOVLM; } break;
					case 0b0111: {SH2JIT_DIV0S; } break;
					case 0b1000: {SH2JIT_TST; } break;
					case 0b1001: {SH2JIT_AND; } break;
					case 0b1010: {SH2JIT_XOR; } break;
					case 0b1011: {SH2JIT_OR; } break;
					case 0b1100: {SH2JIT_CMPSTR; } break;
					case 0b1101: {SH2JIT_XTRCT; } break;
					case 0b1110: {SH2JIT_MULU; } break;
					case 0b1111: {SH2JIT_MULS; } break;
				}
			} break;
			case 0b0011: {
				switch (inst & 0xF) { // 0011_NNNN_MMMM_XXXX
					case 0b0000: {SH2JIT_CMPEQ; } break;
					case 0b0001: {SH2JIT_NoOpCode(); } break;
					case 0b0010: {SH2JIT_CMPHS; } break;
					case 0b0011: {SH2JIT_CMPGE; } break;
					case 0b0100: {SH2JIT_DIV1; } break;
					case 0b0101: {SH2JIT_DMULU; cycles+=1; } break;
					case 0b0110: {SH2JIT_CMPHI; } break;
					case 0b0111: {SH2JIT_CMPGT; } break;
					case 0b1000: {SH2JIT_SUB; } break;
					case 0b1001: {SH2JIT_NoOpCode(); } break;
					case 0b1010: {SH2JIT_SUBC; } break;
					case 0b1011: {SH2JIT_SUBV; } break;
					case 0b1100: {SH2JIT_ADD; } break;
					case 0b1101: {SH2JIT_DMULS; cycles+=1; } break;
					case 0b1110: {SH2JIT_ADDC; } break;
					case 0b1111: {SH2JIT_ADDV; } break;
				}
			} break;
			case 0b0100: {
				switch (inst & 0xF) { // 0100_NNNN_.FX._XXXX
					case 0b000000: {SH2JIT_SHLL; } break;
					case 0b010000: {SH2JIT_DT; } break;
					case 0b100000: {SH2JIT_SHAL; } break;
					case 0b000001: {SH2JIT_SHLR; } break;
					case 0b010001: {SH2JIT_CMPPZ; } break;
					case 0b100001: {SH2JIT_SHAR; } break;
					case 0b000010: {SH2JIT_STSMMACH; } break;
					case 0b010010: {SH2JIT_STSMMACL; } break;
					case 0b100010: {SH2JIT_STSMPR; } break;
					case 0b000011: {SH2JIT_STCMSR; cycles+=1; } break;
					case 0b010011: {SH2JIT_STCMGBR; cycles+=1; } break;
					case 0b100011: {SH2JIT_STCMVBR; cycles+=1; } break;
					case 0b000100: {SH2JIT_ROTL; } break;
					case 0b100100: {SH2JIT_ROTCL; } break;
					case 0b000101: {SH2JIT_ROTR; } break;
					case 0b010101: {SH2JIT_CMPPL; } break;
					case 0b100101: {SH2JIT_ROTCR; } break;
					case 0b000110: {SH2JIT_LDSMMACH; } break;
					case 0b010110: {SH2JIT_LDSMMACL; } break;
					case 0b100110: {SH2JIT_LDSMPR; } break;
					case 0b000111: {SH2JIT_LDCMSR; cycles+=2; } break;
					case 0b010111: {SH2JIT_LDCMGBR; cycles+=2; } break;
					case 0b100111: {SH2JIT_LDCMVBR; cycles+=2; } break;
					case 0b001000: {SH2JIT_SHLL2; } break;
					case 0b011000: {SH2JIT_SHLL8; } break;
					case 0b101000: {SH2JIT_SHLL16; } break;
					case 0b001001: {SH2JIT_SHLR2; } break;
					case 0b011001: {SH2JIT_SHLR8; } break;
					case 0b101001: {SH2JIT_SHLR16; } break;
					case 0b001010: {SH2JIT_LDSMACH; } break;
					case 0b011010: {SH2JIT_LDSMACL; } break;
					case 0b101010: {SH2JIT_LDSPR; } break;
					case 0b001011: {SH2JIT_JSR; cycles+=1; delay_slot = 1; continue_block = 0;} break;
					case 0b011011: {SH2JIT_TAS; cycles+=4; } break;
					case 0b101011: {SH2JIT_JMP; cycles+=1; delay_slot = 1; continue_block = 0;} break;
					case 0b001110: {SH2JIT_LDCSR; } break;
					case 0b011110: {SH2JIT_LDCGBR; } break;
					case 0b101110: {SH2JIT_LDCVBR; } break;
					case 0b001111:
					case 0b011111:
					case 0b101111:
					case 0b111111: {SH2JIT_MACW; cycles+=1; } break;
					default: {SH2JIT_NoOpCode();} break;
				}
			} break;
			case 0b0101: { SH2JIT_MOVLL4; } break;
			case 0b0110: {
				switch (inst & 0xF) { // 0100_NNNN_.FX._XXXX
					case 0b0000: {SH2JIT_MOVBL; } break;
					case 0b0001: {SH2JIT_MOVWL; } break;
					case 0b0010: {SH2JIT_MOVLL; } break;
					case 0b0011: {SH2JIT_MOV; } break;
					case 0b0100: {SH2JIT_MOVBP; } break;
					case 0b0101: {SH2JIT_MOVWP; } break;
					case 0b0110: {SH2JIT_MOVLP; } break;
					case 0b0111: {SH2JIT_NOT; } break;
					case 0b1000: {SH2JIT_SWAPB; } break;
					case 0b1001: {SH2JIT_SWAPW; } break;
					case 0b1010: {SH2JIT_NEGC; } break;
					case 0b1011: {SH2JIT_NEG; } break;
					case 0b1100: {SH2JIT_EXTUB; } break;
					case 0b1101: {SH2JIT_EXTUW; } break;
					case 0b1110: {SH2JIT_EXTSB; } break;
					case 0b1111: {SH2JIT_EXTSW; } break;
				}
			} break;
			case 0b0111: { SH2JIT_ADDI; } break;
			case 0b1000: {
				switch ((inst >> 8) & 0xF) { // 0100_XXXX_YYYY_YYYY
					case 0b0000: {SH2JIT_MOVBS4; } break;
					case 0b0001: {SH2JIT_MOVWS4; } break;
					case 0b0010: {SH2JIT_NoOpCode(); } break;
					case 0b0011: {SH2JIT_NoOpCode(); } break;
					case 0b0100: {SH2JIT_MOVBL4; } break;
					case 0b0101: {SH2JIT_MOVWL4; } break;
					case 0b0110: {SH2JIT_NoOpCode(); } break;
					case 0b0111: {SH2JIT_NoOpCode(); } break;
					case 0b1000: {SH2JIT_CMPIM; } break;
					case 0b1001: {SH2JIT_BT; continue_block = 0; delay_slot = 1;} break; //TODO: Cycles are weird
					case 0b1010: {SH2JIT_NoOpCode(); } break;
					case 0b1011: {SH2JIT_BF; continue_block = 0; delay_slot = 1;} break; //TODO: Cycles are weird
					case 0b1100: {SH2JIT_NoOpCode(); } break;
					case 0b1101: {SH2JIT_BTS; continue_block = 0; delay_slot = 1;} break; //TODO: Cycles are weird and Delay Slot is weird
					case 0b1110: {SH2JIT_NoOpCode(); } break;
					case 0b1111: {SH2JIT_BFS; continue_block = 0; delay_slot = 1;} break; //TODO: Cycles are weird and Delay Slot is weird
				}
			} break;
			case 0b1001: { SH2JIT_MOVWI; } break;
			case 0b1010: { SH2JIT_BRA; cycles+=1; continue_block = 0; delay_slot = 1;} break;
			case 0b1011: { SH2JIT_BSR; cycles+=1; continue_block = 0; delay_slot = 1;} break;
			case 0b1100: {
				switch ((inst >> 8) & 0xF) { // 0100_XXXX_YYYY_YYYY
					case 0b0000: {SH2JIT_MOVBSG; } break;
					case 0b0001: {SH2JIT_MOVWSG; } break;
					case 0b0010: {SH2JIT_MOVLSG; } break;
					case 0b0011: {SH2JIT_TRAPA; cycles+=8; continue_block = 0;} break;
					case 0b0100: {SH2JIT_MOVBLG; } break;
					case 0b0101: {SH2JIT_MOVWLG; } break;
					case 0b0110: {SH2JIT_MOVLLG; } break;
					case 0b0111: {SH2JIT_MOVA; } break;
					case 0b1000: {SH2JIT_TSTI; } break;
					case 0b1001: {SH2JIT_ANDI; } break;
					case 0b1010: {SH2JIT_XORI; } break;
					case 0b1011: {SH2JIT_ORI; } break;
					case 0b1100: {SH2JIT_TSTM; cycles+=2; } break;
					case 0b1101: {SH2JIT_ANDM; cycles+=2; } break;
					case 0b1110: {SH2JIT_XORM; cycles+=2; } break;
					case 0b1111: {SH2JIT_ORM; cycles+=2; } break;
				}
			} break;
			case 0b1101: { SH2JIT_MOVLI; } break;
			case 0b1110: { SH2JIT_MOVI; } break;
			case 0b1111: { SH2JIT_NoOpCode(); ++cycles; } break;
		}
		++cycles; /* At least one cycle */
		curr_pc += 2;
	}
	//TODO: Save only registers used in block
	//for (u32 i = 0; i < 16; ++i) {
	//	PPCE_SAVE(GP_R(i), r[i]);
	//}
	for (u32 i = 0; i < 16; ++i) {
		if ((strregs >> i) & 1) {
			PPCE_SAVE(GP_R(i), r[i]);
		}
	}
	PPCE_SAVE(GP_SR, sr);

	PPCC_END_BLOCK();

	/* Fill block code */
	iblock->code = (DrcCode) &drc_code[drc_code_pos];
	iblock->ret_addr = 0;
	iblock->start_addr = addr;
	iblock->sh2_len = instr_count;
	iblock->ppc_len = ((u32) icache_ptr - (u32)iblock->code) >> 2;
	iblock->cycle_count = cycles;

	DCFlushRange((void*)(((u32) iblock->code) & ~0x1F), ((iblock->ppc_len * 4) & ~0x1F) + 0x20);
	ICBlockInvalidate((void*)(((u32) iblock->code) & ~0x1F));

	/* Add length of block to drc code position */
	//TODO: Should be aligned 32Bytes?
	drc_code_pos += iblock->ppc_len;
	return 0;
}
#if 0
//box86.org/2024/07/revisiting-the-dynarec/
// Generates a new code block
u32 __GenBlock(u16 pc)
{
	Block drc_block;
	u32 inst_count;
	u32 pc_end; // pc of end of block
	u32 inst_count = BlockPass0(pc, &pc_end);

	//Compute jump destinations and add some barriers
	//Compute predecessors
	u32 size_pred, fill_pred;

	//Propagate the flags to compute
	update_need

	//Clean up dead code...?
	Propagate ymm0s: updateYmm0s..?

	BlockPass1(); // Compute final register allocations

	BlockPass2(); // Compute number of native instructions and addess of all instructions

	//Allocate executable memory for instructions

	BlockPass3(); // Compute final register allocations

	// Copy the drc_block into a new metadata block

	return 0;
}
#endif

void sh2_DrcReset(void)
{
	//Clears all hash code
	HashClearAll();
}

#include <gccore.h>
#include <stdio.h>
#include <ogc/machine/processor.h>

s32 sh2_DrcExec(SH2 *sh, s32 cycles)
{
	//XXX: implement the interpreter
	u32 addr = sh->pc;
	sh->cycles += cycles; //???
	while(cycles > 0) {
		u32 block_found = 0;
		Block *iblock = HashGet(addr, &block_found);
		//printf("MSH2: %p SSH2: %p SH2_CTX: %p\n", &msh2, &ssh2, &sh);
		//printf("IBLOCK: %p\n", iblock);

		if (!block_found) {
			_jit_GenBlock(addr, iblock);
			//printf("IBLOCK Gen: %p %d %d %d %d %d\n",
			//	iblock->code,
			//	iblock->start_addr,
			//	iblock->ret_addr,
			//	iblock->ppc_len,
			//	iblock->sh2_len,
			//	iblock->cycle_count);
		}
		iblock->code(sh);
		addr = sh->pc;
		cycles -= iblock->cycle_count; //XXX: also subtract from delta cycles
	}
	return cycles;
}

