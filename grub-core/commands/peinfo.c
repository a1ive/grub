/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

GRUB_MOD_LICENSE ("GPLv3+");

#pragma GCC diagnostic ignored "-Wcast-align"

enum {
  FMT_NONE = 0,
  FMT_STR,
  FMT_DEC,
  FMT_HEX2,
  FMT_HEX4,
  FMT_HEX8
};

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uint64_t ULONGLONG;
typedef int64_t LONGLONG;

#define MAGIC_MZ 0x5a4d

typedef struct _IMAGE_DOS_HEADER {
    WORD    e_magic;
    WORD    e_cblp;
    WORD    e_cp;
    WORD    e_crlc;
    WORD    e_cparhdr;
    WORD    e_minalloc;
    WORD    e_maxalloc;
    WORD    e_ss;
    WORD    e_sp;
    WORD    e_csum;
    WORD    e_ip;
    WORD    e_cs;
    WORD    e_lfarlc;
    WORD    e_ovno;
    WORD    e_res[4];
    WORD    e_oemid;
    WORD    e_oeminfo;
    WORD    e_res2[10];
    LONG    e_lfanew;
} IMAGE_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER {
    WORD    Machine;
    WORD    NumberOfSections;
    DWORD   TimeDateStamp;
    DWORD   PointerToSymbolTable;
    DWORD   NumberOfSymbols;
    WORD    SizeOfOptionalHeader;
    WORD    Characteristics;
} IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

#define IMAGE_FILE_RELOCS_STRIPPED          0x0001
#define IMAGE_FILE_EXECUTABLE_IMAGE         0x0002
#define IMAGE_FILE_32BIT_MACHINE            0x0100
#define IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP  0x0400
#define IMAGE_FILE_SYSTEM                   0x1000
#define IMAGE_FILE_DLL                      0x2000

typedef struct _PE32_IMAGE_NT_HEADERS {
    DWORD                   Signature;
    IMAGE_FILE_HEADER       FileHeader;
} IMAGE_NT_HEADERS, *PIMAGE_NT_HEADERS;

typedef struct _IMAGE_DATA_DIRECTORY {
    DWORD   VirtualAddress;
    DWORD   Size;
} IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES    16
#define MAGIC_PE                            0x00004550

#define MAGIC_PE32                          0x010b
#define MAGIC_PE32P                         0x020b
#define MAGIC_ROM                           0x0107

typedef struct _OPT_HDR_STD {
    WORD    Magic;
    BYTE    MajorLinkerVersion;
    BYTE    MinorLinkerVersion;
    DWORD   SizeOfCode;
    DWORD   SizeOfInitializedData;
    DWORD   SizeOfUninitializedData;
    DWORD   AddressOfEntryPoint;
    DWORD   BaseOfCode;
    DWORD   BaseOfData; /* XXX: This field is absent in PE32+ */
} OPT_HDR_STD, *POPT_HDR_STD;

typedef struct _IMAGE_PE32_OPTIONAL_HEADER {
    WORD    Magic;
    BYTE    MajorLinkerVersion;
    BYTE    MinorLinkerVersion;
    DWORD   SizeOfCode;
    DWORD   SizeOfInitializedData;
    DWORD   SizeOfUninitializedData;
    DWORD   AddressOfEntryPoint;
    DWORD   BaseOfCode;
    DWORD   BaseOfData;

    DWORD   ImageBase;
    DWORD   SectionAlignment;
    DWORD   FileAlignment;
    WORD    MajorOperatingSystemVersion;
    WORD    MinorOperatingSystemVersion;
    WORD    MajorImageVersion;
    WORD    MinorImageVersion;
    WORD    MajorSubsystemVersion;
    WORD    MinorSubsystemVersion;
    DWORD   Win32VersionValue;
    DWORD   SizeOfImage;
    DWORD   SizeOfHeaders;
    DWORD   CheckSum;
    WORD    Subsystem;
    WORD    DllCharacteristics;
    DWORD   SizeOfStackReserve;
    DWORD   SizeOfStackCommit;
    DWORD   SizeOfHeapReserve;
    DWORD   SizeOfHeapCommit;
    DWORD   LoaderFlags;
    DWORD   NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[0];
} IMAGE_PE32_OPTIONAL_HEADER, *PIMAGE_PE32_OPTIONAL_HEADER;

