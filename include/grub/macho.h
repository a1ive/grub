/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#ifndef GRUB_MACHO_H
#define GRUB_MACHO_H 1
#include <grub/types.h>

/* Multi-architecture header. Always in big-endian. */
struct grub_macho_fat_header
{
  grub_uint32_t magic;
  grub_uint32_t nfat_arch;
} __attribute__ ((packed));
#define GRUB_MACHO_FAT_MAGIC 0xcafebabe

typedef grub_uint32_t grub_macho_cpu_type_t;
typedef grub_uint32_t grub_macho_cpu_subtype_t;

/* Architecture descriptor. Always in big-endian. */
struct grub_macho_fat_arch
{
  grub_macho_cpu_type_t cputype;
  grub_macho_cpu_subtype_t cpusubtype;
  grub_uint32_t offset;
  grub_uint32_t size;
  grub_uint32_t align;
} __attribute__ ((packed));

/* File header for 32-bit. Always in native-endian. */
struct grub_macho_header32
{
#define GRUB_MACHO_MAGIC32 0xfeedface
  grub_uint32_t magic;
  grub_macho_cpu_type_t cputype;
  grub_macho_cpu_subtype_t cpusubtype;
  grub_uint32_t filetype;
  grub_uint32_t ncmds;
  grub_uint32_t sizeofcmds;
  grub_uint32_t flags;
} __attribute__ ((packed));

/* File header for 64-bit. Always in native-endian. */
struct grub_macho_header64
{
#define GRUB_MACHO_MAGIC64 0xfeedfacf
  grub_uint32_t magic;
  grub_macho_cpu_type_t cputype;
  grub_macho_cpu_subtype_t cpusubtype;
  grub_uint32_t filetype;
  grub_uint32_t ncmds;
  grub_uint32_t sizeofcmds;
  grub_uint32_t flags;
  grub_uint32_t reserved;
} __attribute__ ((packed));

/* Convenience union. What do we need to load to identify the file type. */
union grub_macho_filestart
{
  struct grub_macho_fat_header fat;
  struct grub_macho_header32 thin32;
  struct grub_macho_header64 thin64;
} __attribute__ ((packed));

/* Common header of Mach-O commands. */
struct grub_macho_cmd
{
  grub_uint32_t cmd;
  grub_uint32_t cmdsize;
} __attribute__ ((packed));

typedef grub_uint32_t grub_macho_vmprot_t;

/* 32-bit segment command. */
struct grub_macho_segment32
{
#define GRUB_MACHO_CMD_SEGMENT32  1
  grub_uint32_t cmd;
  grub_uint32_t cmdsize;
  grub_uint8_t segname[16];
  grub_uint32_t vmaddr;
  grub_uint32_t vmsize;
  grub_uint32_t fileoff;
  grub_uint32_t filesize;
  grub_macho_vmprot_t maxprot;
  grub_macho_vmprot_t initprot;
  grub_uint32_t nsects;
  grub_uint32_t flags;
} __attribute__ ((packed));

/* 64-bit segment command. */
struct grub_macho_segment64
{
#define GRUB_MACHO_CMD_SEGMENT64  0x19
  grub_uint32_t cmd;
  grub_uint32_t cmdsize;
  grub_uint8_t segname[16];
  grub_uint64_t vmaddr;
  grub_uint64_t vmsize;
  grub_uint64_t fileoff;
  grub_uint64_t filesize;
  grub_macho_vmprot_t maxprot;
  grub_macho_vmprot_t initprot;
  grub_uint32_t nsects;
  grub_uint32_t flags;
} __attribute__ ((packed));

#define GRUB_MACHO_CMD_THREAD	5

struct grub_macho_section32
{
  char sectname[16];
  char segname[16];
  grub_uint32_t addr;
  grub_uint32_t size;
  grub_uint32_t offset;
  grub_uint32_t align;
  grub_uint32_t reloff;
  grub_uint32_t nreloc;
  grub_uint32_t flags;
  grub_uint32_t reserved1;
  grub_uint32_t reserved2;
} __attribute__ ((packed));

struct grub_macho_section64
{
  char sectname[16];
  char segname[16];
  grub_uint64_t addr;
  grub_uint64_t size;
  grub_uint32_t offset;
  grub_uint32_t align;
  grub_uint32_t reloff;
  grub_uint32_t nreloc;
  grub_uint32_t flags;
  grub_uint32_t reserved1;
  grub_uint32_t reserved2;
  grub_uint32_t reserved3;
} __attribute__ ((packed));

struct grub_macho_symtab
{
#define GRUB_MACHO_CMD_SYMTAB	2
  grub_uint32_t cmd;
  grub_uint32_t cmdsize;
  grub_uint32_t symoff;
  grub_uint32_t nsyms;
  grub_uint32_t stroff;
  grub_uint32_t strsize;
} __attribute__ ((packed));

