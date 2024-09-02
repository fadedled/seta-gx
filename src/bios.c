/*  Copyright 2006-2007 Theo Berkau

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

#include "memory.h"
#include "cs0.h"
#include "debug.h"
#include "sh2core.h"
#include "bios.h"
#include "smpc.h"

static u8 sh2masklist[0x20] = {
0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90, 0x80,
0x80, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70
};

static u32 scumasklist[0x20] = {
0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFC, 0xFFFFFFF8,
0xFFFFFFF0, 0xFFFFFFE0, 0xFFFFFFC0, 0xFFFFFF80,
0xFFFFFF80, 0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00,
0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00,
0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00,
0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00,
0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00,
0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00, 0xFFFFFE00
};

u32 interruptlist[2][0x80];

//////////////////////////////////////////////////////////////////////////////

void BiosInit(void)
{
   int i;

   // Setup vectors
   mem_Write32(0x06000600, 0x002B0009); // rte, nop
   mem_Write32(0x06000604, 0xE0F0600C); // mov #0xF0, r0; extu.b r0, r0
   mem_Write32(0x06000608, 0x400E8BFE); // ldc r0, sr; bf
   mem_Write32(0x0600060C, 0x00090009); // nop
   mem_Write32(0x06000610, 0x000B0009); // rts, nop

   for (i = 0; i < 0x200; i+=4)
   {
      mem_Write32(0x06000000+i, 0x06000600);
      mem_Write32(0x06000400+i, 0x06000600);
      interruptlist[0][i >> 2] = 0x06000600;
      interruptlist[1][i >> 2] = 0x06000600;
   }

   mem_Write32(0x06000010, 0x06000604);
   mem_Write32(0x06000018, 0x06000604);
   mem_Write32(0x06000024, 0x06000604);
   mem_Write32(0x06000028, 0x06000604);
   interruptlist[0][4] = 0x06000604;
   interruptlist[0][6] = 0x06000604;
   interruptlist[0][9] = 0x06000604;
   interruptlist[0][10] = 0x06000604;

   mem_Write32(0x06000410, 0x06000604);
   mem_Write32(0x06000418, 0x06000604);
   mem_Write32(0x06000424, 0x06000604);
   mem_Write32(0x06000428, 0x06000604);
   interruptlist[1][4] = 0x06000604;
   interruptlist[1][6] = 0x06000604;
   interruptlist[1][9] = 0x06000604;
   interruptlist[1][10] = 0x06000604;

   // Scu Interrupts
   for (i = 0; i < 0x38; i+=4)
   {
      mem_Write32(0x06000100+i, 0x00000400+i);
      interruptlist[0][0x40+(i >> 2)] = 0x00000400+i;
   }

   for (i = 0; i < 0x40; i+=4)
   {
      mem_Write32(0x06000140+i, 0x00000440+i);
      interruptlist[0][0x50+(i >> 2)] = 0x00000440+i;
   }

   for (i = 0; i < 0x100; i+=4)
      mem_Write32(0x06000A00+i, 0x06000610);

   // Setup Bios Functions
   mem_Write32(0x06000210, 0x00000210);
   mem_Write32(0x0600026C, 0x0000026C);
   mem_Write32(0x06000274, 0x00000274);
   mem_Write32(0x06000280, 0x00000280);
   mem_Write32(0x0600029C, 0x0000029C);
   mem_Write32(0x060002DC, 0x000002DC);
   mem_Write32(0x06000300, 0x00000300);
   mem_Write32(0x06000304, 0x00000304);
   mem_Write32(0x06000310, 0x00000310);
   mem_Write32(0x06000314, 0x00000314);
   mem_Write32(0x06000320, 0x00000320);
   mem_Write32(0x06000324, 0x00000000);
   mem_Write32(0x06000330, 0x00000330);
   mem_Write32(0x06000334, 0x00000334);
   mem_Write32(0x06000340, 0x00000340);
   mem_Write32(0x06000344, 0x00000344);
   mem_Write32(0x06000348, 0xFFFFFFFF);
   mem_Write32(0x06000354, 0x00000000);
   mem_Write32(0x06000358, 0x00000358);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosSetScuInterrupt(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

//   LOG("BiosSetScuInterrupt. vector = %02X, func = %08X\n", sh->regs.R[4], sh->regs.R[5]);

   if (sh->regs.R[5] == 0)
   {
      mem_Write32(0x06000900+(sh->regs.R[4] << 2), 0x06000610);
      sh->cycles += 8;
   }
   else
   {
      mem_Write32(0x06000900+(sh->regs.R[4] << 2), sh->regs.R[5]);
      sh->cycles += 9;
   }

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosGetScuInterrupt(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   // check me
//   LOG("BiosGetScuInterrupt\n");

   sh->regs.R[0] = mem_Read32(0x06000900+(sh->regs.R[4] << 2));
   sh->cycles += 5;

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosSetSh2Interrupt(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

//   LOG("BiosSetSh2Interrupt\n");

   if (sh->regs.R[5] == 0)
   {
      mem_Write32(sh->regs.VBR+(sh->regs.R[4] << 2), interruptlist[sh->isslave][sh->regs.R[4]]);
      sh->cycles += 8;
   }
   else
   {
      mem_Write32(sh->regs.VBR+(sh->regs.R[4] << 2), sh->regs.R[5]);
      sh->cycles += 9;
   }

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosGetSh2Interrupt(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   // check me
//   LOG("BiosGetSh2Interrupt\n");

   sh->regs.R[0] = mem_Read32(sh->regs.VBR+(sh->regs.R[4] << 2));
   sh->cycles += 5;

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosSetScuInterruptMask(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   // check me
   LOG("BiosSetScuInterruptMask\n");

   mem_Write32(0x06000348, sh->regs.R[4]);
   mem_Write32(0x25FE00A0, sh->regs.R[4]); // Interrupt Mask Register

   if (!(sh->regs.R[4] & 0x8000)) // double check this
      mem_Write32(0x25FE00A8, 1); // A-bus Interrupt Acknowledge

   sh->cycles += 17;

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosChangeScuInterruptMask(SH2_struct * sh)
{
   u32 newmask;

   SH2GetRegisters(sh, &sh->regs);

//   LOG("BiosChangeScuInterruptMask\n");

   // Read Stored Scu Interrupt Mask, AND it by R4, OR it by R5, then put it back
   newmask = (mem_Read32(0x06000348) & sh->regs.R[4]) | sh->regs.R[5];
   mem_Write32(0x06000348, newmask);
   mem_Write32(0x25FE00A0, newmask); // Interrupt Mask Register
   mem_Write32(0x25FE00A4, (u32)(s16)sh->regs.R[4]); // Interrupt Status Register

   if (!(sh->regs.R[4] & 0x8000)) // double check this
      mem_Write32(0x25FE00A8, 1); // A-bus Interrupt Acknowledge

   sh->cycles += 20;

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosCDINIT2(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosCDINIT1(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosGetSemaphore(SH2_struct * sh)
{
   u8 temp;

   SH2GetRegisters(sh, &sh->regs);

   // check me
   LOG("BiosGetSemaphore\n");

   if ((temp = mem_Read8(0x06000B00 + sh->regs.R[4])) == 0)
      sh->regs.R[0] = 1;
   else
      sh->regs.R[0] = 0;

   temp |= 0x80;
   mem_Write8(0x06000B00 + sh->regs.R[4], temp);

   sh->cycles += 11;
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosClearSemaphore(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   // check me
   LOG("BiosClearSemaphore\n");

   mem_Write8(0x06000B00 + sh->regs.R[4], 0);

   sh->cycles += 5;
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosChangeSystemClock(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosChangeSystemClock\n");

   mem_Write32(0x06000324, sh->regs.R[4]);

   SmpcCKCHG(sh->regs.R[4] != 0);



   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosChangeScuInterruptPriority(SH2_struct * sh)
{
   int i;

   SH2GetRegisters(sh, &sh->regs);

   // check me
//   LOG("BiosChangeScuInterruptPriority\n");

   for (i = 0; i < 0x20; i++)
   {
      scumasklist[i] = mem_Read32(sh->regs.R[4]+(i << 2));
      sh2masklist[i] = (scumasklist[i] >> 16);
      if (scumasklist[i] & 0x8000)
         scumasklist[i] |= 0xFFFF0000;
      else
         scumasklist[i] &= 0x0000FFFF;
   }

   sh->cycles += 186;
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosExecuteCDPlayer(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosExecuteCDPlayer\n");

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosPowerOnMemoryClear(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosPowerOnMemoryClear\n");

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosCheckMPEGCard(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosCheckMPEGCard\n");

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static u32 GetDeviceStats(u32 device, u32 *size, u32 *addr, u32 *blocksize)
{
   switch(device)
   {
      case 0:
         *addr = 0x00180000;
         *size = 0x8000;
         *blocksize = 0x40;
         return 0;
      case 1:
         if ((CartridgeArea->cartid & 0xF0) == 0x20)
         {
            *addr = 0x04000000;
            *size = 0x40000 << (CartridgeArea->cartid & 0x0F);
            if (CartridgeArea->cartid == 0x24)
               *blocksize = 0x400;
            else
               *blocksize = 0x200;

            return 0;
         }
         else
            return 1;
      default:
         *addr = 0;
         *size = 0;
         *blocksize = 0;
         return 1;
   }

   return 1;
}

//////////////////////////////////////////////////////////////////////////////

static int CheckHeader(UNUSED u32 device)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int CalcSaveSize(u32 tableaddr, int blocksize)
{
   int numblocks=0;

   // Now figure out how many blocks this save is
   for(;;)
   {
       u16 block;
       block = (mem_Read8(tableaddr) << 8) | mem_Read8(tableaddr + 2);
       if (block == 0)
         break;
       tableaddr += 4;
       if (((tableaddr-1) & ((blocksize << 1) - 1)) == 0)
          tableaddr += 8;
       numblocks++;
   }

   return numblocks;
}

//////////////////////////////////////////////////////////////////////////////

static u32 GetFreeSpace(UNUSED u32 device, u32 size, u32 addr, u32 blocksize)
{
   u32 i;
   u32 usedblocks=0;

   for (i = ((2 * blocksize) << 1); i < (size << 1); i += (blocksize << 1))
   {
      // Find a block with the start of a save
      if (((s8)mem_Read8(addr + i + 1)) < 0)
      {
         // Now figure out how many blocks this save is
         usedblocks += (CalcSaveSize(addr+i+0x45, blocksize) + 1);
      }
   }

   return ((size / blocksize) - 2 - usedblocks);
}

//////////////////////////////////////////////////////////////////////////////

static u32 FindSave(UNUSED u32 device, u32 stringaddr, u32 blockoffset, u32 size, u32 addr, u32 blocksize)
{
   u32 i;

   for (i = ((blockoffset * blocksize) << 1); i < (size << 1); i += (blocksize << 1))
   {
      // Find a block with the start of a save
      if (((s8)mem_Read8(addr + i + 1)) < 0)
      {
         int i3;

         // See if string matches, or if there's no string to check, just copy
         // the data over
         for (i3 = 0; i3 < 11; i3++)
         {
            u8 data = mem_Read8(stringaddr+i3);

            if (mem_Read8(addr+i+0x9+(i3*2)) != data)
            {
               if (data == 0)
                  // There's no string to match
                  return ((i / blocksize) >> 1);
               else
                  // No Match, move onto the next block
                  i3 = 12;
            }
            else
            {
               // Match
               if (i3 == 10 || data == 0)
                  return ((i / blocksize) >> 1);
            }
         }
      }
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static u32 FindSave2(UNUSED u32 device, const char *string, u32 blockoffset, u32 size, u32 addr, u32 blocksize)
{
   u32 i;

   for (i = ((blockoffset * blocksize) << 1); i < (size << 1); i += (blocksize << 1))
   {
      // Find a block with the start of a save
      if (((s8)mem_Read8(addr + i + 1)) < 0)
      {
         int i3;

         // See if string matches, or if there's no string to check, just copy
         // the data over
         for (i3 = 0; i3 < 11; i3++)
         {
            if (mem_Read8(addr+i+0x9+(i3*2)) != string[i3])
            {
               if (string[i3] == 0)
                  // There's no string to match
                  return ((i / blocksize) >> 1);
               else
                  // No Match, move onto the next block
                  i3 = 12;
            }
            else
            {
               // Match
               if (i3 == 10 || string[i3] == 0)
                  return ((i / blocksize) >> 1);
            }
         }
      }
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void DeleteSave(u32 addr, u32 blockoffset, u32 blocksize)
{
    mem_Write8(addr + (blockoffset * blocksize * 2) + 0x1, 0x00);
}

//////////////////////////////////////////////////////////////////////////////

static u16 *GetFreeBlocks(u32 addr, u32 blocksize, u32 numblocks, u32 size)
{
   u8 *blocktbl;
   u16 *freetbl;
   u32 tableaddr;
   u32 i;
   u32 blockcount=0;

   // Create a table that tells us which blocks are free and used
   if ((blocktbl = (u8 *)malloc(sizeof(u8) * (size / blocksize))) == NULL)
      return NULL;

   memset(blocktbl, 0, (size / blocksize));

   for (i = ((2 * blocksize) << 1); i < (size << 1); i += (blocksize << 1))
   {
      // Find a block with the start of a save
      if (((s8)mem_Read8(addr + i + 1)) < 0)
      {
         tableaddr = addr+i+0x45;
         blocktbl[i / (blocksize << 1)] = 1;

         // Now let's figure out which blocks are used
         for(;;)
         {
            u16 block;
            block = (mem_Read8(tableaddr) << 8) | mem_Read8(tableaddr + 2);
            if (block == 0)
               break;
            tableaddr += 4;
            if (((tableaddr-1) & ((blocksize << 1) - 1)) == 0)
               tableaddr += 8;
            // block is used
            blocktbl[block] = 1;
         }
      }
   }

   if ((freetbl = (u16 *)malloc(sizeof(u16) * numblocks)) == NULL)
   {
      free(blocktbl);
      return NULL;
   }

   // Find some free blocks for us to use
   for (i = 2; i < (size / blocksize); i++)
   {
      if (blocktbl[i] == 0)
      {
         freetbl[blockcount] = (u16)i;
         blockcount++;

         if (blockcount >= numblocks)
            break;
      }
   }

   // Ok, we're all done
   free(blocktbl);

   return freetbl;
}

//////////////////////////////////////////////////////////////////////////////

static u16 *ReadBlockTable(u32 addr, u32 *tableaddr, int block, int blocksize, int *numblocks, int *blocksread)
{
   u16 *blocktbl;
   int i=0;

   tableaddr[0] = addr + (block * blocksize * 2) + 0x45;
   blocksread[0]=0;

   // First of all figure out how large of buffer we need
   numblocks[0] = CalcSaveSize(tableaddr[0], blocksize);

   // Allocate buffer
   if ((blocktbl = (u16 *)malloc(sizeof(u16) * numblocks[0])) == NULL)
      return NULL;

   // Now read in the table
   for(i = 0; i < numblocks[0]; i++)
   {
       u16 block;
       block = (mem_Read8(tableaddr[0]) << 8) | mem_Read8(tableaddr[0] + 2);
       tableaddr[0] += 4;

       if (((tableaddr[0]-1) & ((blocksize << 1) - 1)) == 0)
       {
          tableaddr[0] = addr + (blocktbl[blocksread[0]] * blocksize * 2) + 9;
          blocksread[0]++;
       }
       blocktbl[i] = block;
   }

   tableaddr[0] += 4;

   return blocktbl;
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPInit(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

//   LOG("BiosBUPInit. arg1 = %08X, arg2 = %08X, arg3 = %08X\n", sh->regs.R[4], sh->regs.R[5], sh->regs.R[6]);

   // Setup Function table
   mem_Write32(0x06000354, sh->regs.R[5]);
   mem_Write32(sh->regs.R[5]+0x00, 0x00000380);
   mem_Write32(sh->regs.R[5]+0x04, 0x00000384);
   mem_Write32(sh->regs.R[5]+0x08, 0x00000388);
   mem_Write32(sh->regs.R[5]+0x0C, 0x0000038C);
   mem_Write32(sh->regs.R[5]+0x10, 0x00000390);
   mem_Write32(sh->regs.R[5]+0x14, 0x00000394);
   mem_Write32(sh->regs.R[5]+0x18, 0x00000398);
   mem_Write32(sh->regs.R[5]+0x1C, 0x0000039C);
   mem_Write32(sh->regs.R[5]+0x20, 0x000003A0);
   mem_Write32(sh->regs.R[5]+0x24, 0x000003A4);
   mem_Write32(sh->regs.R[5]+0x28, 0x000003A8);
   mem_Write32(sh->regs.R[5]+0x2C, 0x000003AC);

   // Setup Device list

   // First Device
   mem_Write16(sh->regs.R[6], 1); // ID
   mem_Write16(sh->regs.R[6]+0x2, 1); // Number of partitions

   // Second Device
   if ((CartridgeArea->cartid & 0xF0) == 0x20)
   {
      mem_Write16(sh->regs.R[6]+0x4, 2); // ID
      mem_Write16(sh->regs.R[6]+0x6, 1); // Number of partitions
   }
   else
   {
      mem_Write16(sh->regs.R[6]+0x4, 0); // ID
      mem_Write16(sh->regs.R[6]+0x6, 0); // Number of partitions
   }

   // Third Device
   mem_Write16(sh->regs.R[6]+0x08, 0); // ID
   mem_Write16(sh->regs.R[6]+0x0A, 0); // Number of partitions

   // cycles need to be incremented

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPSelectPartition(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosBUPSelectPartition. PR = %08X\n", sh->regs.PR);

   sh->regs.R[0] = 0; // returns 0 if there's no error
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPFormat(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

//   LOG("BiosBUPFormat. PR = %08X\n", sh->regs.PR);

   BupFormat(sh->regs.R[4]);

   sh->regs.R[0] = 0; // returns 0 if there's no error
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPStatus(SH2_struct * sh)
{
   u32 size;
   u32 addr;
   u32 blocksize;
   u32 ret;
   u32 freeblocks=0;

   SH2GetRegisters(sh, &sh->regs);

//   LOG("BiosBUPStatus. arg1 = %d, arg2 = %d, arg3 = %08X, PR = %08X\n", sh->regs.R[4], sh->regs.R[5], sh->regs.R[6], sh->regs.PR);

   // Fill in status variables
   ret = GetDeviceStats(sh->regs.R[4], &size, &addr, &blocksize);

   // Make sure there's a proper header, and return if there's any other errors
   if (ret == 1 || CheckHeader(sh->regs.R[4]) != 0)
   {
      // Error
      sh->regs.R[0] = ret;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   freeblocks = GetFreeSpace(sh->regs.R[4], size, addr, blocksize);

   mem_Write32(sh->regs.R[6], size); // Size of Backup Ram (in bytes)
   mem_Write32(sh->regs.R[6]+0x4, size / blocksize); // Size of Backup Ram (in blocks)
   mem_Write32(sh->regs.R[6]+0x8, blocksize); // Size of block
   mem_Write32(sh->regs.R[6]+0xC, ((blocksize - 6) * freeblocks) - 30); // Free space(in bytes)
   mem_Write32(sh->regs.R[6]+0x10, freeblocks); // Free space(in blocks)
   mem_Write32(sh->regs.R[6]+0x14, freeblocks); // Not sure, but seems to be the same as Free Space(in blocks)

   // cycles need to be incremented

   sh->regs.R[0] = ret; // returns 0 if there's no error
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPWrite(SH2_struct * sh)
{
   u32 size;
   u32 addr;
   u32 blocksize;
   u32 block;
   u32 ret;
   u32 savesize;
   u16 *blocktbl;
   u32 workaddr;
   u32 blockswritten=0;
   u32 datasize;
   u32 i;

   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosBUPWrite. arg1 = %d, arg2 = %08X, arg3 = %08X, arg4 = %d, PR = %08X\n", sh->regs.R[4], sh->regs.R[5], sh->regs.R[6], sh->regs.R[7], sh->regs.PR);

   // Fill in status variables
   ret = GetDeviceStats(sh->regs.R[4], &size, &addr, &blocksize);
   if (ret == 1)
   {
      // Error
      sh->regs.R[0] = ret;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   // See if save exists already
   if ((block = FindSave(sh->regs.R[4], sh->regs.R[5], 2, size, addr, blocksize)) != 0)
   {
      // save exists

      // Should we be overwriting it?
      if (sh->regs.R[7] != 0)
      {
         // Nope, let's bail instead
         sh->regs.R[0] = 6;
         sh->regs.PC = sh->regs.PR;
         SH2SetRegisters(sh, &sh->regs);
         return;
      }

      // Delete old save
      DeleteSave(addr, block, blocksize);
   }

   // Let's figure out how many blocks will be needed for the save
   datasize = mem_Read32(sh->regs.R[5]+0x1C);
   savesize = (datasize + 0x1D) / (blocksize - 6);
   if ((datasize + 0x1D) % (blocksize - 6))
      savesize++;

   // Will it blend? Err... fit
   if (savesize > GetFreeSpace(sh->regs.R[4], size, addr, blocksize))
   {
      // Nope, time to bail
      sh->regs.R[0] = 4;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   // Find free blocks for the save
   if ((blocktbl = GetFreeBlocks(addr, blocksize, savesize, size)) == NULL)
   {
      // Just return an error that might make sense
      sh->regs.R[0] = 8;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   // Create save
   workaddr = addr + (blocktbl[0] * blocksize * 2);

   mem_Write8(workaddr+0x1, 0x80);

   // Copy over filename
   for (i = workaddr+0x9; i < ((workaddr+0x9) + (11 * 2)); i+=2)
   {
      mem_Write8(i, mem_Read8(sh->regs.R[5]));
      sh->regs.R[5]++;
   }

   sh->regs.R[5]++;

   // Copy over comment
   for (i = workaddr+0x21; i < ((workaddr+0x21) + (10 * 2)); i+=2)
   {
      mem_Write8(i, mem_Read8(sh->regs.R[5]));
      sh->regs.R[5]++;
   }

   // Copy over language
   mem_Write8(workaddr+0x1F, mem_Read8(sh->regs.R[5]));
   sh->regs.R[5]++;

   sh->regs.R[5]++;

   // Copy over date
   for (i = workaddr+0x35; i < ((workaddr+0x35) + (4 * 2)); i+=2)
   {
      mem_Write8(i, mem_Read8(sh->regs.R[5]));
      sh->regs.R[5]++;
   }

   // Copy over data size
   for (i = workaddr+0x3D; i < ((workaddr+0x3D) + (4 * 2)); i+=2)
   {
      mem_Write8(i, mem_Read8(sh->regs.R[5]));
      sh->regs.R[5]++;
   }

   // write the block table
   workaddr += 0x45;

   for (i = 1; i < savesize; i++)
   {
      mem_Write8(workaddr, (u8)(blocktbl[i] >> 8));
      workaddr+=2;
      mem_Write8(workaddr, (u8)blocktbl[i]);
      workaddr+=2;

      if (((workaddr-1) & ((blocksize << 1) - 1)) == 0)
      {
         // Next block
         blockswritten++;
         workaddr = addr + (blocktbl[blockswritten] * blocksize * 2) + 9;
      }
   }

   // Write 2 blank bytes so we now how large the table size is next time
   mem_Write8(workaddr, 0);
   workaddr+=2;
   mem_Write8(workaddr, 0);
   workaddr+=2;

   // Lastly, write the actual save data
   while (datasize > 0)
   {
      mem_Write8(workaddr, mem_Read8(sh->regs.R[6]));
      datasize--;
      sh->regs.R[6]++;
      workaddr+=2;

      if (((workaddr-1) & ((blocksize << 1) - 1)) == 0)
      {
         // Next block
         blockswritten++;
         workaddr = addr + (blocktbl[blockswritten] * blocksize * 2) + 9;
      }
   }

   free(blocktbl);

   sh->regs.R[0] = 0; // returns 0 if there's no error
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPRead(SH2_struct * sh)
{
   u32 size;
   u32 addr;
   u32 blocksize;
   u32 block;
   u32 ret;
   u32 tableaddr;
   u16 *blocktbl;
   int numblocks;
   int blocksread;
   u32 datasize;

   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosBUPRead\n", sh->regs.PR);

   ret = GetDeviceStats(sh->regs.R[4], &size, &addr, &blocksize);

   if (ret == 1)
   {
      // Error
      sh->regs.R[0] = ret;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   // See if save exists
   if ((block = FindSave(sh->regs.R[4], sh->regs.R[5], 2, size, addr, blocksize)) == 0)
   {
      // save doesn't exist
      sh->regs.R[0] = 5;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   tableaddr = addr + (block * blocksize * 2) + 0x3D;
   datasize = (mem_Read8(tableaddr) << 24) | (mem_Read8(tableaddr + 2) << 16) |
              (mem_Read8(tableaddr+4) << 8) | mem_Read8(tableaddr + 6);

   // Read in Block Table
   if ((blocktbl = ReadBlockTable(addr, &tableaddr, block, blocksize, &numblocks, &blocksread)) == NULL)
   {
      // Just return an error that might make sense
      sh->regs.R[0] = 8;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   // Now let's read in the data
   while (datasize > 0)
   {
      mem_Write8(sh->regs.R[6], mem_Read8(tableaddr));
      datasize--;
      sh->regs.R[6]++;
      tableaddr+=2;

      if (((tableaddr-1) & ((blocksize << 1) - 1)) == 0)
      {
         // Load up the next block
         tableaddr = addr + (blocktbl[blocksread] * blocksize * 2) + 9;
         blocksread++;
      }
   }

   free(blocktbl);

   sh->regs.R[0] = 0; // returns 0 if there's no error
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPDelete(SH2_struct * sh)
{
   u32 size;
   u32 addr;
   u32 blocksize;
   u32 block;
   u32 ret;

   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosBUPDelete. PR = %08X\n", sh->regs.PR);

   // Fill in status variables
   ret = GetDeviceStats(sh->regs.R[4], &size, &addr, &blocksize);
   if (ret == 1)
   {
      // Error
      sh->regs.R[0] = ret;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   // See if save exists
   if ((block = FindSave(sh->regs.R[4], sh->regs.R[5], 2, size, addr, blocksize)) == 0)
   {
      // Since the save doesn't exist, let's bail with an error

      sh->regs.R[0] = 5;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   DeleteSave(addr, block, blocksize);

   sh->regs.R[0] = 0; // returns 0 if there's no error
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPDirectory(SH2_struct * sh)
{
   u32 size;
   u32 addr;
   u32 blocksize;
   u32 ret;
   u32 i;
   u32 blockoffset=2;
	//u8 filename[16];
   SH2GetRegisters(sh, &sh->regs);

//   int findmatch = mem_Read8(sh->regs.R[5]);

   for (i = 0; i < 12; i++)
      mem_Read8(sh->regs.R[5]+i);

   LOG("BiosBUPDirectory. arg1 = %d, arg2 = %s, arg3 = %08X, arg4 = %08X, PR = %08X\n", sh->regs.R[4], filename, sh->regs.R[6], sh->regs.R[7], sh->regs.PR);

   ret = GetDeviceStats(sh->regs.R[4], &size, &addr, &blocksize);

   if (ret == 1)
   {
      // Error
      sh->regs.R[0] = ret;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   for (i = 0; i < sh->regs.R[6]; i++)
   {
      u32 i4;
      u32 datasize=0;
      u32 block = FindSave(sh->regs.R[4], sh->regs.R[5], blockoffset, size, addr, blocksize);

      if (block == 0)
         break;

      blockoffset = block+1;

      // Alright, we found a match :) Time to copy over some data
      block = addr + (blocksize * block * 2);

      // Copy over filename
      for (i4 = block+0x9; i4 < ((block+0x9) + (11 * 2)); i4+=2)
      {
         mem_Write8(sh->regs.R[7], mem_Read8(i4));
         sh->regs.R[7]++;
      }
      mem_Write8(sh->regs.R[7], 0);
      sh->regs.R[7]++;

      // Copy over comment
      for (i4 = block+0x21; i4 < ((block+0x21) + (10 * 2)); i4+=2)
      {
         mem_Write8(sh->regs.R[7], mem_Read8(i4));
         sh->regs.R[7]++;
      }

      // Copy over language
      mem_Write8(sh->regs.R[7], mem_Read8(block+0x1F));
      sh->regs.R[7]++;

      mem_Write8(sh->regs.R[7], 0);
      sh->regs.R[7]++;

      // Copy over date
      for (i4 = block+0x35; i4 < ((block+0x35) + (4 * 2)); i4+=2)
      {
         mem_Write8(sh->regs.R[7], mem_Read8(i4));
         sh->regs.R[7]++;
      }

      // Copy over data size
      for (i4 = block+0x3D; i4 < ((block+0x3D) + (4 * 2)); i4+=2)
      {
         u8 data;
         datasize <<= 8;
         data = mem_Read8(i4);
         mem_Write8(sh->regs.R[7], data);
         datasize |= data;
         sh->regs.R[7]++;
      }

      // Calculate block size from the data size, and then copy it over
      mem_Write16(sh->regs.R[7], (u16)(((datasize + 0x1D) / (blocksize - 6)) + 1));
      sh->regs.R[7] += 4;
   }

   sh->regs.R[0] = i; // returns the number of successfully read dir entries
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPVerify(SH2_struct * sh)
{
   u32 size;
   u32 addr;
   u32 blocksize;
   u32 block;
   u32 ret;
   u32 tableaddr;
   u32 datasize;
   u16 *blocktbl;
   int numblocks;
   int blocksread;

   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosBUPVerify. PR = %08X\n", sh->regs.PR);

   ret = GetDeviceStats(sh->regs.R[4], &size, &addr, &blocksize);

   if (ret == 1)
   {
      // Error
      sh->regs.R[0] = ret;
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   // See if save exists
   if ((block = FindSave(sh->regs.R[4], sh->regs.R[5], 2, size, addr, blocksize)) == 0)
   {
      // Since the save doesn't exist, let's bail with an error
      sh->regs.R[0] = 5; // Not found
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   tableaddr = addr + (block * blocksize * 2) + 0x3D;
   datasize = (mem_Read8(tableaddr) << 24) | (mem_Read8(tableaddr + 2) << 16) |
              (mem_Read8(tableaddr+4) << 8) | mem_Read8(tableaddr + 6);

   // Read in Block Table
   if ((blocktbl = ReadBlockTable(addr, &tableaddr, block, blocksize, &numblocks, &blocksread)) == NULL)
   {
      // Just return an error that might make sense
      sh->regs.R[0] = 8; // Broken
      sh->regs.PC = sh->regs.PR;
      SH2SetRegisters(sh, &sh->regs);
      return;
   }

   // Now let's read in the data, and check to see if it matches
   while (datasize > 0)
   {
      if (mem_Read8(sh->regs.R[6]) != mem_Read8(tableaddr))
      {
         free(blocktbl);
         // Ok, the data doesn't match
         sh->regs.R[0] = 7; // No match
         sh->regs.PC = sh->regs.PR;
         SH2SetRegisters(sh, &sh->regs);
         return;
      }

      datasize--;
      sh->regs.R[6]++;
      tableaddr+=2;

      if (((tableaddr-1) & ((blocksize << 1) - 1)) == 0)
      {
         // Load up the next block
         tableaddr = addr + (blocktbl[blocksread] * blocksize * 2) + 9;
         blocksread++;
      }
   }

   free(blocktbl);

   sh->regs.R[0] = 0; // returns 0 if there's no error
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void ConvertMonthAndDay(u32 data, u32 monthaddr, u32 dayaddr, int type)
{
   int i;
   u16 monthtbl[11] = { 31, 31+28, 31+28+31, 31+28+31+30, 31+28+31+30+31,
                        31+28+31+30+31+30, 31+28+31+30+31+30+31,
                        31+28+31+30+31+30+31+31, 31+28+31+30+31+30+31+31+30,
                        31+28+31+30+31+30+31+31+30+31,
                        31+28+31+30+31+30+31+31+30+31+30 };

   if (data < monthtbl[0])
   {
      // Month
      mem_Write8(monthaddr, 1);

      // Day
      mem_Write8(dayaddr, (u8)(data + 1));
      return;
   }

   for (i = 1; i < 11; i++)
   {
      if (data <= monthtbl[i])
         break;
   }

   if (type == 1)
   {
      // Month
      mem_Write8(monthaddr, (u8)(i + 1));

      // Day
      if ((i + 1) == 2)
         mem_Write8(dayaddr, (u8)(data - monthtbl[(i - 1)] + 1));
      else
         mem_Write8(dayaddr, (u8)(data - monthtbl[(i - 1)]));
   }
   else
   {
      // Month
      mem_Write8(monthaddr, (u8)(i + 1));

      // Day
      mem_Write8(dayaddr, (u8)(data - monthtbl[(i - 1)] + 1));
   }
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPGetDate(SH2_struct * sh)
{
   u32 date;
   u32 div;
   u32 yearoffset;
   u32 yearremainder;

   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosBUPGetDate. PR = %08X\n", sh->regs.PR);

   date = sh->regs.R[4];

   // Time
   mem_Write8(sh->regs.R[5]+3, (u8)((date % 0x5A0) / 0x3C));

   // Minute
   mem_Write8(sh->regs.R[5]+4, (u8)(date % 0x3C));

   div = date / 0x5A0;

   // Week
   if (div > 0xAB71)
      mem_Write8(sh->regs.R[5]+5, (u8)((div + 1) % 7));
   else
      mem_Write8(sh->regs.R[5]+5, (u8)((div + 2) % 7));

   yearremainder = div % 0x5B5;

   if (yearremainder > 0x16E)
   {
      yearoffset = (yearremainder - 1) / 0x16D;
      ConvertMonthAndDay((yearremainder - 1) % 0x16D, sh->regs.R[5]+1, sh->regs.R[5]+2, 0);
   }
   else
   {
      yearoffset = 0;
      ConvertMonthAndDay(0, sh->regs.R[5]+1, sh->regs.R[5]+2, 1);
   }

   // Year
   mem_Write8(sh->regs.R[5], (u8)(((div / 0x5B5) * 4) + yearoffset));

   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosBUPSetDate(SH2_struct * sh)
{
   u32 date;
   u8 data;
   u32 remainder;
   u16 monthtbl[11] = { 31, 31+28, 31+28+31, 31+28+31+30, 31+28+31+30+31,
                        31+28+31+30+31+30, 31+28+31+30+31+30+31,
                        31+28+31+30+31+30+31+31, 31+28+31+30+31+30+31+31+30,
                        31+28+31+30+31+30+31+31+30+31,
                        31+28+31+30+31+30+31+31+30+31+30 };

   SH2GetRegisters(sh, &sh->regs);

   LOG("BiosBUPSetDate. PR = %08X\n", sh->regs.PR);

   // Year
   data = mem_Read8(sh->regs.R[4]);
   date = (data / 4) * 0x5B5;
   remainder = data % 4;
   if (remainder)
      date += (remainder * 0x16D) + 1;

   // Month
   data = mem_Read8(sh->regs.R[4]+1);
   if (data != 1 && data < 13)
   {
      date += monthtbl[data - 2];
      if (date > 2 && remainder == 0)
         date++;
   }

   // Day
   date += mem_Read8(sh->regs.R[4]+2) - 1;
   date *= 0x5A0;

   // Hour
   date += (mem_Read8(sh->regs.R[4]+3) * 0x3C);

   // Minute
   date += mem_Read8(sh->regs.R[4]+4);

   sh->regs.R[0] = date;
   sh->regs.PC = sh->regs.PR;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosHandleScuInterrupt(SH2_struct * sh, int vector)
{
   SH2GetRegisters(sh, &sh->regs);

   // Save R0-R7, PR, GBR, and old Interrupt mask to stack
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.R[0]);
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.R[1]);
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.R[2]);
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.R[3]);
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], mem_Read32(0x06000348));
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.R[4]);
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.R[5]);
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.R[6]);
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.R[7]);
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.PR);
   sh->regs.R[15] -= 4;
   mem_Write32(sh->regs.R[15], sh->regs.GBR);

   // Set SR according to vector
   sh->regs.SR.all = (u32)sh2masklist[vector - 0x40];

   // Write new Interrupt mask value
   mem_Write32(0x06000348, mem_Read32(0x06000348) | scumasklist[vector - 0x40]);
   mem_Write32(0x25FE00A0, mem_Read32(0x06000348) | scumasklist[vector - 0x40]);

   // Set PR to our Interrupt Return handler
   sh->regs.PR = 0x00000480;

   // Now execute the interrupt
   sh->regs.PC = mem_Read32(0x06000900+(vector << 2));
//   LOG("Interrupt PC = %08X. Read from %08X\n", sh->regs.PC, 0x06000900+(vector << 2));

   sh->cycles += 33;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

static void FASTCALL BiosHandleScuInterruptReturn(SH2_struct * sh)
{
   u32 oldmask;

   SH2GetRegisters(sh, &sh->regs);

   // Restore R0-R7, PR, GBR, and old Interrupt mask from stack
   sh->regs.GBR = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.PR = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.R[7] = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.R[6] = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.R[5] = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.R[4] = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   // Return SR back to normal
   sh->regs.SR.all = 0xF0;
   oldmask = mem_Read32(sh->regs.R[15]);
   mem_Write32(0x06000348, oldmask);
   mem_Write32(0x25FE00A0, oldmask);
   sh->regs.R[15] += 4;
   sh->regs.R[3] = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.R[2] = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.R[1] = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.R[0] = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;

   sh->regs.PC = mem_Read32(sh->regs.R[15]);
   sh->regs.R[15] += 4;
   sh->regs.SR.all = mem_Read32(sh->regs.R[15]) & 0x000003F3;
   sh->regs.R[15] += 4;

   sh->cycles += 24;
   SH2SetRegisters(sh, &sh->regs);
}

//////////////////////////////////////////////////////////////////////////////

int FASTCALL BiosHandleFunc(SH2_struct * sh)
{
   SH2GetRegisters(sh, &sh->regs);

   // Let's see if it's a bios function
   switch((sh->regs.PC - 0x200) >> 2)
   {
      case 0x04: // 0x06000210
         BiosPowerOnMemoryClear(sh);
         break;
      case 0x1B: // 0x0600026C
         BiosExecuteCDPlayer(sh);
         break;
      case 0x1D: // 0x06000274
         BiosCheckMPEGCard(sh);
         break;
      case 0x20: // 0x06000280
         BiosChangeScuInterruptPriority(sh);
         break;
      case 0x27: // 0x0600029C
         BiosCDINIT2(sh);
         break;
      case 0x37: // 0x060002DC
         BiosCDINIT1(sh);
         break;
      case 0x40: // 0x06000300
         BiosSetScuInterrupt(sh);
         break;
      case 0x41: // 0x06000304
         BiosGetScuInterrupt(sh);
         break;
      case 0x44: // 0x06000310
         BiosSetSh2Interrupt(sh);
         break;
      case 0x45: // 0x06000314
         BiosGetSh2Interrupt(sh);
         break;
      case 0x48: // 0x06000320
         BiosChangeSystemClock(sh);
         break;
      case 0x4C: // 0x06000330
         BiosGetSemaphore(sh);
         break;
      case 0x4D: // 0x06000334
         BiosClearSemaphore(sh);
         break;
      case 0x50: // 0x06000340
         BiosSetScuInterruptMask(sh);
         break;
      case 0x51: // 0x06000344
         BiosChangeScuInterruptMask(sh);
         break;
      case 0x56: // 0x06000358
         BiosBUPInit(sh);
         break;
      case 0x60: // 0x06000380
         break;
      case 0x61: // 0x06000384
         BiosBUPSelectPartition(sh);
         break;
      case 0x62: // 0x06000388
         BiosBUPFormat(sh);
         break;
      case 0x63: // 0x0600038C
         BiosBUPStatus(sh);
         break;
      case 0x64: // 0x06000390
         BiosBUPWrite(sh);
         break;
      case 0x65: // 0x06000394
         BiosBUPRead(sh);
         break;
      case 0x66: // 0x06000398
         BiosBUPDelete(sh);
         break;
      case 0x67: // 0x0600039C
         BiosBUPDirectory(sh);
         break;
      case 0x68: // 0x060003A0
         BiosBUPVerify(sh);
         break;
      case 0x69: // 0x060003A4
         BiosBUPGetDate(sh);
         break;
      case 0x6A: // 0x060003A8
         BiosBUPSetDate(sh);
         break;
      case 0x6B: // 0x060003AC
         break;
      case 0x80: // Interrupt Handler
      case 0x81:
      case 0x82:
      case 0x83:
      case 0x84:
      case 0x85:
      case 0x86:
      case 0x87:
      case 0x88:
      case 0x89:
      case 0x8A:
      case 0x8B:
      case 0x8C:
      case 0x8D:
      case 0x90:
      case 0x91:
      case 0x92:
      case 0x93:
      case 0x94:
      case 0x95:
      case 0x96:
      case 0x97:
      case 0x98:
      case 0x99:
      case 0x9A:
      case 0x9B:
      case 0x9C:
      case 0x9D:
      case 0x9E:
      case 0x9F:
         BiosHandleScuInterrupt(sh, (sh->regs.PC - 0x300) >> 2);
         break;
      case 0xA0: // Interrupt Handler Return
         BiosHandleScuInterruptReturn(sh);
         break;
      default:
         return 0;
   }

   return 1;
}

//////////////////////////////////////////////////////////////////////////////

deviceinfo_struct *BupGetDeviceList(int *numdevices)
{
   deviceinfo_struct *device;
   int devicecount=1;

   if ((CartridgeArea->cartid & 0xF0) == 0x20)
      devicecount++;

   if ((device = (deviceinfo_struct *)malloc(devicecount * sizeof(deviceinfo_struct))) == NULL)
   {
      *numdevices = 0;
      return NULL;
   }

   *numdevices = devicecount;

   device[0].id = 0;
   sprintf(device[0].name, "Internal Backup RAM");

   if ((CartridgeArea->cartid & 0xF0) == 0x20)
   {
      device[1].id = 1;
      sprintf(device[1].name, "%d Mbit Backup RAM Cartridge", 1 << ((CartridgeArea->cartid & 0xF)+1));
   }

   // For now it's only internal backup ram and cartridge, no floppy :(
//   device[2].id = 2;
//   sprintf(device[1].name, "Floppy Disk Drive");

   return device;
}

//////////////////////////////////////////////////////////////////////////////

int BupGetStats(u32 device, u32 *freespace, u32 *maxspace)
{
   u32 ret;
   u32 size;
   u32 addr;
   u32 blocksize;

   ret = GetDeviceStats(device, &size, &addr, &blocksize);

   // Make sure there's a proper header, and return if there's any other errors
   if (ret == 1 || CheckHeader(device) != 0)
      return 0;

   *maxspace = size / blocksize;
   *freespace = GetFreeSpace(device, size, addr, blocksize);

   return 1;
}

//////////////////////////////////////////////////////////////////////////////

saveinfo_struct *BupGetSaveList(u32 device, int *numsaves)
{
   u32 ret;
   u32 size;
   u32 addr;
   u32 blocksize;
   saveinfo_struct *save;
   int savecount=0;
   u32 i, j;
   u32 workaddr;

   ret = GetDeviceStats(device, &size, &addr, &blocksize);

   // Make sure there's a proper header, and return if there's any other errors
   if (ret == 1 || CheckHeader(device) != 0)
   {
      *numsaves = 0;
      return NULL;
   }

   for (i = ((2 * blocksize) << 1); i < (size << 1); i += (blocksize << 1))
   {
      // Find a block with the start of a save
      if (((s8)mem_Read8(addr + i + 1)) < 0)
         savecount++;
   }

   if ((save = (saveinfo_struct *)malloc(savecount * sizeof(saveinfo_struct))) == NULL)
   {
      *numsaves = 0;
      return NULL;
   }

   *numsaves = savecount;

   savecount = 0;

   for (i = ((2 * blocksize) << 1); i < (size << 1); i += (blocksize << 1))
   {
      // Find a block with the start of a save
      if (((s8)mem_Read8(addr + i + 1)) < 0)
      {
         workaddr = addr + i;

         // Copy over filename
         for (j = 0; j < 11; j++)
            save[savecount].filename[j] = mem_Read8(workaddr+0x9+(j * 2));
         save[savecount].filename[11] = '\0';

         // Copy over comment
         for (j = 0; j < 10; j++)
            save[savecount].comment[j] = mem_Read8(workaddr+0x21+(j * 2));
         save[savecount].comment[10] = '\0';

         // Copy over language
         save[savecount].language = mem_Read8(workaddr+0x1F);

         // Copy over Date(fix me)
         save[savecount].year = 0;
         save[savecount].month = 0;
         save[savecount].day = 0;
         save[savecount].hour = 0;
         save[savecount].minute = 0;
         save[savecount].week = 0;

         // Copy over data size
         save[savecount].datasize = (mem_Read8(workaddr+0x3D) << 24) |
                                    (mem_Read8(workaddr+0x3F) << 16) |
                                    (mem_Read8(workaddr+0x41) << 8) |
                                    mem_Read8(workaddr+0x43);

         // Calculate size in blocks
         save[savecount].blocksize = CalcSaveSize(workaddr+0x45, blocksize) + 1;
         savecount++;
      }
   }

   return save;
}

//////////////////////////////////////////////////////////////////////////////

int BupDeleteSave(u32 device, const char *savename)
{
   u32 ret;
   u32 size;
   u32 addr;
   u32 blocksize;
   u32 block;

   ret = GetDeviceStats(device, &size, &addr, &blocksize);

   // Make sure there's a proper header, and return if there's any other errors
   if (ret == 1 || CheckHeader(device) != 0)
      return -1;

   // Let's find and delete the save game
   if ((block = FindSave2(device, savename, 2, size, addr, blocksize)) != 0)
   {
      // Delete old save
      DeleteSave(addr, block, blocksize);
      return 0;
   }

   return -2;
}

//////////////////////////////////////////////////////////////////////////////

void BupFormat(u32 device)
{
   switch (device)
   {
      case 0: //Format local bup
         FormatBackupRam(bup_ram, 0x10000);
         break;
      case 1: //Format cart bup (will not happen)
         break;
      case 2:
         LOG("Formatting FDD not supported\n");
      default: break;
   }
}

//////////////////////////////////////////////////////////////////////////////

int BupCopySave(UNUSED u32 srcdevice, UNUSED u32 dstdevice, UNUSED const char *savename)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

int BupImportSave(UNUSED u32 device, const char *filename)
{
   FILE *fp;
   u32 filesize;
   u8 *buffer;

   if (!filename)
      return -1;

   if ((fp = fopen(filename, "rb")) == NULL)
      return -1;

   // Calculate file size
   fseek(fp, 0, SEEK_END);
   filesize = ftell(fp);
   fseek(fp, 0, SEEK_SET);

   if ((buffer = (u8 *)malloc(filesize)) == NULL)
   {
      fclose(fp);
      return -2;
   }

   fread((void *)buffer, 1, filesize, fp);
   fclose(fp);

   // Write save here

   free(buffer);
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

int BupExportSave(UNUSED u32 device, UNUSED const char *savename, UNUSED const char *filename)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

