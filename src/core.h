/*  Copyright 2005 Guillaume Duhamel
    Copyright 2005-2006 Theo Berkau

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

#ifndef CORE_H
#define CORE_H

#include <stdio.h>
#include <string.h>

#ifndef ALIGNED
#ifdef _MSC_VER
#define ALIGNED(x) __declspec(align(x))
#else
#define ALIGNED(x) __attribute__((aligned(x)))
#endif
#endif

#ifndef STDCALL
#ifdef _MSC_VER
#define STDCALL __stdcall
#else
#define STDCALL
#endif
#endif

#ifndef FASTCALL
#ifdef __MINGW32__
#define FASTCALL __attribute__((fastcall))
#elif defined (__i386__)
#define FASTCALL __attribute__((regparm(3)))
#else
#define FASTCALL
#endif
#endif


#ifndef INLINE
#ifdef _MSC_VER
#define INLINE _inline
#else
#define INLINE inline
#endif
#endif

/* Wii has both stdint.h and "yabause" definitions of fixed
size types */
#include <gccore.h>
typedef unsigned long pointer;


typedef struct {
	unsigned int size;
	unsigned int done;
} IOCheck_struct;

static INLINE void ywrite(IOCheck_struct * check, void * ptr, size_t size, size_t nmemb, FILE * stream) {
   check->done += (unsigned int)fwrite(ptr, size, nmemb, stream);
   check->size += (unsigned int)nmemb;
}

static INLINE void yread(IOCheck_struct * check, void * ptr, size_t size, size_t nmemb, FILE * stream) {
   check->done += (unsigned int)fread(ptr, size, nmemb, stream);
   check->size += (unsigned int)nmemb;
}

static INLINE int StateWriteHeader(FILE *fp, const char *name, int version) {
   IOCheck_struct check;
   fprintf(fp, "%s", name);
   check.done = 0;
   check.size = 0;
   ywrite(&check, (void *)&version, sizeof(version), 1, fp);
   ywrite(&check, (void *)&version, sizeof(version), 1, fp); // place holder for size
   return (check.done == check.size) ? ftell(fp) : -1;
}

static INLINE int StateFinishHeader(FILE *fp, int offset) {
   IOCheck_struct check;
   int size = 0;
   size = ftell(fp) - offset;
   fseek(fp, offset - 4, SEEK_SET);
   check.done = 0;
   check.size = 0;
   ywrite(&check, (void *)&size, sizeof(size), 1, fp); // write true size
   fseek(fp, 0, SEEK_END);
   return (check.done == check.size) ? (size + 12) : -1;
}

static INLINE int StateCheckRetrieveHeader(FILE *fp, const char *name, int *version, int *size) {
   char id[4];
   size_t ret;

   if ((ret = fread((void *)id, 1, 4, fp)) != 4)
      return -1;

   if (strncmp(name, id, 4) != 0)
      return -2;

   if ((ret = fread((void *)version, 4, 1, fp)) != 1)
      return -1;

   if (fread((void *)size, 4, 1, fp) != 1)
      return -1;

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

// Terrible, but I'm not sure how to do the equivalent in inline
#ifdef HAVE_C99_VARIADIC_MACROS
#define AddString(s, ...) \
   { \
      sprintf(s, __VA_ARGS__); \
      s += strlen(s); \
   }
#else
#define AddString(s, r...) \
   { \
      sprintf(s, ## r); \
      s += strlen(s); \
   }
#endif

//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////

/* Minimum/maximum values */

#undef MIN
#undef MAX
#define MIN(a,b)  ((a) < (b) ? (a) : (b))
#define MAX(a,b)  ((a) > (b) ? (a) : (b))

//////////////////////////////////////////////////////////////////////////////

#ifdef __GNUC__

#define UNUSED __attribute ((unused))

#ifdef DEBUG
#define USED_IF_DEBUG
#else
#define USED_IF_DEBUG __attribute ((unused))
#endif

#ifdef SMPC_DEBUG
#define USED_IF_SMPC_DEBUG
#else
#define USED_IF_SMPC_DEBUG __attribute ((unused))
#endif

/* LIKELY(x) indicates that x is likely to be true (nonzero);
 * UNLIKELY(x) indicates that x is likely to be false (zero).
 * Use like: "if (UNLIKELY(a < b)) {...}" */
#define LIKELY(x) (__builtin_expect(!!(x), 1))
#define UNLIKELY(x) (__builtin_expect(!!(x), 0))

#else

#define UNUSED
#define USED_IF_DEBUG
#define USED_IF_SMPC_DEBUG
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)

#endif

#endif
