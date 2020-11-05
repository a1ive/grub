 /*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019,2020  Free Software Foundation, Inc.
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
 *
 */

#include <grub/dl.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/disk.h>
#include <grub/device.h>
#include <grub/err.h>
#include <grub/env.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/loader.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/types.h>
#include <grub/term.h>
#include <grub/ventoy.h>

#include <iso.h>
#include <guid.h>
#include <misc.h>

GRUB_MOD_LICENSE ("GPLv3+");

static unsigned int last_id = 0;
static grub_efi_handle_t boot_image_handle = 0;

const grub_efi_guid_t dp_guid = GRUB_EFI_DEVICE_PATH_GUID;
const grub_efi_guid_t blk_io_guid = GRUB_EFI_BLOCK_IO_GUID;

static grub_err_t
grub_efiloader_unload (void)
{
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  efi_call_1 (b->unload_image, boot_image_handle);
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_efiloader_boot (void)
{
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  grub_script_execute_sourcecode ("terminal_output console");
  grub_printf ("Booting from vdisk ...\n");
  grub_refresh ();
  efi_call_3 (b->start_image, boot_image_handle, 0, NULL);

  grub_loader_unset ();

  return grub_errno;
}

static void
unmap_efidisk (grub_disk_t disk)
{
  struct grub_efidisk_data *efidisk;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  if (!disk || !disk->data)
    return;
  efidisk = disk->data;
  efi_call_6 (b->uninstall_multiple_protocol_interfaces,
              efidisk->handle, &dp_guid, efidisk->device_path,
              &blk_io_guid, efidisk->block_io, NULL);
}

static void
unmap_efivdisk (grub_disk_t disk)
{
  struct grub_efivdisk_data *d;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  if (!disk || !disk->data)
    return;
  d = disk->data;
  if (d->vpart.handle)
    efi_call_6 (b->uninstall_multiple_protocol_interfaces,
                d->vpart.handle, &dp_guid, d->vpart.dp,
                &blk_io_guid, &d->vpart.block_io, NULL);
  efi_call_6 (b->uninstall_multiple_protocol_interfaces,
              d->vdisk.handle, &dp_guid, d->vdisk.dp,
              &blk_io_guid, &d->vdisk.block_io, NULL);
}

static grub_err_t
grub_efi_unmap_device (const char *name)
{
  grub_disk_t disk = 0;
  disk = grub_disk_open (name);
  if (!disk)
    return grub_error (GRUB_ERR_BAD_DEVICE, "failed to open disk %s.", name);
  if (grub_strcmp (disk->dev->name, "efidisk") == 0)
    unmap_efidisk (disk);
  else if (grub_strcmp (disk->dev->name, "efivdisk") == 0)
    unmap_efivdisk (disk);
  else
    return grub_error (GRUB_ERR_BAD_DEVICE, "invalid disk: %s", disk->dev->name);

  return GRUB_ERR_NONE;
}

static int
grub_efivdisk_iterate (grub_disk_dev_iterate_hook_t hook, void *hook_data,
                       grub_disk_pull_t pull)
{
  struct grub_efivdisk_data *d;
  if (pull != GRUB_DISK_PULL_NONE)
    return 0;
  for (d = grub_efivdisk_list; d; d = d->next)
  {
    if (hook (d->devname, hook_data))
      return 1;
  }
  return 0;
}

static grub_err_t
grub_efivdisk_open (const char *name, grub_disk_t disk)
{
  unsigned long i = 0;
  struct grub_efivdisk_data *dev;

  for (dev = grub_efivdisk_list; dev; dev = dev->next, i++)
    if (grub_strcmp (dev->devname, name) == 0)
      break;

  if (! dev)
    return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "can't open device");

  /* Use the filesize for the disk size, round up to a complete sector.  */
  if (dev->vdisk.size != GRUB_FILE_SIZE_UNKNOWN)
    disk->total_sectors = (dev->vdisk.size + GRUB_DISK_SECTOR_SIZE - 1)
                          >> GRUB_DISK_SECTOR_BITS;
  else
    disk->total_sectors = GRUB_DISK_SIZE_UNKNOWN;
  /* Avoid reading more than 512M.  */
  disk->max_agglomerate = 1 << (29 - GRUB_DISK_SECTOR_BITS
                          - GRUB_DISK_CACHE_BITS);

  disk->id = i;

  disk->data = dev;

  return 0;
}