struct grub_macho_dysymtab
{
#define GRUB_MACHO_CMD_DYSYMTAB	11
  grub_uint32_t cmd;
  grub_uint32_t cmdsize;
  grub_uint32_t ilocalsym;
  grub_uint32_t nlocalsym;
  grub_uint32_t iextdefsym;
  grub_uint32_t nextdefsym;
  grub_uint32_t iundefsym;
  grub_uint32_t nundefsym;
  grub_uint32_t tocoff;
  grub_uint32_t ntoc;
  grub_uint32_t modtaboff;
  grub_uint32_t nmodtab;
  grub_uint32_t extrefsymoff;
  grub_uint32_t nextrefsyms;
  grub_uint32_t indirectsymoff;
  grub_uint32_t nindirectsyms;
  grub_uint32_t extreloff;
  grub_uint32_t nextrel;
  grub_uint32_t locreloff;
  grub_uint32_t nlocrel;
} __attribute__ ((packed));

#define GRUB_MACHO_N_TYPE	0xe
#define GRUB_MACHO_N_EXT	1
#define GRUB_MACHO_N_UNDEF	0
#define GRUB_MACHO_N_SECT	0xe

struct grub_macho_nlist32
{
  grub_int32_t n_strx;
  grub_uint8_t n_type;
  grub_uint8_t n_sect;
  grub_int16_t n_desc;
  grub_uint32_t n_value;
} __attribute__ ((packed));

struct grub_macho_nlist64
{
  grub_uint32_t n_strx;
  grub_uint8_t n_type;
  grub_uint8_t n_sect;
  grub_int16_t n_desc;
  grub_uint64_t n_value;
} __attribute__ ((packed));

#ifdef GRUB_TARGET_WORDS_BIGENDIAN

#define GRUB_MACHO_REL_SYMBOLNUM(a)	(a >> 8)
#define GRUB_MACHO_REL_PCREL(a)		((a >> 7) & 1)
#define GRUB_MACHO_REL_LENGTH(a)	((a >> 5) & 3)
#define GRUB_MACHO_REL_EXTERN(a)	((a >> 4) & 1)
#define GRUB_MACHO_REL_TYPE(a)		(a & 0xf)

#else

#define GRUB_MACHO_REL_SYMBOLNUM(a)	(a & 0xffffff)
#define GRUB_MACHO_REL_PCREL(a)		((a >> 24) & 1)
#define GRUB_MACHO_REL_LENGTH(a)	((a >> 25) & 3)
#define GRUB_MACHO_REL_EXTERN(a)	((a >> 27) & 1)
#define GRUB_MACHO_REL_TYPE(a)		((a >> 28) & 0xf)

#endif

struct grub_macho_relocation_info
{
  grub_uint32_t r_address;
  grub_uint32_t r_info;
};

#define GRUB_MACHO_SREL_SCATTERED(a)	((a >> 31) & 1)
#define GRUB_MACHO_SREL_PCREL(a)	((a >> 30) & 1)
#define GRUB_MACHO_SREL_LENGTH(a)	((a >> 28) & 3)
#define GRUB_MACHO_SREL_TYPE(a)		((a >> 24) & 0xf)
#define GRUB_MACHO_SREL_ADDRESS(a)	(a & 0xffffff)

struct grub_macho_scattered_relocation_info
{
  grub_uint32_t r_info;
  grub_uint32_t r_value;
};

#define GRUB_MACHO_RELOC_VANILLA	0
#define GRUB_MACHO_RELOC_PAIR		1
#define GRUB_MACHO_RELOC_SECTDIFF	2
#define GRUB_MACHO_RELOC_PB_LA_PTR	3
#define GRUB_MACHO_RELOC_LOCAL_SECTDIFF	4

#define GRUB_MACHO_X86_64_RELOC_UNSIGNED 0
#define GRUB_MACHO_X86_64_RELOC_SIGNED	1
#define GRUB_MACHO_X86_64_RELOC_BRANCH	2
#define GRUB_MACHO_X86_64_RELOC_GOT_LD	3
#define GRUB_MACHO_X86_64_RELOC_GOT	4
#define GRUB_MACHO_X86_64_RELOC_SUB	5
#define GRUB_MACHO_X86_64_RELOC_SIGNED1	6
#define GRUB_MACHO_X86_64_RELOC_SIGNED2	7
#define GRUB_MACHO_X86_64_RELOC_SIGNED4	8

#define GRUB_MACHO_PPC_RELOC_BR24	3
#define GRUB_MACHO_PPC_RELOC_HI16	4
#define GRUB_MACHO_PPC_RELOC_LO16	5
#define GRUB_MACHO_PPC_RELOC_HA16	6
#define GRUB_MACHO_PPC_RELOC_SDIFF	8
#define GRUB_MACHO_PPC_RELOC_HI16_SDIFF	10
#define GRUB_MACHO_PPC_RELOC_LO16_SDIFF	11
#define GRUB_MACHO_PPC_RELOC_HA16_SDIFF	12
#define GRUB_MACHO_PPC_RELOC_LOCAL_SDIFF 15

#endif
