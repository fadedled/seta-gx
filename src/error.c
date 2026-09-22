/*  Copyright 2005-2006 Theo Berkau

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "error.h"

#ifdef USE_SH2_OLD
	#include "sh2old/sh2core.h"
#else
	#include "sh2/sh2.h"
#endif
//////////////////////////////////////////////////////////////////////////////

static void AllocAmendPrintString(const char *string1, const char *string2)
{
   char *string;

   if ((string = (char *)malloc(strlen(string1) + strlen(string2) + 2)) == NULL)
      return;

   sprintf(string, "%s%s\n", string1, string2);
   //YuiErrorMsg(string);

   free(string);
}

//////////////////////////////////////////////////////////////////////////////

void YabSetError(int type, const void *extra)
{

   switch (type)
   {
      case YAB_ERR_FILENOTFOUND:
         AllocAmendPrintString("File not found: ", extra);
         break;
      case YAB_ERR_MEMORYALLOC:
         //YuiErrorMsg("Error allocating memory\n");
         break;
      case YAB_ERR_FILEREAD:
         AllocAmendPrintString("Error reading file: ", extra);
         break;
      case YAB_ERR_FILEWRITE:
         AllocAmendPrintString("Error writing file: ", extra);
         break;
      case YAB_ERR_CANNOTINIT:
         AllocAmendPrintString("Cannot initialize ", extra);
         break;
      case YAB_ERR_SH2INVALIDOPCODE:
#ifdef USE_SH2_OLD
		SH2_struct *sh;
         sh = (SH2_struct *)extra;
         SH2GetRegisters(sh, &sh->regs);
         //YuiErrorMsg(tempstr);
#endif
         break;
      case YAB_ERR_SH2READ:
         //YuiErrorMsg("SH2 read error\n"); // fix me
         break;
      case YAB_ERR_SH2WRITE:
         //YuiErrorMsg("SH2 write error\n"); // fix me
         break;
      case YAB_ERR_SDL:
         AllocAmendPrintString("SDL Error: ", extra);
         break;
      case YAB_ERR_OTHER:
         //YuiErrorMsg((char *)extra);
         break;
      case YAB_ERR_UNKNOWN:
      default:
         //YuiErrorMsg("Unknown error occurred\n");
         break;
   }
}

//////////////////////////////////////////////////////////////////////////////


//void err_AssertError(u32 assert, const char * format, ...) {
//	if (!assert) { //
//		va_list args;
//
//
//
//		va_start(args, format);
//		vprintf(format, args);
//		va_end(args);
//		printf(format, );
//	}
//}


void YabErrorMsg(const char * format, ...) {
    va_list l;
    int n;
    char * buffer;

    va_start(l, format);
    n = vsnprintf(NULL, 0, format, l);
    va_end(l);

    buffer = malloc(n + 1);

    va_start(l, format);
    vsprintf(buffer, format, l);
    va_end(l);

    //YuiErrorMsg(buffer);
    free(buffer);
}

//////////////////////////////////////////////////////////////////////////////