typedef struct _IMAGE_PE32P_OPTIONAL_HEADER {
    WORD    Magic;
    BYTE    MajorLinkerVersion;
    BYTE    MinorLinkerVersion;
    DWORD   SizeOfCode;
    DWORD   SizeOfInitializedData;
    DWORD   SizeOfUninitializedData;
    DWORD   AddressOfEntryPoint;
    DWORD   BaseOfCode;

    ULONGLONG ImageBase;
    DWORD   SectionAlignment;
    DWORD   FileAlignment;
    WORD    MajorOperatingSystemVersion;
    WORD    MinorOperatingSystemVersion;
    WORD    MajorImageVersion;
    WORD    MinorImageVersion;
    WORD    MajorSubsystemVersion;
    WORD    MinorSubsystemVersion;
    DWORD   Win32VersionValue;
    DWORD   SizeOfImage;
    DWORD   SizeOfHeaders;
    DWORD   CheckSum;
    WORD    Subsystem;
    WORD    DllCharacteristics;
    ULONGLONG Si0zeOfStackReserve;
    ULONGLONG SizeOfStackCommit;
    ULONGLONG SizeOfHeapReserve;
    ULONGLONG SizeOfHeapCommit;
    DWORD   LoaderFlags;
    DWORD   NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[0];
} IMAGE_PE32P_OPTIONAL_HEADER, *PIMAGE_PE32P_OPTIONAL_HEADER;

#define IMAGE_SIZEOF_SHORT_NAME 8

typedef struct _IMAGE_SECTION_HEADER {
    BYTE        Name[IMAGE_SIZEOF_SHORT_NAME];
    union {
        DWORD   PhysicalAddress;
        DWORD   VirtualSize;
    } Misc;
    DWORD       VirtualAddress;
    DWORD       SizeOfRawData;
    DWORD       PointerToRawData;
    DWORD       PointerToRelocations;
    DWORD       PointerToLinenumbers;
    WORD        NumberOfRelocations;
    WORD        NumberOfLinenumbers;
    DWORD       Characteristics;
} IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

#define IMAGE_SCN_CNT_CODE                  0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA      0x00000040
#define IMAGE_SCN_CNT_UNINITIALIZED_DATA    0x00000080
#define IMAGE_SCN_MEM_EXECUTE               0x20000000
#define IMAGE_SCN_MEM_READ                  0x40000000
#define IMAGE_SCN_MEM_WRITE                 0x80000000

static char *format_str(char *buf, size_t size, void *value, int format)
{
  if (!buf)
    return NULL;

  switch (format) {
    case FMT_STR:
      snprintf(buf, size, "%s", (char *)value);
      break;
    case FMT_DEC:
      snprintf(buf, size, "%d", *(int *)value);
      break;
    case FMT_HEX2:
      snprintf(buf, size, "0x%04x", *(unsigned short *)value);
      break;
    case FMT_HEX4:
      snprintf(buf, size, "0x%08x", *(unsigned int *)value);
      break;
    case FMT_HEX8:
      snprintf(buf, size, "0x%016llx", *(unsigned long long *)value);
      break;
    default:
      printf("unexpected format type\n");
      return NULL;
  }

  return buf;
}

static void print_kv(const char *key, void *value, int format)
{
  const size_t BUFSIZE = 1024;
  char *buf = malloc(sizeof(char) * BUFSIZE);
  if (!buf)
    return;
  buf = format_str(buf, BUFSIZE, value, format);
  if (!buf)
    goto free;

  printf("%s: %s\n", key, buf);

free:
  free(buf);
}

static void print_fhdr(IMAGE_FILE_HEADER *fhdr)
{
  if (!fhdr)
    return;

  print_kv("Machine", &fhdr->Machine, FMT_HEX2);
  print_kv("NumberOfSections", &fhdr->NumberOfSections, FMT_DEC);
  print_kv("TimeDateStamp", &fhdr->TimeDateStamp, FMT_DEC);
  print_kv("PointerToSymbolTable", &fhdr->PointerToSymbolTable, FMT_HEX4);
  print_kv("NumberOfSymbols", &fhdr->NumberOfSymbols, FMT_DEC);
  print_kv("SizeOfOptionalHeader", &fhdr->SizeOfOptionalHeader, FMT_HEX2);
  print_kv("Characteristics", &fhdr->Characteristics, FMT_HEX2);
}

