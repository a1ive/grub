#ifndef _SSCANF_H
#define _SSCANF_H

#include <stdio.h>

int vsscanf(const char *str, const char *fmt, va_list ap);
int sscanf(const char *str, const char *format, ...);

#endif

