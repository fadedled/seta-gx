/*  Copyright 2003-2006 Guillaume Duhamel
    Copyright 2004-2007 Theo Berkau

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

#ifndef VIDSHARED_H
#define VIDSHARED_H

#include "core.h"
#include "vdp2.h"
#include "debug.h"


typedef struct
{
   short LineScrollValH;
   short LineScrollValV;
   int CoordinateIncH;
} vdp2Lineinfo;

typedef struct
{
   u32 PlaneAddrv[16];
   float Xst;
   float Yst;
   float Zst;
   float deltaXst;
   float deltaYst;
   float deltaX;
   float deltaY;
   float A;
   float B;
   float C;
   float D;
   float E;
   float F;
   float Px;
   float Py;
   float Pz;
   float Cx;
   float Cy;
   float Cz;
   float Mx;
   float My;
   float kx;
   float ky;
   float KAst;
   float deltaKAst;
   float deltaKAx;
   u32 coeftbladdr;
   int coefenab;
   int coefmode;
   int coefdatasize;
   int use_coef_for_linecolor;
   float Xp;
   float Yp;
   float dX;
   float dY;
   int screenover;
   int msb;
   u32 charaddr;
   int planew, planew_bits, planeh, planeh_bits;
   int MaxH,MaxV;
   float Xsp;
   float Ysp;
   float dx;
   float dy;
   float lkx;
   float lky;
   int KtablV;
   int ShiftPaneX;
   int ShiftPaneY;
   int MskH;
   int MskV;
   u32 lineaddr;
   u32 k_mem_type;
   u32 over_pattern_name;
   int linecoefenab;
   void FASTCALL(*PlaneAddr)(void *, int, Vdp2*);
} vdp2rotationparameter_struct;

typedef struct
{
   int  WinShowLine;
    int WinHStart;
    int WinHEnd;
} vdp2WindowInfo;

typedef u32 FASTCALL (*Vdp2ColorRamGetColor_func)(void *, u32 , int);
typedef vdp2rotationparameter_struct * FASTCALL (*Vdp2GetRParam_func)(void *, int, int);

typedef struct
{
   int vertices[8];
   int cellw, cellh;
   int flipfunction;
   int priority;
   int dst;
   int uclipmode;
   int blendmode;
   /* The above fields MUST NOT BE CHANGED (including inserting new fields)
    * unless YglSprite is also updated in ygl.h */

   int cellw_bits, cellh_bits;
   int mapwh;
   int planew, planew_bits, planeh, planeh_bits;
   int pagewh, pagewh_bits;
   int patternwh, patternwh_bits;
   int patterndatasize, patterndatasize_bits;
   int specialfunction;
   int specialcolorfunction;
   int specialcolormode;
   int specialcode;
   u32 addr, charaddr, paladdr;
   int colornumber;
   int isbitmap;
   u16 supplementdata;
   int auxmode;
   int enable;
   int x, y;
   int sh,sv;
   int alpha;
   int coloroffset;
   int transparencyenable;
   int specialprimode;

   s32 cor;
   s32 cog;
   s32 cob;
   u32 oldpixel;
   int colorcalcmode;
   int docolorcalcenable;

   float coordincx, coordincy;
   void FASTCALL (* PlaneAddr)(void *, int);
   u32 FASTCALL (*Vdp2ColorRamGetColor)(void *, u32 , int );
   u32 FASTCALL (*PostPixelFetchCalc)(void *, u32);
   u32 FASTCALL (*PostPixelFetchCalcBIG)(void *, u32);
   int patternpixelwh;
   int draww;
   int drawh;
   int rotatenum;
   int rotatemode;
   int mosaicxmask;
   int mosaicymask;
   int islinescroll;
   u32 linescrolltbl;
   u32 lineinc;
   vdp2Lineinfo * lineinfo;
   int wctl;
   int islinewindow;
   int isverticalscroll;
   u32 verticalscrolltbl;
   int verticalscrollinc;
   int linescreen;
   u32 prioffs;

   // WindowMode
   u8  LogicWin;    // Window Logic AND OR
   u8  bEnWin0;     // Enable Window0
   u8  bEnWin1;     // Enable Window1
   u8  WindowArea0; // Window Area Mode 0
   u8  WindowArea1; // Window Area Mode 1

   // Rotate Screen
   vdp2WindowInfo * pWinInfo;
   int WindwAreaMode;
   vdp2rotationparameter_struct * FASTCALL (*GetKValueA)(vdp2rotationparameter_struct*,int);
   vdp2rotationparameter_struct * FASTCALL (*GetKValueB)(vdp2rotationparameter_struct*,int);
   vdp2rotationparameter_struct * FASTCALL (*GetRParam)(void *, int h,int v);
   u32 LineColorBase;

   void (*LoadLineParams)(void *, u32 line);
} vdp2draw_struct;