static void print_opthdr_std(OPT_HDR_STD *opthdr_std)
{
  if (!opthdr_std)
    return;

  print_kv("Magic", &opthdr_std->Magic, FMT_HEX2);
  print_kv("MajorLinkerVersion", &opthdr_std->MajorLinkerVersion, FMT_DEC);
  print_kv("MinorLinkerVersion", &opthdr_std->MinorLinkerVersion, FMT_DEC);
  print_kv("SizeOfCode", &opthdr_std->SizeOfCode, FMT_HEX4);
  print_kv("SizeOfInitializedData", &opthdr_std->SizeOfInitializedData, FMT_HEX4);
  print_kv("SizeOfUnnitializedData", &opthdr_std->SizeOfUninitializedData, FMT_HEX4);
  print_kv("AddressOfEntryPoint", &opthdr_std->AddressOfEntryPoint, FMT_HEX4);
  print_kv("BaseOfCode", &opthdr_std->BaseOfCode, FMT_HEX4);
  if (opthdr_std->Magic == MAGIC_PE32)
    print_kv("BaseOfData", &opthdr_std->BaseOfData, FMT_HEX4);
}

static void print_datadir(IMAGE_DATA_DIRECTORY *dir)
{
  if (!dir)
    return;

  print_kv("VirtualAddress", &dir->VirtualAddress, FMT_HEX4);
  print_kv("Size", &dir->Size, FMT_HEX4);
}

static void print_datadirs(IMAGE_DATA_DIRECTORY dir[], int ndirs)
{
  if (!dir)
    return;

  for (int i = 0; i < ndirs; i++)
    print_datadir(&dir[i]);
}

static void print_pe32_opthdr(IMAGE_PE32_OPTIONAL_HEADER *opthdr)
{
  if (!opthdr)
    return;

  print_opthdr_std((OPT_HDR_STD *)opthdr);

  print_kv("ImageBase", &opthdr->ImageBase, FMT_HEX4);
  print_kv("SectionAlignment", &opthdr->SectionAlignment, FMT_HEX4);
  print_kv("FileAlignment", &opthdr->FileAlignment, FMT_HEX4);
  /* TODO: more fields to be printed */
  print_kv("SizeOfImage", &opthdr->SizeOfImage, FMT_HEX4);
  print_kv("SizeOfHeaders", &opthdr->SizeOfHeaders, FMT_HEX4);

  print_datadirs(opthdr->DataDirectory, opthdr->NumberOfRvaAndSizes);
}

static void print_pe32p_opthdr(IMAGE_PE32P_OPTIONAL_HEADER *opthdr)
{
  if (!opthdr)
    return;

  print_opthdr_std((OPT_HDR_STD *)opthdr);

  print_kv("ImageBase", &opthdr->ImageBase, FMT_HEX8);
  print_kv("SectionAlignment", &opthdr->SectionAlignment, FMT_HEX4);
  print_kv("FileAlignment", &opthdr->FileAlignment, FMT_HEX4);
  /* TODO: more fields to be printed */
  print_kv("SizeOfImage", &opthdr->SizeOfImage, FMT_HEX4);
  print_kv("SizeOfHeaders", &opthdr->SizeOfHeaders, FMT_HEX4);

  print_datadirs(opthdr->DataDirectory, opthdr->NumberOfRvaAndSizes);
}

static void print_sechdr(IMAGE_SECTION_HEADER *sechdr)
{
  if (!sechdr)
    return;

  print_kv("Name", &sechdr->Name, FMT_STR);
  print_kv("VirtualSize", &sechdr->Misc.VirtualSize, FMT_HEX4);
  print_kv("VirtualAddress", &sechdr->VirtualAddress, FMT_HEX4);
  print_kv("SizeOfRawData", &sechdr->SizeOfRawData, FMT_HEX4);
  print_kv("PointerToRawData", &sechdr->PointerToRawData, FMT_HEX4);
  print_kv("PointerToRelocations", &sechdr->PointerToRelocations, FMT_HEX4);
  print_kv("PointerToLinenumbers", &sechdr->PointerToLinenumbers, FMT_HEX4);
  print_kv("NumberOfRelocations", &sechdr->NumberOfRelocations, FMT_HEX2);
  print_kv("NumberOfLinenumbers", &sechdr->NumberOfLinenumbers, FMT_HEX2);
  print_kv("Characteristics", &sechdr->Characteristics, FMT_HEX4);
}