static grub_err_t
grub_efivdisk_read (grub_disk_t disk, grub_disk_addr_t sector,
                    grub_size_t size, char *buf)
{
  grub_file_t file = ((struct grub_efivdisk_data *) disk->data)->vdisk.file;
  grub_off_t start = ((struct grub_efivdisk_data *) disk->data)->vdisk.addr;
  grub_off_t len = ((struct grub_efivdisk_data *) disk->data)->vdisk.size;
  grub_off_t pos = (sector + size) << GRUB_DISK_SECTOR_BITS;

  file_read (file, buf, size << GRUB_DISK_SECTOR_BITS,
             (sector << GRUB_DISK_SECTOR_BITS) + start);

  if (grub_errno)
    return grub_errno;
  /* In case there is more data read than there is available, in case
     of files that are not a multiple of GRUB_DISK_SECTOR_SIZE, fill
     the rest with zeros.  */
  if (pos > len)
  {
    grub_size_t amount = pos - len;
    grub_memset (buf + (size << GRUB_DISK_SECTOR_BITS) - amount, 0, amount);
  }
  return 0;
}

static grub_err_t
grub_efivdisk_write (grub_disk_t disk, grub_disk_addr_t sector,
                     grub_size_t size, const char *buf)
{
  grub_file_t file = ((struct grub_efivdisk_data *) disk->data)->vdisk.file;
  grub_off_t start = ((struct grub_efivdisk_data *) disk->data)->vdisk.addr;
  file_write (file, buf, size << GRUB_DISK_SECTOR_BITS,
              (sector << GRUB_DISK_SECTOR_BITS) + start);
  return 0;
}

static struct grub_disk_dev grub_efivdisk_dev =
{
  .name = "efivdisk",
  .id = GRUB_DISK_DEVICE_EFIVDISK_ID,
  .disk_iterate = grub_efivdisk_iterate,
  .disk_open = grub_efivdisk_open,
  .disk_read = grub_efivdisk_read,
  .disk_write = grub_efivdisk_write,
  .next = 0
};

static void
grub_efivdisk_append (struct grub_efivdisk_data *disk)
{
  disk->next = grub_efivdisk_list;
  grub_efivdisk_list = disk;
}

static grub_err_t
mount_eltorito (struct grub_efivdisk_data *src, const char *name)
{
  struct grub_efivdisk_data *dst = NULL;
  grub_off_t ofs, len;

  if (!grub_iso_get_eltorito (src->vdisk.file, &ofs, &len))
    return grub_error (GRUB_ERR_FILE_READ_ERROR, "eltorito image not found");;
  dst = grub_zalloc (sizeof (struct grub_efivdisk_data));
  if (!dst)
    return grub_error (GRUB_ERR_BAD_OS, "out of memory");

  grub_printf ("Found UEFI El Torito image at %"
               PRIuGRUB_UINT64_T"+%"PRIuGRUB_UINT64_T"\n", ofs, len);
  grub_memcpy (dst, src, sizeof (struct grub_efivdisk_data));
  dst->type = FD;
  dst->vpart.size = len;
  dst->vpart.addr = ofs;
  grub_memcpy (&dst->vdisk, &dst->vpart, sizeof (grub_efivdisk_t));
  grub_snprintf (dst->devname, 20, "%s", name);
  last_id++;
  grub_efivdisk_append (dst);

  return GRUB_ERR_NONE;
}

struct grub_efi_mapinfo
{
  char magic[8]; /* "GNU GRUB" */
  grub_uint8_t type;
  grub_uint64_t addr;
  grub_uint64_t size;
  char path[0];
} GRUB_PACKED;

#define GRUB_EFI_MAP_GUID \
  { 0x19260817, 0xAAAA, 0xBBBB, { 0x47, 0x4E, 0x55, 0x20, 0x47, 0x52, 0x55, 0x42 }}

static void
set_map_info (grub_efivdisk_t *vdisk)
{
  grub_err_t err;
  struct grub_efi_mapinfo info;
  grub_efi_guid_t guid = GRUB_EFI_MAP_GUID;
  grub_memcpy (info.magic, "GNU GRUB", 8);
  if (grub_ismemfile (vdisk->file->name))
    info.addr = (grub_addr_t) vdisk->file->data;
  else
    info.addr = 0;
  info.size = vdisk->size;
  err = grub_efi_set_var_attr ("GrubDisk", &guid, &info, sizeof (info),
                               GRUB_EFI_VARIABLE_BOOTSERVICE_ACCESS |
                               GRUB_EFI_VARIABLE_RUNTIME_ACCESS);
  if (err)
    grub_printf ("can't set EFI variable.\n");
  //else
  //  grub_printf ("map addr 0x%lx size %lu.\n", info.addr, info.size);
}

