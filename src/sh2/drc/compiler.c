

#include "emit_ppc.inc"
#include "../sh2.h"
#include "compiler.h"
#include "ogc/pad.h"
#include "ogc/video.h"
#include <malloc.h>
#include <stdlib.h>


#define MAX_BLOCK_INST	1024			/* NOTE: change this value eventually */
#define BLOCK_ARR_SIZE	4096*2			//TODO:x16?
#define DRC_CODE_SIZE	1024*1024		/* 4Mb of instructions */

/*Instruction metadata
 * we can get an instruction and store its metadata with information
 * useful for future optimizations:
 * like [T bit used in the future (1bit)][Opcode decodification (8 bits)][is delay slot (1bit)][store regs used (2 bits)][used regs (1 bit)]
 * (more can be added later)*/

//Define all instruction flag codes to store metadata
#define IFC_T_USED				(0x0100)				/* If the T bit generated is used by any subsequent instruction */
#define IFC_ILGL_BRANCH			(0x0200)				/* Instruction is a branch in a delay slot (which is illegal slot) */
#define IFC_IS_DELAY			(0x0400)				/* Instruction is a delay slot instruction */
#define IFC_RLS(rls)			((rls) << 12)			/* If RN needs to be stored */
#define IFC_RLS_GET(ifc)		(((ifc) >> 12) & 0xF)	/* If RN needs to be stored */

enum RegisterLoadStoreCode {
	RLS_NONE,
	RLS_L0,		/* R0 needs to be loaded */
	RLS_L15,	/* R15 needs to be loaded */
	RLS_LN,		/* Rn needs to be loaded */
	RLS_LNM,	/* Rn and Rm need to be loaded */
	RLS_LM0,	/* Rm and R0 need to be loaded */
	RLS_LNM0,	/* Rn, Rm and R0 need to be loaded */
	RLS_LM_S0,	/* Rm needs to be loaded and R0 is stored (and loaded) */
	RLS_LM_SN,	/* Rm needs to be loaded and Rn is stored (and loaded) */
	RLS_LM0_SN,	/* Rm and R0 need to be loaded and Rn is stored (and loaded) */
	RLS_S0,		/* R0 needs to be stored (and loaded) */
	RLS_S15,	/* R15 needs to be stored (and loaded) */
	RLS_SN,		/* Rn needs to be stored (and loaded) */
	RLS_SNM,	/* Rn and Rm need to be stored (and loaded) */
};


enum InstFlagCode {
	IFC_ILLEGAL,
	//Algebraic/Logical
	IFC_ADD,   IFC_ADDI,   IFC_ADDC,  IFC_ADDV,
	IFC_SUB,   IFC_SUBC,   IFC_SUBV,  IFC_AND,
	IFC_ANDI,  IFC_ANDM,   IFC_OR,    IFC_ORI,
	IFC_ORM,   IFC_XOR,    IFC_XORI,  IFC_XORM,
	IFC_ROTCL, IFC_ROTCR,  IFC_ROTL,  IFC_ROTR,
	IFC_SHAL,  IFC_SHAR,   IFC_SHLL,  IFC_SHLL2,
	IFC_SHLL8, IFC_SHLL16, IFC_SHLR,  IFC_SHLR2,
	IFC_SHLR8, IFC_SHLR16, IFC_NOT,   IFC_NEG,
	IFC_NEGC,  IFC_DT,     IFC_EXTSB, IFC_EXTSW,
	IFC_EXTUB, IFC_EXTUW,
	//Mult and Division
	IFC_DIV0S, IFC_DIV0U,  IFC_DIV1,  IFC_DMULS,
	IFC_DMULU, IFC_MACL,   IFC_MACW,  IFC_MULL,
	IFC_MULS,  IFC_MULU,
	//Set and Clear
	IFC_CLRMAC, IFC_CLRT, IFC_SETT,
	//Compare
	IFC_CMPEQ, IFC_CMPGE, IFC_CMPGT,  IFC_CMPHI,
	IFC_CMPHS, IFC_CMPPL, IFC_CMPPZ,  IFC_CMPSTR,
	IFC_CMPIM,
	//Load and Store
	IFC_LDCSR,   IFC_LDCGBR,   IFC_LDCVBR,   IFC_LDCMSR,
	IFC_LDCMGBR, IFC_LDCMVBR,  IFC_LDSMACH,  IFC_LDSMACL,
	IFC_LDSPR,   IFC_LDSMMACH, IFC_LDSMMACL, IFC_LDSMPR,
	IFC_STCSR,   IFC_STCGBR,   IFC_STCVBR,   IFC_STCMSR,
	IFC_STCMGBR, IFC_STCMVBR,  IFC_STSMACH,  IFC_STSMACL,
	IFC_STSPR,   IFC_STSMMACH, IFC_STSMMACL, IFC_STSMPR,
	//Move Data
	IFC_MOV,    IFC_MOVBS,  IFC_MOVWS,  IFC_MOVLS,
	IFC_MOVBL,  IFC_MOVWL,  IFC_MOVLL,  IFC_MOVBM,
	IFC_MOVWM,  IFC_MOVLM,  IFC_MOVBP,  IFC_MOVWP,
	IFC_MOVLP,  IFC_MOVBS0, IFC_MOVWS0, IFC_MOVLS0,
	IFC_MOVBL0, IFC_MOVWL0, IFC_MOVLL0, IFC_MOVI,
	IFC_MOVWI,  IFC_MOVLI,  IFC_MOVBLG, IFC_MOVWLG,
	IFC_MOVLLG, IFC_MOVBSG, IFC_MOVWSG, IFC_MOVLSG,
	IFC_MOVBS4, IFC_MOVWS4, IFC_MOVLS4, IFC_MOVBL4,
	IFC_MOVWL4, IFC_MOVLL4, IFC_MOVA,   IFC_MOVT,
	//Branch and Jumps
	IFC_BF,  IFC_BFS,  IFC_BRA, IFC_BRAF,
	IFC_BSR, IFC_BSRF, IFC_BT,  IFC_BTS,
	IFC_JMP, IFC_JSR,  IFC_RTE, IFC_RTS,
	//Other
	IFC_NOP,  IFC_SLEEP, IFC_SWAPB, IFC_SWAPW,
	IFC_TAS,  IFC_TRAPA, IFC_TST,   IFC_TSTI,
	IFC_TSTM, IFC_XTRCT
};

typedef void (*DrcCode)(void *shctx);

typedef struct Block_t {
	DrcCode code;		/*  */

	u32 ret_addr; 		/* If return is a constant address (hashed) */
	u32 start_addr; 	/* Address of original code */
	u32 sh2_len;		/* Number of original code instructions //UNUSED*/
	u32 ppc_len;		/* Number of native code instructions */
	//TODO: this can be returned by the block
	u32 cycle_count;	/* Number of base block cycles */
	//u32 flags;		//Ends in sleep or if return address is constant
} Block;

extern SH2 *sh_ctx;

//u32 drc_code[DRC_CODE_SIZE] ATTRIBUTE_ALIGN(32);
//Block drc_blocks[BLOCK_ARR_SIZE] ATTRIBUTE_ALIGN(32);
u32 *drc_code;
Block *drc_blocks;
u32 ifc_array[4*1024] ATTRIBUTE_ALIGN(32);	//8k instructions
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
	//key >>= 6; // Terrible implementation
#endif
	return (key >> 2);
}


