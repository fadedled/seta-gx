/*  Copyright 2005-2006 Guillaume Duhamel
    Copyright 2005 Theo Berkau

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

#ifndef MEMORY_H
#define MEMORY_H

#include <stdlib.h>
#include "core.h"
//#include "sh2core.h"
#include "vm/vm.h"

/* Type 1 Memory, faster for byte (8 bits) accesses */

u8 * T1MemoryInit(u32);
void T1MemoryDeInit(u8 *);

static INLINE u8 T1ReadByte(u8 * mem, u32 addr)
{
   return mem[addr];
}

static INLINE u16 T1ReadWord(u8 * mem, u32 addr)
{
   return *((u16 *) (mem + addr));
}

static INLINE u32 T1ReadLong(u8 * mem, u32 addr)
{
   return *((u32 *) (mem + addr));
}

static INLINE void T1WriteByte(u8 * mem, u32 addr, u8 val)
{
   mem[addr] = val;
}

static INLINE void T1WriteWord(u8 * mem, u32 addr, u16 val)
{
   *((u16 *) (mem + addr)) = val;
}

static INLINE void T1WriteLong(u8 * mem, u32 addr, u32 val)
{
   *((u32 *) (mem + addr)) = val;
}

/* Type 2 Memory, faster for word (16 bits) accesses */

#define T2MemoryInit(x) (T1MemoryInit(x))
#define T2MemoryDeInit(x) (T1MemoryDeInit(x))

static INLINE u8 T2ReadByte(u8 * mem, u32 addr)
{
   return mem[addr];
}

static INLINE u16 T2ReadWord(u8 * mem, u32 addr)
{
   return *((u16 *) (mem + addr));
}

static INLINE u32 T2ReadLong(u8 * mem, u32 addr)
{
   return *((u32 *) (mem + addr));
}

static INLINE void T2WriteByte(u8 * mem, u32 addr, u8 val)
{
   mem[addr] = val;
}

static INLINE void T2WriteWord(u8 * mem, u32 addr, u16 val)
{
   *((u16 *) (mem + addr)) = val;
}

static INLINE void T2WriteLong(u8 * mem, u32 addr, u32 val)
{
   *((u32 *) (mem + addr)) = val;
}

/* Type 3 Memory, faster for long (32 bits) accesses */

typedef struct
{
   u8 * base_mem;
   u8 * mem;
} T3Memory;

T3Memory * T3MemoryInit(u32);
void T3MemoryDeInit(T3Memory *);

static INLINE u8 T3ReadByte(T3Memory * mem, u32 addr)
{
	return mem->mem[addr];
}

static INLINE u16 T3ReadWord(T3Memory * mem, u32 addr)
{
	return *((u16 *) (mem->mem + addr));
}

static INLINE u32 T3ReadLong(T3Memory * mem, u32 addr)
{
	return *((u32 *) (mem->mem + addr));
}

static INLINE void T3WriteByte(T3Memory * mem, u32 addr, u8 val)
{
	mem->mem[addr] = val;
}

static INLINE void T3WriteWord(T3Memory * mem, u32 addr, u16 val)
{
	*((u16 *) (mem->mem + addr)) = val;
}

static INLINE void T3WriteLong(T3Memory * mem, u32 addr, u32 val)
{
	*((u32 *) (mem->mem + addr)) = val;
}

static INLINE int T123Load(void * mem, u32 size, int type, const char *filename)
{
   FILE *fp;
   u32 filesize, filesizecheck;
   u8 *buffer;
   u32 i;

   if (!filename)
      return -1;

   if ((fp = fopen(filename, "rb")) == NULL)
      return -1;

   // Calculate file size
   fseek(fp, 0, SEEK_END);
   filesize = ftell(fp);
   fseek(fp, 0, SEEK_SET);

   if (filesize > size)
      return -1;

   if ((buffer = (u8 *)malloc(filesize)) == NULL)
   {
      fclose(fp);
      return -1;
   }

   filesizecheck = (u32)fread((void *)buffer, 1, filesize, fp);
   fclose(fp);

   if (filesizecheck != filesize) return -1;

   switch (type)
   {
      case 1:
      {
         for (i = 0; i < filesize; i++)
            T1WriteByte((u8 *) mem, i, buffer[i]);
         break;
      }
      case 2:
      {
         for (i = 0; i < filesize; i++)
            T2WriteByte((u8 *) mem, i, buffer[i]);
         break;
      }
      case 3:
      {
         for (i = 0; i < filesize; i++)
            T3WriteByte((T3Memory *) mem, i, buffer[i]);
         break;
      }
      default:
      {
         free(buffer);
         return -1;
      }
   }

   free(buffer);

   return 0;
}

static INLINE int T123Save(void * mem, u32 size, int type, const char *filename)
{
   FILE *fp;
   u8 *buffer;
   u32 i;
   u32 sizecheck;

   if (filename == NULL)
      return 0;

   if (filename[0] == 0x00)
      return 0;

   if ((buffer = (u8 *)malloc(size)) == NULL)
      return -1;

   switch (type)
   {
      case 1:
      {
         for (i = 0; i < size; i++)
            buffer[i] = T1ReadByte((u8 *) mem, i);
         break;
      }
      case 2:
      {
         for (i = 0; i < size; i++)
            buffer[i] = T2ReadByte((u8 *) mem, i);
         break;
      }
      case 3:
      {
         for (i = 0; i < size; i++)
            buffer[i] = T3ReadByte((T3Memory *) mem, i);
         break;
      }
      default:
      {
         free(buffer);
         return -1;
      }
   }

   if ((fp = fopen(filename, "wb")) == NULL)
   {
      free(buffer);
      return -1;
   }

   sizecheck = (u32)fwrite((void *)buffer, 1, size, fp);
   fclose(fp);
   free(buffer);

   if (sizecheck != size) return -1;

   return 0;
}



