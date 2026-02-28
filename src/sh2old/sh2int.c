/*  Copyright 2003-2005 Guillaume Duhamel
    Copyright 2004-2007 Theo Berkau
    Copyright 2005 Fabien Coulon

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

// SH2 Interpreter Core

#include "sh2core.h"
#include "sh2int.h"
#include "sh2idle.h"
#include "../cart.h"
#include "../debug.h"
#include "../error.h"
#include "../memory.h"
#include "../yabause.h"

// #define SH2_TRACE  // Uncomment to enable tracing

int SH2InterpreterInit(void);
void SH2InterpreterDeInit(void);
void SH2InterpreterReset(SH2_struct *context);
void FASTCALL SH2InterpreterExec(SH2_struct *context, u32 cycles);
void SH2InterpreterGetRegisters(SH2_struct *context, sh2regs_struct *regs);
u32 SH2InterpreterGetGPR(SH2_struct *context, int num);
u32 SH2InterpreterGetSR(SH2_struct *context);
u32 SH2InterpreterGetGBR(SH2_struct *context);
u32 SH2InterpreterGetVBR(SH2_struct *context);
u32 SH2InterpreterGetMACH(SH2_struct *context);
u32 SH2InterpreterGetMACL(SH2_struct *context);
u32 SH2InterpreterGetPR(SH2_struct *context);
u32 SH2InterpreterGetPC(SH2_struct *context);
void SH2InterpreterSetRegisters(SH2_struct *context, const sh2regs_struct *regs);
void SH2InterpreterSetGPR(SH2_struct *context, int num, u32 value);
void SH2InterpreterSetSR(SH2_struct *context, u32 value);
void SH2InterpreterSetGBR(SH2_struct *context, u32 value);
void SH2InterpreterSetVBR(SH2_struct *context, u32 value);
void SH2InterpreterSetMACH(SH2_struct *context, u32 value);
void SH2InterpreterSetMACL(SH2_struct *context, u32 value);
void SH2InterpreterSetPR(SH2_struct *context, u32 value);
void SH2InterpreterSetPC(SH2_struct *context, u32 value);
void SH2InterpreterSendInterrupt(SH2_struct *context, u8 level, u8 vector);
int SH2InterpreterGetInterrupts(SH2_struct *context,
                                interrupt_struct interrupts[MAX_INTERRUPTS]);
void SH2InterpreterSetInterrupts(SH2_struct *context, int num_interrupts,
                                 const interrupt_struct interrupts[MAX_INTERRUPTS]);


opcodefunc opcodes[0x10000];


static void FASTCALL SH2delay(SH2_struct * sh, u32 addr);

SH2Interface_struct SH2Interpreter = {
   SH2CORE_INTERPRETER,
   "SH2 Interpreter",

   SH2InterpreterInit,
   SH2InterpreterDeInit,
   SH2InterpreterReset,
   SH2InterpreterExec,

   SH2InterpreterGetRegisters,
   SH2InterpreterGetGPR,
   SH2InterpreterGetSR,
   SH2InterpreterGetGBR,
   SH2InterpreterGetVBR,
   SH2InterpreterGetMACH,
   SH2InterpreterGetMACL,
   SH2InterpreterGetPR,
   SH2InterpreterGetPC,

   SH2InterpreterSetRegisters,
   SH2InterpreterSetGPR,
   SH2InterpreterSetSR,
   SH2InterpreterSetGBR,
   SH2InterpreterSetVBR,
   SH2InterpreterSetMACH,
   SH2InterpreterSetMACL,
   SH2InterpreterSetPR,
   SH2InterpreterSetPC,

   SH2InterpreterSendInterrupt,
   SH2InterpreterGetInterrupts,
   SH2InterpreterSetInterrupts,

   NULL  // SH2WriteNotify not used
};

fetchfunc fetchlist[0x100];

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL FetchBios(u32 addr)
{
	return bios_Read16(addr);
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL FetchCs0(u32 addr)
{
   return cs0_read16(addr);
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL FetchLWram(u32 addr)
{
   return T2ReadWord(wram, addr & 0xFFFFF);
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL FetchHWram(u32 addr)
{
   return T2ReadWord(wram, (addr & 0xFFFFF) | 0x100000);
}

//////////////////////////////////////////////////////////////////////////////

static u32 FASTCALL FetchInvalid(UNUSED u32 addr)
{
   return 0xFFFF;
}


//////////////////////////////////////////////////////////////////////////////

static void FASTCALL sh2_IllegalInst(SH2_struct * sh)
{
   int vectnum;

   YabSetError(YAB_ERR_SH2INVALIDOPCODE, sh);

   // Save regs.SR on stack
   sh->regs.R[15]-=4;
   mem_Write32(sh->regs.R[15],sh->regs.SR.all);

   // Save regs.PC on stack
   sh->regs.R[15]-=4;
   mem_Write32(sh->regs.R[15],sh->regs.PC + 2);

   // What caused the exception? The delay slot or a general instruction?
   // 4 for General Instructions, 6 for delay slot
   vectnum = 4; //  Fix me

   // Jump to Exception service routine
   sh->regs.PC = mem_Read32(sh->regs.VBR+(vectnum<<2));
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2add(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] += sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2addi(SH2_struct * sh)
{
   s32 cd = (s32)(s8)INSTRUCTION_CD(sh->instruction);
   s32 b = INSTRUCTION_B(sh->instruction);

   sh->regs.R[b] += cd;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

//25 inst -> 20 inst
static void FASTCALL SH2addc(SH2_struct * sh)
{
   u32 tmp;//0, tmp1;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
#if 0
   tmp1 = sh->regs.R[source] + sh->regs.R[dest];
   tmp0 = sh->regs.R[dest];

   sh->regs.R[dest] = tmp1 + sh->regs.SR.part.T;
   sh->regs.SR.part.T = (tmp0 > tmp1) | (tmp1 > sh->regs.R[dest]);
#else
	asm("rlwinm %0, %2, 29, 2, 2\n\t"
		"mtxer %0\n\t"
		"addo %1, %1, %3\n\t"
		"mfxer %0\n\t"
		"rlwimi %2, %0, 3, 31, 31"
		: "=r"(tmp), "+r"(sh->regs.R[n]), "+r"(sh->regs.SR.all)
		: "r" (sh->regs.R[m])
	);
#endif
	sh->regs.PC += 2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

//23 inst -> 18 inst
static void FASTCALL SH2addv(SH2_struct * sh)
{
	u32 tmp;
   u32 n = INSTRUCTION_B(sh->instruction);
   u32 m = INSTRUCTION_C(sh->instruction);

#if 0
      s32 dest,src,ans;

   dest = ((s32) sh->regs.R[n] < 0);
   src = ((s32) sh->regs.R[m] < 0) + dest;
   sh->regs.R[n] += sh->regs.R[m];
   ans = ((s32) sh->regs.R[n] < 0) + dest;

	sh->regs.SR.part.T = ans & !(src & 1);
#else
	asm("addo %1, %1, %3\n\t"			/*Add rn + rm*/
		"mfxer %0\n\t"					/*Load XER[CA] to TMP*/
		"rlwimi %2, %0, 2, 31, 31"		/*Store T*/
		: "=r"(tmp), "+r"(sh->regs.R[n]), "+r"(sh->regs.SR.all)
		: "r" (sh->regs.R[m])
	);