Block* HashGet(u32 key, u32 *found)
{
	u32 i = __GetDrcHash(key & 0x07FFFFFF) & (BLOCK_ARR_SIZE - 1);
	u32 count = 0;
	//No more blocks can be generated, start over
	if (drc_blocks_size == BLOCK_ARR_SIZE - 1) {
		HashClearAll();
	}
	while (drc_blocks[i].start_addr != -1) {
		// do linear probing
		if (drc_blocks[i].start_addr == key) {
			*found = 1;
			return &drc_blocks[i];
		}
		i = (i + 1) & (BLOCK_ARR_SIZE - 1);
	}
	drc_blocks_size++;
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


#define SH2JIT_ADD			/* ADD Rm,Rn  0011nnnnmmmm1100 */ \
	PPCC_ADD(rn, rn, rm);


#define SH2JIT_ADDI			/* ADD #imm,Rn  0111nnnniiiiiiii */ \
	if((imm & 0xff) != 0) {PPCC_ADDI(rn, rn, EXT_IMM8(imm));}


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



u32 mult = 0;
/*Mult and Division*/
#define SH2JIT_DIV0S /* DIV0S Rm,Rn  0010nnnnmmmm0111 */ \
	PPCC_XOR(GP_TMP, rn, rm);					/*Q ^ M*/ \
	PPCC_RLWIMI(GP_SR, rn, 8+1, 31-8, 31-8);	/*Store MSB of Rn as Q bit*/ \
	PPCC_RLWIMI(GP_SR, rm, 9+1, 31-9, 31-9);	/*Store MSB of Rm as M bit*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 1, 31, 31);		/*Store Q ^ M as T bit*/ \
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
	PPCC_MFXER(GP_TMP2); 						/* Load XER[CA] to TMP2 (Reuse) */ \
	PPCC_RLWIMI(GP_TMP2, GP_TMP2, 3, 31, 31);	 \
	PPCC_XOR(GP_TMP, GP_TMP, GP_TMP2);			 \
	/* Q = M ^ MSB */							 \
	PPCC_XOR(3, 3, GP_TMP);						 \
	PPCC_RLWIMI(GP_SR, 3, 8, 31-8, 31-8);		/* Store Q bit*/ \
	/* T = !MSB */								 \
	PPCC_NOR(GP_TMP, GP_TMP, GP_TMP);			 \
	PPCC_RLWIMI(GP_SR, GP_TMP, 0, 31, 31);		/* Store T bit*/ \


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
	PPCC_RLWINM(GP_TMP, GP_SR, 15, 15, 15); 	/* saturate if S bit == 1 */ \
	PPCC_NEG(GP_TMP, GP_TMP);					/*Mask from S bit (active when T=1)*/ \
	PPCC_AND(GP_TMP, GP_TMP, GP_MACH);			/*Mask off offset */ \
	PPCC_EXTSH(GP_TMP, GP_TMP); 				/* MACH & -(MACH & 0x0s00) */ \
	PPCC_OR(GP_MACH, GP_MACH, GP_TMP);			\
	PPCE_SAVE(GP_MACH, mach);					\
	PPCE_SAVE(GP_MACL, macl);					\


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
	PPCC_MULLW(GP_TMP, rn, rm);				 \
	PPCE_SAVE(GP_TMP, macl);					 \


#define SH2JIT_MULS			/* MULS Rm,Rn  0010nnnnmmmm1111 */ \
	PPCC_EXTSH(GP_TMP, rn);					/* (s16) Rn  */ \
	PPCC_EXTSH(GP_TMP2, rm);				/* (s16) Rm  */ \
	PPCC_MULLW(GP_TMP, GP_TMP, GP_TMP2);	/* Rn * Rm -> MACL */ \
	PPCE_SAVE(GP_TMP, macl);				 \


#define SH2JIT_MULU			/* MULU Rm,Rn  0010nnnnmmmm1110 */ \
	PPCC_ANDI(GP_TMP, rn, 0xFFFF);			/* (u16) Rn  */ \
	PPCC_ANDI(GP_TMP2, rm, 0xFFFF);			/* (u16) Rm  */ \
	PPCC_MULLW(GP_TMP, GP_TMP, GP_TMP2);	/* Rn * Rm -> MACL */ \
	PPCE_SAVE(GP_TMP, macl);				 \



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
	PPCE_SAVE(rn, gbr);			/*Rn -> Save GBR*/ \


#define SH2JIT_LDCVBR		/* LDC Rm,VBR  0100mmmm00101110 */ \
	PPCE_SAVE(rn, vbr);			/*Rn -> Save VBR*/ \


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
	PPCC_BL(sh2_Read8);			/*Read 8 bit value*/ \
	PPCC_EXTSB(rn, 3);			/*EXT8(read value) -> Rn*/


#define SH2JIT_MOVWL \
	PPCC_ORI(3, rm, 0x0);		/*Rm -> address*/ \
	PPCC_BL(sh2_Read16);		/*Read 16 bit value*/ \
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
	PPCC_BL(sh2_Read8);		/*Read 8 bit value*/ \
	PPCC_EXTSB(rn, 3);			/*EXT8(read value) -> Rn*/ \
	if (rn != rm) { PPCC_ADDI(rm, rm, 1); }		/*increment 4 to Rn*/


#define SH2JIT_MOVWP \
	PPCC_ORI(3, rm, 0x0);		/*Rm -> address*/ \
	PPCC_BL(sh2_Read16);		/*Read 16 bit value*/ \
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
	PPCC_BL(sh2_Read8);			/*Read 8 bit value*/ \
	PPCC_EXTSB(rn, 3);			/*EXT8(read value) -> Rn*/


#define SH2JIT_MOVWL0 \
	PPCC_ADD(3, rm, GP_R0);		/*R0+Rm -> address*/ \
	PPCC_BL(sh2_Read16);		/*Read 16 bit value*/ \
	PPCC_EXTSH(rn, 3);			/*EXT16(read value) -> Rn*/


#define SH2JIT_MOVLL0 \
	PPCC_ADD(3, rm, GP_R0);		/*R0+Rm -> address*/ \
	PPCC_BL(sh2_Read32);		/*Read 32 bit value*/ \
	PPCC_MOV(rn, 3);			/*read value -> Rn*/


#define SH2JIT_MOVI /* MOV #imm,Rn  1110nnnniiiiiiii */ \
	PPCC_ADDI(rn, 0, EXT_IMM8(imm));	//NOTE: Must sign extend immidiate


#define SH2JIT_MOVWI /* MOV.W @(disp,PC),Rn  1001nnnndddddddd */ \
	u32 iaddr = (curr_pc+4) + ((disp & 0xFF) << 1); \
	PPCC_ADDIS(3, 0, (iaddr >> 16));		/*Set high imm 16 bits */ \
	PPCC_ORI(3, 3, iaddr);				/*Set low imm 16 bits */ \
	PPCC_BL(sh2_Read16);				/*Read 16 bit value*/ \
	PPCC_EXTSH(rn, 3);					/*Extend s16*/
	//XXX: the address is constant so we can ignore sh2_Read16 and go straigth to its handling function
	//TODO: Check for delay-slot

#define SH2JIT_MOVLI /* MOV.L @(disp,PC),Rn  1101nnnndddddddd */ \
	u32 iaddr = ((curr_pc) & 0xFFFFFFFC) + 4 + ((disp & 0xFF) << 2); \
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
	u32 iaddr = ((curr_pc+4) & 0xFFFFFFFC) + ((disp & 0xFF) << 2); \
	PPCC_ADDIS(GP_R0, 0, (iaddr >> 16));			/*Set high imm 16 bits */ \
	PPCC_ORI(GP_R0, GP_R0, iaddr);				/*Set low imm 16 bits */
	//TODO: Check for delay-slot


#define SH2JIT_MOVT /* MOVT Rn  0000nnnn00101001 */ \
	PPCC_ANDI(rn, GP_SR, 0x0001);


/*Branch and Jumps*/
#define SH2JIT_BF			/* BF disp  10001011dddddddd */ \
	u32 offset = (EXT_IMM8(disp) << 1) + 2; \
	PPCC_ADDIS(GP_PC, 0, (curr_pc+2) >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, curr_pc+2);		/*Set low imm 16 bits */ \
	PPCC_ANDI(GP_TMP, GP_SR, 0x0001);		/*Get T bit */ \
	PPCC_ADDI(GP_TMP, GP_TMP, -1);			/*Mask from T bit (active when T=0)*/ \
	PPCC_ANDI(GP_TMP, GP_TMP, offset);		/*Mask off offset */ \
	PPCC_EXTSH(GP_TMP, GP_TMP);				/*Extend to 32bits */ \
	PPCC_ADD(GP_PC, GP_PC, GP_TMP);			/*Add offset and store in ret value */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \


#define SH2JIT_BFS		/* BFS disp  10001111dddddddd */ \
	u32 offset = (EXT_IMM8(disp) << 1); \
	PPCC_ADDIS(GP_PC, 0, (curr_pc+4) >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, curr_pc+4);		/*Set low imm 16 bits */ \
	PPCC_ANDI(GP_TMP, GP_SR, 0x0001);		/*Get T bit */ \
	PPCC_ADDI(GP_TMP, GP_TMP, -1);			/*Mask from T bit (active when T=0)*/ \
	PPCC_ANDI(GP_TMP, GP_TMP, offset);		/*Mask off offset */ \
	PPCC_EXTSH(GP_TMP, GP_TMP);				/*Extend to 32bits */ \
	PPCC_ADD(GP_PC, GP_PC, GP_TMP);			/*Add offset and store in ret value */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \


#define SH2JIT_BRA			/* BRA disp  1010dddddddddddd */ \
	u32 new_pc = curr_pc + (EXT_IMM12(disp) << 1) + 4; \
	PPCC_ADDIS(GP_PC, 0, new_pc >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, new_pc);		/*Set low imm 16 bits */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \

#define SH2JIT_BRAF						/* BRAF Rm  0000mmmm00100011 */ \
	u32 new_pc = curr_pc + 4; 				\
	PPCC_ADDIS(GP_PC, 0, new_pc >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, new_pc);		/*Set low imm 16 bits */ \
	PPCC_ADD(GP_PC, GP_PC, rn)				/*Add Rn to PC */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \

#define SH2JIT_BSR			/* BSR disp  1011dddddddddddd */ \
	u32 new_pc = curr_pc + 4; 				\
	u32 offset = (EXT_IMM12(disp) << 1);    \
	PPCC_ADDIS(GP_PC, 0, new_pc >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PR, GP_PC, new_pc);		/*Set low imm 16 bits (PR <- PC) */ \
	PPCE_SAVE(GP_PR, pr);					/* Save PR */ \
	PPCC_ADDI(GP_PC, GP_PR, offset)			/* PC <- PC + Ext(offset)*/ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \

#define SH2JIT_BSRF			/* BSRF Rm  0000mmmm00000011 */ \
	u32 new_pc = curr_pc + 4; 				\
	PPCC_ADDIS(GP_TMP, 0, new_pc >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_TMP, GP_TMP, new_pc);		/*Set low imm 16 bits (PR <- PC) */ \
	PPCE_SAVE(GP_TMP, pr);					/* Save PR */ \
	PPCC_ADD(GP_PC, GP_TMP, rn)				/* PC <- Rn + PC */ \
	PPCE_SAVE(GP_PC, pc);					/* Save PC */ \


#define SH2JIT_BT			/* BT disp  10001001dddddddd */ \
	u32 offset = (EXT_IMM8(disp) << 1) + 2; \
	PPCC_ADDIS(GP_PC, 0, (curr_pc+2) >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, curr_pc+2);			/*Set low imm 16 bits */ \
	PPCC_ANDI(GP_TMP, GP_SR, 0x0001);			/*Get T bit */ \
	PPCC_NEG(GP_TMP, GP_TMP);					/*Mask from T bit (active when T=1)*/ \
	PPCC_ANDI(GP_TMP, GP_TMP, offset);			/*Mask off offset */ \
	PPCC_EXTSH(GP_TMP, GP_TMP);					/*Extend to 32bits */ \
	PPCC_ADD(GP_PC, GP_PC, GP_TMP);				/*Add offset and store in ret value */ \
	PPCE_SAVE(GP_PC, pc);						/* Save PC */ \


#define SH2JIT_BTS			/* BTS disp  10001101dddddddd */ \
	u32 offset = (EXT_IMM8(disp) << 1); 		\
	PPCC_ADDIS(GP_PC, 0, (curr_pc+4) >> 16);	/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, curr_pc+4);			/*Set low imm 16 bits */ \
	PPCC_ANDI(GP_TMP, GP_SR, 0x0001);			/*Get T bit */ \
	PPCC_NEG(GP_TMP, GP_TMP);					/*Mask from T bit (active when T=1)*/ \
	PPCC_ANDI(GP_TMP, GP_TMP, offset);			/*Mask off offset */ \
	PPCC_EXTSH(GP_TMP, GP_TMP);					/*Extend to 32bits */ \
	PPCC_ADD(GP_PC, GP_PC, GP_TMP);				/*Add offset and store in ret value */ \
	PPCE_SAVE(GP_PC, pc);						/* Save PC */ \

