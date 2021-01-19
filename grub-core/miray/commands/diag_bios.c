/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010-2012 Miray Software <oss@miray.de>
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

#include "miray_cmd_diag.h"

#include <grub/misc.h>

#include <grub/i386/pc/memory.h>
#include <grub/i386/pc/biosdisk.h>
#include <grub/i386/pc/int.h>
#include <grub/lib/hexdump.h>

static int driveno(const char *str)
{
  if (*str == '(')
    str++;
  if ((str[0] == 'f' || str[0] == 'h') && str[1] == 'd')
    {
      grub_uint8_t bios_num = (str[0] == 'h') ? 0x80 : 0x00;
      unsigned long drivenum = grub_strtoul (str + 2, 0, 0);
      if (grub_errno == GRUB_ERR_NONE && drivenum < 128)
        {
          bios_num |= drivenum;
          return bios_num;
        }
    }

  return -1;
}




grub_err_t
miray_cmd_diag_bios_print_hdextinfo (grub_command_t cmd __attribute__ ((unused)),
		       int argc, char **args )
{
  int version;
  int drive;
  const char *version_str;
  struct grub_biosdisk_drp *drp
        = (struct grub_biosdisk_drp *) GRUB_MEMORY_MACHINE_SCRATCH_ADDR;

  struct grub_bios_int_registers hdversion_regs =
  {
    .eax = 0x4100,
    .ebx = 0x55aa,
  };

  struct grub_bios_int_registers hdinfo_regs = 
  {
    .eax = 0x4800,
    .esi = (unsigned long)drp & 0xf,
    .ds = ((unsigned long)drp >> 4) & 0xffff,
  };

  if (argc < 1)
    return grub_error(GRUB_ERR_BAD_ARGUMENT, "no harddrive specified");

  drive = driveno(args[0]);
  if (drive < 0)
    return grub_error(GRUB_ERR_UNKNOWN_DEVICE, "invalid drive name");
  hdversion_regs.edx = drive & 0xff;

  grub_bios_interrupt (0x13, &hdversion_regs);
  // version = grub_biosdisk_check_int13_extensions(drive);

  if (hdversion_regs.flags & GRUB_CPU_INT_FLAGS_CARRY)
    return grub_error(GRUB_ERR_BAD_DEVICE, "Drive extensions not supported");

  version = (hdversion_regs.eax & 0xff00) >> 8;
  switch (version) 
  {
    case 0x01:
      version_str = "1.x";
      break;
    case 0x20:
      version_str = "2.0 / EDD-1.0";
      break;
    case 0x21:
      version_str = "2.1 / EDD-1.1";
      break;
    case 0x30:
      version_str = "EDD-3.0";
      break;
    default:
      version_str = 0;
      break;
  }

  if (version_str)
    grub_printf("Drive extensions version = %s\n", version_str);
  else
    grub_printf("Drive extensions version = 0x%x\n", version);

  grub_memset(drp, 0, sizeof(*drp));
  drp->size = sizeof(*drp);
  hdinfo_regs.edx = drive & 0xff;
  grub_bios_interrupt(0x13, &hdinfo_regs);
  // ret = grub_biosdisk_get_diskinfo_int13_extensions(drive, drp);

  if (hdinfo_regs.flags & GRUB_CPU_INT_FLAGS_CARRY)
    return grub_error(GRUB_ERR_BAD_DEVICE, "fetching diskinfo extensions failed");

  hexdump (0, (char *)drp, drp->size);   

  grub_printf("size = 0x%x\n", drp->size);			// u16
  grub_printf("flags = 0x%x\n", drp->flags);			// u16
  grub_printf("C/H/S = %d/%d/%d\n", drp->cylinders, drp->heads, drp->sectors); // u32/u32/u32
  grub_printf("total_sectors = %" PRIuGRUB_UINT64_T "\n", drp->total_sectors);
  grub_printf("bytes_per_sector = %d\n", drp->bytes_per_sector);

  if (version >= 0x20 && version < 0x30)
  {
    grub_printf("configuration parameters = 0x%x\n", drp->EDD_configuration_parameters);
  }
  else
  {
    hexdump (0x20, (char *)drp + 0x20, drp->length_dpi);   
    grub_printf("device path info length = 0x%x\n", drp->length_dpi);;
    grub_printf("name of host bus = %4s\n", drp->name_of_host_bus);
    grub_printf("name of interface type = %8s\n", drp->name_of_interface_type);
  }


  return 0;
}

grub_err_t
miray_cmd_diag_bios_print_cdinfo (grub_command_t cmd __attribute__ ((unused)),
		       int argc, char **args )
{
  int ret;
  struct grub_biosdisk_cdrp *cdrp
        = (struct grub_biosdisk_cdrp *) GRUB_MEMORY_MACHINE_SCRATCH_ADDR;

  struct grub_bios_int_registers cdinfo_regs = 
  {
    .eax = 0x4b01,
    .esi = (unsigned long)cdrp & 0xf,
    .ds = ((unsigned long)cdrp >> 4) & 0xffff,
  };

  if (argc < 1)
  {
    grub_printf("drive missing\n");
    return -1;
  }

  ret = driveno(args[0]);
  if (ret < 0)
  {
    grub_printf("invalid drive name\n");
    return -1;
  }
  cdinfo_regs.edx = ret & 0xff;

  grub_memset (cdrp, 0, sizeof (*cdrp));

  cdrp->size = sizeof (*cdrp);
  cdrp->media_type = 0xFF;
  
  //ret = grub_biosdisk_get_cdinfo_int13_extensions (drive, cdrp);
  grub_bios_interrupt (0x13, &cdinfo_regs);
  ret = cdinfo_regs.eax;
  grub_printf("grub_biosdisk_get_cdinfo_int13_extensions ret = %d\n", ret);


  if (ret == 0)
  {

    grub_printf("size = %d\n", cdrp->size);			// u8
    grub_printf("media_type = 0x%x\n", cdrp->media_type );	// u8
    grub_printf("drive_no = %s%d\n", cdrp->drive_no & 0x80 ? "hd": "fd", cdrp->drive_no & 0x7f);		// u8
    grub_printf("controller_no = %d\n", cdrp->controller_no);	// u8
    grub_printf("image_lba = %d\n", cdrp->image_lba);		// u32
    grub_printf("device_spec = %d\n", cdrp->device_spec);	// u16
    grub_printf("cache_seg = %d\n", cdrp->cache_seg);		// u16
    grub_printf("load_seg = %d\n", cdrp->load_seg);		// u16
    grub_printf("length_sec512 = %d\n", cdrp->length_sec512);	// u16
    grub_printf("C/H/S = %d/%d/%d\n", cdrp->cylinders, cdrp->sectors, cdrp->heads); // u8/u8/u8

    hexdump (0, (char *)cdrp, cdrp->size);

  }

  return 0;
    
}