#define FP_SIZE 16
typedef s32 fixed32;

typedef struct
{
   fixed32 Xst;
   fixed32 Yst;
   fixed32 Zst;
   fixed32 deltaXst;
   fixed32 deltaYst;
   fixed32 deltaX;
   fixed32 deltaY;
   fixed32 A;
   fixed32 B;
   fixed32 C;
   fixed32 D;
   fixed32 E;
   fixed32 F;
   fixed32 Px;
   fixed32 Py;
   fixed32 Pz;
   fixed32 Cx;
   fixed32 Cy;
   fixed32 Cz;
   fixed32 Mx;
   fixed32 My;
   fixed32 kx;
   fixed32 ky;
   fixed32 KAst;
   fixed32 deltaKAst;
   fixed32 deltaKAx;
   u32 coeftbladdr;
   int coefenab;
   int coefmode;
   int coefdatasize;
   fixed32 Xp;
   fixed32 Yp;
   fixed32 dX;
   fixed32 dY;
   int screenover;
   int msb;
   int linescreen;

   void FASTCALL (* PlaneAddr)(void *, int);
} vdp2rotationparameterfp_struct;

typedef struct
{
   int xstart, ystart;
   int xend, yend;
} clipping_struct;

#ifndef NOGEKKO
#define tofixed(v) ((v) * (1 << FP_SIZE))
#define toint(v) ((v) >> FP_SIZE)
#define touint(v) ((u16)((v) >> FP_SIZE))
#define tofloat(v) ((float)(v) / (float)(1 << FP_SIZE))
#define mulfixed(a,b) ((fixed32)((s64)(a) * (s64)(b) >> FP_SIZE))
#else
// a litte fast, but not accurate
#define tofixed(v)                     \
({fixed32 _v=(v);                      \
    asm volatile("slwi %0,%0,16"       \
                 :"=r" (_v)            \
                 :"0" (_v)             \
                );                     \
     (fixed32)_v;                      \
 })
#define toint(v)                       \
({fixed32 _v=(v);                      \
    asm volatile("srawi %0,%0,16"      \
                 :"=r" (_v)            \
                 :"0" (_v)             \
                );                     \
     (fixed32)_v;                      \
 })
#define touint(v)                      \
({fixed32 _v=(v);                      \
    asm volatile("srwi %0,%0,16"       \
                 :"=r" (_v)            \
                 :"0" (_v)             \
                );                     \
     (u16)_v;                          \
 })
#define tofloat(v)                     \
({float _v=(v), _w=(float)0x10000;     \
    asm volatile("fdivs  %0,%0,%2"     \
                 :"=f" (_v)            \
                 :"0" (_v), "f" (_w)   \
                );                     \
     (float)_v;                        \
 })
#define mulfixed(a,b)                  \
({fixed32 _a=(a), _b=(b);              \
    asm volatile("mullw 8,%2,%0;"      \
                 "mulhw 7,%2,%0;"      \
                 "srwi 8,8,16;"        \
                 "slwi 7,7,16;"        \
                 "or %0,7,8"           \
                 :"=r" (_a)            \
                 :"0" (_a), "r" (_b)   \
                 :"7","8"              \
                );                     \
     (fixed32)_a;                      \
})
#endif
#define divfixed(a,b) (((s64)(a) << FP_SIZE) / (b))
#define decipart(v) (v & ((1 << FP_SIZE) - 1))

void FASTCALL Vdp2NBG0PlaneAddr(vdp2draw_struct *info, int i);
void FASTCALL Vdp2NBG1PlaneAddr(vdp2draw_struct *info, int i);
void FASTCALL Vdp2NBG2PlaneAddr(vdp2draw_struct *info, int i);
void FASTCALL Vdp2NBG3PlaneAddr(vdp2draw_struct *info, int i);
void Vdp2ReadRotationTable(int which, vdp2rotationparameter_struct *parameter);
void Vdp2ReadRotationTableFP(int which, vdp2rotationparameterfp_struct *parameter);
void FASTCALL Vdp2ParameterAPlaneAddr(vdp2draw_struct *info, int i);
void FASTCALL Vdp2ParameterBPlaneAddr(vdp2draw_struct *info, int i);
float Vdp2ReadCoefficientMode0_2(vdp2rotationparameter_struct *parameter, u32 addr);
fixed32 Vdp2ReadCoefficientMode0_2FP(vdp2rotationparameterfp_struct *parameter, u32 addr);