#define SH2JIT_JMP			/* JMP @Rm  0100mmmm00101011 */ \
	PPCE_SAVE(rn, pc);			/* Rm(Rn here) -> Save PC */


#define SH2JIT_JSR			/* JSR @Rm  0100mmmm00001011 */ \
	u32 new_pc = curr_pc + 4;  \
	PPCC_ADDIS(GP_TMP, 0, new_pc >> 16);	/*Set high imm 16 bits (PR = PC)*/ \
	PPCC_ORI(GP_TMP, GP_TMP, new_pc);		/*Set low imm 16 bits (PR = PC)*/ \
	PPCE_SAVE(GP_TMP, pr);					/* Save PR = PC */ \
	PPCE_SAVE(rn, pc);						/* Rm(Rn here) -> Save PC */


#define SH2JIT_RTE 			/* RTE  0000000000101011 */ \
	PPCC_MOV(3, GP_R(15));					/*R15 -> address*/ \
	PPCC_BL(sh2_Read32);					/*Read 32 bit value*/ \
	PPCE_SAVE(3, pc);						/* @(R15) -> Save PC */ \
	PPCC_ADDI(3, GP_R(15), 0x4);			/*R15 + 4 -> address*/ \
	PPCC_BL(sh2_Read32);					/*Read 32 bit value*/ \
	PPCC_ANDI(GP_SR, 3, 0x3F3);				/*Result & 0x03F3 -> SR*/ \
	PPCC_ADDI(GP_R(15), GP_R(15), 0x8);		/*R15 + 4 -> address*/ \



#define SH2JIT_RTS 			/* RTS  0000000000001011 */ \
	PPCE_LOAD(GP_TMP, pr);				/* Loas PR */ \
	PPCE_SAVE(GP_TMP, pc);				/* PR -> Save PC */



/*Other*/
#define SH2JIT_NOP 			/* NOP  0000000000001001 */
	/*Does nothing*/


#define SH2JIT_SLEEP 		/* SLEEP  0000000000011011 */ \
	PPCC_ADDIS(GP_PC, 0, curr_pc >> 16);		/*Set high imm 16 bits */ \
	PPCC_ORI(GP_PC, GP_PC, curr_pc);			/*Set low imm 16 bits */ \
	PPCE_SAVE(GP_PC, pc);						/* Save PC */
	/*TODO: Does nothing else ?...*/


#define SH2JIT_SWAPB 		/* SWAP.B Rm,Rn  0110nnnnmmmm1000 */ \
	PPCC_MOV(GP_TMP, rm);					/*Store in temporal reg */ \
	PPCC_RLWIMI(GP_TMP, rm, 8, 16, 24);		/*shift to left lower 8 bits*/ \
	PPCC_RLWIMI(GP_TMP, rm, 32-8, 24, 31);	/*shift to right higher 8 bits*/ \
	PPCC_MOV(rn, GP_TMP);


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
	PPCC_ADDI(3, GP_R(15), -4);				/*R15 - 4 -> address*/ \
	PPCC_MOV(4, GP_SR);						/*SR -> value*/ \
	PPCC_BL(sh2_Write32);					/*Write 32bit value*/ \
	PPCC_ADDI(3, GP_R(15), -8);				/*R15 - 8 -> address*/ \
	PPCC_ADDIS(4, 0, (curr_pc+2) >> 16);	/*Load PC-2 immediate -> value*/ \
	PPCC_ORI(4, 4, curr_pc+2);		 		\
	PPCC_BL(sh2_Write32);					/*Write 32 bit value*/ \
	PPCC_ADDI(GP_R(15), GP_R(15), -8);		/*R15 - 8 -> address*/ \
	PPCE_LOAD(GP_VBR, vbr);					 \
	PPCC_ADDI(3, GP_VBR, dispimm);			/* VBR + dispimm -> address*/ \
	PPCC_BL(sh2_Read32);					/*Read addr*/ \
	/* PPCC_MOV(GP_PC, 3); */				/*Result -> PC*/ \
	PPCE_SAVE(3, pc);						/* Save PC */


