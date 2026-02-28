


#ifndef __SH2_COMPILER_H__
#define __SH2_COMPILER_H__

#include <gccore.h>


void HashClearAll(void);
void HashClearRange(u32 start_addr, u32 end_addr);

void sh2_DrcInit(void);
void sh2_DrcReset(void);
s32 sh2_DrcExec(SH2 *sh, s32 cycles);

#endif /* __SH2_COMPILER_H__ */