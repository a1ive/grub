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

#include <efi.h>
#include <efilib.h>

#include <sstdlib.h>

/* memcpy */
void *
std_memcpy (void *dest, const void *src, size_t len)
{
  CopyMem (dest, src, len);
  return dest;
}

/* memset */
void *
std_memset (void *dest, int c, size_t len)
{
  SetMem (dest, len, c);
  return dest;
}

/* memmove */
void *
std_memmove (void *dest, const void *src, size_t n)
{
  char *d = (char *) dest;
  const char *s = (const char *) src;
  if (d < s)
    while (n--)
      *d++ = *s++;
  else
  {
    d += n;
    s += n;
    while (n--)
      *--d = *--s;
  }
  return dest;
}

int
memcmp (const void *src1, const void *src2, size_t len)
{
  const uint8_t *bytes1 = src1;
  const uint8_t *bytes2 = src2;
  int diff;
  while (len--)
  {
    if ((diff = (*(bytes1++) - *(bytes2++))))
      return diff;
  }
  return 0;
}

int
isspace (int c)
{
  switch (c)
  {
    case ' ' :
    case '\f' :
    case '\n' :
    case '\r' :
    case '\t' :
    case '\v' :
      return 1;
    default:
      return 0;
  }
}

int
islower (int c)
{
  return ((c >= 'a') && (c <= 'z'));
}

int
isupper (int c)
{
  return ((c >= 'A') && (c <= 'Z'));
}

int
toupper (int c)
{
  if (islower (c))
    c -= ('a' - 'A');
  return c;
}

char *
strcpy (char *dest, const char *src)
{
  char *p = dest;
  while ((*p++ = *src++) != '\0')
    ;
  return dest;
}

int
strcmp (const char *str1, const char *str2)
{
  int c1;
  int c2;
  do
  {
    c1 = *(str1++);
    c2 = *(str2++);
  }
  while ((c1 != '\0') && (c1 == c2));
  return (c1 - c2);
}

char *
strchr (const char *s, int c)
{
  do
  {
    if (*s == c)
      return (char *) s;
  }
  while (*s++);
  return 0;
}

char *
strrchr (const char *s, int c)
{
  char *p = NULL;
  do
  {
    if (*s == c)
      p = (char *) s;
  }
  while (*s++);
  return p;
}

size_t
strlen (const char *str)
{
  size_t len = 0;
  while (*(str++))
    len++;
  return len;
}

unsigned long
strtoul (const char *nptr, char **endptr, int base)
{
  unsigned long val = 0;
  int negate = 0;
  unsigned int digit;

  /* Skip any leading whitespace */
  while (isspace (*nptr))
    nptr++;

  /* Parse sign, if present */
  if (*nptr == '+')
  {
    nptr++;
  }
  else if (*nptr == '-')
  {
    nptr++;
    negate = 1;
  }

  /* Parse base */
  if (base == 0)
  {
    /* Default to decimal */
    base = 10;
    /* Check for octal or hexadecimal markers */
    if (*nptr == '0')
    {
      nptr++;
      base = 8;
      if ((*nptr | 0x20) == 'x')
      {
        nptr++;
        base = 16;
      }
    }
  }
  /* Parse digits */
  for (; ; nptr++)
  {
    digit = *nptr;
    if (digit >= 'a')
    {
      digit = (digit - 'a' + 10);
    }
    else if (digit >= 'A')
    {
      digit = (digit - 'A' + 10);
    }
    else if (digit <= '9')
    {
      digit = (digit - '0');
    }
    if (digit >= (unsigned int) base)
      break;
    val = ((val * base) + digit);
  }

  /* Record end marker, if applicable */
  if (endptr)
    *endptr = ((char *) nptr);

  /* Return value */
  return (negate ? -val : val);
}

char *
strdup (const char *s)
{
  size_t len;
  char *p;
  len = strlen (s) + 1;
  p = (char *) malloc (len);
  if (! p)
    return 0;
  return memcpy (p, s, len);
}

char *
strndup (const char *s, size_t n)
{
  size_t len;
  char *p;
  len = strlen (s);
  if (len > n)
    len = n;
  p = (char *) malloc (len + 1);
  if (! p)
    return 0;
  memcpy (p, s, len);
  p[len] = '\0';
  return p;
}

int
wcscasecmp (const wchar_t *str1, const wchar_t *str2)
{
  int c1;
  int c2;
  do
  {
    c1 = towupper (*(str1++));
    c2 = towupper (*(str2++));
  }
  while ((c1 != L'\0') && (c1 == c2));
  return (c1 - c2);
}

size_t
wcslen (const wchar_t *str)
{
  size_t len = 0;
  while (*(str++))
    len++;
  return len;
}

wchar_t *
wcschr (const wchar_t *str, wchar_t c)
{
  for (; *str; str++)
  {
    if (*str == c)
      return ((wchar_t *) str);
  }
  return NULL;
}

size_t
wcrtomb (char *buf, wchar_t wc, mbstate_t *ps __attribute__((unused)))
{
  *buf = wc;
  return 1;
}

