
#include <stdlib.h>
#include <malloc.h>
#include "cart.h"

WriteFunc8 cs0_write8;
WriteFunc16 cs0_write16;
WriteFunc32 cs0_write32;
ReadFunc8 cs0_read8;
ReadFunc16 cs0_read16;
ReadFunc32 cs0_read32;

Cartridge cart;


/*No cart RW functions*/
static u8  cs0_NoneRead8(u32 addr)  { return 0xFF;}
static u16 cs0_NoneRead16(u32 addr) { return 0xFFFF;}
static u32 cs0_NoneRead32(u32 addr) { return 0xFFFFFFFF;}

static void cs0_NoneWrite8(u32 addr, u8 val)   {}
static void cs0_NoneWrite16(u32 addr, u16 val) {}
static void cs0_NoneWrite32(u32 addr, u32 val) {}


/*1MB RAM RW functions*/
static u8 cs0_1MBRamRead8(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		return cart.data[addr];
	}
	return 0xFF;
}

static u16 cs0_1MBRamRead16(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		return *((u16*)(cart.data + addr));
	}
	return 0xFFFF;
}

static u32 cs0_1MBRamRead32(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		return *((u32*)(cart.data + addr));
	}
	return 0xFFFFFFFF;
}

static void cs0_1MBRamWrite8(u32 addr, u8 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		cart.data[addr] = val;
	}
}

static void cs0_1MBRamWrite16(u32 addr, u16 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		*((u16*)(cart.data + addr)) = val;
	}
}

static void cs0_1MBRamWrite32(u32 addr, u32 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0x50;
	addr = (addr & 0x7FFFF) | (addr & 0x200000) >> 2;
	if (mask) {
		*((u32*)(cart.data + addr)) = val;
	}
}


/*4MB RAM RW functions*/
static u8 cs0_4MBRamRead8(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		return cart.data[addr];
	}
	return 0xFF;
}

static u16 cs0_4MBRamRead16(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		return *((u16*)(cart.data + addr));
	}
	return 0xFFFF;
}

static u32 cs0_4MBRamRead32(u32 addr)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		return *((u32*)(cart.data + addr));
	}
	return 0xFFFFFFFF;
}

static void cs0_4MBRamWrite8(u32 addr, u8 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		cart.data[addr] = val;
	}
}

static void cs0_4MBRamWrite16(u32 addr, u16 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		*((u16*)(cart.data + addr)) = val;
	}
}

static void cs0_4MBRamWrite32(u32 addr, u32 val)
{
	u32 mask = (1 << ((addr >> 20) & 0x1F)) & 0xF0;
	if (mask) {
		*((u32*)(cart.data + addr)) = val;
	}
}


/*2MB ROM RW functions*/
static u8 cs0_RomRead8(u32 addr)
{
	return cart.data[addr & 0x1FFFFF];
}

static u16 cs0_RomRead16(u32 addr)
{
	return *((u16*)(cart.data + (addr & 0x1FFFFF)));
}

static u32 cs0_RomRead32(u32 addr)
{
	return *((u32*)(cart.data + (addr & 0x1FFFFF)));
}

static void cs0_RomWrite8(u32 addr, u8 val)
{
	cart.data[addr & 0x1FFFFF] = val;
}

static void cs0_RomWrite16(u32 addr, u16 val)
{
	*((u16*)(cart.data + (addr & 0x1FFFFF))) = val;
}

static void cs0_RomWrite32(u32 addr, u32 val)
{
	*((u32*)(cart.data + (addr & 0x1FFFFF))) = val;
}


/*CS1 area RW functions (This is used for determining CS0 cart type)*/
u8 cs1_Read8(u32 addr)
{
	return cart.id | (-(addr != 0xFFFFFF));
}

u16 cs1_Read16(u32 addr)
{
	return cart.id | (-(addr != 0xFFFFFE));
}

u32 cs1_Read32(u32 addr)
{
	return cart.id | (-(addr != 0xFFFFFC));
}

void cs1_Write8(u32 addr, u8 val)   {/*Does nothing*/}
void cs1_Write16(u32 addr, u16 val) {/*Does nothing*/}
void cs1_Write32(u32 addr, u32 val) {/*Does nothing*/}


//=================================
// Cartridge area functions
//=================================

u32 cart_Init(u32 type, const char * filename)
{
	cart.type = type;
	cart.id = 0xFFFFFFFF;

	cs0_write8  = cs0_NoneWrite8;
	cs0_write16 = cs0_NoneWrite16;
	cs0_write32 = cs0_NoneWrite32;
	cs0_read8   = cs0_NoneRead8;
	cs0_read16  = cs0_NoneRead16;
	cs0_read32  = cs0_NoneRead32;

	switch(type) {
		/*1MB RAM Cartridge*/
		case CART_TYPE_RAM1MB: {
			if (!(cart.data = (u8*) memalign(32, 0x100000))) {
				return -1;
			}

			cart.id = 0xFF5AFF5A;
			cs0_write8  = cs0_1MBRamWrite8;
			cs0_write16 = cs0_1MBRamWrite16;
			cs0_write32 = cs0_1MBRamWrite32;
			cs0_read8   = cs0_1MBRamRead8;
			cs0_read16  = cs0_1MBRamRead16;
			cs0_read32  = cs0_1MBRamRead32;
		} break;
		/*4MB RAM Cartridge*/
		case CART_TYPE_RAM4MB: {
			if (!(cart.data = (u8*) memalign(32, 0x400000))) {
				return -1;
			}

			cart.id = 0xFF5CFF5C;
			cs0_write8  = cs0_4MBRamWrite8;
			cs0_write16 = cs0_4MBRamWrite16;
			cs0_write32 = cs0_4MBRamWrite32;
			cs0_read8   = cs0_4MBRamRead8;
			cs0_read16  = cs0_4MBRamRead16;
			cs0_read32  = cs0_4MBRamRead32;
		} break;
		/*2MB ROM Cartridge*/
		case CART_TYPE_ROM2MB: {
			if (!(cart.data = (u8*) memalign(32, 0x200000))) {
				return -1;
			}

			// TODO: Load the ROM

			cart.id = 0xFFFFFFFF;
			cs0_write8  = cs0_RomWrite8;
			cs0_write16 = cs0_RomWrite16;
			cs0_write32 = cs0_RomWrite32;
			cs0_read8   = cs0_RomRead8;
			cs0_read16  = cs0_RomRead16;
			cs0_read32  = cs0_RomRead32;
		} break;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void cart_Deinit(void)
{
	if (cart.data) {
		free(cart.data);
		cart.data = NULL;
	}
}
