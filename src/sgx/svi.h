#ifndef __SVI_H__
#define __SVI_H__

#include <gccore.h>

#define SS_DISP_WIDTH		320
#define SS_DISP_HEIGHT		224

void SVI_Init(void);
void SVI_CopyXFB(u32 x, u32 y);
void SVI_SwapBuffers(u32 wait_vsync);
void SVI_SetResolution(u32 tvmd);
void SVI_CopyFrame(void);
void SVI_ClearFrame(void);
void SVI_ResetViewport(void);

#endif //__SVI_H__
