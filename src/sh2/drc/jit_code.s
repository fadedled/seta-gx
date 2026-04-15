
#include <ppc-asm.h>
#include <ogc/machine/asm.h>

	.extern _jit_GenBlock
	.extern HashGet
	.extern sh2_HandleInterrupt

FUNC_START(jit_enter)
	stwu r1, -92(r1)
	mflr r0
	stw r0, (92+4)(r1)
	stmw r14, (92-((31-14)*4))(r1)	//store host regs
	lmw r14, 0(r3)					//load sh2 regs
	mr r31, r3						//get sh2 context
	addi r3, r0, 0
	b jit_endblock_test
FUNC_END(jit_enter)

FUNC_START(jit_exit)
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
	stw r30, (16*4)(r31)
	lwz r0, (92+4)(r1)
	mtlr r0
	lmw r14, (92 - ((31-14)*4))(r1)
	addi r1, r1, 92
	blr
FUNC_END(jit_exit)


FUNC_START(jit_endblock_test)
	//check if we need to exit because we finished cycles
	lwz r4, (25*4)(r31)  			//load cycles
	add. r4, r4, r3 				//update and check if > 0
	stw r4, (25*4)(r31)				//store cycles
	bc 0b01100, 1, jit_exit			//move to exit if cycles > 0
	//TODO: check for interrupts
	//mr r3, r31
	//bl sh2_HandleInterrupt
	//lwz r29, (15*4)(r31)
	//lwz r30, (16*4)(r31)
	lwz r3, (17*4)(r31)				//get the next block
	bl HashGet
	lwz r4, (17*4)(r31)
	lis r5, _jit_GenBlock@ha
	ori r5, r5, _jit_GenBlock@l
	mtctr r5
	lwz r5, 0(r3)					//Get block address
	cmpi cr0,0,r5,0
	bcctrl 0b01100, 2 				//Generate block if not found
	lwz r3, 0(r3)					//Get block address
	mtctr r3
	bcctr 0b10100, 1				//move to exit if cycles > 0
FUNC_END(jit_endblock_test)

//Use r8 and r9 for rn and rm
FUNC_START(jit_div1)
	rlwinm r8, r8, 1, 0, 31
	andi. r5, r8, 1
	rlwimi r8, r30, 0, 31, 31
	rlwimi r30, r5, 0, 31, 31
	rlwinm r3, r30, 32-8, 31, 31
	rlwinm r4, r30, 32-9, 31, 31
	xor r3, r3, r4
	addi r3, r3, -1
	mr r7, r8
	xor r9, r9, r3
	srawi r3, r3, 1
	addeo r8, r8, r9
	xor r3, r7, r8
	rlwinm r3, r3, 1, 31, 31
	xor r5, r5, r3
	rlwinm r30, r5, 8, 31-8, 31-8
	xor r5, r5, r4
	xori r5, r5, 1
	rlwimi r30, r5, 0, 31, 31
	blr
FUNC_END(jit_div1)


//Use r3 and r4 for @rn and @rm
FUNC_START(jit_macl)
	mullw r9,r3,r4
	xor r8,r3,r4
	rlwinm r5, r30, 31,31,31
	lwz r7, (22*4)(r31)  		//load macl
	addi r10,r5,-1
	mulhw r3,r3,r4
	lwz r6, (21*4)(r31)  		//load mach
	addc r9,r9,r7
	and r9,r9,r10
	adde r3,r3,r6
	srawi r8,r8,31
	subfic r5,r5,0
	xori r7,r8,0x7fff
	subfe r6,r6,r6
	and r3,r3,r10
	andc r5,r5,r8
	and r7,r7,r6
	or r9,r9,r5
	or r3,r3,r7
	stw r9, (22*4)(r31)  			//store macl
	stw r3, (21*4)(r31)  			//store mach
	blr
FUNC_END(jit_macl)


//Use r3 and r4 for @rn and @rm
FUNC_START(jit_macw)
	mullw r3, r3, r4
	lwz r6, (22*4)(r31)  		//load macl
	lwz r7, (21*4)(r31)  		//load mach

	andi. r5, r3, 0x2			//Check for saturation
	bc 0b10100, 1, saturate
	addc r6, r6, r3
	addze r7, r7,
	//only add the
	b done
saturate:
	addo r3, r6, r3
	mfxer r5
	rlwinm r5, r5, 2, 31, 31
	neg r5, r5
	andc r6, r3, r5
	srawi r3, r3, 31
	addis r3, r3, 0x8000
	and r3, r3, r5
	or r6, r6, r5
	ori r7, r5, 0x1

done:
	stw r6, (22*4)(r31)  			//store macl
	stw r7, (21*4)(r31)  			//store mach
	blr
FUNC_END(jit_macw)