static grub_efi_status_t (EFIAPI *orig_locate_handle)
                      (grub_efi_locate_search_type_t search_type,
                       grub_efi_guid_t *protocol,
                       void *search_key,
                       grub_efi_uintn_t *buffer_size,
                       grub_efi_handle_t *buffer) = NULL;

static int
compare_guid (grub_efi_guid_t *a, grub_efi_guid_t *b)
{
  int i;
  if (a->data1 != b->data1 || a->data2 != b->data2 || a->data3 != b->data3)
    return 0;
  for (i = 0; i < 8; i++)
  {
    if (a->data4[i] != b->data4[i])
      return 0;
  }
  return 1;
}

static grub_efi_handle_t saved_handle;

static grub_efi_status_t EFIAPI
locate_handle_wrapper (grub_efi_locate_search_type_t search_type,
                       grub_efi_guid_t *protocol,
                       void *search_key,
                       grub_efi_uintn_t *buffer_size,
                       grub_efi_handle_t *buffer)
{
  grub_efi_uintn_t i;
  grub_efi_handle_t handle = NULL;
  grub_efi_status_t status = GRUB_EFI_SUCCESS;
  grub_efi_guid_t guid = GRUB_EFI_BLOCK_IO_GUID;

  status = efi_call_5 (orig_locate_handle, search_type,
                       protocol, search_key, buffer_size, buffer);

  if (status != GRUB_EFI_SUCCESS || !protocol)
    return status;

  if (!compare_guid (&guid, protocol))
    return status;

  for (i = 0; i < (*buffer_size) / sizeof(grub_efi_handle_t); i++)
  {
    if (buffer[i] == saved_handle)
    {
      handle = buffer[0];
      buffer[0] = buffer[i];
      buffer[i] = handle;
      break;
    }
  }

  return status;
}


static const struct grub_arg_option options_map[] =
{
  {"mem", 'm', 0, N_("Copy to RAM."), 0, 0},
  {"blocklist", 'l', 0, N_("Convert to blocklist."), 0, 0},
  {"type", 't', 0, N_("Specify the disk type."), N_("CD/HD/FD"), ARG_TYPE_STRING},
  {"rt", 0, 0, N_("Set memory type to RUNTIME_SERVICES_DATA."), 0, 0},
  {"ro", 'o', 0, N_("Disable write support."), 0, 0},

  {"eltorito", 'e', 0,
    N_("Mount UEFI Eltorito image at the same time."), N_("disk"), ARG_TYPE_STRING},
  {"nb", 'n', 0, N_("Don't boot virtual disk."), 0, 0},
  {"unmap", 'x', 0, N_("Unmap devices."), N_("disk"), ARG_TYPE_STRING},
  {"first", 'f', 0, N_("Set as the first drive."), 0, 0},
  {0, 0, 0, 0, 0, 0}
};