#define SH2JIT_TST			/* TST Rm,Rn  0010nnnnmmmm1000 */ \
	PPCC_ANDp(GP_TMP, rn, rm);					/*Rn AND Rm and check if zero*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store CR0[EQ] (Zero) in T*/


#define SH2JIT_TSTI			/* TEST #imm,R0  11001000iiiiiiii */ \
	PPCC_ANDI(GP_TMP, GP_R0, imm & 0xFF);	/*R0 AND imm and check if zero*/ \
	PPCC_MFCR(GP_TMP); 						/*Load CR to TMP*/ \
	PPCC_RLWIMI(GP_SR, GP_TMP, 3, 31, 31); 	/*Store EQ (Zero) in T*/


#define SH2JIT_TSTM			/* TST.B #imm,@(R0,GBR)  11001100iiiiiiii */ \
	PPCE_LOAD(GP_GBR, gbr);					/* Fetch GBR */ \
	PPCC_ADD(3, GP_R0, GP_GBR);				/*GBR+R0 -> addr (Reg3)*/ \
	PPCC_BL(sh2_Read8);						/*Read byte from addr (Reg3) -> byte in Reg3*/ \
	PPCC_ANDI(3, 3, imm & 0xFF);			/*Reg3 AND imm and check if zero*/ \
	PPCC_MFCR(3); 							/*Load CR0 to Reg3*/ \
	PPCC_RLWIMI(GP_SR, 3, 3, 31, 31); 		/*Store CR0[EQ] (Zero) in T*/
	//NOTE: Uses Reg3 for TMP

#define SH2JIT_XTRCT		/* XTRCT Rm,Rn  0010nnnnmmmm1101 */ \
	PPCC_RLWINM(GP_TMP, rn, 16, 16, 31); 	/*Shift Rn*/ \
	PPCC_RLWIMI(GP_TMP, rm, 16, 0, 15); 	/*Store shifted Rm in Rn*/ \
	PPCC_MOV(rn, GP_TMP); 	/*Store shifted Rm in Rn*/
	//XXX: Maybe could be done in two instructions.

#define SH2JIT_ILLEGAL		/* Illegal instruciton handler */ \
	u32 vector = (4 + ((ifc_array[i] & IFC_ILGL_BRANCH) >> 8)) << 2;	/*4 if only illegal 6 if branch value*/	\
	PPCC_ADDI(3, GP_R(15), -4);			/*R15 - 4 -> address*/ \
	PPCC_MOV(4, GP_SR);					/*SR -> value*/ \
	PPCC_BL(sh2_Write32);				/*Write 32bit value*/ \
	PPCC_ADDI(3, GP_R(15), -8);			/*R15 - 8 -> address*/ \
	PPCC_ADDIS(4, 0, (curr_pc+2) >> 16);	/*Load PC+2 immediate -> value*/ \
	PPCC_ORI(4, 4, curr_pc+2);		 	\
	PPCC_BL(sh2_Write32);				/*Write 32 bit value*/ \
	PPCC_ADDI(GP_R(15), GP_R(15), -8);	/*R15 - 8 -> address*/ \
	PPCE_LOAD(GP_VBR, vbr);				 \
	PPCC_ADDI(3, GP_VBR, vector);		/* VBR + dispimm -> address*/ \
	PPCC_BL(sh2_Read32);				/*Read addr*/ \
	/* PPCC_MOV(GP_PC, 3); */			/*Result -> PC*/ \
	PPCE_SAVE(3, pc);				/* Save PC */



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


#define BSTATE_END			0
#define BSTATE_CONT			1
#define BSTATE_IN_DELAY		2
#define BSTATE_SET_DELAY	3

#define REG_NUM_R(n)		(1 << (n))
#define REG_NUM_PR			(1 << 17)
#define REG_NUM_GBR			(1 << 18)
#define REG_NUM_VBR			(1 << 19)
#define REG_NUM_MACH		(1 << 20)
#define REG_NUM_MACL		(1 << 21)
#define REG_NUM_SR			(1 << 22)

struct BlockData {
	u32 ld_regs;
	u32 st_regs;
	u32 instr_count;
	u32 cycle_count;
} block_data;