size_t
mbstowcs (wchar_t *dst, const char *src, size_t size)
{
  if (!dst || !src)
    return 0;
  char *p;
  wchar_t *q;
  size_t i;
  p = (char *) src;
  q = dst;
  for (i=1; i<=size; i++)
  {
    *q++ = *p++;
    if (*p == '\0')
      break;
  }
  *q = L'\0';
  return i;
}

int
putchar (int character)
{
  EFI_SIMPLE_TEXT_OUT_PROTOCOL *conout;
  wchar_t wbuf[2];
  /* Convert LF to CR,LF */
  if (character == '\n')
    putchar ('\r');
  /* Print character to EFI/BIOS console as applicable */
  conout = gST->ConOut;
  wbuf[0] = character;
  wbuf[1] = 0;
  uefi_call_wrapper (conout->OutputString, 2, conout, wbuf);
  return 0;
}

int
getchar (void)
{
  EFI_SIMPLE_TEXT_IN_PROTOCOL *conin;
  EFI_INPUT_KEY key;
  UINTN index;
  int character;
  /* Get character */
  conin = gST->ConIn;
  uefi_call_wrapper (gBS->WaitForEvent, 3, 1, &conin->WaitForKey, &index);
  uefi_call_wrapper (conin->ReadKeyStroke, 2, conin, &key);
  character = key.UnicodeChar;
  return character;
}

#define CHAR_LEN     0  /**< "hh" length modifier */
#define SHORT_LEN    1  /**< "h" length modifier */
#define INT_LEN      2  /**< no length modifier */
#define LONG_LEN     3  /**< "l" length modifier */
#define LONGLONG_LEN 4  /**< "ll" length modifier */
#define SIZE_T_LEN   5  /**< "z" length modifier */

static uint8_t type_sizes[] =
{
  [CHAR_LEN]     = sizeof (char),
  [SHORT_LEN]    = sizeof (short),
  [INT_LEN]      = sizeof (int),
  [LONG_LEN]     = sizeof (long),
  [LONGLONG_LEN] = sizeof (long long),
  [SIZE_T_LEN]   = sizeof (size_t),
};

#define LCASE 0x20
#define ALT_FORM 0x02
#define ZPAD 0x10

static char *
format_hex (char *end, unsigned long long num, int width, int flags)
{
  char *ptr = end;
  int case_mod = (flags & LCASE);
  int pad = ((flags & ZPAD) | ' ');

  /* Generate the number */
  do
  {
    *(--ptr) = "0123456789ABCDEF"[ num & 0xf ] | case_mod;
    num >>= 4;
  }
  while (num);

  /* Pad to width */
  while ((end - ptr) < width)
    *(--ptr) = pad;

  /* Add "0x" or "0X" if alternate form specified */
  if (flags & ALT_FORM)
  {
    *(--ptr) = 'X' | case_mod;
    *(--ptr) = '0';
  }

  return ptr;
}

static char *
format_decimal (char *end, signed long num, int width, int flags)
{
  char *ptr = end;
  int negative = 0;
  int zpad = (flags & ZPAD);
  int pad = (zpad | ' ');

  /* Generate the number */
  if (num < 0)
  {
    negative = 1;
    num = -num;
  }
  do
  {
    *(--ptr) = '0' + (num % 10);
    num /= 10;
  }
  while (num);

  /* Add "-" if necessary */
  if (negative && (! zpad))
    *(--ptr) = '-';

  /* Pad to width */
  while ((end - ptr) < width)
    *(--ptr) = pad;

  /* Add "-" if necessary */
  if (negative && zpad)
    *ptr = '-';

  return ptr;
}

static inline void
cputchar (struct printf_context *ctx, unsigned int c)
{
  ctx->handler (ctx, c);
  ++ctx->len;
}