static grub_err_t
grub_cmd_map (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  struct grub_efivdisk_data *disk = NULL;
  grub_file_t file = 0;
  if (state[MAP_UNMAP].set)
  {
    if (state[MAP_UNMAP].arg[0] == '(')
    {
      state[MAP_UNMAP].arg[grub_strlen(state[MAP_UNMAP].arg) - 1] = '\0';
      grub_efi_unmap_device (&state[MAP_UNMAP].arg[1]);
    }
    else
      grub_efi_unmap_device (state[MAP_UNMAP].arg);
    return grub_errno;
  }
  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
  disk = grub_zalloc (sizeof (struct grub_efivdisk_data));
  if (!disk)
    return grub_error (GRUB_ERR_BAD_OS, "out of memory");
  file = file_open (args[0],
                    state[MAP_MEM].set, state[MAP_BLOCK].set, state[MAP_RT].set);
  if (!file)
  {
    grub_free (disk);
    return grub_error (GRUB_ERR_FILE_READ_ERROR, "failed to open file");
  }
  disk->type = UNKNOWN;
  if (state[MAP_TYPE].set)
  {
    if (state[MAP_TYPE].arg[0] == 'C' || state[MAP_TYPE].arg[0] == 'c')
      disk->type = CD;
    if (state[MAP_TYPE].arg[0] == 'H' || state[MAP_TYPE].arg[0] == 'h')
      disk->type = HD;
    if (state[MAP_TYPE].arg[0] == 'F' || state[MAP_TYPE].arg[0] == 'f')
      disk->type = FD;
  }
  disk->type = grub_vdisk_check_type (args[0], file, disk->type);
  disk->vdisk.file = file;
  disk->vdisk.size = file->size;
  disk->vpart.file = file;
  if (argc < 2)
    grub_snprintf (disk->devname, 20, "vd%u", last_id);
  else
    grub_snprintf (disk->devname, 20, "%s", args[1]);
  last_id++;
  grub_guidgen (&disk->guid);

  grub_efivdisk_install (disk, state);
  grub_efivdisk_append (disk);
  if (disk->type == CD)
    grub_ventoy_set_osparam (args[0]);

  if (disk->type == CD && state[MAP_ELT].set)
    mount_eltorito (disk, state[MAP_ELT].arg);

  set_map_info (&disk->vdisk);

  if (state[MAP_FIRST].set)
  {
    grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
    if (!orig_locate_handle)
    {
      orig_locate_handle = (void *) b->locate_handle;
      b->locate_handle = (void *) locate_handle_wrapper;
    }
    saved_handle = disk->vdisk.handle;
  }

  if (state[MAP_NB].set)
    return grub_errno;

  /* load image */
  if (disk->vpart.dp)
    boot_image_handle = grub_efi_bootpart (disk->vpart.dp,
                                           EFI_REMOVABLE_MEDIA_FILE_NAME);
  if (!boot_image_handle)
    boot_image_handle = grub_efi_bootdisk (disk->vdisk.dp,
                                           EFI_REMOVABLE_MEDIA_FILE_NAME);
  if (!boot_image_handle)
    boot_image_handle = grub_efi_bootpart (disk->vdisk.dp,
                                           EFI_REMOVABLE_MEDIA_FILE_NAME);
  if (boot_image_handle)
  {
    grub_loader_set (grub_efiloader_boot, grub_efiloader_unload, 0);
    return GRUB_ERR_NONE;
  }
  return GRUB_ERR_FILE_NOT_FOUND;
}

static const struct grub_arg_option options_iso[] =
{
  {"offset", 'o', 0, N_("Offset of UEFI El Torito image (in sector unit)."), 0, 0},
  {"length", 'l', 0, N_("Size of UEFI El Torito image (in sector unit)."), 0, 0},
  {"ventoy", 'v', 0, N_("Check for whether ISO is ventoy compatible."), 0, 0},

  {0, 0, 0, 0, 0, 0}
};

enum options_iso
{
  ISO_OFS,
  ISO_LEN,
  ISO_VT,
};

static grub_err_t
grub_cmd_iso (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  grub_file_t file = 0;
  int ret = 0;
  char str[32];
  grub_off_t ofs = 0, len = 0;
  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));
  if (argc < 2 && (state[ISO_OFS].set || state[ISO_LEN].set))
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("varname expected"));
  file = file_open (args[0], 0, 0, 0);
  if (!file)
    return grub_error (GRUB_ERR_FILE_READ_ERROR, "failed to open file");

  if (state[ISO_VT].set)
    ret = grub_iso_check_vt (file);
  else
    ret = grub_iso_get_eltorito (file, &ofs, &len);

  if (state[ISO_OFS].set)
  {
    grub_snprintf (str, 32, "%"PRIuGRUB_UINT64_T, ofs >> GRUB_DISK_SECTOR_BITS);
    grub_env_set (args[1], str);
  }
  if (state[ISO_LEN].set)
  {
    grub_snprintf (str, 32, "%"PRIuGRUB_UINT64_T, len >> GRUB_DISK_SECTOR_BITS);
    grub_env_set (args[1], str);
  }
  return (ret ? GRUB_ERR_NONE : GRUB_ERR_TEST_FAILURE);
}

static grub_extcmd_t cmd_map, cmd_iso;

GRUB_MOD_INIT(map)
{
  cmd_map = grub_register_extcmd ("map", grub_cmd_map, 0, N_("FILE [DISK NAME]"),
                                  N_("Create virtual disk."), options_map);
  cmd_iso = grub_register_extcmd ("isotools", grub_cmd_iso, 0,
                                  N_("[-o|-l] FILE [VARNAME]"),
                                  N_("ISO tools."), options_iso);
  grub_disk_dev_register (&grub_efivdisk_dev);
}

GRUB_MOD_FINI(map)
{
  grub_unregister_extcmd (cmd_map);
  grub_unregister_extcmd (cmd_iso);
  grub_disk_dev_unregister (&grub_efivdisk_dev);
}