void MappedMemoryInit(void);
/*
u8 FASTCALL mem_Read8(u32 addr);
u16 FASTCALL mem_Read16(u32 addr);
u32 FASTCALL mem_Read32(u32 addr);
void FASTCALL mem_Write8(u32 addr, u8 val);
void FASTCALL mem_Write16(u32 addr, u16 val);
void FASTCALL mem_Write32(u32 addr, u32 val);
*/


u8 FASTCALL mem_Read8(u32 addr);
u16 FASTCALL mem_Read16(u32 addr);
u32 FASTCALL mem_Read32(u32 addr);
void FASTCALL mem_Write8(u32 addr, u8 val);
void FASTCALL mem_Write16(u32 addr, u16 val);
void FASTCALL mem_Write32(u32 addr, u32 val);

u8 FASTCALL mem_ReadNoCache8(u32 addr);
u16 FASTCALL mem_ReadNoCache16(u32 addr);
u32 FASTCALL mem_ReadNoCache32(u32 addr);
void FASTCALL mem_WriteNoCache8(u32 addr, u8 val);
void FASTCALL mem_WriteNoCache16(u32 addr, u16 val);
void FASTCALL mem_WriteNoCache32(u32 addr, u32 val);

#define HIGH_WRAM_SIZE		0x100000
#define LOW_WRAM_SIZE		0x100000
#define BIOS_SIZE			0x80000
#define BACKUP_RAM_SIZE		0x10000



#define BIOS_ROM_BASE		(general_ram + 0x00000000)
#define AUDIO_RAM_BASE		(general_ram + 0x00080000)
#define SMPC_REG_BASE		(general_ram + 0x00100000)
#define BUP_RAM_BASE		(general_ram + 0x00180000)
#define LOW_RAM_BASE		(general_ram + 0x00200000)
#define HIGH_RAM_BASE		(general_ram + 0x00300000)

#define DUMMY_MEM_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 1))
#define MINIT_BASE			(SMPC_REG_BASE + (PAGE_SIZE * 2))	//XXX:Find where this is used
#define SINIT_BASE			(SMPC_REG_BASE + (PAGE_SIZE * 3))	//XXX:Find where this is used
#define CS2_REG_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 4))	//XXX:Find where this is used
#define SCSP_REG_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 5))	//XXX:Find where this is used
#define VDP1_REG_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 6))
#define VDP2_CRAM_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 7))
#define VDP2_REG_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 8))
#define SCU_REG_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 9))

#define SH2_REG_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 10))
#define SH2_DARRAY_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 11))
#define SH2_ONCHIP_BASE		(SMPC_REG_BASE + (PAGE_SIZE * 12))



#define MEM_1MiB_SIZE		0x100000
#define MEM_4MiB_SIZE		(MEM_1MiB_SIZE << 2)

extern u8 bup_ram_written;
extern u8 *general_ram;//[MEM_4MiB_SIZE];

typedef void (FASTCALL *WriteFunc8)(u32, u8);
typedef void (FASTCALL *WriteFunc16)(u32, u16);
typedef void (FASTCALL *WriteFunc32)(u32, u32);

typedef u8 (FASTCALL *ReadFunc8)(u32);
typedef u16 (FASTCALL *ReadFunc16)(u32);
typedef u32 (FASTCALL *ReadFunc32)(u32);

extern WriteFunc8 mem_write8_arr[0x100];
extern WriteFunc16 mem_write16_arr[0x100];
extern WriteFunc32 mem_write32_arr[0x100];

extern ReadFunc8 mem_read8_arr[0x100];
extern ReadFunc16 mem_read16_arr[0x100];
extern ReadFunc32 mem_read32_arr[0x100];

#define MEM_GET_FUNC_ADDR(addr) (((addr) >> 19) & 0xFF)

typedef struct {
u32 addr;
u32 val;
} result_struct;

#define SEARCHBYTE              0
#define SEARCHWORD              1
#define SEARCHLONG              2

#define SEARCHEXACT             (0 << 2)
#define SEARCHLESSTHAN          (1 << 2)
#define SEARCHGREATERTHAN       (2 << 2)

#define SEARCHUNSIGNED          (0 << 4)
#define SEARCHSIGNED            (1 << 4)
#define SEARCHHEX               (2 << 4)
#define SEARCHSTRING            (3 << 4)
#define SEARCHREL8BIT           (6 << 4)
#define SEARCHREL16BIT          (7 << 4)


void mem_Init(void);
void mem_Deinit(void);

int LoadBios(const char *filename);
int LoadBackupRam(const char *filename);
void FormatBackupRam(void *mem, u32 size);
u32 getMemClock(u32 addr);

u16 FASTCALL bios_Read16(u32 addr);

#endif
