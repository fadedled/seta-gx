/*
 * PicoDrive - Internal Header File
 * (c) Copyright Dave, 2004
 * (C) notaz, 2006-2010
 *
 * This work is licensed under the terms of MAME license.
 * See COPYING file in the top-level directory.
 */

#ifndef PICO_INTERNAL_INCLUDED
#define PICO_INTERNAL_INCLUDED

#include <stdio.h>
#include <string.h>

#if defined(__GNUC__) && defined(__i386__)
#define REGPARM(x) __attribute__((regparm(x)))
#else
#define REGPARM(x)
#endif

#ifdef __GNUC__
#define NOINLINE    __attribute__((noinline))
#define ALIGNED(n)  __attribute__((aligned(n)))
#define unlikely(x) __builtin_expect((x), 0)
#define likely(x)   __builtin_expect(!!(x), 1)
#else
#define NOINLINE
#define ALIGNED(n)
#define unlikely(x) (x)
#define likely(x) (x)
#endif


#define	CPU_IS_LE	0
// address/offset operations
#define MEM_BE2(a)	(a)
#define MEM_BE4(a)	(a)
#define MEM_LE2(a)	((a)^1)
#define MEM_LE4(a)	((a)^3)
// swapping
#define CPU_BE2(v)	(v)
#define CPU_BE4(v)	(v)
#define CPU_LE2(v)	((u32)((u64)(v)<<16)|((u32)(v)>>16))
#define CPU_LE4(v)	(((u32)(v)>>24)|(((v)>>8)&0x00ff00)| \
			(((v)<<8)&0xff0000)|(u32)((v)<<24))

//Using 24
#define PICO_MSH2_HZ ((int)(7670442.0 * 2.9))
#define PICO_SSH2_HZ ((int)(7670442.0 * 2.9))

//#include "carthw/carthw.h"

//
#define USE_POLL_DETECT

#ifndef PICO_INTERNAL
#define PICO_INTERNAL
#endif
#ifndef PICO_INTERNAL_ASM
#define PICO_INTERNAL_ASM
#endif

// to select core, define EMU_C68K, EMU_M68K or EMU_F68K in your makefile or project
// ----------------------- SH2 CPU -----------------------

#include "sh2/sh2.h"

extern SH2 sh2s[2];
#define msh2 sh2s[0]
#define ssh2 sh2s[1]

//XXX: useful?
typedef uintptr_t      uptr;

#ifndef DRC_SH2
# define sh2_end_run(sh2, after_) do { \
  if ((sh2)->icount > (after_)) { \
    (sh2)->cycles_timeslice -= (sh2)->icount - (after_); \
    (sh2)->icount = after_; \
  } \
} while (0)
# define sh2_cycles_left(sh2) (sh2)->icount
# define sh2_burn_cycles(sh2, n) (sh2)->icount -= n
# define sh2_pc(sh2) (sh2)->ppc
#else
# define sh2_end_run(sh2, after_) do { \
  int left_ = (signed int)(sh2)->sr >> 12; \
  if (left_ > (after_)) { \
    (sh2)->cycles_timeslice -= left_ - (after_); \
    (sh2)->sr &= 0xfff; \
    (sh2)->sr |= (after_) << 12; \
  } \
} while (0)
# define sh2_cycles_left(sh2) ((signed int)(sh2)->sr >> 12)
# define sh2_burn_cycles(sh2, n) (sh2)->sr -= ((n) << 12)
# define sh2_pc(sh2) (sh2)->pc
#endif

#define sh2_cycles_done(sh2) ((int)(sh2)->cycles_timeslice - sh2_cycles_left(sh2))
#define sh2_cycles_done_t(sh2) \
  ((sh2)->m68krcycles_done * 3 + sh2_cycles_done(sh2))
#define sh2_cycles_done_m68k(sh2) \
  ((sh2)->m68krcycles_done + (sh2_cycles_done(sh2) / 3))

#define sh2_reg(c, x) (c) ? ssh2.r[x] : msh2.r[x]
#define sh2_gbr(c)    (c) ? ssh2.gbr : msh2.gbr
#define sh2_vbr(c)    (c) ? ssh2.vbr : msh2.vbr
#define sh2_sr(c)   (((c) ? ssh2.sr : msh2.sr) & 0xfff)

#define sh2_set_gbr(c, v) \
  { if (c) ssh2.gbr = v; else msh2.gbr = v; }
#define sh2_set_vbr(c, v) \
  { if (c) ssh2.vbr = v; else msh2.vbr = v; }

