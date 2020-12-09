/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2020  Free Software Foundation, Inc.
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

#ifndef GRUB_FOR_DOS_H
#define GRUB_FOR_DOS_H

#include <grub/types.h>

#ifdef GRUB_MACHINE_EFI

/* The size of the drive map.  */
#define DRIVE_MAP_SIZE            8

/* The size of the drive_map_slot struct.  */
#define DRIVE_MAP_SLOT_SIZE       24

/* The fragment of the drive map.  */
#define DRIVE_MAP_FRAGMENT        32

#define FRAGMENT_MAP_SLOT_SIZE    0x280

struct g4d_drive_map_slot
{
  /* Remember to update DRIVE_MAP_SLOT_SIZE once this is modified.
   * The struct size must be a multiple of 4.
   */
  unsigned char from_drive;
  unsigned char to_drive;            /* 0xFF indicates a memdrive */

  unsigned char max_head;

  unsigned char max_sector:6;     //unused
  unsigned char disable_lba:1;    //unused
  unsigned char read_only:1;      //unused

  unsigned short to_cylinder:13;  //unused
  unsigned short from_cdrom:1;
  unsigned short to_cdrom:1;      //unused
  unsigned short to_support_lba:1;//unused

  unsigned char to_head;          //unused

  unsigned char to_sector:6;
  unsigned char fake_write:1;     //unused
  unsigned char in_situ:1;        //unused

  unsigned long long start_sector;
  unsigned long long sector_count;
} GRUB_PACKED;

struct g4d_fragment_map_slot
{
  unsigned short slot_len;
  unsigned char from;
  unsigned char to;
  unsigned long long fragment_data[0];
};

struct g4d_fragment
{
  unsigned long long start_sector;
  unsigned long long sector_count;
};

#else

#define DRIVE_MAP_SIZE              16

/* The size of the drive_map_slot struct.  */
#define DRIVE_MAP_SLOT_SIZE         24

/* The fragment of the drive map.  */
#define DRIVE_MAP_FRAGMENT          32

#define FRAGMENT_MAP_SLOT_SIZE      0x280

struct g4d_drive_map_slot
{
  /* Remember to update DRIVE_MAP_SLOT_SIZE once this is modified.
   * The struct size must be a multiple of 4.
   */

  /* X=max_sector bit 7: read only or fake write */
  /* Y=to_sector  bit 6: safe boot or fake write */
  /* ------------------------------------------- */
  /* X Y: meaning of restrictions imposed on map */
  /* ------------------------------------------- */
  /* 1 1: read only=0, fake write=1, safe boot=0 */
  /* 1 0: read only=1, fake write=0, safe boot=0 */
  /* 0 1: read only=0, fake write=0, safe boot=1 */
  /* 0 0: read only=0, fake write=0, safe boot=0 */
  unsigned char from_drive;           //00
  unsigned char to_drive;             //01 /* 0xFF indicates a memdrive */
  unsigned char max_head;             //02
  unsigned char max_sector;           //03 /* bit 7: read only */
                                           /* bit 6: disable lba */
  unsigned short to_cylinder;         //04 /* max cylinder of the TO drive */
          /* bit 15:  TO  drive support LBA */
          /* bit 14:  TO  drive is CDROM(with big 2048-byte sector) */
          /* bit 13: FROM drive is CDROM(with big 2048-byte sector) */
  unsigned char to_head;              //06 /* max head of the TO drive */
  unsigned char to_sector;            //07 /* max sector of the TO drive */
          /* bit 7: in-situ */
          /* bit 6: fake-write or safe-boot */
  unsigned long long start_sector;    //08
  //unsigned long start_sector_hi;  /* hi dword of the 64-bit value */
  unsigned long long sector_count;    //16
  //unsigned long sector_count_hi;  /* hi dword of the 64-bit value */
};

struct g4d_fragment_map_slot
{
  unsigned short slot_len;
  unsigned char from;
  unsigned char to;
  unsigned long long fragment_data[0];
};

#endif

void g4d_add_drive (grub_file_t file, int is_cdrom);

#endif
