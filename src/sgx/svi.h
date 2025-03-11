#ifndef __SVI_H__
#define __SVI_H__

#include <gccore.h>

#define SS_DISP_WIDTH		320
#define SS_DISP_HEIGHT		224

void SVI_Init(void);
void SVI_CopyXFB();
void SVI_SwapBuffers(u32 wait_vsync);
void SVI_SetResolution(u32 tvmd);
void SVI_EndFrame(u32 black);

#endif //__SVI_H__