#define elprintf_sh2(sh2, w, f, ...) \
	elprintf(w,"%csh2 "f,(sh2)->is_slave?'s':'m',##__VA_ARGS__)

// ---------------------------------------------------------

// main oscillator clock which controls timing
#define OSC_NTSC 53693100
#define OSC_PAL  53203424

// 32X
#define P32XS_FM    (1<<15)
#define P32XS_nCART (1<< 8)
#define P32XS_REN   (1<< 7)
#define P32XS_nRES  (1<< 1)
#define P32XS_ADEN  (1<< 0)
#define P32XS2_ADEN (1<< 9)
#define P32XS_FULL  (1<< 7) // DREQ FIFO full
#define P32XS_68S   (1<< 2)
#define P32XS_DMA   (1<< 1)
#define P32XS_RV    (1<< 0)

#define P32XV_nPAL  (1<<15) // VDP
#define P32XV_PRI   (1<< 7)
#define P32XV_Mx    (3<< 0) // display mode mask

#define P32XV_SFT   (1<< 0)

#define P32XV_VBLK  (1<<15)
#define P32XV_HBLK  (1<<14)
#define P32XV_PEN   (1<<13)
#define P32XV_nFEN  (1<< 1)
#define P32XV_FS    (1<< 0)

#define P32XP_RTP   (1<<7)  // PWM control
#define P32XP_FULL  (1<<15) // PWM pulse
#define P32XP_EMPTY (1<<14)

#define P32XF_68KCPOLL   (1 << 0)
#define P32XF_68KVPOLL   (1 << 1)
#define P32XF_Z80_32X_IO (1 << 7) // z80 does 32x io
#define P32XF_DRC_ROM_C  (1 << 8) // cached code from ROM

#define P32XI_VRES (1 << 14/2) // IRL/2
#define P32XI_VINT (1 << 12/2)
#define P32XI_HINT (1 << 10/2)
#define P32XI_CMD  (1 <<  8/2)
#define P32XI_PWM  (1 <<  6/2)

// peripheral reg access
#define PREG8(regs,offs) ((unsigned char *)regs)[offs ^ 3]

#define DMAC_FIFO_LEN (4*2)
#define PWM_BUFF_LEN 1024 // in one channel samples

#define SH2_DRCBLK_RAM_SHIFT 1
#define SH2_DRCBLK_DA_SHIFT  1

#define SH2_READ_SHIFT 25
#define SH2_WRITE_SHIFT 25

struct Pico32x
{
  unsigned short regs[0x20];
  unsigned short vdp_regs[0x10]; // 0x40
  unsigned short sh2_regs[3];    // 0x60
  unsigned char pending_fb;
  unsigned char dirty_pal;
  unsigned int emu_flags;
  unsigned char sh2irq_mask[2];
  unsigned char sh2irqi[2];      // individual
  unsigned int sh2irqs;          // common irqs
  unsigned short dmac_fifo[DMAC_FIFO_LEN];
  unsigned int pad[4];
  unsigned int dmac0_fifo_ptr;
  unsigned short vdp_fbcr_fake;
  unsigned short pad2;
  unsigned char comm_dirty;
  unsigned char pad3;            // was comm_dirty_sh2
  unsigned char pwm_irq_cnt;
  unsigned char pad1;
  unsigned short pwm_p[2];       // pwm pos in fifo
  unsigned int pwm_cycle_p;      // pwm play cursor (32x cycles)
  unsigned int reserved[6];
};

struct Pico32xMem
{
  unsigned char  sdram[0x40000];
#ifdef DRC_SH2
  unsigned char drcblk_ram[1 << (18 - SH2_DRCBLK_RAM_SHIFT)];
  unsigned char drclit_ram[1 << (18 - SH2_DRCBLK_RAM_SHIFT)];
#endif
  unsigned short dram[2][0x20000/2];    // AKA fb
  union {
    unsigned char  m68k_rom[0x100];
    unsigned char  m68k_rom_bank[0x10000]; // M68K_BANK_SIZE
  };
#ifdef DRC_SH2
  unsigned char drcblk_da[2][1 << (12 - SH2_DRCBLK_DA_SHIFT)];
  unsigned char drclit_da[2][1 << (12 - SH2_DRCBLK_DA_SHIFT)];
#endif
  union {
    unsigned char  b[0x800];
    unsigned short w[0x800/2];
  } sh2_rom_m;
  union {
    unsigned char  b[0x400];
    unsigned short w[0x400/2];
  } sh2_rom_s;
  unsigned short pal[0x100];
  unsigned short pal_native[0x100];     // converted to native (for renderer)
  signed short   pwm[2*PWM_BUFF_LEN];   // PWM buffer for current frame
  unsigned short pwm_fifo[2][4];        // [0] - current raw, others - fifo entries
  unsigned       pwm_index[2];          // ringbuffer index for pwm_fifo
};

// memory.c
PICO_INTERNAL void PicoMemSetup(void);
unsigned int PicoRead8_io(unsigned int a);
unsigned int PicoRead16_io(unsigned int a);
void PicoWrite8_io(unsigned int a, unsigned int d);
void PicoWrite16_io(unsigned int a, unsigned int d);

// pico/memory.c
extern struct Pico32xMem *Pico32xMem;
u32 PicoRead8_32x(u32 a);
u32 PicoRead16_32x(u32 a);
void PicoWrite8_32x(u32 a, u32 d);
void PicoWrite16_32x(u32 a, u32 d);
void PicoMemSetup32x(void);
void Pico32xSwapDRAM(int b);
void Pico32xMemStateLoaded(void);
void p32x_update_banks(void);
void p32x_m68k_poll_event(u32 flags);
u32 REGPARM(3) p32x_sh2_poll_memory8(u32 a, u32 d, SH2 *sh2);
u32 REGPARM(3) p32x_sh2_poll_memory16(u32 a, u32 d, SH2 *sh2);
u32 REGPARM(3) p32x_sh2_poll_memory32(u32 a, u32 d, SH2 *sh2);
void *p32x_sh2_get_mem_ptr(u32 a, u32 *mask, SH2 *sh2);
int p32x_sh2_mem_is_rom(u32 a, SH2 *sh2);
void p32x_sh2_poll_detect(u32 a, SH2 *sh2, u32 flags, int maxcnt);
void p32x_sh2_poll_event(SH2 *sh2, u32 flags, u32 m68k_cycles);
int p32x_sh2_memcpy(u32 dst, u32 src, int count, int size, SH2 *sh2);


// 32x/sh2soc.c
void p32x_dreq0_trigger(void);
void p32x_dreq1_trigger(void);
void p32x_timers_recalc(void);
void p32x_timers_do(unsigned int m68k_slice);
void sh2_peripheral_reset(SH2 *sh2);
unsigned int sh2_peripheral_read8(unsigned int a, SH2 *sh2);
unsigned int sh2_peripheral_read16(unsigned int a, SH2 *sh2);
unsigned int sh2_peripheral_read32(unsigned int a, SH2 *sh2);
void REGPARM(3) sh2_peripheral_write8(unsigned int a, unsigned int d, SH2 *sh2);
void REGPARM(3) sh2_peripheral_write16(unsigned int a, unsigned int d, SH2 *sh2);
void REGPARM(3) sh2_peripheral_write32(unsigned int a, unsigned int d, SH2 *sh2);

/* avoid dependency on newer glibc */
static __inline int isspace_(int c)
{
	return (0x09 <= c && c <= 0x0d) || c == ' ';
}

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

#define EL_HVCNT   0x00000001 /* hv counter reads */
#define EL_SR      0x00000002 /* SR reads */
#define EL_INTS    0x00000004 /* ints and acks */
#define EL_YMTIMER 0x00000008 /* ym2612 timer stuff */
#define EL_INTSW   0x00000010 /* log irq switching on/off */
#define EL_ASVDP   0x00000020 /* VDP accesses during active scan */
#define EL_VDPDMA  0x00000040 /* VDP DMA transfers and their timing */
#define EL_BUSREQ  0x00000080 /* z80 busreq r/w or reset w */
#define EL_Z80BNK  0x00000100 /* z80 i/o through bank area */
#define EL_SRAMIO  0x00000200 /* sram i/o */
#define EL_EEPROM  0x00000400 /* eeprom debug */
#define EL_UIO     0x00000800 /* unmapped i/o */
#define EL_IO      0x00001000 /* all i/o */
#define EL_CDPOLL  0x00002000 /* MCD: log poll detection */
#define EL_SVP     0x00004000 /* SVP stuff */
#define EL_PICOHW  0x00008000 /* Pico stuff */
#define EL_IDLE    0x00010000 /* idle loop det. */
#define EL_CDREGS  0x00020000 /* MCD: register access */
#define EL_CDREG3  0x00040000 /* MCD: register 3 only */
#define EL_32X     0x00080000
#define EL_PWM     0x00100000 /* 32X PWM stuff (LOTS of output) */
#define EL_32XP    0x00200000 /* 32X peripherals */
#define EL_CD      0x00400000 /* MCD */

#define EL_STATUS  0x40000000 /* status messages */
#define EL_ANOMALY 0x80000000 /* some unexpected conditions (during emulation) */


#endif // PICO_INTERNAL_INCLUDED

// vim:shiftwidth=2:ts=2:expandtab