size_t
vcprintf (struct printf_context *ctx, const char *fmt, va_list args)
{
  int flags;
  int width;
  uint8_t *length;
  char *ptr;
  char tmp_buf[32]; /* 32 is enough for all numerical formats.
         * Insane width fields could overflow this buffer. */
  wchar_t *wptr;

  /* Initialise context */
  ctx->len = 0;

  for (; *fmt ; fmt++)
  {
    /* Pass through ordinary characters */
    if (*fmt != '%')
    {
      cputchar (ctx, *fmt);
      continue;
    }
    fmt++;
    /* Process flag characters */
    flags = 0;
    for (; ; fmt++)
    {
      if (*fmt == '#')
      {
        flags |= ALT_FORM;
      }
      else if (*fmt == '0')
      {
        flags |= ZPAD;
      }
      else
      {
        /* End of flag characters */
        break;
      }
    }
    /* Process field width */
    width = 0;
    for (; ; fmt++)
    {
      if (((unsigned) (*fmt - '0')) < 10)
      {
        width = (width * 10) + (*fmt - '0');
      }
      else
      {
        break;
      }
    }
    /* We don't do floating point */
    /* Process length modifier */
    length = &type_sizes[INT_LEN];
    for (; ; fmt++)
    {
      if (*fmt == 'h')
      {
        length--;
      }
      else if (*fmt == 'l')
      {
        length++;
      }
      else if (*fmt == 'z')
      {
        length = &type_sizes[SIZE_T_LEN];
      }
      else
      {
        break;
      }
    }
    /* Process conversion specifier */
    ptr = tmp_buf + sizeof (tmp_buf) - 1;
    *ptr = '\0';
    wptr = NULL;
    if (*fmt == 'c')
    {
      if (length < &type_sizes[LONG_LEN])
      {
        cputchar (ctx, va_arg (args, unsigned int));
      }
      else
      {
        wchar_t wc;
        size_t len;

        wc = va_arg (args, wint_t);
        len = wcrtomb (tmp_buf, wc, NULL);
        tmp_buf[len] = '\0';
        ptr = tmp_buf;
      }
    }
    else if (*fmt == 's')
    {
      if (length < &type_sizes[LONG_LEN])
      {
        ptr = va_arg (args, char *);
      }
      else
      {
        wptr = va_arg (args, wchar_t *);
      }
      if ((ptr == NULL) && (wptr == NULL))
        ptr = "<NULL>";
    }
    else if (*fmt == 'p')
    {
      intptr_t ptrval;

      ptrval = (intptr_t) va_arg (args, void *);
      ptr = format_hex (ptr, ptrval, width, 
             (ALT_FORM | LCASE));
    }
    else if ((*fmt & ~0x20) == 'X')
    {
      unsigned long long hex;

      flags |= (*fmt & 0x20); /* LCASE */
      if (*length >= sizeof (unsigned long long))
      {
        hex = va_arg (args, unsigned long long);
      }
      else if (*length >= sizeof (unsigned long))
      {
        hex = va_arg (args, unsigned long);
      }
      else
      {
        hex = va_arg (args, unsigned int);
      }
      ptr = format_hex (ptr, hex, width, flags);
    }
    else if ((*fmt == 'd') || (*fmt == 'i'))
    {
      signed long decimal;

      if (*length >= sizeof (signed long))
      {
        decimal = va_arg (args, signed long);
      }
      else
      {
        decimal = va_arg (args, signed int);
      }
      ptr = format_decimal (ptr, decimal, width, flags);
    }
    else
    {
      *(--ptr) = *fmt;
    }
    /* Write out conversion result */
    if (wptr == NULL)
    {
      for (; *ptr ; ptr++)
      {
        cputchar (ctx, *ptr);
      }
    }
    else
    {
      for (; *wptr ; wptr++)
      {
        size_t len = wcrtomb (tmp_buf, *wptr, NULL);
        for (ptr = tmp_buf ; len-- ; ptr++)
        {
          cputchar (ctx, *ptr);
        }
      }
    }
  }
  return ctx->len;
}

struct sputc_context
{
  struct printf_context ctx;
  char *buf;
  size_t max_len;  
};

static void
printf_sputc (struct printf_context *ctx, unsigned int c)
{
  struct sputc_context * sctx =
    STD_CR (ctx, struct sputc_context, ctx);

  if (ctx->len < sctx->max_len)
    sctx->buf[ctx->len] = c;
}

int
vsnprintf (char *buf, size_t size, const char *fmt, va_list args)
{
  struct sputc_context sctx;
  size_t len;
  size_t end;

  /* Hand off to vcprintf */
  sctx.ctx.handler = printf_sputc;
  sctx.buf = buf;
  sctx.max_len = size;
  len = vcprintf (&sctx.ctx, fmt, args);

  /* Add trailing NUL */
  if (size)
  {
    end = size - 1;
    if (len < end)
      end = len;
    buf[end] = '\0';
  }
  return len;
}

int
snprintf (char *buf, size_t size, const char *fmt, ...)
{
  va_list args;
  int i;

  va_start (args, fmt);
  i = vsnprintf (buf, size, fmt, args);
  va_end (args);
  return i;
}

int
vssnprintf (char *buf, ssize_t ssize, const char *fmt, va_list args) {

  /* Treat negative buffer size as zero buffer size */
  if (ssize < 0)
    ssize = 0;
  /* Hand off to vsnprintf */
  return vsnprintf (buf, ssize, fmt, args);
}

int
ssnprintf (char *buf, ssize_t ssize, const char *fmt, ...) {
  va_list args;
  int len;
  /* Hand off to vssnprintf */
  va_start (args, fmt);
  len = vssnprintf (buf, ssize, fmt, args);
  va_end (args);
  return len;
}

static void
printf_putchar (struct printf_context *ctx __unused, unsigned int c)
{
  putchar (c);
}

int
vprintf (const char *fmt, va_list args)
{
  struct printf_context ctx;

  /* Hand off to vcprintf */
  ctx.handler = printf_putchar;  
  return vcprintf (&ctx, fmt, args);  
}

int
printf (const char *fmt, ...)
{
  va_list args;
  int i;

  va_start (args, fmt);
  i = vprintf (fmt, args);
  va_end (args);
  return i;
}
