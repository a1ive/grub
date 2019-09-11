/**
 * vsscanf - a simple vsscanf() implementation, with very few dependencies.
 * Note: it does not support floating point numbers (%f), nor gnu "m" pointers.
 *
 * ---------------------------------------------------------------------------
 * MIT License
 *
 * Copyright (c) 2018 MrBadNews <viorel dot irimia at gmail dot come>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN 
 * THE SOFTWARE.
 */

#include <stdint.h>     /* size_t */
#include <string.h>     /* memset, strchr */
#include <assert.h>
#include <ctype.h>
#include "sscanf.h"

#ifndef isdigit
#define isdigit(c) (c >= '0' && c <= '9')
#endif

#ifndef isspace
#define isspace(c) (c == ' ' || c == '\f' || c == '\n' || \
                    c == '\r' || c == '\t' || c == '\v')
#endif

/**
 * Build a table of 256, with values 1 or 0, if character is allowed or not.
 * So, if A is allowed, set[65] = 1.
 * If we will have a character, to see if it is matching, just check the table,
 * for it's ascii code.
 * It can be simplified with a bitmap, but.
 */
static int build_set(const char **fmt, char *set, int len)
{
    unsigned char c, prev, next;
    char neg = 0;
    int i; 

    if (*(*fmt)++ != '[')
        return -1;
    if (**fmt == '^') {
        (*fmt)++;
        neg = 1;
    }
    if (neg == 1 && **fmt == ']')
        (*fmt)++;
    
    memset(set, neg, len); // One can rewrite this if memset is not available.

    for (; **fmt; (*fmt)++) {
        c = **fmt;
        // - is allowed at the start and end of the set
        if (c == '-') {
            prev = *(*fmt - 1);
            next = *(*fmt + 1); 
            if (prev == '[' || next == ']') {
                set[c] = !neg;
            } else {
                // use memset!
                for (i = prev; i <= next; i++)
                    set[i] = !neg;
            }
        } else if (c == ']') {
            return 0;
        } else {
            set[c] = !neg;
        }
    }
    return -1;
}

/**
 * Check if the first letter c of a string, can be the start of a valid
 *  integer in base n, having sign or not. 
 */
static int valid_sint(char c, int base, int sign)
{
    if (base == 2 && c >= '0' && c < '2')
        return 0;
    else if (base == 8 && c >= '0' && c < '8')
        return 0;
    else if (base == 10 && 
            ((c >= '0' && c <= '9') || (sign && (c == '-' || c == '+'))))
        return 0;
    else if (base == 16 && ((c >= '0' && c <= '9') || 
            (c >='a' && c <= 'f') || (c >='A' && c <= 'F')))
        return 0;

    return -1;
}

/**
 * Having a string, consumes width chars from it, and return an integer
 * in base base, having sign or not.
 * Will work for base 2, 8, 10, 16
 * For base 16 will skip 0x infront of number.
 * For base 10, if signed, will consider - infront of number.
 * For base 8, should skip 0
 * I should reimplement this using sets. 
 */
static long long get_int(const char **str, int width, int base, int sign)
{
    long long n = 0;
    int w = width > 0;
    int neg = 0;
    int xskip = 0;
    char c;

    if (base != 2 && base != 8 && base != 10 && base != 16)
        return 0;

    for (n = 0; **str; (*str)++) {
        c = **str;
        if (sign && neg == 0 && base == 10 && c == '-') {
            neg = 1;
            if (w) width--;
            continue;
        }
        if (base == 16 && !xskip && (c == 'x' || c == 'X')) {
            xskip = 1;
            n = 0;
            continue;
        }
        if (w && width-- == 0)
            break;
        c = c >= 'a' ? c - ('a' - 'A') : c; // to upper
        if (!isdigit(c)) {
            if (base != 16)
                break;
            else if (c < 'A' || c > 'F')
                break;
        }
        if (base == 2 && c > '1')
            break;
        else if (base == 8 && c > '7')
            break;
        if (base == 16 && c >= 'A')
            c = c - 'A' + 10 + '0';
        n = n * base + c - '0';
    }
    if (neg && n > 0)
        n = -n;

    return n;
}

/**
 * Gets a string from str and puts it into ptr, if skip is not set
 * if ptr is NULL, it will consume the string, but not save it
 * if width is set, it will stop after max width characters.
 * if set[256] is set, it will only accept characters from the set,
 * eg: set['a'] = 1 - it will accept 'a'
 * otherwise it will stop on first space, or end of string.
 * Returns the number of characters matched
 */
static int get_str(const char **str, char *ptr, char *set, int width)
{
    int n, w, skip;
    unsigned char c;
    w = (width > 0);
    skip = (ptr == NULL);

    for (n = 0; **str; (*str)++, n++) {
        c = **str;
        if ((w && width-- == 0) || (!set && isspace(c)))
            break;
        if (set && (set[c] == 0))
            break;
        if (!skip)
            *ptr++ = c;
    }
    if (!skip)
        *ptr = 0;

    return n;
}

