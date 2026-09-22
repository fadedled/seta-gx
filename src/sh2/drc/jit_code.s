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

#include <ppc-asm.h>
#include <ogc/machine/asm.h>

	.extern _jit_GenBlock
	.extern HashGet
	.extern sh2_HandleInterrupt
	.extern HashClearRange

//r3 contains sh2 context and r4 the number of cycles to exectue
FUNC_START(sh2_DrcExec)
	stwu r1, -92(r1)
	mflr r0
	stw r0, (92+4)(r1)
	stmw r14, (92-((31-14)*4))(r1)	//store host regs
	mr r31, r3						//move to sh2 context (r31)
	lwz r5, (25*4)(r31)  			//load cycles
	subf r5, r4, r5 				//subtract the cycles we need to run
	stw r5, (25*4)(r31)				//store cycles
	bl sh2_HandleInterrupt			//handle interrupts at the start
	//Load sh2 regs
	lwz r14, (0*4)(r31)
	lwz r15, (1*4)(r31)
	lwz r16, (2*4)(r31)
	lwz r17, (3*4)(r31)
	lwz r18, (4*4)(r31)
	lwz r19, (5*4)(r31)
	lwz r20, (6*4)(r31)
	lwz r21, (7*4)(r31)
	lwz r22, (8*4)(r31)
	lwz r23, (9*4)(r31)
	lwz r24, (10*4)(r31)
	lwz r25, (11*4)(r31)
	lwz r26, (12*4)(r31)
	lwz r27, (13*4)(r31)
	lwz r28, (14*4)(r31)
	lwz r29, (15*4)(r31)			//load R15 register
	lwz r30, (16*4)(r31)			//load SR register
	li r3, 0						//clear r3
	b jit_endblock_test
FUNC_END(sh2_DrcExec)


FUNC_START(jit_exit)
	//Store all the sh2 regs
	stw r14, (0*4)(r31)
	stw r15, (1*4)(r31)
	stw r16, (2*4)(r31)
	stw r17, (3*4)(r31)
	stw r18, (4*4)(r31)
	stw r19, (5*4)(r31)
	stw r20, (6*4)(r31)
	stw r21, (7*4)(r31)
	stw r22, (8*4)(r31)
	stw r23, (9*4)(r31)
	stw r24, (10*4)(r31)
	stw r25, (11*4)(r31)
	stw r26, (12*4)(r31)
	stw r27, (13*4)(r31)
	stw r28, (14*4)(r31)
	stw r29, (15*4)(r31)
	stw r30, (16*4)(r31) //SR
	//Load host regs and return to SH2Exec
	lwz r0, (92+4)(r1)
	mtlr r0
	lmw r14, (92 - ((31-14)*4))(r1)
	addi r1, r1, 92
	blr
FUNC_END(jit_exit)

//In r3 are the number cycles that are added to the sh2 cycles
FUNC_START(jit_endblock_test)
	//check if we need to exit because we finished cycles
	lwz r4, (25*4)(r31)  			//load cycles
	add. r4, r4, r3 				//update and check if cycles >= 0
	stw r4, (25*4)(r31)				//store cycles
	bc 0b00100, 0, jit_exit			//move to JIT exit if cycles >= 0
	lwz r3, (17*4)(r31)				//get the next block from PC
	bl HashGet
	mtctr r3						//Set block (could be _jit_GenBlock)
	lwz r3, (17*4)(r31)				//reload PC if block was not found
	//lwz r5, 0(r3)					//Get block address (could be _jit_GenBlock)
	bcctrl 0b10100, 1				//execute block (linked if _jit_GenBlock is called)
	mtctr r3
	bcctr 0b10100, 1				//execute block
FUNC_END(jit_endblock_test)


//r3 number of cycles, r4 destination block address
FUNC_START(jit_known_dest_endblock_test)
	//check if we need to exit because we finished cycles
	lwz r5, (25*4)(r31)				//load cycles
	add. r5, r5, r3 				//update and check if cycles >= 0
	stw r5, (25*4)(r31)				//store cycles
	bc 0b00100, 0, jit_exit			//move to JIT exit if cycles >= 0
	lwz r3, 0(r4)					//fetch block address from previous block
	mtctr r3						//Set block (could be _jit_GenBlock)
	lwz r3, (17*4)(r31)				//load PC if we need to recompile block
	bcctrl 0b10100, 1				//execute block (linked if _jit_GenBlock is called)
	mtctr r3
	bcctr 0b10100, 1				//execute block
FUNC_END(jit_known_dest_endblock_test)

//================================
//Implementation of DIV1/MACL/MACW
//================================

//Use r3 and r4 for rn and rm
FUNC_START(jit_div1)
	mtcrf	0x07, r30
	creqv	3, 31-9, 31-8	//(M == oldQ)
	bc	    0b00100, 3,	div1_skip_rm_neg // do not negate if false
	neg     r4, r4					//Rm = -Rm
div1_skip_rm_neg:
	mtcrf	0x80, r3			//Q = MSB(Rn)
	crxor   31-8, 0, 31-8		//Q ^= oldQ
	rlwimi  r30, r3, 1, 0, 30	// Rn = (Rn << 1) | T
	addc    r3, r30, r4			// Rn += Rm
	mcrxr   0					// Q ^= carry(Rn += Rm)
	creqv   31-8, 2, 31-8
	creqv   31, 31-8, 31-9		//T = (Q == M)
	mfcr    r5
	andi.	r30, r5, 0xFFF
	blr
FUNC_END(jit_div1)


