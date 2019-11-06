#ifndef _WIMBOOT_H
#define _WIMBOOT_H

#include <grub/file.h>
#include <grub/term.h>

struct grub_wimboot_component
{
  grub_file_t file;
  char *file_name;
  grub_off_t size;
};

struct grub_wimboot_context
{
  int nfiles;
  struct grub_wimboot_component *components;
  grub_size_t size;
};

//WIMBOOT BINARY
#ifdef __x86_64__
#include <grub/x86_64/efi/wimboot_bin.h>
#elif defined(__i386__)
#include <grub/i386/efi/wimboot_bin.h>
#endif /* WIMBOOT_BIN */
#endif /* _WIMBOOT_H */
