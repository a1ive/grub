/*
 *  SSTD  -- Simple StdLib for UEFI
 *  Copyright (C) 2019  a1ive
 *
 *  SSTD is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  SSTD is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SSTD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _SSTDLIB_H
#define _SSTDLIB_H

#include <efi.h>
#include <efilib.h>

typedef __SIZE_TYPE__ size_t;
typedef signed long ssize_t;

typedef __WCHAR_TYPE__ wchar_t;
typedef __WINT_TYPE__ wint_t;

typedef void mbstate_t;

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define __unused __attribute__ (( unused ))

#define STD_CR(RECORD, TYPE, FIELD) \
    ((TYPE *) ((char *) (RECORD) - (char *) &(((TYPE *) 0)->FIELD)))

#define L( x ) _L ( x )
#define _L( x ) L ## x

#define malloc(size) AllocatePool((UINTN) size)
#define zalloc(size) AllocateZeroPool((UINTN) size)
#define free(ptr)    FreePool(ptr)

#define memcpy std_memcpy
void *std_memcpy (void *dest, const void *src, size_t len);
#define memset std_memset
void *std_memset (void *dest, int c, size_t len);
#define memmove std_memmove
void *std_memmove (void *dest, const void *src, size_t n);
int memcmp (const void *src1, const void *src2, size_t len);

int isspace (int c);
int islower (int c);
int isupper (int c);
int toupper (int c);
char *strcpy (char *dest, const char *src);
int strcmp (const char *str1, const char *str2);
char *strchr (const char *s, int c);
char *strrchr (const char *s, int c);
size_t strlen (const char *str);
unsigned long strtoul (const char *nptr, char **endptr, int base);
char *strdup (const char *s);
char *strndup (const char *s, size_t n);

static inline int
iswlower (wint_t c)
{
  return islower (c);
}
static inline int
iswupper (wint_t c)
{
  return isupper (c);
}
static inline int
towupper (wint_t c)
{
  return toupper (c);
}
static inline int
iswspace (wint_t c)
{
  return isspace (c);
}

int wcscasecmp (const wchar_t *str1, const wchar_t *str2);
size_t wcslen (const wchar_t *str);
wchar_t *wcschr (const wchar_t *str, wchar_t c);
size_t wcrtomb (char *buf, wchar_t wc, mbstate_t *ps __attribute__((unused)));
size_t mbstowcs (wchar_t *dst, const char *src, size_t size);

int putchar (int character);
int getchar (void);

struct printf_context
{
  void (* handler) (struct printf_context *ctx, unsigned int c);
  size_t len;
};

size_t vcprintf (struct printf_context *ctx, const char *fmt, va_list args);
int vssnprintf (char *buf, ssize_t ssize, const char *fmt, va_list args);
int __attribute__ ((format (printf, 3, 4)))
ssnprintf (char *buf, ssize_t ssize, const char *fmt, ...);

int __attribute__ ((format (printf, 1, 2)))
printf (const char *fmt, ...);
int __attribute__ ((format (printf, 3, 4)))
snprintf (char *buf, size_t size, const char *fmt, ...);
int vprintf (const char *fmt, va_list args);
int vsnprintf (char *buf, size_t size, const char *fmt, va_list args);

#endif
