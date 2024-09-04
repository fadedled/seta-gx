
#ifndef __CART_H__
#define __CART_H__

#include "memory.h"

/*
 * Cart types:
 * Only useful cart types are included. Back-up ram
 * is not used since we save to file.
 */
#define CART_TYPE_NONE       0
#define CART_TYPE_RAM1MB     1
#define CART_TYPE_RAM4MB     2
#define CART_TYPE_ROM2MB     3
#define CART_TYPE_NETLINK    4 /*Unused*/
#define CART_TYPE_JAPMODEM   5 /*Unused*/

typedef struct Cartridge_t {
	u32 type;
	u32 id;
	u8 *data;
} Cartridge;

extern Cartridge cart;

extern WriteFunc8  cs0_write8;
extern WriteFunc16 cs0_write16;
extern WriteFunc32 cs0_write32;
extern ReadFunc8   cs0_read8;
extern ReadFunc16  cs0_read16;
extern ReadFunc32  cs0_read32;

u8   cs1_Read8(u32 addr);
u16  cs1_Read16(u32 addr);
u32  cs1_Read32(u32 addr);
void cs1_Write8(u32 addr, u8 val);
void cs1_Write16(u32 addr, u16 val);
void cs1_Write32(u32 addr, u32 val);

u32 cart_Init(u32 type, const char *filename);
void cart_Deinit(void);

#endif /*__CART_H__*/