static void *parse_pe32_opthdr(char *buf)
{
  if (!buf)
    return NULL;

  IMAGE_PE32_OPTIONAL_HEADER *opthdr = (IMAGE_PE32_OPTIONAL_HEADER *)buf;

  if (opthdr->Magic != MAGIC_PE32)
    return NULL;

  print_pe32_opthdr(opthdr);

  return buf + sizeof(IMAGE_PE32_OPTIONAL_HEADER) + sizeof(IMAGE_DATA_DIRECTORY) * opthdr->NumberOfRvaAndSizes;
}

static void *parse_pe32p_opthdr(char *buf)
{
  if (!buf)
    return NULL;

  IMAGE_PE32P_OPTIONAL_HEADER *opthdr = (IMAGE_PE32P_OPTIONAL_HEADER *)buf;

  if (opthdr->Magic != MAGIC_PE32P)
    return NULL;

  print_pe32p_opthdr(opthdr);

  return buf + sizeof(IMAGE_PE32P_OPTIONAL_HEADER) + sizeof(IMAGE_DATA_DIRECTORY) * opthdr->NumberOfRvaAndSizes;
}

static void parse_sechdrs(char *buf, int nsec)
{
  if (!buf)
    return;

  for (int i = 0; i < nsec; i++) {
    IMAGE_SECTION_HEADER *sechdr = (IMAGE_SECTION_HEADER *)(buf + sizeof(IMAGE_SECTION_HEADER) * i);
    print_sechdr(sechdr);
  }
}

static int parse_pehdr(char *buf)
{
  IMAGE_DOS_HEADER *doshdr;
  IMAGE_NT_HEADERS *nthdrs;
  IMAGE_FILE_HEADER *fhdr;
  void *opthdr;
  void *sechdr;

  if (!buf) {
    printf("Invalid input\n");
    return 1;
  }

  doshdr = (IMAGE_DOS_HEADER *)buf;
  if (doshdr->e_magic != MAGIC_MZ) {
    printf("Unsupported file format\n");
    return 1;
  }

  nthdrs = (IMAGE_NT_HEADERS *)(buf + doshdr->e_lfanew);
  if (nthdrs->Signature != MAGIC_PE) {
    printf("Unsupported file format\n");
    return 1;
  }

  fhdr = &nthdrs->FileHeader;
  print_fhdr(fhdr);

  opthdr = (char *)nthdrs + sizeof(IMAGE_NT_HEADERS);

  sechdr = parse_pe32_opthdr(opthdr);
  if (sechdr)
    parse_sechdrs(sechdr, fhdr->NumberOfSections);
  sechdr = parse_pe32p_opthdr(opthdr);
  if (sechdr)
    parse_sechdrs(sechdr, fhdr->NumberOfSections);

  return 0;
}

static grub_err_t
grub_cmd_peinfo (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                 int argc, char **args)
{
  grub_file_t file = 0;
  grub_ssize_t size = 0;
  void *buf = NULL;
  int rc = 0;

  if (argc != 1)
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_("unexpected arguments"));
    return grub_errno;
  }

  file = grub_file_open (args[0], GRUB_FILE_TYPE_EFI_CHAINLOADED_IMAGE);
  if (! file)
    goto fail;
  size = grub_file_size (file);
  if (!size)
  {
    grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"), args[0]);
    goto fail;
  }

  buf = malloc (size);
  if (!buf)
  {
    grub_error (GRUB_ERR_OUT_OF_MEMORY, N_("out of memory"));
    goto fail;
  }
  if (grub_file_read (file, buf, size) != size)
  {
    if (grub_errno == GRUB_ERR_NONE)
      grub_error (GRUB_ERR_BAD_OS, N_("premature end of file %s"), args[0]);
    goto fail;
  }
  grub_file_close (file);
  rc = parse_pehdr(buf);
  free (buf);
  return rc;

fail:
  if (buf)
    free (buf);
  if (file)
    grub_file_close (file);
  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(peinfo)
{
  cmd = grub_register_extcmd ("peinfo", grub_cmd_peinfo, 0,
				   N_("FILE"),
				   N_("Portable Executable Header Viewer"),
				   0);
}

GRUB_MOD_FINI(peinfo)
{
  grub_unregister_extcmd (cmd);
}