//This function generates block of metadata for subsequent passes
u32 _jit_GenIFCBlock(u32 addr)
{
	u32 bstate = BSTATE_CONT;
	u32 curr_pc = addr;
	//Pass 0, check instructions and decode one by one:
	u16 *inst_ptr = (u16*) sh2_GetPCAddr(curr_pc);
	//u32 last_t_mod = 0; // Instruction
	u32 instr_count = 0;
	u32 cycles = 0;
	u32 ld_regs = REG_NUM_SR;	// Allways have SR loaded
	u32 st_regs = REG_NUM_SR;	// Allways have SR loaded
	while (bstate != BSTATE_END) {
		u32 inst = *(inst_ptr++);
		u32 ifc = 0;
		_jit_opcode = inst;

		//TODO!!!!:
		// INSTRUCTIONS THAT SET THE DELAY TO A BRANCH INSTRUCTION USE A ILLEGAL SLOT INSTRUCTION
		//They are BF BT BFS BTS BRA BRAF BSR BSRF JMP JRE RTS
		switch ((inst >> 12) & 0xF) {
			case 0b0000: {
			switch(inst) { //Constant instructions
				case 0x0008: {ifc = IFC_CLRT; } break;
				case 0x0018: {ifc = IFC_SETT; } break;
				case 0x0028: {ifc = IFC_CLRMAC; } break;
				case 0x0009: {ifc = IFC_NOP; } break;
				case 0x0019: {ifc = IFC_DIV0U; } break;
				case 0x000B: {ifc = IFC_RTS; cycles+=1; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break;
				case 0x001B: {ifc = IFC_SLEEP; cycles+=2; bstate = BSTATE_END;} break; //Waits for interrupt
				case 0x002B: {ifc = IFC_RTE | IFC_RLS(RLS_S15); cycles+=3; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break;
				default:
				switch (inst & 0xF) {
					case 0b0100: {ifc = IFC_MOVBS0 | IFC_RLS(RLS_LNM0); } break;
					case 0b0101: {ifc = IFC_MOVWS0 | IFC_RLS(RLS_LNM0); } break;
					case 0b0110: {ifc = IFC_MOVLS0 | IFC_RLS(RLS_LNM0); } break;
					case 0b0111: {ifc = IFC_MULL   | IFC_RLS(RLS_LNM); cycles+=1; } break;
					case 0b1100: {ifc = IFC_MOVBL0 | IFC_RLS(RLS_LM0_SN); } break;
					case 0b1101: {ifc = IFC_MOVWL0 | IFC_RLS(RLS_LM0_SN); } break;
					case 0b1110: {ifc = IFC_MOVLL0 | IFC_RLS(RLS_LM0_SN); } break;
					case 0b1111: {ifc = IFC_MACL   | IFC_RLS(RLS_SN); cycles+=1; } break;
					default:
					switch (inst & 0x3F) {
						case 0b000010: {ifc = IFC_STCSR   | IFC_RLS(RLS_SN); } break;
						case 0b010010: {ifc = IFC_STCGBR  | IFC_RLS(RLS_SN); } break;
						case 0b000011: {ifc = IFC_BSRF    | IFC_RLS(RLS_LN); cycles+=1; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break;
						case 0b100010: {ifc = IFC_STCVBR  | IFC_RLS(RLS_SN); } break;
						case 0b100011: {ifc = IFC_BRAF    | IFC_RLS(RLS_LN); cycles+=1; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break;
						case 0b101001: {ifc = IFC_MOVT    | IFC_RLS(RLS_SN); } break;
						case 0b001010: {ifc = IFC_STSMACH | IFC_RLS(RLS_SN); } break;
						case 0b011010: {ifc = IFC_STSMACL | IFC_RLS(RLS_SN); } break;
						case 0b101010: {ifc = IFC_STSPR   | IFC_RLS(RLS_SN); } break;
						default:       {ifc = IFC_ILLEGAL | IFC_RLS(RLS_S15); bstate = BSTATE_END;} break;
					} break;
				} break;
			}} break;
			case 0b0001: { // MOV.L Rn Rm disp
				ifc = IFC_MOVLS4 | IFC_RLS(RLS_LNM);
			} break;
			case 0b0010: {
			switch (inst & 0xF) { // 0010_NNNN_MMMM_XXXX
				case 0b0000: {ifc = IFC_MOVBS   | IFC_RLS(RLS_LNM); } break;
				case 0b0001: {ifc = IFC_MOVWS   | IFC_RLS(RLS_LNM); } break;
				case 0b0010: {ifc = IFC_MOVLS   | IFC_RLS(RLS_LNM); } break;
				case 0b0011: {ifc = IFC_ILLEGAL | IFC_RLS(RLS_S15); bstate = BSTATE_END;} break;
				case 0b0100: {ifc = IFC_MOVBM   | IFC_RLS(RLS_LM_SN); } break;
				case 0b0101: {ifc = IFC_MOVWM   | IFC_RLS(RLS_LM_SN); } break;
				case 0b0110: {ifc = IFC_MOVLM   | IFC_RLS(RLS_LM_SN); } break;
				case 0b0111: {ifc = IFC_DIV0S   | IFC_RLS(RLS_LNM); } break;
				case 0b1000: {ifc = IFC_TST     | IFC_RLS(RLS_LNM); } break;
				case 0b1001: {ifc = IFC_AND     | IFC_RLS(RLS_LM_SN); } break;
				case 0b1010: {ifc = IFC_XOR     | IFC_RLS(RLS_LM_SN); } break;
				case 0b1011: {ifc = IFC_OR      | IFC_RLS(RLS_LM_SN); } break;
				case 0b1100: {ifc = IFC_CMPSTR  | IFC_RLS(RLS_LNM); } break;
				case 0b1101: {ifc = IFC_XTRCT   | IFC_RLS(RLS_LM_SN); } break;
				case 0b1110: {ifc = IFC_MULU    |  IFC_RLS(RLS_LNM); } break;
				case 0b1111: {ifc = IFC_MULS    | IFC_RLS(RLS_LNM); } break;
			}} break;
			case 0b0011: {
			switch (inst & 0xF) { // 0011_NNNN_MMMM_XXXX
				case 0b0000: {ifc = IFC_CMPEQ   | IFC_RLS(RLS_LNM); } break;
				case 0b0010: {ifc = IFC_CMPHS   | IFC_RLS(RLS_LNM); } break;
				case 0b0011: {ifc = IFC_CMPGE   | IFC_RLS(RLS_LNM); } break;
				case 0b0100: {ifc = IFC_DIV1    | IFC_RLS(RLS_LM_SN); } break;
				case 0b0101: {ifc = IFC_DMULU   | IFC_RLS(RLS_LNM); cycles+=1; } break;
				case 0b0110: {ifc = IFC_CMPHI   | IFC_RLS(RLS_LNM); } break;
				case 0b0111: {ifc = IFC_CMPGT   | IFC_RLS(RLS_LNM); } break;
				case 0b1000: {ifc = IFC_SUB     | IFC_RLS(RLS_LM_SN); } break;
				case 0b1010: {ifc = IFC_SUBC    | IFC_RLS(RLS_LM_SN); } break;
				case 0b1011: {ifc = IFC_SUBV    | IFC_RLS(RLS_LM_SN); } break;
				case 0b1100: {ifc = IFC_ADD     | IFC_RLS(RLS_LM_SN); } break;
				case 0b1101: {ifc = IFC_DMULS   | IFC_RLS(RLS_LNM); cycles+=1; } break;
				case 0b1110: {ifc = IFC_ADDC    | IFC_RLS(RLS_LM_SN); } break;
				case 0b1111: {ifc = IFC_ADDV    | IFC_RLS(RLS_LM_SN); } break;
				default:     {ifc = IFC_ILLEGAL | IFC_RLS(RLS_S15); bstate = BSTATE_END;} break;
			}} break;
			case 0b0100: {
			switch (inst & 0x3F) { // 0100_NNNN_.FX._XXXX
				case 0b000000: {ifc = IFC_SHLL     | IFC_RLS(RLS_SN); } break;
				case 0b010000: {ifc = IFC_DT       | IFC_RLS(RLS_SN); } break;
				case 0b100000: {ifc = IFC_SHAL     | IFC_RLS(RLS_SN); } break;
				case 0b000001: {ifc = IFC_SHLR     | IFC_RLS(RLS_SN); } break;
				case 0b010001: {ifc = IFC_CMPPZ    | IFC_RLS(RLS_LN); } break;
				case 0b100001: {ifc = IFC_SHAR     | IFC_RLS(RLS_SN); } break;
				case 0b000010: {ifc = IFC_STSMMACH | IFC_RLS(RLS_SN); } break;
				case 0b010010: {ifc = IFC_STSMMACL | IFC_RLS(RLS_SN); } break;
				case 0b100010: {ifc = IFC_STSMPR   | IFC_RLS(RLS_SN); } break;
				case 0b000011: {ifc = IFC_STCMSR   | IFC_RLS(RLS_SN); cycles+=1; } break;
				case 0b010011: {ifc = IFC_STCMGBR  | IFC_RLS(RLS_SN); cycles+=1; } break;
				case 0b100011: {ifc = IFC_STCMVBR  | IFC_RLS(RLS_SN); cycles+=1; } break;
				case 0b000100: {ifc = IFC_ROTL     | IFC_RLS(RLS_SN); } break;
				case 0b100100: {ifc = IFC_ROTCL    | IFC_RLS(RLS_SN); } break;
				case 0b000101: {ifc = IFC_ROTR     | IFC_RLS(RLS_SN); } break;
				case 0b010101: {ifc = IFC_CMPPL    | IFC_RLS(RLS_LN); } break;
				case 0b100101: {ifc = IFC_ROTCR    | IFC_RLS(RLS_SN); } break;
				case 0b000110: {ifc = IFC_LDSMMACH | IFC_RLS(RLS_SN); } break;
				case 0b010110: {ifc = IFC_LDSMMACL | IFC_RLS(RLS_SN); } break;
				case 0b100110: {ifc = IFC_LDSMPR   | IFC_RLS(RLS_SN); } break;
				case 0b000111: {ifc = IFC_LDCMSR   | IFC_RLS(RLS_SN); cycles+=2; } break;
				case 0b010111: {ifc = IFC_LDCMGBR  | IFC_RLS(RLS_SN); cycles+=2; } break;
				case 0b100111: {ifc = IFC_LDCMVBR  | IFC_RLS(RLS_SN); cycles+=2; } break;
				case 0b001000: {ifc = IFC_SHLL2    | IFC_RLS(RLS_SN); } break;
				case 0b011000: {ifc = IFC_SHLL8    | IFC_RLS(RLS_SN); } break;
				case 0b101000: {ifc = IFC_SHLL16   | IFC_RLS(RLS_SN); } break;
				case 0b001001: {ifc = IFC_SHLR2    | IFC_RLS(RLS_SN); } break;
				case 0b011001: {ifc = IFC_SHLR8    | IFC_RLS(RLS_SN); } break;
				case 0b101001: {ifc = IFC_SHLR16   | IFC_RLS(RLS_SN); } break;
				case 0b001010: {ifc = IFC_LDSMACH  | IFC_RLS(RLS_LN); } break;
				case 0b011010: {ifc = IFC_LDSMACL  | IFC_RLS(RLS_LN); } break;
				case 0b101010: {ifc = IFC_LDSPR    | IFC_RLS(RLS_LN); } break;
				case 0b001011: {ifc = IFC_JSR      | IFC_RLS(RLS_LN); cycles+=1; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break;
				case 0b011011: {ifc = IFC_TAS      | IFC_RLS(RLS_LN); cycles+=4; } break;
				case 0b101011: {ifc = IFC_JMP      | IFC_RLS(RLS_LN); cycles+=1; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break;
				case 0b001110: {ifc = IFC_LDCSR    | IFC_RLS(RLS_LN); } break;
				case 0b011110: {ifc = IFC_LDCGBR   | IFC_RLS(RLS_LN); } break;
				case 0b101110: {ifc = IFC_LDCVBR   | IFC_RLS(RLS_LN); } break;
				case 0b001111:
				case 0b011111:
				case 0b101111:
				case 0b111111: {ifc = IFC_MACW     | IFC_RLS(RLS_LNM); cycles+=1; } break;
				default:       {ifc = IFC_ILLEGAL  | IFC_RLS(RLS_S15); bstate = BSTATE_END;} break;
			}} break;
			case 0b0101: { ifc = IFC_MOVLL4 | IFC_RLS(RLS_LM_SN); } break;
			case 0b0110: {
			switch (inst & 0xF) { // 0100_NNNN_.FX._XXXX
				case 0b0000: {ifc = IFC_MOVBL | IFC_RLS(RLS_LM_SN); } break;
				case 0b0001: {ifc = IFC_MOVWL | IFC_RLS(RLS_LM_SN); } break;
				case 0b0010: {ifc = IFC_MOVLL | IFC_RLS(RLS_LM_SN); } break;
				case 0b0011: {ifc = IFC_MOV   | IFC_RLS(RLS_LM_SN); } break;
				case 0b0100: {ifc = IFC_MOVBP | IFC_RLS(RLS_SNM); } break;
				case 0b0101: {ifc = IFC_MOVWP | IFC_RLS(RLS_SNM); } break;
				case 0b0110: {ifc = IFC_MOVLP | IFC_RLS(RLS_SNM); } break;
				case 0b0111: {ifc = IFC_NOT   | IFC_RLS(RLS_LM_SN); } break;
				case 0b1000: {ifc = IFC_SWAPB | IFC_RLS(RLS_LM_SN); } break;
				case 0b1001: {ifc = IFC_SWAPW | IFC_RLS(RLS_LM_SN); } break;
				case 0b1010: {ifc = IFC_NEGC  | IFC_RLS(RLS_LM_SN); } break;
				case 0b1011: {ifc = IFC_NEG   | IFC_RLS(RLS_LM_SN); } break;
				case 0b1100: {ifc = IFC_EXTUB | IFC_RLS(RLS_LM_SN); } break;
				case 0b1101: {ifc = IFC_EXTUW | IFC_RLS(RLS_LM_SN); } break;
				case 0b1110: {ifc = IFC_EXTSB | IFC_RLS(RLS_LM_SN); } break;
				case 0b1111: {ifc = IFC_EXTSW | IFC_RLS(RLS_LM_SN); } break;
			}} break;
			case 0b0111: { ifc = IFC_ADDI | IFC_RLS(RLS_SN); } break;
			case 0b1000: {
			switch ((inst >> 8) & 0xF) { // 0100_XXXX_YYYY_YYYY
				case 0b0000: {ifc = IFC_MOVBS4 | IFC_RLS(RLS_LM0); } break;
				case 0b0001: {ifc = IFC_MOVWS4 | IFC_RLS(RLS_LM0); } break;
				case 0b0100: {ifc = IFC_MOVBL4 | IFC_RLS(RLS_LM_S0); } break;
				case 0b0101: {ifc = IFC_MOVWL4 | IFC_RLS(RLS_LM_S0); } break;
				case 0b1000: {ifc = IFC_CMPIM  | IFC_RLS(RLS_L0); } break;
				case 0b1001: {ifc = IFC_BT; bstate = BSTATE_END;} break; //TODO: Cycles are weird
				case 0b1011: {ifc = IFC_BF; bstate = BSTATE_END;} break; //TODO: Cycles are weird
				case 0b1101: {ifc = IFC_BTS; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break; //TODO: Cycles are weird and Delay Slot is weird
				case 0b1111: {ifc = IFC_BFS; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break; //TODO: Cycles are weird and Delay Slot is weird
				default:     {ifc = IFC_ILLEGAL | IFC_RLS(RLS_S15); bstate = BSTATE_END;} break;
			}} break;
			case 0b1001: { ifc = IFC_MOVWI | IFC_RLS(RLS_SN); } break;
			case 0b1010: { ifc = IFC_BRA; cycles+=1; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break;
			case 0b1011: { ifc = IFC_BSR; cycles+=1; if(bstate == BSTATE_CONT) {bstate = BSTATE_SET_DELAY;}} break;
			case 0b1100: {
			switch ((inst >> 8) & 0xF) { // 0100_XXXX_YYYY_YYYY
				case 0b0000: {ifc = IFC_MOVBSG | IFC_RLS(RLS_L0); } break;
				case 0b0001: {ifc = IFC_MOVWSG | IFC_RLS(RLS_L0); } break;
				case 0b0010: {ifc = IFC_MOVLSG | IFC_RLS(RLS_L0); } break;
				case 0b0011: {ifc = IFC_TRAPA  | IFC_RLS(RLS_S15); cycles+=8; bstate = BSTATE_END;} break;
				case 0b0100: {ifc = IFC_MOVBLG | IFC_RLS(RLS_S0); } break;
				case 0b0101: {ifc = IFC_MOVWLG | IFC_RLS(RLS_S0); } break;
				case 0b0110: {ifc = IFC_MOVLLG | IFC_RLS(RLS_S0); } break;
				case 0b0111: {ifc = IFC_MOVA   | IFC_RLS(RLS_S0); } break;
				case 0b1000: {ifc = IFC_TSTI   | IFC_RLS(RLS_L0); } break;
				case 0b1001: {ifc = IFC_ANDI   | IFC_RLS(RLS_L0); } break;
				case 0b1010: {ifc = IFC_XORI   | IFC_RLS(RLS_L0); } break;
				case 0b1011: {ifc = IFC_ORI    | IFC_RLS(RLS_L0); } break;
				case 0b1100: {ifc = IFC_TSTM   | IFC_RLS(RLS_L0); cycles+=2; } break;
				case 0b1101: {ifc = IFC_ANDM   | IFC_RLS(RLS_L0); cycles+=2; } break;
				case 0b1110: {ifc = IFC_XORM   | IFC_RLS(RLS_L0); cycles+=2; } break;
				case 0b1111: {ifc = IFC_ORM    | IFC_RLS(RLS_L0); cycles+=2; } break;
			}} break;
			case 0b1101: { ifc = IFC_MOVLI   | IFC_RLS(RLS_SN); } break;
			case 0b1110: { ifc = IFC_MOVI    | IFC_RLS(RLS_SN); } break;
			case 0b1111: { ifc = IFC_ILLEGAL | IFC_RLS(RLS_S15); bstate = BSTATE_END;} break;
		}

		//Mark regs that need to be loadaed/stored
		u32 rn = REG_NUM_R((inst >> 8) & 0xF);
		u32 rm = REG_NUM_R((inst >> 4) & 0xF);
		switch (IFC_RLS_GET(ifc)) {
			case RLS_L0:     { ld_regs |= REG_NUM_R(0); } break;
			case RLS_L15:    { ld_regs |= REG_NUM_R(15); } break;
			case RLS_LN:     { ld_regs |= rn; } break;
			case RLS_LNM:    { ld_regs |= rn | rm; } break;
			case RLS_LM0:    { ld_regs |= rm | REG_NUM_R(0); } break;
			case RLS_LNM0:   { ld_regs |= rn | rm | REG_NUM_R(0); } break;
			case RLS_LM_S0:  { ld_regs |= rm                | (st_regs |= REG_NUM_R(0)); } break;
			case RLS_LM_SN:  { ld_regs |= rm                | (st_regs |= rn); } break;
			case RLS_LM0_SN: { ld_regs |= rm | REG_NUM_R(0) | (st_regs |= rn); } break;
			case RLS_S0:     { ld_regs |=                     (st_regs |= REG_NUM_R(0)); } break;
			case RLS_S15:    { ld_regs |=                     (st_regs |= REG_NUM_R(15)); } break;
			case RLS_SN:     { ld_regs |=                     (st_regs |= rn); } break;
			case RLS_SNM:    { ld_regs |=                     (st_regs |= rn | rm); } break;
		}

		//End the block if it was in delay state
		switch (bstate) {
			case BSTATE_SET_DELAY: {bstate = BSTATE_IN_DELAY;} break;
			case BSTATE_IN_DELAY: {
				if (ifc >= IFC_BF && ifc <= IFC_RTS) {
					ifc |= IFC_ILLEGAL | IFC_RLS(RLS_S15) | IFC_ILGL_BRANCH;
				}
				bstate = BSTATE_END;
				ifc |= IFC_IS_DELAY;} break;
		}

		ifc_array[instr_count] = ifc;
		++instr_count;
		++cycles; /* At least one cycle */
		curr_pc += 2;
	}

	//Set values
	block_data.st_regs = st_regs;
	block_data.ld_regs = ld_regs;
	block_data.cycle_count = cycles;
	block_data.instr_count = instr_count;

	return 0;
}


u32 _jit_GenBlock(u32 addr, Block *iblock)
{
	u32 curr_pc = addr;
	//Pass 0, check instructions and decode one by one:
	u16 *inst_ptr = (u16*) sh2_GetPCAddr(curr_pc);
	u32 *icache_ptr = &drc_code[drc_code_pos];
	u32 reg_indx[32];
	_jit_GenIFCBlock(addr);

	PPCC_BEGIN_BLOCK();
	PPCC_MOV(GP_CTX, 3); //SH2 context is in r3
	//Load registers used in block
	PPCE_LOAD(GP_SR, sr);
	u32 reg_curr = 29;
	for (u32 i = 0; i < 16; ++i) {
		if ((block_data.ld_regs >> i) & 1) {
			reg_indx[i] = reg_curr;
			PPCE_LOAD(GP_R(i), r[i]);
			reg_curr--;
		}
	}
	for (u32 i = 0; i < block_data.instr_count; ++i) {
		u32 inst = *(inst_ptr++);
		_jit_opcode = inst;
		rn = GP_R((inst >> 8) & 0xF);
		rm = GP_R((inst >> 4) & 0xF);

		switch (ifc_array[i] & 0xFF) {
		//Algebraic/Logical
			case IFC_ILLEGAL:{SH2JIT_ILLEGAL;} break;
			case IFC_ADD:    {SH2JIT_ADD;} break;
			case IFC_ADDI:   {SH2JIT_ADDI;} break;
			case IFC_ADDC:   {SH2JIT_ADDC;} break;
			case IFC_ADDV:   {SH2JIT_ADDV;} break;
			case IFC_SUB:    {SH2JIT_SUB;} break;
			case IFC_SUBC:   {SH2JIT_SUBC;} break;
			case IFC_SUBV:   {SH2JIT_SUBV;} break;
			case IFC_AND:    {SH2JIT_AND;} break;
			case IFC_ANDI:   {SH2JIT_ANDI;} break;
			case IFC_ANDM:   {SH2JIT_ANDM;} break;
			case IFC_OR:     {SH2JIT_OR;} break;
			case IFC_ORI:    {SH2JIT_ORI;} break;
			case IFC_ORM:    {SH2JIT_ORM;} break;
			case IFC_XOR:    {SH2JIT_XOR;} break;
			case IFC_XORI:   {SH2JIT_XORI;} break;
			case IFC_XORM:   {SH2JIT_XORM;} break;
			case IFC_ROTCL:  {SH2JIT_ROTCL;} break;
			case IFC_ROTCR:  {SH2JIT_ROTCR;} break;
			case IFC_ROTL:   {SH2JIT_ROTL;} break;
			case IFC_ROTR:   {SH2JIT_ROTR;} break;
			case IFC_SHAL:   {SH2JIT_SHAL;} break;
			case IFC_SHAR:   {SH2JIT_SHAR;} break;
			case IFC_SHLL:   {SH2JIT_SHLL;} break;
			case IFC_SHLL2:  {SH2JIT_SHLL2;} break;
			case IFC_SHLL8:  {SH2JIT_SHLL8;} break;
			case IFC_SHLL16: {SH2JIT_SHLL16;} break;
			case IFC_SHLR:   {SH2JIT_SHLR;} break;
			case IFC_SHLR2:  {SH2JIT_SHLR2;} break;
			case IFC_SHLR8:  {SH2JIT_SHLR8;} break;
			case IFC_SHLR16: {SH2JIT_SHLR16;} break;
			case IFC_NOT:    {SH2JIT_NOT;} break;
			case IFC_NEG:    {SH2JIT_NEG;} break;
			case IFC_NEGC:   {SH2JIT_NEGC;} break;
			case IFC_DT:     {SH2JIT_DT;} break;
			case IFC_EXTSB:  {SH2JIT_EXTSB;} break;
			case IFC_EXTSW:  {SH2JIT_EXTSW;} break;
			case IFC_EXTUB:  {SH2JIT_EXTUB;} break;
			case IFC_EXTUW:  {SH2JIT_EXTUW;} break;
		//Mult and Division
			case IFC_DIV0S: {SH2JIT_DIV0S;} break;
			case IFC_DIV0U: {SH2JIT_DIV0U;} break;
			case IFC_DIV1:  {SH2JIT_DIV1;} break;
			case IFC_DMULS: {SH2JIT_DMULS;} break;
			case IFC_DMULU: {SH2JIT_DMULU;} break;
			case IFC_MACL:  {SH2JIT_MACL;} break;
			case IFC_MACW:  {SH2JIT_MACW;} break;
			case IFC_MULL:  {SH2JIT_MULL;} break;
			case IFC_MULS:  {SH2JIT_MULS;} break;
			case IFC_MULU:  {SH2JIT_MULU;} break;
		//Set and Clear
			case IFC_CLRMAC: {SH2JIT_CLRMAC;} break;
			case IFC_CLRT:   {SH2JIT_CLRT;} break;
			case IFC_SETT:   {SH2JIT_SETT;} break;
		//Compare
			case IFC_CMPEQ:  {SH2JIT_CMPEQ;} break;
			case IFC_CMPGE:  {SH2JIT_CMPGE;} break;
			case IFC_CMPGT:  {SH2JIT_CMPGT;} break;
			case IFC_CMPHI:  {SH2JIT_CMPHI;} break;
			case IFC_CMPHS:  {SH2JIT_CMPHS;} break;
			case IFC_CMPPL:  {SH2JIT_CMPPL;} break;
			case IFC_CMPPZ:  {SH2JIT_CMPPZ;} break;
			case IFC_CMPSTR: {SH2JIT_CMPSTR;} break;
			case IFC_CMPIM:  {SH2JIT_CMPIM;} break;
		//Load and Store
			case IFC_LDCSR:    {SH2JIT_LDCSR;} break;
			case IFC_LDCGBR:   {SH2JIT_LDCGBR;} break;
			case IFC_LDCVBR:   {SH2JIT_LDCVBR;} break;
			case IFC_LDCMSR:   {SH2JIT_LDCMSR;} break;
			case IFC_LDCMGBR:  {SH2JIT_LDCMGBR;} break;
			case IFC_LDCMVBR:  {SH2JIT_LDCMVBR;} break;
			case IFC_LDSMACH:  {SH2JIT_LDSMACH;} break;
			case IFC_LDSMACL:  {SH2JIT_LDSMACL;} break;
			case IFC_LDSPR:    {SH2JIT_LDSPR;} break;
			case IFC_LDSMMACH: {SH2JIT_LDSMMACH;} break;
			case IFC_LDSMMACL: {SH2JIT_LDSMMACL;} break;
			case IFC_LDSMPR:   {SH2JIT_LDSMPR;} break;
			case IFC_STCSR:    {SH2JIT_STCSR;} break;
			case IFC_STCGBR:   {SH2JIT_STCGBR;} break;
			case IFC_STCVBR:   {SH2JIT_STCVBR;} break;
			case IFC_STCMSR:   {SH2JIT_STCMSR;} break;
			case IFC_STCMGBR:  {SH2JIT_STCMGBR;} break;
			case IFC_STCMVBR:  {SH2JIT_STCMVBR;} break;
			case IFC_STSMACH:  {SH2JIT_STSMACH;} break;
			case IFC_STSMACL:  {SH2JIT_STSMACL;} break;
			case IFC_STSPR:    {SH2JIT_STSPR;} break;
			case IFC_STSMMACH: {SH2JIT_STSMMACH;} break;
			case IFC_STSMMACL: {SH2JIT_STSMMACL;} break;
			case IFC_STSMPR:   {SH2JIT_STSMPR;} break;
		//Move Data
			case IFC_MOV:    {SH2JIT_MOV;} break;
			case IFC_MOVBS:  {SH2JIT_MOVBS;} break;
			case IFC_MOVWS:  {SH2JIT_MOVWS;} break;
			case IFC_MOVLS:  {SH2JIT_MOVLS;} break;
			case IFC_MOVBL:  {SH2JIT_MOVBL;} break;
			case IFC_MOVWL:  {SH2JIT_MOVWL;} break;
			case IFC_MOVLL:  {SH2JIT_MOVLL;} break;
			case IFC_MOVBM:  {SH2JIT_MOVBM;} break;
			case IFC_MOVWM:  {SH2JIT_MOVWM;} break;
			case IFC_MOVLM:  {SH2JIT_MOVLM;} break;
			case IFC_MOVBP:  {SH2JIT_MOVBP;} break;
			case IFC_MOVWP:  {SH2JIT_MOVWP;} break;
			case IFC_MOVLP:  {SH2JIT_MOVLP;} break;
			case IFC_MOVBS0: {SH2JIT_MOVBS0;} break;
			case IFC_MOVWS0: {SH2JIT_MOVWS0;} break;
			case IFC_MOVLS0: {SH2JIT_MOVLS0;} break;
			case IFC_MOVBL0: {SH2JIT_MOVBL0;} break;
			case IFC_MOVWL0: {SH2JIT_MOVWL0;} break;
			case IFC_MOVLL0: {SH2JIT_MOVLL0;} break;
			case IFC_MOVI:   {SH2JIT_MOVI;} break;
			case IFC_MOVWI:  {SH2JIT_MOVWI;} break;
			case IFC_MOVLI:  {SH2JIT_MOVLI;} break;
			case IFC_MOVBLG: {SH2JIT_MOVBLG;} break;
			case IFC_MOVWLG: {SH2JIT_MOVWLG;} break;
			case IFC_MOVLLG: {SH2JIT_MOVLLG;} break;
			case IFC_MOVBSG: {SH2JIT_MOVBSG;} break;
			case IFC_MOVWSG: {SH2JIT_MOVWSG;} break;
			case IFC_MOVLSG: {SH2JIT_MOVLSG;} break;
			case IFC_MOVBS4: {SH2JIT_MOVBS4;} break;
			case IFC_MOVWS4: {SH2JIT_MOVWS4;} break;
			case IFC_MOVLS4: {SH2JIT_MOVLS4;} break;
			case IFC_MOVBL4: {SH2JIT_MOVBL4;} break;
			case IFC_MOVWL4: {SH2JIT_MOVWL4;} break;
			case IFC_MOVLL4: {SH2JIT_MOVLL4;} break;
			case IFC_MOVA:   {SH2JIT_MOVA;} break;
			case IFC_MOVT:   {SH2JIT_MOVT;} break;
		//Branch and Jumps
			case IFC_BF:	{SH2JIT_BF;} break;
			case IFC_BFS:	{SH2JIT_BFS;} break;
			case IFC_BRA:	{SH2JIT_BRA;} break;
			case IFC_BRAF:	{SH2JIT_BRAF;} break;
			case IFC_BSR:	{SH2JIT_BSR;} break;
			case IFC_BSRF:	{SH2JIT_BSRF;} break;
			case IFC_BT:	{SH2JIT_BT;} break;
			case IFC_BTS:	{SH2JIT_BTS;} break;
			case IFC_JMP:	{SH2JIT_JMP;} break;
			case IFC_JSR:	{SH2JIT_JSR;} break;
			case IFC_RTE:	{SH2JIT_RTE;} break;
			case IFC_RTS:	{SH2JIT_RTS;} break;
		//Other
			case IFC_NOP:	{SH2JIT_NOP;} break;
			case IFC_SLEEP:	{SH2JIT_SLEEP;} break;
			case IFC_SWAPB:	{SH2JIT_SWAPB;} break;
			case IFC_SWAPW:	{SH2JIT_SWAPW;} break;
			case IFC_TAS:	{SH2JIT_TAS;} break;
			case IFC_TRAPA:	{SH2JIT_TRAPA;} break;
			case IFC_TST:	{SH2JIT_TST;} break;
			case IFC_TSTI:	{SH2JIT_TSTI;} break;
			case IFC_TSTM:	{SH2JIT_TSTM;} break;
			case IFC_XTRCT:	{SH2JIT_XTRCT;} break;
		}
		curr_pc += 2;
	}
	//Store registers that were modified in block
	for (u32 i = 0; i < 16; ++i) {
		if ((block_data.st_regs >> i) & 1) {
			PPCE_SAVE(GP_R(i), r[i]);
		}
	}
	PPCE_SAVE(GP_SR, sr);
	PPCC_END_BLOCK();

	/* Fill block code */
	iblock->code = (DrcCode) &drc_code[drc_code_pos];
	iblock->ret_addr = 0;
	iblock->start_addr = addr;
	iblock->sh2_len = block_data.instr_count;
	iblock->ppc_len = ((u32) icache_ptr - (u32)iblock->code) >> 2;
	iblock->cycle_count = block_data.cycle_count;

	DCStoreRange((void*)(((u32) iblock->code) & ~0x1F), ((iblock->ppc_len * 4) & ~0x1F) + 0x20);
	ICInvalidateRange((void*)(((u32) iblock->code) & ~0x1F), ((iblock->ppc_len * 4) & ~0x1F) + 0x20);

	/* Add length of block to drc code position */
	//TODO: Should be aligned 32Bytes?
	drc_code_pos += iblock->ppc_len;
	return 0;
}


void sh2_DrcInit(void)
{
	drc_code = memalign(32, DRC_CODE_SIZE*sizeof(*drc_code));
	drc_blocks = memalign(32, BLOCK_ARR_SIZE*sizeof(*drc_blocks));
	HashClearAll();
}

void sh2_DrcReset(void)
{
	//Clears all hash code
	HashClearAll();
}


s32 sh2_DrcExec(SH2 *sh, s32 cycles)
{
	u32 block_found = 0;
	Block *iblock = HashGet(sh->pc, &block_found);
	while(sh->cycles < cycles) {
		u32 addr = sh->pc;
		if (addr != iblock->start_addr) {
			iblock = HashGet(addr, &block_found);
			if (!block_found) {
				_jit_GenBlock(addr, iblock);
			}
		}
		iblock->code(sh);
		sh->cycles += iblock->cycle_count; //XXX: also subtract from delta cycles
	}
	sh->cycles -= cycles;
	return 0;
}

// BIOS CODE PPC TO SSH
//801F0470 -> 2000039E
//801F0778 -> 20000214
//801f0c88 -> 00000262
//801f0a10 -> 000002CE
//801f0ea8 -> 00000282
//801f0f08 -> 0000028E
//801f1000 -> 06000680
//801f28d8 -> 000042ec
//801f2a50 -> 000022fa