//Use r3 and r4 for @rn and @rm
FUNC_START(jit_macl)
	lwz   r6, (21*4)(r31)  		// load mach
	mullw r5, r3, r4
	mulhw r4, r3, r4
	lwz   r7, (22*4)(r31)  		// load macl
	andi. r8, r30, 0x2			// check for S bit
	addc  r7, r7, r5
	bc 0b01100, 1, macl_saturate	//if S=1 then do saturation
	adde  r6, r6, r4
	stw   r6, (21*4)(r31)
	stw   r7, (22*4)(r31)
	blr
macl_saturate:
	extsh r4, r4
	extsh r6, r6
	srawi r3, r6, 31
	addeo r6, r6, r4
	bc 0b00100, 3, macl_end
	xori  r6, r3, 0xFFFF
	not   r7, r3
macl_end:
	stw   r6, (21*4)(r31)
	stw   r7, (22*4)(r31)
	blr

//	mullw r8, r3, r4			// calc low(rn*rm)
//	xor r9, r3, r4				// xor rn * rm and MSB for flipping saturation
//	srawi r9, r9, 31			// extend MSB
//	lwz r6, (22*4)(r31)  		// load macl
//	mulhw r7, r3, r4			// calc high(rn*rm)
//	addc r6, r6, r8				// result low macl + low(rn*rm)
//	mtcrf 0x01, r30		 		// is S set? move S to CR
//	lwz r5, (21*4)(r31)  		// load mach
//	adde r5, r5, r7				// result high macl + high(rn*rm)
//	addic. r8, r5, -0x8000		// add 0xFFFF8000_00000000 to result
//	crandc 0, 30, 0				// is S set and is it negative?
//	bc	0b00101, 0,	macl_skip_saturation // branch to end if false
//	xori r5, r9, 0x7FFF
//	not r6, r9
//macl_skip_saturation:
//	stw r5, (21*4)(r31)  		// store mach
//	stw r6, (22*4)(r31)  		// store macl
//	blr

FUNC_END(jit_macl)


//Use r3 and r4 for @rn and @rm
FUNC_START(jit_macw)
	mullw r3, r3, r4
	lwz r6, (22*4)(r31)  		//load macl
	lwz r7, (21*4)(r31)  		//load mach
	andi. r5, r30, 0x2			//Check for saturation (S bit)
	bc 0b01100, 1, macw_saturate
	srawi r4, r3, 31
	addc r6, r6, r3 			//TODO: Add lower, this can be optimized with addco
	adde r7, r7, r4
	stw r6, (22*4)(r31)  			//store macl
	stw r7, (21*4)(r31)  			//store mach
	blr
macw_saturate:
	addo r3, r6, r3
	mfxer r5
	rlwinm r5, r5, 2, 31, 31
	neg r5, r5
	andc r6, r3, r5
	srawi r3, r3, 31
	addis r3, r3, 0x8000
	and r3, r3, r5
	or r6, r6, r3
	ori r7, r5, 0x1
	stw r6, (22*4)(r31)  			//store macl
	stw r7, (21*4)(r31)  			//store mach
	blr
FUNC_END(jit_macw)


/*Common idle loop: found in Cotton-Boomerang, cotton 2, princess Crown, radiant silvergun
	C6F3 - mov.l @(0x3CC, gbr), r0 //TODO: CHECK THIS INSTRUCTION
	400b - jsr @r0
	0009 - nop
	C413 - mov.b @(0x13, gbr), r0
	4011 - cmp/pz r0
	89F9 - bt 0x60370D8
	 -> 000B rts
	 -> 0009 nop
*/
FUNC_START(jit_idle_loop0)

//C6F3 - mov.l @(0x3CC, gbr), r0
	lwz    r3, (19*4)(r31)     // load gbr+0x3CC
	addi   r3, r3, 0x3CC
	bl     sh2_Read32
	mr     r14, r3		       //store in r0

//400b - jsr @r0
//0009 - nop
	//TODO: check if it jumps to a 0x000B0009 (rts/nop)
	bl     sh2_GetPCAddr
	lwz    r3, 0(r3)		//get instructionpair
	lis    r4, 0x000B
	ori    r4, r4, 0x0009
	cmp    cr0, r3, r4
	bc     0b01100, 2, no_jsr	//if true (equal) then skip the jsr instruction
	lwz    r3, (17*4)(r31)		//Do JSR (should be +6 for pr)
	addi   r3, r3, 6
	stw    r3, (18*4)(r31)
	stw    r14, (17*4)(r31)
	li     r3, 4
	b      jit_endblock_test

//C413 - mov.b @(0x13, gbr), r0
no_jsr:
	lwz    r3, (19*4)(r31)     // load gbr+0x13
	addi   r3, r3, 0x13
	bl     sh2_Read8
	extsb  r14, r3		       //store in r0

//4011 - cmp/pz r0
	rlwimi r30, r14, 1, 31, 31 //Store MSB in T
	xori   r30, r30, 1			//negate T

//89F9 - bt to begining of block
	andi.  r4, r30, 0x0001
	bc     0b00100, 2, skip_idle0
	lwz    r3, (17*4)(r31)		//Update PC (should be 12 from current pc)
	addi   r3, r3, 12
	stw    r3, (17*4)(r31)
	li     r3, 11		        //Update cycles (should be 8 + 3 from rts/nop)
	b      jit_endblock_test
skip_idle0:
	//no need to update pc since it's a loop
	addi   r4, 0, 0				//Set cycles to zero and exit
	stw    r4, (25*4)(r31)
	b      jit_exit
FUNC_END(jit_idle_loop0)