/* Flags */
#define F_SKIP   0001   // don't save matched value - '*'
#define F_ALLOC  0002   // Allocate memory for the pointer - 'm'
#define F_SIGNED 0004   // is this a signed number (%d,%i)?

/* Format states */
#define S_DEFAULT   0
#define S_FLAGS     1
#define S_WIDTH     2
#define S_PRECIS    3
#define S_LENGTH    4
#define S_CONV      5

/* Lenght flags */
#define L_CHAR      1
#define L_SHORT     2
#define L_LONG      3
#define L_LLONG     4
#define L_DOUBLE    5

/**
 * Shrinked down, vsscanf implementation.
 *  This will not handle floating numbers (yet), nor allocated (gnu) pointers.
 */
int vsscanf(const char *str, const char *fmt, va_list ap)
{
    size_t n = 0; // number of matched input items
    char state = S_DEFAULT;
    void *ptr;
    long long num;
    int base, sign, flags = 0, width, lflags;
    char set[256];

    if (!fmt)
        return 0;
 
    for (; *fmt && *str; fmt++) {
        if (state == S_DEFAULT) {
            if (*fmt == '%') {
                flags = 0;
                state = S_FLAGS;
            } else if (isspace(*fmt)) {
                while (isspace(*str)) 
                    str++;
            } else {
                if (*fmt != *str++)
                    break;
            }
            continue;
        }
        if (state == S_FLAGS) {
            switch (*fmt) {
                case '*': flags = F_SKIP; break;
                case 'm': if ((flags & F_SKIP) == 0) flags = F_ALLOC; break;
                default: width = 0; state = S_WIDTH;
            }
        }
        if (state == S_WIDTH) {
            if (isdigit(*fmt) && *fmt > '0')
                width = get_int(&fmt, 0, 10, 0);;
            lflags = 0;
            state = S_LENGTH;
        }
        if (state == S_LENGTH) {
            switch (*fmt) {
                case 'h': lflags = lflags == L_CHAR ? L_SHORT : L_CHAR; break;
                case 'l': lflags = lflags == L_LONG ? L_LLONG : L_LONG; break;
                case 'L': lflags = L_DOUBLE; break;
                default: state = S_CONV;
            }
        }
        if (state == S_CONV) {
            if (strchr("douixXb", *fmt)) {
                state = S_DEFAULT;
                base = 10;
                sign = 0;
                if (*fmt == 'd' || *fmt == 'i') 
                    sign = 1;
                if (*fmt == 'b')
                    base = 2;
                else if (*fmt == 'o')
                    base = 8;
                else if (*fmt == 'x' || *fmt == 'X')
                    base = 16;

                /* Numbers should skip starting spaces "  123l", 
                 *  strings, chars not
                 */
                while (isspace(*str)) 
                    str++;
                
                if (valid_sint(*str, base, sign) < 0)
                    break;

                num = get_int(&str, width, base, sign);
                if (flags & F_SKIP) {
                    continue;
                }
                ptr = va_arg(ap, void *);
                switch (lflags) {
                    case L_DOUBLE:
                    case L_LLONG: *(long long *)ptr = num; break;
                    case L_LONG: *(long *) ptr = num; break; 
                    case L_SHORT: *(short *) ptr = num; break;
                    case L_CHAR: *(char *) ptr = num; break;
                    default: *(int *) ptr = num;
                }
                n++;
            } else if ('c' == *fmt) {
                state = S_DEFAULT;
                if (flags & F_SKIP) {
                    str++;
                    continue;
                }
                ptr = va_arg(ap, void *);
                *(char *)ptr = *(str)++;
                n++;
            } else if ('s' == *fmt) {
                state = S_DEFAULT;
                if (flags & F_SKIP) {
                    get_str(&str, NULL, NULL, width);
                    continue;
                }
                ptr = va_arg(ap, void *);
                get_str(&str, (char *)ptr, NULL, width);
                n++;
            } else if ('[' == *fmt) {
                state = S_DEFAULT;
                if (build_set(&fmt, set, sizeof(set)) < 0)
                    break;
                if (flags & F_SKIP) {
                    get_str(&str, NULL, set, width);
                    continue;
                }
                ptr = va_arg(ap, void *);
                get_str(&str, ptr, set, width);
            } else if ('%' == *fmt) {
                state = S_DEFAULT;
                if (*str != '%')
                    break;
                str++;
            } else {
                break;
            }
        }
    }

    return n;
}

int sscanf(const char *str, const char *format, ...)
{
  va_list ap;
  int ret;

  va_start (ap, format);
  ret = vsscanf (str, format, ap);
  va_end (ap);

  return ret;
}