#endif

   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2y_and(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] &= sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////
//12 inst
static void FASTCALL SH2andi(SH2_struct * sh)
{
   sh->regs.R[0] &= INSTRUCTION_CD(sh->instruction);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2andm(SH2_struct * sh)
{
   s32 temp;
   s32 source = INSTRUCTION_CD(sh->instruction);

   temp = (s32) mem_Read8(sh->regs.GBR + sh->regs.R[0]);
   temp &= source;
   mem_Write8((sh->regs.GBR + sh->regs.R[0]),temp);
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2bf(SH2_struct * sh)
{
   if (sh->regs.SR.part.T == 0)
   {
      s32 disp = (s32)(s8)sh->instruction;

      sh->regs.PC = sh->regs.PC+(disp<<1)+4;
      sh->cycles += 3;
   }
   else
   {
      sh->regs.PC+=2;
      sh->cycles++;
   }
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2bfs(SH2_struct * sh)
{
   if (sh->regs.SR.part.T == 0)
   {
      s32 disp = (s32)(s8)sh->instruction;
      u32 temp = sh->regs.PC;

      sh->regs.PC = sh->regs.PC + (disp << 1) + 4;

      sh->cycles += 2;
      SH2delay(sh, temp + 2);
   }
   else
   {
      sh->regs.PC += 2;
      sh->cycles++;
   }
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2bra(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_BCD(sh->instruction);
   u32 temp = sh->regs.PC;

   disp |= (-(disp & 0x800));

   sh->regs.PC = sh->regs.PC + (disp<<1) + 4;

   sh->cycles += 2;
   SH2delay(sh, temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2braf(SH2_struct * sh)
{
   u32 temp;
   s32 m = INSTRUCTION_B(sh->instruction);

   temp = sh->regs.PC;
   sh->regs.PC += sh->regs.R[m] + 4;

   sh->cycles += 2;
   SH2delay(sh, temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2bsr(SH2_struct * sh)
{
   u32 temp;
   s32 disp = INSTRUCTION_BCD(sh->instruction);

   temp = sh->regs.PC;
   disp |= (-(disp & 0x800));
   sh->regs.PR = sh->regs.PC + 4;
   sh->regs.PC = sh->regs.PC+(disp<<1) + 4;

   sh->cycles += 2;
   SH2delay(sh, temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2bsrf(SH2_struct * sh)
{
   u32 temp = sh->regs.PC;
   sh->regs.PR = sh->regs.PC + 4;
   sh->regs.PC += sh->regs.R[INSTRUCTION_B(sh->instruction)] + 4;
   sh->cycles += 2;
   SH2delay(sh, temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2bt(SH2_struct * sh)
{
   if (sh->regs.SR.part.T == 1)
   {
      s32 disp = (s32)(s8)sh->instruction;

      sh->regs.PC += (disp<<1) + 4;
      sh->cycles += 3;
   }
   else
   {
      sh->regs.PC += 2;
      sh->cycles++;
   }
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2bts(SH2_struct * sh)
{
   if (sh->regs.SR.part.T)
   {
      s32 disp = (s32)(s8)sh->instruction;
      u32 temp = sh->regs.PC;

      sh->regs.PC += (disp << 1) + 4;
      sh->cycles += 2;
      SH2delay(sh, temp + 2);
   }
   else
   {
      sh->regs.PC+=2;
      sh->cycles++;
   }
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2clrmac(SH2_struct * sh)
{
   sh->regs.MACH = 0;
   sh->regs.MACL = 0;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2clrt(SH2_struct * sh)
{
   sh->regs.SR.part.T = 0;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////
//19 ins -> 16 ins
static void FASTCALL SH2cmpeq(SH2_struct * sh)
{
	//u32 tmp;
	u32 n = INSTRUCTION_B(sh->instruction);
	u32 m = INSTRUCTION_C(sh->instruction);
#if 0
	sh->regs.SR.part.T = (sh->regs.R[n] == sh->regs.R[m]);
#else
	u32 tmp;
	asm("cmp 0, 1, %2, %3\n\t"			/*Compare rn and rm*/
		"mfcr %0\n\t"					/*Load CR to TMP*/
		"rlwimi %1, %0, 3, 31, 31"		/*Store EQ in T*/
		:"=r" (tmp), "+r" (sh->regs.SR.all)
		:"r" (sh->regs.R[n]), "r" (sh->regs.R[m])
		:"cc"
	);
#endif

	sh->regs.PC += 2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2cmpge(SH2_struct * sh)
{
	sh->regs.SR.part.T = (	(s32)sh->regs.R[INSTRUCTION_B(sh->instruction)] >=
							(s32)sh->regs.R[INSTRUCTION_C(sh->instruction)]);
	sh->regs.PC += 2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////
//20 ins
static void FASTCALL SH2cmpgt(SH2_struct * sh)
{
   sh->regs.SR.part.T = ((s32)sh->regs.R[INSTRUCTION_B(sh->instruction)] > (s32)sh->regs.R[INSTRUCTION_C(sh->instruction)]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2cmphi(SH2_struct * sh)
{
	sh->regs.SR.part.T = (	(u32)sh->regs.R[INSTRUCTION_B(sh->instruction)] >
							(u32)sh->regs.R[INSTRUCTION_C(sh->instruction)]);
	sh->regs.PC+=2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2cmphs(SH2_struct * sh)
{
	sh->regs.SR.part.T = (	(u32)sh->regs.R[INSTRUCTION_B(sh->instruction)] >=
							(u32)sh->regs.R[INSTRUCTION_C(sh->instruction)]);
	sh->regs.PC+=2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2cmpim(SH2_struct * sh)
{
   s32 imm;
   s32 i = INSTRUCTION_CD(sh->instruction);

   imm = (s32)(s8)i;

   sh->regs.SR.part.T = (sh->regs.R[0] == (u32) imm);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2cmppl(SH2_struct * sh)
{
   sh->regs.SR.part.T = ((s32)sh->regs.R[INSTRUCTION_B(sh->instruction)] > 0);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2cmppz(SH2_struct * sh)
{
   sh->regs.SR.part.T = (s32)sh->regs.R[INSTRUCTION_B(sh->instruction)] >= 0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2cmpstr(SH2_struct * sh)
{
   u32 temp;
   s32 HH,HL,LH,LL;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   temp=sh->regs.R[n]^sh->regs.R[m];
   HH = (temp>>24) & 0x000000FF;
   HL = (temp>>16) & 0x000000FF;
   LH = (temp>>8) & 0x000000FF;
   LL = temp & 0x000000FF;
   sh->regs.SR.part.T = !(HH && HL && LH && LL);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2div0s(SH2_struct * sh)
{
	u32 m = INSTRUCTION_C(sh->instruction);
	u32 n = INSTRUCTION_B(sh->instruction);
	sh->regs.SR.part.Q = (sh->regs.R[n] >> 31);
	sh->regs.SR.part.M = (sh->regs.R[m] >> 31);
	sh->regs.SR.part.T = !(sh->regs.SR.part.M == sh->regs.SR.part.Q);
	sh->regs.PC += 2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2div0u(SH2_struct * sh)
{
   sh->regs.SR.part.M = sh->regs.SR.part.Q = sh->regs.SR.part.T = 0;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2div1(SH2_struct * sh)
{
	u32 tmp0;
	u8 old_q, tmp1;
	s32 m = INSTRUCTION_C(sh->instruction);
	s32 n = INSTRUCTION_B(sh->instruction);

	old_q = sh->regs.SR.part.Q;
	sh->regs.SR.part.Q = (u8)((0x80000000 & sh->regs.R[n])!=0);
	sh->regs.R[n] <<= 1;
	sh->regs.R[n]|=(u32) sh->regs.SR.part.T;

	tmp0 = sh->regs.R[n];
	switch(sh->regs.SR.part.M ^ old_q) {
		case 0:
			sh->regs.R[n] -= sh->regs.R[m];
			tmp1 = (sh->regs.R[n] > tmp0);
			break;
		case 1:
			sh->regs.R[n] += sh->regs.R[m];
			tmp1 = (sh->regs.R[n] < tmp0);
			break;
	}
	sh->regs.SR.part.Q = tmp1 ^ sh->regs.SR.part.Q ^ sh->regs.SR.part.M;
	sh->regs.SR.part.T = (sh->regs.SR.part.Q == sh->regs.SR.part.M);
	sh->regs.PC += 2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////
//62 inst -> 16 inst
static void FASTCALL SH2dmuls(SH2_struct * sh)
{
	u32 m = INSTRUCTION_C(sh->instruction);
	u32 n = INSTRUCTION_B(sh->instruction);

#if 1
	asm("mulhw %0, %2, %3\n\t"
		"mullw %1, %2, %3"
		: "=r" (sh->regs.MACH), "=r" (sh->regs.MACL)
		: "r" (sh->regs.R[n]), "r" (sh->regs.R[m])
	);
#else
	u32 RnL,RnH,RmL,RmH,Res0,Res1,Res2;
	u32 temp0,temp1,temp2,temp3;
	s32 tempm,tempn,fnLmL;

	tempn = (s32)sh->regs.R[n];
   tempm = (s32)sh->regs.R[m];
   if (tempn < 0)
      tempn = 0 - tempn;
   if (tempm < 0)
      tempm = 0 - tempm;
   fnLmL = -((s32) (sh->regs.R[n] ^ sh->regs.R[m]) < 0);

   temp1 = (u32) tempn;
   temp2 = (u32) tempm;

   RnL = temp1 & 0x0000FFFF;
   RnH = (temp1 >> 16) & 0x0000FFFF;
   RmL = temp2 & 0x0000FFFF;
   RmH = (temp2 >> 16) & 0x0000FFFF;

   temp0 = RmL * RnL;
   temp1 = RmH * RnL;
   temp2 = RmL * RnH;
   temp3 = RmH * RnH;

   Res1 = temp1 + temp2;
   Res2 = (Res1 < temp1) << 16;

   temp1 = (Res1 << 16) & 0xFFFF0000;
   Res0 = temp0 + temp1;
   Res2 += (Res0 < temp0) + ((Res1 >> 16) & 0x0000FFFF) + temp3;

   if (fnLmL < 0)
   {
      Res2 = ~Res2;
      if (Res0 == 0)
         Res2++;
      else
         Res0 =(~Res0) + 1;
   }
   sh->regs.MACH = Res2;
   sh->regs.MACL = Res0;
#endif
   sh->regs.PC += 2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////
//34 ins -> 16 ins
static void FASTCALL SH2dmulu(SH2_struct * sh)
{
   u32 m = INSTRUCTION_C(sh->instruction);
   u32 n = INSTRUCTION_B(sh->instruction);
#if 1
	asm("mulhwu %0, %2, %3\n\t"
		"mullw %1, %2, %3"
		: "=r" (sh->regs.MACH), "=r" (sh->regs.MACL)
		: "r" (sh->regs.R[n]), "r" (sh->regs.R[m])
	);
#else
   u32 RnL,RnH,RmL,RmH,Res0,Res1,Res2;
   u32 temp0,temp1,temp2,temp3;

	RnL = sh->regs.R[n] & 0x0000FFFF;
   RnH = (sh->regs.R[n] >> 16) & 0x0000FFFF;
   RmL = sh->regs.R[m] & 0x0000FFFF;
   RmH = (sh->regs.R[m] >> 16) & 0x0000FFFF;

   temp0 = RmL * RnL;
   temp1 = RmH * RnL;
   temp2 = RmL * RnH;
   temp3 = RmH * RnH;

   Res1 = temp1 + temp2;
   Res2 = ((Res1 < temp1) << 16);

   temp1 = (Res1 << 16) & 0xFFFF0000;
   Res0 = temp0 + temp1;
   Res2+= (Res0 < temp0);

   Res2 = Res2 + ((Res1 >> 16) & 0x0000FFFF) + temp3;

   sh->regs.MACH = Res2;
   sh->regs.MACL = Res0;
#endif
   sh->regs.PC += 2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2dt(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n]--;
   sh->regs.SR.part.T = !sh->regs.R[n];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2extsb(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (u32)(s8)sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2extsw(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (u32)(s16)sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2extub(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (u32)(u8)sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2extuw(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (u32)(u16)sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2jmp(SH2_struct * sh)
{
   u32 temp;
   s32 m = INSTRUCTION_B(sh->instruction);

   temp=sh->regs.PC;
   sh->regs.PC = sh->regs.R[m];
   sh->cycles += 2;
   SH2delay(sh, temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2jsr(SH2_struct * sh)
{
   u32 temp;
   s32 m = INSTRUCTION_B(sh->instruction);

   temp = sh->regs.PC;
   sh->regs.PR = sh->regs.PC + 4;
   sh->regs.PC = sh->regs.R[m];
   sh->cycles += 2;
   SH2delay(sh, temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldcgbr(SH2_struct * sh)
{
   sh->regs.GBR = sh->regs.R[INSTRUCTION_B(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldcmgbr(SH2_struct * sh)
{
   s32 m = INSTRUCTION_B(sh->instruction);

   sh->regs.GBR = mem_Read32(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldcmsr(SH2_struct * sh)
{
   s32 m = INSTRUCTION_B(sh->instruction);

   sh->regs.SR.all = mem_Read32(sh->regs.R[m]) & 0x000003F3;
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldcmvbr(SH2_struct * sh)
{
   s32 m = INSTRUCTION_B(sh->instruction);

   sh->regs.VBR = mem_Read32(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldcsr(SH2_struct * sh)
{
   sh->regs.SR.all = sh->regs.R[INSTRUCTION_B(sh->instruction)]&0x000003F3;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldcvbr(SH2_struct * sh)
{
   s32 m = INSTRUCTION_B(sh->instruction);

   sh->regs.VBR = sh->regs.R[m];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldsmach(SH2_struct * sh)
{
   sh->regs.MACH = sh->regs.R[INSTRUCTION_B(sh->instruction)];
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldsmacl(SH2_struct * sh)
{
   sh->regs.MACL = sh->regs.R[INSTRUCTION_B(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldsmmach(SH2_struct * sh)
{
   s32 m = INSTRUCTION_B(sh->instruction);
   sh->regs.MACH = mem_Read32(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldsmmacl(SH2_struct * sh)
{
   s32 m = INSTRUCTION_B(sh->instruction);
   sh->regs.MACL = mem_Read32(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldsmpr(SH2_struct * sh)
{
   s32 m = INSTRUCTION_B(sh->instruction);
   sh->regs.PR = mem_Read32(sh->regs.R[m]);
   sh->regs.R[m] += 4;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ldspr(SH2_struct * sh)
{
   sh->regs.PR = sh->regs.R[INSTRUCTION_B(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////
//110 ins -> 50
static void FASTCALL SH2macl(SH2_struct * sh)
{
	u32 m = INSTRUCTION_C(sh->instruction);
	u32 n = INSTRUCTION_B(sh->instruction);
#if 1
	u32 tmp_h, tmp_l;
	u32 mem_n = mem_Read32(sh->regs.R[n]);
	sh->regs.R[n] += 4;
	u32 mem_m = mem_Read32(sh->regs.R[m]);
	sh->regs.R[m] += 4;

	asm("mullw %1, %4, %5\n\t"
		"addco %3, %3, %1\n\t"
		"mulhw %0, %4, %5\n\t"
		"adde %2, %2, %0\n\t"
		: "=&r" (tmp_h), "=&r" (tmp_l), "+r" (sh->regs.MACH), "+r" (sh->regs.MACL)
		: "r" (mem_n), "r" (mem_m)
	);

	sh->regs.MACH = (sh->regs.SR.part.S ? (s16) sh->regs.MACH : sh->regs.MACH);
#else

   u32 RnL,RnH,RmL,RmH,Res0,Res1,Res2;
   u32 temp0,temp1,temp2,temp3;
   s32 tempm,tempn,fnLmL;

   tempn = (s32) mem_Read32(sh->regs.R[n]);
   sh->regs.R[n] += 4;
   tempm = (s32) mem_Read32(sh->regs.R[m]);
   sh->regs.R[m] += 4;

   fnLmL = -((s32) (tempn^tempm) < 0);
   if (tempn < 0)
      tempn = 0 - tempn;
   if (tempm < 0)
      tempm = 0 - tempm;

   temp1 = (u32) tempn;
   temp2 = (u32) tempm;

   RnL = temp1 & 0x0000FFFF;
   RnH = (temp1 >> 16) & 0x0000FFFF;
   RmL = temp2 & 0x0000FFFF;
   RmH = (temp2 >> 16) & 0x0000FFFF;

   temp0 = RmL * RnL;
   temp1 = RmH * RnL;
   temp2 = RmL * RnH;
   temp3 = RmH * RnH;

   Res1 = temp1 + temp2;
   Res2 = (Res1 < temp1) << 16;

   temp1 = (Res1 << 16) & 0xFFFF0000;
   Res0 = temp0 + temp1;
   Res2+= (Res0 < temp0);

   Res2=Res2+((Res1>>16)&0x0000FFFF)+temp3;

   if(fnLmL < 0)
   {
      Res2=~Res2;
      if (Res0==0)
         Res2++;
      else
         Res0 = (~Res0)+1;
   }
   if(sh->regs.SR.part.S == 1)
   {
      Res0=sh->regs.MACL+Res0;
      Res2+= sh->regs.MACL > Res0;
      if (!(sh->regs.MACH & 0x00008000))
         Res2 += sh->regs.MACH | 0xFFFF0000;
      Res2+=(sh->regs.MACH&0x0000FFFF);
      if(((s32)Res2<0)&&(Res2<0xFFFF8000))
      {
         Res2=0x00008000;
         Res0=0x00000000;
      }
      if(((s32)Res2>0)&&(Res2>0x00007FFF))
      {
         Res2=0x00007FFF;
         Res0=0xFFFFFFFF;
      };

      sh->regs.MACH=Res2;
      sh->regs.MACL=Res0;
   }
   else
   {
      Res0=sh->regs.MACL+Res0;
      Res2+=sh->regs.MACH + (sh->regs.MACL > Res0);

      sh->regs.MACH=Res2;
      sh->regs.MACL=Res0;
   }
#endif
   sh->regs.PC+=2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////
//78 ins
static void FASTCALL SH2macw(SH2_struct * sh)
{
   s32 tempm,tempn,dest,src,ans;
   u32 templ;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   tempn = (s32) mem_Read16(sh->regs.R[n]);
   sh->regs.R[n] += 2;
   tempm = (s32) mem_Read16(sh->regs.R[m]);
   sh->regs.R[m] += 2;
   templ = sh->regs.MACL;
   tempm = ((s32)(s16)tempn*(s32)(s16)tempm);

	//XXX: can make this better, dont use compare, use shift
   dest = (s32)sh->regs.MACL < 0;
   src = (s32)tempm < 0;
   tempn = -src;

   src += dest;
   sh->regs.MACL += tempm;
   ans = ((s32)sh->regs.MACL < 0) + dest;
   if (sh->regs.SR.part.S == 1)
   {
	 if (ans & !(src & 0x1))
		sh->regs.MACL = 0x7FFFFFFF + (src >> 1);
   }
   else
   {
      sh->regs.MACH += tempn + (templ > sh->regs.MACL);
   }
   sh->regs.PC+=2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2mov(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)]=sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2mova(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   sh->regs.R[0]=((sh->regs.PC+4)&0xFFFFFFFC)+(disp<<2);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movbl(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s8)mem_Read8(sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movbl0(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s8)mem_Read8(sh->regs.R[m] + sh->regs.R[0]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movbl4(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 disp = INSTRUCTION_D(sh->instruction);

   sh->regs.R[0] = (s32)(s8)mem_Read8(sh->regs.R[m] + disp);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movblg(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   sh->regs.R[0] = (s32)(s8)mem_Read8(sh->regs.GBR + disp);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movbm(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   mem_Write8((sh->regs.R[n] - 1),sh->regs.R[m]);
   sh->regs.R[n] -= 1;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movbp(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s8)mem_Read8(sh->regs.R[m]);
   sh->regs.R[m] += (n != m);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movbs(SH2_struct * sh)
{
   int b = INSTRUCTION_B(sh->instruction);
   int c = INSTRUCTION_C(sh->instruction);

   mem_Write8(sh->regs.R[b], sh->regs.R[c]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movbs0(SH2_struct * sh)
{
   mem_Write8(sh->regs.R[INSTRUCTION_B(sh->instruction)] + sh->regs.R[0],
                         sh->regs.R[INSTRUCTION_C(sh->instruction)]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movbs4(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_D(sh->instruction);
   s32 n = INSTRUCTION_C(sh->instruction);

   mem_Write8(sh->regs.R[n]+disp,sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movbsg(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   mem_Write8(sh->regs.GBR + disp,sh->regs.R[0]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movi(SH2_struct * sh)
{
   s32 i = INSTRUCTION_CD(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s8)i;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movli(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_CD(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = mem_Read32(((sh->regs.PC + 4) & 0xFFFFFFFC) + (disp << 2));
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movll(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] = mem_Read32(sh->regs.R[INSTRUCTION_C(sh->instruction)]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movll0(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] = mem_Read32(sh->regs.R[INSTRUCTION_C(sh->instruction)] + sh->regs.R[0]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movll4(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 disp = INSTRUCTION_D(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = mem_Read32(sh->regs.R[m] + (disp << 2));
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movllg(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   sh->regs.R[0] = mem_Read32(sh->regs.GBR + (disp << 2));
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movlm(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   mem_Write32(sh->regs.R[n] - 4,sh->regs.R[m]);
   sh->regs.R[n] -= 4;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movlp(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = mem_Read32(sh->regs.R[m]);
   sh->regs.R[m] += (n != m) << 2;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movls(SH2_struct * sh)
{
   int b = INSTRUCTION_B(sh->instruction);
   int c = INSTRUCTION_C(sh->instruction);

   mem_Write32(sh->regs.R[b], sh->regs.R[c]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movls0(SH2_struct * sh)
{
   mem_Write32(sh->regs.R[INSTRUCTION_B(sh->instruction)] + sh->regs.R[0],
                         sh->regs.R[INSTRUCTION_C(sh->instruction)]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movls4(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 disp = INSTRUCTION_D(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   mem_Write32(sh->regs.R[n]+(disp<<2),sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movlsg(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   mem_Write32(sh->regs.GBR+(disp<<2),sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movt(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] = (0x00000001 & sh->regs.SR.all);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movwi(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_CD(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s16)mem_Read16(sh->regs.PC + (disp<<1) + 4);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movwl(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s16)mem_Read16(sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movwl0(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s16)mem_Read16(sh->regs.R[m]+sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movwl4(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 disp = INSTRUCTION_D(sh->instruction);

   sh->regs.R[0] = (s32)(s16)mem_Read16(sh->regs.R[m]+(disp<<1));
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movwlg(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   sh->regs.R[0] = (s32)(s16)mem_Read16(sh->regs.GBR+(disp<<1));
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movwm(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   mem_Write16(sh->regs.R[n] - 2,sh->regs.R[m]);
   sh->regs.R[n] -= 2;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movwp(SH2_struct * sh)
{
   u32 m = INSTRUCTION_C(sh->instruction);
   u32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.R[n] = (s32)(s16)mem_Read16(sh->regs.R[m]);
   sh->regs.R[m] += (n != m) << 1;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movws(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   mem_Write16(sh->regs.R[n],sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movws0(SH2_struct * sh)
{
   mem_Write16(sh->regs.R[INSTRUCTION_B(sh->instruction)] + sh->regs.R[0],
                         sh->regs.R[INSTRUCTION_C(sh->instruction)]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movws4(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_D(sh->instruction);
   s32 n = INSTRUCTION_C(sh->instruction);

   mem_Write16(sh->regs.R[n]+(disp<<1),sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2movwsg(SH2_struct * sh)
{
   s32 disp = INSTRUCTION_CD(sh->instruction);

   mem_Write16(sh->regs.GBR+(disp<<1),sh->regs.R[0]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2mull(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.MACL = sh->regs.R[n] * sh->regs.R[m];
   sh->regs.PC+=2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2muls(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.MACL = ((s32)(s16)sh->regs.R[n]*(s32)(s16)sh->regs.R[m]);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2mulu(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.MACL = ((u32)(u16)sh->regs.R[n] * (u32)(u16)sh->regs.R[m]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2neg(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)]=0-sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2negc(SH2_struct * sh)
{
   u32 temp;
   u32 m = INSTRUCTION_C(sh->instruction);
   u32 n = INSTRUCTION_B(sh->instruction);

   temp = 0 - sh->regs.R[m];
   sh->regs.R[n] = temp - sh->regs.SR.part.T;
   //XXX: if temp is u32 then (0 < temp) == !!temp == (temp != 0)
   sh->regs.SR.part.T = (0 < temp) | (temp < sh->regs.R[n]);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2nop(SH2_struct * sh)
{
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2y_not(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] = ~sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2y_or(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] |= sh->regs.R[INSTRUCTION_C(sh->instruction)];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2ori(SH2_struct * sh)
{
   sh->regs.R[0] |= INSTRUCTION_CD(sh->instruction);
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2orm(SH2_struct * sh)
{
   s32 temp;
   s32 source = INSTRUCTION_CD(sh->instruction);

   temp = (s32) mem_Read8(sh->regs.GBR + sh->regs.R[0]);
   temp |= source;
   mem_Write8(sh->regs.GBR + sh->regs.R[0],temp);
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2rotcl(SH2_struct * sh)
{
	u32 temp;
	u32 n = INSTRUCTION_B(sh->instruction);

	temp = sh->regs.R[n] >> 31;
	sh->regs.R[n] = (sh->regs.R[n] << 1) | sh->regs.SR.part.T;
	sh->regs.SR.part.T = temp;
	sh->regs.PC+=2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2rotcr(SH2_struct * sh)
{
	u32 temp;
	u32 n = INSTRUCTION_B(sh->instruction);
	temp = sh->regs.R[n] & 1;

	sh->regs.R[n]>>=1;
	sh->regs.R[n] = (sh->regs.R[n] >> 1) | (sh->regs.SR.part.T << 31);
	sh->regs.SR.part.T = temp;
	sh->regs.PC+=2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////
//15 ins
static void FASTCALL SH2rotl(SH2_struct * sh)
{
	u32 n = INSTRUCTION_B(sh->instruction);
#if 1
	asm("rlwimi %0, %1, 1, 31, 31\n\t"
		"rlwinm %1, %1, 1, 0, 31"
		: "+r" (sh->regs.SR.all), "+r" (sh->regs.R[n])
	:);
#else
	sh->regs.SR.part.T = sh->regs.R[n] >> 31;
	sh->regs.R[n] = (sh->regs.R[n] << 1) | (sh->regs.SR.part.T);
#endif
	sh->regs.PC+=2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2rotr(SH2_struct * sh)
{
	u32 n = INSTRUCTION_B(sh->instruction);
	u32 tmp = sh->regs.R[n] & 0x1;
	sh->regs.SR.part.T = tmp;
	sh->regs.R[n] = (sh->regs.R[n] >> 1) | (tmp << 31);
	sh->regs.PC+=2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2rte(SH2_struct * sh)
{
   u32 temp;
   temp=sh->regs.PC;
   sh->regs.PC = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.SR.all = mem_Read32(sh->regs.R[15]) & 0x000003F3;
   sh->regs.R[15] += 4;
   sh->cycles += 4;
   SH2delay(sh, temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2rts(SH2_struct * sh)
{
   u32 temp;

   temp = sh->regs.PC;
   sh->regs.PC = sh->regs.PR;

   sh->cycles += 2;
   SH2delay(sh, temp + 2);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2sett(SH2_struct * sh)
{
   sh->regs.SR.part.T = 1;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shal(SH2_struct * sh)
{
	u32 n = INSTRUCTION_B(sh->instruction);
	sh->regs.SR.part.T = (sh->regs.R[n] >> 31);
	sh->regs.R[n] <<= 1;
	sh->regs.PC += 2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shar(SH2_struct * sh)
{
   u32 temp;
   u32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.SR.part.T = sh->regs.R[n] & 0x1;

   temp = (sh->regs.R[n] & 0x80000000);
   sh->regs.R[n] = (sh->regs.R[n] >> 1) | temp;

   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shll(SH2_struct * sh)
{
	s32 n = INSTRUCTION_B(sh->instruction);
	sh->regs.SR.part.T = sh->regs.R[n] >> 31;

	sh->regs.R[n]<<=1;
	sh->regs.PC+=2;
	sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shll2(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)] <<= 2;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shll8(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)]<<=8;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shll16(SH2_struct * sh)
{
   sh->regs.R[INSTRUCTION_B(sh->instruction)]<<=16;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shlr(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.SR.part.T = sh->regs.R[n] & 1;

   sh->regs.R[n]>>=1;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shlr2(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]>>=2;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shlr8(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]>>=8;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2shlr16(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]>>=16;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stcgbr(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]=sh->regs.GBR;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stcmgbr(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]-=4;
   mem_Write32(sh->regs.R[n],sh->regs.GBR);
   sh->regs.PC+=2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stcmsr(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]-=4;
   mem_Write32(sh->regs.R[n],sh->regs.SR.all);
   sh->regs.PC+=2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stcmvbr(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]-=4;
   mem_Write32(sh->regs.R[n],sh->regs.VBR);
   sh->regs.PC+=2;
   sh->cycles += 2;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stcsr(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] = sh->regs.SR.all;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stcvbr(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]=sh->regs.VBR;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stsmach(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]=sh->regs.MACH;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stsmacl(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]=sh->regs.MACL;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stsmmach(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] -= 4;
   mem_Write32(sh->regs.R[n],sh->regs.MACH);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stsmmacl(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] -= 4;
   mem_Write32(sh->regs.R[n],sh->regs.MACL);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stsmpr(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] -= 4;
   mem_Write32(sh->regs.R[n],sh->regs.PR);
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2stspr(SH2_struct * sh)
{
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n] = sh->regs.PR;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2sub(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   sh->regs.R[n]-=sh->regs.R[m];
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2subc(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   u32 tmp0,tmp1;

   tmp1 = sh->regs.R[n] - sh->regs.R[m];
   tmp0 = sh->regs.R[n];
   sh->regs.R[n] = tmp1 - sh->regs.SR.part.T;

   sh->regs.SR.part.T = (tmp0 < tmp1) | (tmp1 < sh->regs.R[n]);

   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2subv(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   s32 dest, src, ans;

   //XXX: Can do this better (use shift).
   dest = (s32)sh->regs.R[n] < 0;
   src = ((s32)sh->regs.R[m] < 0) + dest;
   sh->regs.R[n] -= sh->regs.R[m];

   ans = ((s32)sh->regs.R[n] < 0) + dest;

   sh->regs.SR.part.T = (ans & 1) & (src & 1);

   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2swapb(SH2_struct * sh)
{
   u32 temp0,temp1;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   temp0=sh->regs.R[m]&0xffff0000;
   temp1=(sh->regs.R[m]&0x000000ff)<<8;
   sh->regs.R[n]=(sh->regs.R[m]>>8)&0x000000ff;
   sh->regs.R[n]=sh->regs.R[n]|temp1|temp0;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2swapw(SH2_struct * sh)
{
   u32 temp;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);
   temp=(sh->regs.R[m]>>16)&0x0000FFFF;
   sh->regs.R[n]=sh->regs.R[m]<<16;
   sh->regs.R[n]|=temp;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2tas(SH2_struct * sh)
{
   s32 temp;
   s32 n = INSTRUCTION_B(sh->instruction);

   temp=(s32) mem_Read8(sh->regs.R[n]);

   sh->regs.SR.part.T = !temp;

   temp|=0x00000080;
   mem_Write8(sh->regs.R[n],temp);
   sh->regs.PC+=2;
   sh->cycles += 4;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2trapa(SH2_struct * sh)
{
   s32 imm = INSTRUCTION_CD(sh->instruction);

   sh->regs.R[15]-=4;
   mem_Write32(sh->regs.R[15],sh->regs.SR.all);
   sh->regs.R[15]-=4;
   mem_Write32(sh->regs.R[15],sh->regs.PC + 2);
   sh->regs.PC = mem_Read32(sh->regs.VBR+(imm<<2));
   sh->cycles += 8;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2tst(SH2_struct * sh)
{
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   sh->regs.SR.part.T = !(sh->regs.R[n] & sh->regs.R[m]);

   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2tsti(SH2_struct * sh)
{
   s32 temp;
   s32 i = INSTRUCTION_CD(sh->instruction);

   temp = sh->regs.R[0] & i;

   sh->regs.SR.part.T = !temp;

   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2tstm(SH2_struct * sh)
{
   s32 temp;
   s32 i = INSTRUCTION_CD(sh->instruction);

   temp=(s32) mem_Read8(sh->regs.GBR+sh->regs.R[0]);
   temp&=i;

   sh->regs.SR.part.T = !temp;

   sh->regs.PC+=2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2y_xor(SH2_struct * sh)
{
   int b = INSTRUCTION_B(sh->instruction);
   int c = INSTRUCTION_C(sh->instruction);

   sh->regs.R[b] ^= sh->regs.R[c];
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2xori(SH2_struct * sh)
{
   s32 source = INSTRUCTION_CD(sh->instruction);
   sh->regs.R[0] ^= source;
   sh->regs.PC += 2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2xorm(SH2_struct * sh)
{
   s32 source = INSTRUCTION_CD(sh->instruction);
   s32 temp;

   temp = (s32) mem_Read8(sh->regs.GBR + sh->regs.R[0]);
   temp ^= source;
   mem_Write8(sh->regs.GBR + sh->regs.R[0],temp);
   sh->regs.PC += 2;
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2xtrct(SH2_struct * sh)
{
   u32 temp;
   s32 m = INSTRUCTION_C(sh->instruction);
   s32 n = INSTRUCTION_B(sh->instruction);

   temp=(sh->regs.R[m]<<16)&0xFFFF0000;
   sh->regs.R[n]=(sh->regs.R[n]>>16)&0x0000FFFF;
   sh->regs.R[n]|=temp;
   sh->regs.PC+=2;
   sh->cycles++;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2sleep(SH2_struct * sh)
{
   sh->cycles += 3;
}

//////////////////////////////////////////////////////////////////////////////


opcodefunc opcode_0000_arr[64] ATTRIBUTE_ALIGN(32) = {
	sh2_IllegalInst, sh2_IllegalInst, SH2stcsr, SH2bsrf,
	SH2movbs0, SH2movws0, SH2movls0, SH2mull,
	SH2clrt, SH2nop, SH2stsmach, SH2rts,
	SH2movbl0, SH2movwl0, SH2movll0, SH2macl,
	sh2_IllegalInst, sh2_IllegalInst, SH2stcgbr, sh2_IllegalInst,
	SH2movbs0, SH2movws0, SH2movls0, SH2mull,
	SH2sett, SH2div0u, SH2stsmacl, SH2sleep,
	SH2movbl0, SH2movwl0, SH2movll0, SH2macl,
	sh2_IllegalInst, sh2_IllegalInst, SH2stcvbr, SH2braf,
	SH2movbs0, SH2movws0, SH2movls0, SH2mull,
	SH2clrmac, SH2movt, SH2stspr, SH2rte,
	SH2movbl0, SH2movwl0, SH2movll0, SH2macl,
	sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst,
	SH2movbs0, SH2movws0, SH2movls0, SH2mull,
	sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst,
	SH2movbl0, SH2movwl0, SH2movll0, SH2macl
};

opcodefunc opcode_0010_arr[16] ATTRIBUTE_ALIGN(32) = {
	SH2movbs, SH2movws, SH2movls, sh2_IllegalInst,
	SH2movbm, SH2movwm, SH2movlm, SH2div0s,
	SH2tst, SH2y_and, SH2y_xor, SH2y_or,
	SH2cmpstr, SH2xtrct, SH2mulu, SH2muls
};

opcodefunc opcode_0011_arr[16] ATTRIBUTE_ALIGN(32) = {
	SH2cmpeq, sh2_IllegalInst, SH2cmphs, SH2cmpge,
	SH2div1, SH2dmulu, SH2cmphi, SH2cmpgt,
	SH2sub, sh2_IllegalInst, SH2subc, SH2subv,
	SH2add, SH2dmuls, SH2addc, SH2addv
};

opcodefunc opcode_0100_arr[64] ATTRIBUTE_ALIGN(32) = {
	SH2shll, SH2shlr, SH2stsmmach, SH2stcmsr,
	SH2rotl, SH2rotr, SH2ldsmmach, SH2ldcmsr,
	SH2shll2, SH2shlr2, SH2ldsmach, SH2jsr,
	sh2_IllegalInst, sh2_IllegalInst, SH2ldcsr, SH2macw,
	SH2dt, SH2cmppz, SH2stsmmacl, SH2stcmgbr,
	sh2_IllegalInst, SH2cmppl, SH2ldsmmacl, SH2ldcmgbr,
	SH2shll8, SH2shlr8, SH2ldsmacl, SH2tas,
	sh2_IllegalInst, sh2_IllegalInst, SH2ldcgbr, SH2macw,
	SH2shal, SH2shar, SH2stsmpr, SH2stcmvbr,
	SH2rotcl, SH2rotcr, SH2ldsmpr, SH2ldcmvbr,
	SH2shll16, SH2shlr16, SH2ldspr, SH2jmp,
	sh2_IllegalInst, sh2_IllegalInst, SH2ldcvbr, SH2macw,
	sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst,
	sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst,
	sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst,
	sh2_IllegalInst, sh2_IllegalInst, sh2_IllegalInst, SH2macw
};

opcodefunc opcode_0110_arr[16] ATTRIBUTE_ALIGN(32) = {
	SH2movbl, SH2movwl, SH2movll, SH2mov,
	SH2movbp, SH2movwp, SH2movlp, SH2y_not,
	SH2swapb, SH2swapw, SH2negc, SH2neg,
	SH2extub, SH2extuw, SH2extsb, SH2extsw
};

opcodefunc opcode_1000_arr[16] ATTRIBUTE_ALIGN(32) = {
	SH2movbs4, SH2movws4, sh2_IllegalInst, sh2_IllegalInst,
	SH2movbl4, SH2movwl4, sh2_IllegalInst, sh2_IllegalInst,
	SH2cmpim, SH2bt, sh2_IllegalInst, SH2bf,
	sh2_IllegalInst, SH2bts, sh2_IllegalInst, SH2bfs
};

opcodefunc opcode_1100_arr[16] ATTRIBUTE_ALIGN(32) = {
	SH2movbsg, SH2movwsg, SH2movlsg, SH2trapa,
	SH2movblg, SH2movwlg, SH2movllg, SH2mova,
	SH2tsti, SH2andi, SH2xori, SH2ori,
	SH2tstm, SH2andm, SH2xorm, SH2orm
};

static void sh2_opcode0000(SH2_struct * sh) { opcode_0000_arr[sh->instruction & 0x3F](sh); }
static void sh2_opcode0010(SH2_struct * sh) { opcode_0010_arr[sh->instruction & 0xF](sh); }
static void sh2_opcode0011(SH2_struct * sh) { opcode_0011_arr[sh->instruction & 0xF](sh); }
static void sh2_opcode0100(SH2_struct * sh) { opcode_0100_arr[sh->instruction & 0x3F](sh); }
static void sh2_opcode0110(SH2_struct * sh) { opcode_0110_arr[sh->instruction & 0xF](sh); }
static void sh2_opcode1000(SH2_struct * sh) { opcode_1000_arr[(sh->instruction >> 8) & 0xF](sh); }
static void sh2_opcode1100(SH2_struct * sh) { opcode_1100_arr[(sh->instruction >> 8) & 0xF](sh); }


opcodefunc opcode_arr[16] ATTRIBUTE_ALIGN(32) = {
	//Opcodes 0000xxxxxxxxxxx : 64 entries
	sh2_opcode0000,
	//Opcodes 0001xxxxxxxxxxx : 1 entrie
	SH2movls4,
	//Opcodes 0010xxxxxxxxxxx : 16 entries
	sh2_opcode0010,
	//Opcodes 0011xxxxxxxxxxx : 16 entries
	sh2_opcode0011,
	//Opcodes 0100xxxxxxxxxxx : 64 entries
	sh2_opcode0100,
	//Opcodes 0101xxxxxxxxxxx : 1 entries
	SH2movll4,
	//Opcodes 0110xxxxxxxxxxx : 16 entries
	sh2_opcode0110,
	//Opcodes 0111xxxxxxxxxxx : 1 entries
	SH2addi,
	//Opcodes 1000xxxxxxxxxxx : 16 entries
	sh2_opcode1000,
	//Opcodes 1001xxxxxxxxxxx : 1 entries
	SH2movwi,
	//Opcodes 1010xxxxxxxxxxx : 1 entries
	SH2bra,
	//Opcodes 1011xxxxxxxxxxx : 1 entries
	SH2bsr,
	//Opcodes 1100xxxxxxxxxxx : 16 entries
	sh2_opcode1100,
	//Opcodes 1101xxxxxxxxxxx : 1 entries
	SH2movli,
	//Opcodes 1110xxxxxxxxxxx : 1 entries
	SH2movi,
	//Opcodes 1111xxxxxxxxxxx : 1 entries
	sh2_IllegalInst,
};



//////////////////////////////////////////////////////////////////////////////

static void FASTCALL SH2delay(SH2_struct * sh, u32 addr)
{
	#ifdef SH2_TRACE
	sh2_trace(sh, addr);
	#endif

	// Fetch Instruction
	#ifdef EXEC_FROM_CACHE
	if ((addr & 0xC0000000) == 0xC0000000) sh->instruction = DataArrayReadWord(addr);
	else
		#endif
		sh->instruction = fetchlist[(addr >> 20) & 0x0FF](addr);

	// Execute it
	opcode_arr[(sh->instruction >> 12) & 0xF](sh);

	sh->regs.PC -= 2;
}


//////////////////////////////////////////////////////////////////////////////

int SH2InterpreterInit()
{
   int i;

   for (i = 0; i < 0x100; i++)
   {
      switch (i)
      {
         case 0x000: // Bios
            fetchlist[i] = FetchBios;
            break;
         case 0x002: // Low Work Ram
            fetchlist[i] = FetchLWram;
            break;
         case 0x020: // CS0
            fetchlist[i] = FetchCs0;
            break;
         case 0x060: // High Work Ram
         case 0x061:
         case 0x062:
         case 0x063:
         case 0x064:
         case 0x065:
         case 0x066:
         case 0x067:
         case 0x068:
         case 0x069:
         case 0x06A:
         case 0x06B:
         case 0x06C:
         case 0x06D:
         case 0x06E:
         case 0x06F:
            fetchlist[i] = FetchHWram;
            break;
         default:
            fetchlist[i] = FetchInvalid;
            break;
      }
   }

   MSH2->breakpointEnabled = 0;
   SSH2->breakpointEnabled = 0;

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterDeInit()
{
   // DeInitialize any internal variables here
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterReset(UNUSED SH2_struct *context)
{
   // Reset any internal variables here
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void SH2UBCInterrupt(SH2_struct *context, u32 flag)
{
   if (15 > context->regs.SR.part.I) // Since UBC's interrupt are always level 15
   {
      context->regs.R[15] -= 4;
      mem_Write32(context->regs.R[15], context->regs.SR.all);
      context->regs.R[15] -= 4;
      mem_Write32(context->regs.R[15], context->regs.PC);
      context->regs.SR.part.I = 15;
      context->regs.PC = mem_Read32(context->regs.VBR + (12 << 2));
      LOG("interrupt successfully handled\n");
   }
   context->onchip.BRCR |= flag;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void SH2HandleInterrupts(SH2_struct *context)
{
   if (context->NumberOfInterrupts != 0)
   {
      if (context->interrupts[context->NumberOfInterrupts-1].level > context->regs.SR.part.I)
      {
         context->regs.R[15] -= 4;
         mem_Write32(context->regs.R[15], context->regs.SR.all);
         context->regs.R[15] -= 4;
         mem_Write32(context->regs.R[15], context->regs.PC);
         context->regs.SR.part.I = context->interrupts[context->NumberOfInterrupts-1].level;
         context->regs.PC = mem_Read32(context->regs.VBR + (context->interrupts[context->NumberOfInterrupts-1].vector << 2));
         context->NumberOfInterrupts--;
         context->isIdle = 0;
         context->isSleeping = 0;
      }
   }
}


//////////////////////////////////////////////////////////////////////////////

FASTCALL void SH2InterpreterExec(SH2_struct *context, u32 cycles)
{
	SH2HandleInterrupts(context);

	if (context->isIdle)
		SH2idleParse(context, cycles);
	else
		SH2idleCheck(context, cycles);

   while(context->cycles < cycles)
   {
      // Fetch Instruction and execute it
      context->instruction = fetchlist[(context->regs.PC >> 20) & 0x0FF](context->regs.PC);
	  opcode_arr[(context->instruction >> 12) & 0xF](context);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterGetRegisters(SH2_struct *context, sh2regs_struct *regs)
{
   memcpy(regs, &context->regs, sizeof(sh2regs_struct));
}

//////////////////////////////////////////////////////////////////////////////

u32 SH2InterpreterGetGPR(SH2_struct *context, int num)
{
    return context->regs.R[num];
}

//////////////////////////////////////////////////////////////////////////////

u32 SH2InterpreterGetSR(SH2_struct *context)
{
    return context->regs.SR.all;
}

//////////////////////////////////////////////////////////////////////////////

u32 SH2InterpreterGetGBR(SH2_struct *context)
{
    return context->regs.GBR;
}

//////////////////////////////////////////////////////////////////////////////

u32 SH2InterpreterGetVBR(SH2_struct *context)
{
    return context->regs.VBR;
}

//////////////////////////////////////////////////////////////////////////////

u32 SH2InterpreterGetMACH(SH2_struct *context)
{
    return context->regs.MACH;
}

//////////////////////////////////////////////////////////////////////////////

u32 SH2InterpreterGetMACL(SH2_struct *context)
{
    return context->regs.MACL;
}

//////////////////////////////////////////////////////////////////////////////

u32 SH2InterpreterGetPR(SH2_struct *context)
{
    return context->regs.PR;
}

//////////////////////////////////////////////////////////////////////////////

u32 SH2InterpreterGetPC(SH2_struct *context)
{
    return context->regs.PC;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetRegisters(SH2_struct *context, const sh2regs_struct *regs)
{
   memcpy(&context->regs, regs, sizeof(sh2regs_struct));
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetGPR(SH2_struct *context, int num, u32 value)
{
    context->regs.R[num] = value;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetSR(SH2_struct *context, u32 value)
{
    context->regs.SR.all = value;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetGBR(SH2_struct *context, u32 value)
{
    context->regs.GBR = value;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetVBR(SH2_struct *context, u32 value)
{
    context->regs.VBR = value;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetMACH(SH2_struct *context, u32 value)
{
    context->regs.MACH = value;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetMACL(SH2_struct *context, u32 value)
{
    context->regs.MACL = value;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetPR(SH2_struct *context, u32 value)
{
    context->regs.PR = value;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetPC(SH2_struct *context, u32 value)
{
    context->regs.PC = value;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSendInterrupt(SH2_struct *context, u8 vector, u8 level)
{
   u32 i, i2;
   interrupt_struct tmp;

   // Make sure interrupt doesn't already exist
   for (i = 0; i < context->NumberOfInterrupts; i++)
   {
      if (context->interrupts[i].vector == vector)
         return;
   }

   context->interrupts[context->NumberOfInterrupts].level = level;
   context->interrupts[context->NumberOfInterrupts].vector = vector;
   context->NumberOfInterrupts++;

   // Sort interrupts
   for (i = 0; i < (context->NumberOfInterrupts-1); i++)
   {
      for (i2 = i+1; i2 < context->NumberOfInterrupts; i2++)
      {
         if (context->interrupts[i].level > context->interrupts[i2].level)
         {
            tmp.level = context->interrupts[i].level;
            tmp.vector = context->interrupts[i].vector;
            context->interrupts[i].level = context->interrupts[i2].level;
            context->interrupts[i].vector = context->interrupts[i2].vector;
            context->interrupts[i2].level = tmp.level;
            context->interrupts[i2].vector = tmp.vector;
         }
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

int SH2InterpreterGetInterrupts(SH2_struct *context,
                                interrupt_struct interrupts[MAX_INTERRUPTS])
{
   memcpy(interrupts, context->interrupts, sizeof(interrupt_struct) * MAX_INTERRUPTS);
   return context->NumberOfInterrupts;
}

//////////////////////////////////////////////////////////////////////////////

void SH2InterpreterSetInterrupts(SH2_struct *context, int num_interrupts,
                                 const interrupt_struct interrupts[MAX_INTERRUPTS])
{
   memcpy(context->interrupts, interrupts, sizeof(interrupt_struct) * MAX_INTERRUPTS);
   context->NumberOfInterrupts = num_interrupts;
}

//////////////////////////////////////////////////////////////////////////////
