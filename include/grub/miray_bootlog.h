/* miray_bootlog.h - prepare memory for logging or sync it after reboot */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
 *  Copyright (C) 2010,2011,2012 Miray Software <oss@miray.de>
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/types.h>
#include <grub/symbol.h>
#include <grub/err.h>

#ifndef MIRAY_BOOTLOG_HEADER
#define MIRAY_BOOTLOG_HEADER  1

#define USE_MIRAY_BOOTLOG 1 // More or less easy way to disable the bootlog code again

#ifdef USE_MIRAY_BOOTLOG

#define MIRAY_BOOTLOG_SIZE_SHIFT 20 // Reserve 2^20 Bytes (1 MB) for now 
#define MIRAY_BOOTLOG_SIZE (1ULL << MIRAY_BOOTLOG_SIZE_SHIFT)

#define MIRAY_BOOTLOG_MAGIC_SIZE 8

static const grub_uint8_t  mirayBootlogMemMagic[MIRAY_BOOTLOG_MAGIC_SIZE] = "BTLOGMEM"; // Memspace has been initialized
static const grub_uint8_t  mirayBootlogLogMagic[MIRAY_BOOTLOG_MAGIC_SIZE] = "BTLOGDTA"; // Bootlog with data

struct mirayBootlogHeader
{
   grub_uint8_t  magic[MIRAY_BOOTLOG_MAGIC_SIZE];
   grub_uint64_t addr;
   grub_uint64_t memsize;
   grub_uint64_t datasize;
} __attribute__ ((packed));

grub_err_t EXPORT_FUNC(miray_getBootlogHeader)(void ** data, grub_size_t * size);

static inline void * miray_bootlog_dataAddr(const struct mirayBootlogHeader * header)
{
   return &((grub_uint8_t *)(grub_addr_t)(header->addr))[sizeof(struct mirayBootlogHeader)];
}

static inline grub_size_t miray_bootlog_memSize(const struct mirayBootlogHeader * header)
{
   return header->memsize - sizeof(struct mirayBootlogHeader);
}


#endif

#endif