//////////////////////////////////////////////////////////////////////////////

static INLINE int GenerateRotatedXPos(vdp2rotationparameter_struct *p, int x, int y)
{
   float Xsp = p->A * ((p->Xst + p->deltaXst * y) - p->Px) +
               p->B * ((p->Yst + p->deltaYst * y) - p->Py) +
               p->C * (p->Zst - p->Pz);

   return (int)(p->kx * (Xsp + p->dX * (float)x) + p->Xp);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void GenerateRotatedVarFP(vdp2rotationparameterfp_struct *p, fixed32 *xmul, fixed32 *ymul, fixed32 *C, fixed32 *F)
{
   *xmul = p->Xst - p->Px;
   *ymul = p->Yst - p->Py;
   *C = mulfixed(p->C, (p->Zst - p->Pz));
   *F = mulfixed(p->F, (p->Zst - p->Pz));
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int GenerateRotatedYPos(vdp2rotationparameter_struct *p, int x, int y)
{
   float Ysp = p->D * ((p->Xst + p->deltaXst * y) - p->Px) +
               p->E * ((p->Yst + p->deltaYst * y) - p->Py) +
               p->F * (p->Zst - p->Pz);

   return (int)(p->ky * (Ysp + p->dY * (float)x) + p->Yp);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int GenerateRotatedXPosFP(vdp2rotationparameterfp_struct *p, int x, fixed32 xmul, fixed32 ymul, fixed32 C)
{
   fixed32 Xsp = mulfixed(p->A, xmul) + mulfixed(p->B, ymul) + C;

   return touint(mulfixed(p->kx, (Xsp + mulfixed(p->dX, tofixed(x)))) + p->Xp);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int GenerateRotatedYPosFP(vdp2rotationparameterfp_struct *p, int x, fixed32 xmul, fixed32 ymul, fixed32 F)
{
   fixed32 Ysp = mulfixed(p->D, xmul) + mulfixed(p->E, ymul) + F;

   return touint(mulfixed(p->ky, (Ysp + mulfixed(p->dY, tofixed(x)))) + p->Yp);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void CalculateRotationValues(vdp2rotationparameter_struct *p)
{
   p->Xp=p->A * (p->Px - p->Cx) +
         p->B * (p->Py - p->Cy) +
         p->C * (p->Pz - p->Cz) +
         p->Cx + p->Mx;
   p->Yp=p->D * (p->Px - p->Cx) +
         p->E * (p->Py - p->Cy) +
         p->F * (p->Pz - p->Cz) +
         p->Cy + p->My;
   p->dX=p->A * p->deltaX +
         p->B * p->deltaY;
   p->dY=p->D * p->deltaX +
         p->E * p->deltaY;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void CalculateRotationValuesFP(vdp2rotationparameterfp_struct *p)
{
   p->Xp=mulfixed(p->A, (p->Px - p->Cx)) +
         mulfixed(p->B, (p->Py - p->Cy)) +
         mulfixed(p->C, (p->Pz - p->Cz)) +
         p->Cx + p->Mx;
   p->Yp=mulfixed(p->D, (p->Px - p->Cx)) +
         mulfixed(p->E, (p->Py - p->Cy)) +
         mulfixed(p->F, (p->Pz - p->Cz)) +
         p->Cy + p->My;
   p->dX=mulfixed(p->A, p->deltaX) +
         mulfixed(p->B, p->deltaY);
   p->dY=mulfixed(p->D, p->deltaX) +
         mulfixed(p->E, p->deltaY);
}

//////////////////////////////////////////////////////////////////////////////

//DONE
static INLINE void CalcPlaneAddr(vdp2draw_struct *info, u32 tmp)
{
	u32 deca = info->planeh + info->planew - 2;
	//XXX: is info->patterndatasize and info->patternwh binary?
	u32 ptrn = ((!info->patternwh_bits) << 1) | (info->patterndatasize != 1);
	u32 multi = (info->planeh * info->planew) << (11 + ptrn);
	int mask = 0xFF >> ptrn;	//this should be enough
	//this uses the previous implementation.
	//u32 mask = 0xFFFFFFFF >> (ptrn + (24 * (ptrn != 0)));

	info->addr = ((tmp & mask) >> deca) * multi;
}

//////////////////////////////////////////////////////////////////////////////

//DONE
static INLINE void ReadBitmapSize(vdp2draw_struct *info, u16 bm, int mask)
{
	u32 w_bit = (bm & mask) >> 1;
	u32 h_bit = bm & 1;

	info->cellw_bits = 9 + w_bit;
	info->cellh_bits = 8 + h_bit;
	info->cellw = 0x200 << w_bit;
	info->cellh = 0x100 << h_bit;
}

//////////////////////////////////////////////////////////////////////////////

//DONE
static INLINE void ReadPlaneSize(vdp2draw_struct *info, u16 reg)
{
	//XXX: Check if the planew and planeh are not swapped... (check prev function)
	info->planew_bits = reg & 0x1;
	info->planeh_bits = (reg >> 1) & info->planew_bits;
	info->planew = info->planew_bits + 1;
	info->planeh = info->planeh_bits + 1;
}

//////////////////////////////////////////////////////////////////////////////

//DONE
static INLINE void ReadPatternData(vdp2draw_struct *info, u16 pnc, int chctlwh)
{
	//XXX: make chctlwh be a 1 bit value beforehand
	info->patterndatasize_bits = !(pnc & 0x8000);
	info->patterndatasize = info->patterndatasize_bits + 1;
	info->patternwh_bits = (chctlwh != 0);
	info->patternwh = info->patternwh_bits + 1;

	info->pagewh = 64>>info->patternwh_bits;
	info->pagewh_bits = 6-info->patternwh_bits;
	info->cellw = info->cellh = 8;
	info->cellw_bits = info->cellh_bits = 3;
	info->supplementdata = pnc & 0x3FF;
	info->auxmode = (pnc & 0x4000) >> 14;
}

//////////////////////////////////////////////////////////////////////////////

//DONE
static INLINE void ReadMosaicData(vdp2draw_struct *info, u16 mask)
{
	u32 comp_mask = (-(Vdp2Regs->MZCTL & mask)) >> 8;
	u32 val = Vdp2Regs->MZCTL & comp_mask;
	u32 xm = ((val >> 8) & 0xF);
	u32 ym = (val >> 12);
	info->mosaicxmask = xm + 1;
	info->mosaicymask = ym + 1;
}

//////////////////////////////////////////////////////////////////////////////

//HALF DONE
static INLINE void ReadLineScrollData(vdp2draw_struct *info, u16 mask, u32 tbl)
{
	info->islinescroll = 0;
	info->lineinc = 0;

	if (mask & 0xE) {
		info->linescrolltbl = (tbl & 0x7FFFE) << 1;
		info->islinescroll = (mask >> 1) & 0x7;
		info->lineinc = 1 << ((mask >> 4) & 0x03);
	}
}


//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadWindowCoordinates(int num, clipping_struct * clip)
{
	if (num == 0)
	{
		// Window 0
		clip->xstart = Vdp2Regs->WPSX0;
		clip->ystart = Vdp2Regs->WPSY0 & 0x1FF;
		clip->xend = Vdp2Regs->WPEX0;
		clip->yend = Vdp2Regs->WPEY0 & 0x1FF;
	}
	else
	{
		// Window 1
		clip->xstart = Vdp2Regs->WPSX1;
		clip->ystart = Vdp2Regs->WPSY1 & 0x1FF;
		clip->xend = Vdp2Regs->WPEX1;
		clip->yend = Vdp2Regs->WPEY1 & 0x1FF;
	}

	switch ((Vdp2Regs->TVMD >> 1) & 0x3)
	{
		case 0: // Normal
			clip->xstart = (clip->xstart >> 1) & 0x1FF;
			clip->xend = (clip->xend >> 1) & 0x1FF;
			break;
		case 1: // Hi-Res
			clip->xstart = clip->xstart & 0x3FF;
			clip->xend = clip->xend & 0x3FF;
			break;
		case 2: // Exclusive Normal
			clip->xstart = clip->xstart & 0x1FF;
			clip->xend = clip->xend & 0x1FF;
			break;
		case 3: // Exclusive Hi-Res
			clip->xstart = (clip->xstart & 0x3FF) >> 1;
			clip->xend = (clip->xend & 0x3FF) >> 1;
			//XXX: From Saturn Wii
			//clip->xstart = (clip->xstart & 0x1FF) << 1;
			//clip->xend = (clip->xend & 0x1FF) << 1;
		break;
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadWindowData(int wctl, clipping_struct *clip)
{
   clip[0].xstart = clip[0].ystart = clip[0].xend = clip[0].yend = 0;
   clip[1].xstart = clip[1].ystart = clip[1].xend = clip[1].yend = 0;

   if (wctl & 0x2)
   {
      ReadWindowCoordinates(0, clip);
   }

   if (wctl & 0x8)
   {
      ReadWindowCoordinates(1, clip + 1);
   }

   if (wctl & 0x20)
   {
      // fix me
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadLineWindowData(int *islinewindow, int wctl, u32 *linewnd0addr, u32 *linewnd1addr)
{
   islinewindow[0] = 0;

   if (wctl & 0x2 && Vdp2Regs->LWTA0.all & 0x80000000)
   {
      islinewindow[0] |= 0x1;
      linewnd0addr[0] = (Vdp2Regs->LWTA0.all & 0x7FFFE) << 1;
   }
   if (wctl & 0x8 && Vdp2Regs->LWTA1.all & 0x80000000)
   {
      islinewindow[0] |= 0x2;
      linewnd1addr[0] = (Vdp2Regs->LWTA1.all & 0x7FFFE) << 1;
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void ReadOneLineWindowClip(clipping_struct *clip, u32 *linewndaddr)
{
	clip->xstart = T1ReadWord(Vdp2Ram, *linewndaddr);
	*linewndaddr += 2;
	clip->xend = T1ReadWord(Vdp2Ram, *linewndaddr);
	*linewndaddr += 2;

	/* Ok... that looks insane... but there's at least two games (3D Baseball and
	Panzer Dragoon Saga) that set the line window end to 0xFFFF and expect the line
	window to be invalid for those lines... */
	if (clip->xend == 0xFFFF) {
		clip->xstart = 0;
		clip->xend = 0;
		return;
	}

	u32 h_res = (Vdp2Regs->TVMD >> 1) & 0x1;
	clip->xstart = (clip->xstart & 0x3FF) >> h_res;
	clip->xend = (clip->xend & 0x3FF) >> h_res;
}

static INLINE void ReadLineWindowClip(int islinewindow, clipping_struct *clip, u32 *linewnd0addr, u32 *linewnd1addr)
{
   if (islinewindow)
   {
      // Fetch new xstart and xend values from table
      if (islinewindow & 0x1)
         // Window 0
         ReadOneLineWindowClip(clip, linewnd0addr);
      if (islinewindow & 0x2)
         // Window 1
         ReadOneLineWindowClip(clip + 1, linewnd1addr);
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int IsScreenRotated(vdp2rotationparameter_struct *parameter)
{
  return (parameter->deltaXst == 0.0 &&
          parameter->deltaYst == 1.0 &&
          parameter->deltaX == 1.0 &&
          parameter->deltaY == 0.0 &&
          parameter->A == 1.0 &&
          parameter->B == 0.0 &&
          parameter->C == 0.0 &&
          parameter->D == 0.0 &&
          parameter->E == 1.0 &&
          parameter->F == 0.0);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int IsScreenRotatedFP(vdp2rotationparameterfp_struct *parameter)
{
  return (parameter->deltaXst == 0x0 &&
          parameter->deltaYst == 0x10000 &&
          parameter->deltaX == 0x10000 &&
          parameter->deltaY == 0x0 &&
          parameter->A == 0x10000 &&
          parameter->B == 0x0 &&
          parameter->C == 0x0 &&
          parameter->D == 0x0 &&
          parameter->E == 0x10000 &&
          parameter->F == 0x0);
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void Vdp2ReadCoefficient(vdp2rotationparameter_struct *parameter, u32 addr)
{
   switch (parameter->coefmode)
   {
      case 0: // coefficient for kx and ky
         parameter->kx = parameter->ky = Vdp2ReadCoefficientMode0_2(parameter, addr);
         break;
      case 1: // coefficient for kx
         parameter->kx = Vdp2ReadCoefficientMode0_2(parameter, addr);
         break;
      case 2: // coefficient for ky
         parameter->ky = Vdp2ReadCoefficientMode0_2(parameter, addr);
         break;
      case 3: // coefficient for Xp
      {
         s32 i;

         if (parameter->coefdatasize == 2)
         {
            i = T1ReadWord(Vdp2Ram, addr);
            parameter->msb = (i >> 15) & 0x1;
            parameter->Xp = (float) (signed) ((i & 0x7FFF) | (i & 0x4000 ? 0xFFFFC000 : 0x00000000)) / 4;
         }
         else
         {
            i = T1ReadLong(Vdp2Ram, addr);
            parameter->msb = (i >> 31) & 0x1;
            parameter->Xp = (float) (signed) ((i & 0x007FFFFF) | (i & 0x00800000 ? 0xFF800000 : 0x00000000)) / 256;
         }
         break;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void Vdp2ReadCoefficientFP(vdp2rotationparameterfp_struct *parameter, u32 addr)
{
   switch (parameter->coefmode)
   {
      case 0: // coefficient for kx and ky
         parameter->kx = parameter->ky = Vdp2ReadCoefficientMode0_2FP(parameter, addr);
         break;
      case 1: // coefficient for kx
         parameter->kx = Vdp2ReadCoefficientMode0_2FP(parameter, addr);
         break;
      case 2: // coefficient for ky
         parameter->ky = Vdp2ReadCoefficientMode0_2FP(parameter, addr);
         break;
      case 3: // coefficient for Xp
      {
         s32 i;

         if (parameter->coefdatasize == 2)
         {
            i = T1ReadWord(Vdp2Ram, addr);
            parameter->msb = (i >> 15) & 0x1;
            parameter->Xp = (signed) ((i & 0x7FFF) | (i & 0x4000 ? 0xFFFFC000 : 0x00000000)) * 16384;
         }
         else
         {
            i = T1ReadLong(Vdp2Ram, addr);
            parameter->msb = (i >> 31) & 0x1;
            parameter->linescreen = (i >> 24) & 0x7F;
            parameter->Xp = (signed) ((i & 0x007FFFFF) | (i & 0x00800000 ? 0xFF800000 : 0x00000000)) * 256;
         }

         break;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////
typedef struct {
    int normalshadow;
    int msbshadow;
    int priority;
    int colorcalc;
} spritepixelinfo_struct;

static INLINE void Vdp1GetSpritePixelInfo(int type, u32 * pixel, spritepixelinfo_struct *spi)
{
	static const uint8_t priority_shift[16] =
		{ 14, 13, 14, 13,  13, 12, 12, 12,  7, 7, 6, 0,  7, 7, 6, 0 };
	static const uint8_t priority_mask[16] =
		{  3,  7,  1,  3,   3,  7,  7,  7,  1, 1, 3, 0,  1, 1, 3, 0 };
	static const uint8_t alpha_shift[16] =
		{ 11, 11, 11, 11,  10, 11, 10,  9,  0, 6, 0, 6,  0, 6, 0, 6 };
	static const uint8_t alpha_mask[16] =
		{  7,  3,  7,  3,   7,  1,  3,  7,  0, 1, 0, 3,  0, 1, 0, 3 };
	static const uint16_t color_mask[16] =
		{ 0x7FF, 0x7FF, 0x7FF, 0x7FF,  0x3FF, 0x7FF, 0x3FF, 0x1FF,
		0x7F,  0x3F,  0x3F,  0x3F,   0xFF,  0xFF,  0xFF,  0xFF };

	spi->msbshadow = *pixel >> 15;
	spi->priority  = (*pixel >> priority_shift[type]) & priority_mask[type];
	spi->colorcalc = (*pixel >>    alpha_shift[type]) &    alpha_mask[type];
	*pixel     = *pixel & color_mask[type];
	spi->normalshadow = (*pixel == (color_mask[16] - 1));
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void Vdp1ProcessSpritePixel(int type, u32 *pixel, int *shadow, int *priority, int *colorcalc)
{
   spritepixelinfo_struct spi;

   Vdp1GetSpritePixelInfo(type, pixel, &spi);
   *shadow = spi.msbshadow;
   *priority = spi.priority;
   *colorcalc = spi.colorcalc;
}

#define VDPLINE_SZ(a) ((a)&0x04)
#define VDPLINE_SY(a) ((a)&0x02)
#define VDPLINE_SX(a) ((a)&0x01)
#define OVERMODE_REPEAT      0
#define OVERMODE_SELPATNAME  1
#define OVERMODE_TRANSE      2
#define OVERMODE_512         3


//////////////////////////////////////////////////////////////////////////////

#define VDP2_VRAM_A0 (0)
#define VDP2_VRAM_A1 (1)
#define VDP2_VRAM_B0 (2)
#define VDP2_VRAM_B1 (3)

int Vdp2GetBank(u32 addr);
u32 PixelIsSpecialPriority(u32 specialcode, u32 dot);

void VDP2genVRamCyclePattern();


#endif
