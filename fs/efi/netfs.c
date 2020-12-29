/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2010 Bean Lee - All Rights Reserved
 *
 *  BURG is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  BURG is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with BURG.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/disk.h>
#include <grub/file.h>
#include <grub/misc.h>
#include <grub/efi/efi.h>
#include <grub/efi/load_file.h>

static grub_efi_guid_t load_file_guid = GRUB_EFI_LOAD_FILE_GUID;
static grub_efi_load_file_protocol_t **ifs;
static int num_ifs;

static void
find_handles (void)
{
  grub_efi_loaded_image_t *image;
  grub_efi_handle_t *handles;
  grub_efi_uintn_t num, i;

  num_ifs = 0;
  handles = grub_efi_locate_handle (GRUB_EFI_BY_PROTOCOL, &load_file_guid,
				    NULL, &num);
  if ((! handles) || (! num))
    goto quit;

  image = grub_efi_get_loaded_image (grub_efi_image_handle);
  if (! image)
    goto quit;

  ifs = grub_malloc (num * sizeof (ifs[0]));
  if (! ifs)
    goto quit;

  for (i = 0; i < num; i++)
    {
      if (efi_call_3 (grub_efi_system_table->boot_services->handle_protocol,
		      handles[i], &load_file_guid, &ifs[num_ifs]))
	continue;

      if ((num_ifs) && (handles[i] == image->device_handle))
	{
	  grub_efi_load_file_protocol_t *tmp;

	  tmp = ifs[0];
	  ifs[0] = ifs[num_ifs];
	  ifs[num_ifs] = tmp;
	}
      num_ifs++;
    }

 quit:
  grub_free (handles);
}

static int
grub_net_iterate (int (*hook) (const char *name, void *closure),
		  void *closure)
{
  int i;
  char buf[8];

  for (i = 0; i < num_ifs; i++)
    {
      grub_snprintf (buf, sizeof (buf), "net%d", i);
      if (hook (buf, closure))
	return 1;
    }
  return 0;
}

static grub_err_t
grub_net_open (const char *name, grub_disk_t disk)
{
  if ((grub_memcmp (name, "net", 3) != 0) ||
      (name[3] < '0') || (name[3] > '9'))
    return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "not a net disk");

  disk->total_sectors = 0;
  disk->id = grub_strtoul (name + 3, NULL, 0);
  disk->has_partitions = 0;

  return GRUB_ERR_NONE;
}

static void
grub_net_close (grub_disk_t disk __attribute((unused)))
{
}

static grub_err_t
grub_net_read (grub_disk_t disk __attribute((unused)),
	       grub_disk_addr_t sector __attribute((unused)),
	       grub_size_t size __attribute((unused)),
	       char *buf __attribute((unused)))
{
  return GRUB_ERR_OUT_OF_RANGE;
}

static grub_err_t
grub_net_write (grub_disk_t disk __attribute((unused)),
		grub_disk_addr_t sector __attribute((unused)),
		grub_size_t size __attribute((unused)),
		const char *buf __attribute((unused)))
{
  return GRUB_ERR_OUT_OF_RANGE;
}

static struct grub_disk_dev grub_net_dev =
  {
    .name = "net",
    .id = GRUB_DISK_DEVICE_PXE_ID,
    .iterate = grub_net_iterate,
    .open = grub_net_open,
    .close = grub_net_close,
    .read = grub_net_read,
    .write = grub_net_write,
    .next = 0
  };

static grub_err_t
grub_netfs_dir (grub_device_t device,
		const char *path  __attribute__ ((unused)),
		int (*hook) (const char *filename,
			     const struct grub_dirhook_info *info,
			     void *closure __attribute__ ((unused))) __attribute__ ((unused)),
		void *closure __attribute__ ((unused)))
{
  if (device->disk->dev->id != GRUB_DISK_DEVICE_PXE_ID)
    return grub_error (GRUB_ERR_BAD_FS, "not a net fs");

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_netfs_open (struct grub_file *file, const char *name)
{
  int id = file->device->disk->id;
  int len, size;
  grub_efi_device_path_t *dp, *next;
  grub_efi_char16_t *p;
  char *buffer;
  grub_efi_uintn_t buffer_size;

  len = grub_strlen (name) + 1;
  size = sizeof (grub_efi_device_path_t) + len * sizeof (grub_efi_char16_t);
  dp = grub_malloc (size + sizeof (grub_efi_device_path_t));
  if (! dp)
    return grub_errno;

  dp->type = GRUB_EFI_MEDIA_DEVICE_PATH_TYPE;
  dp->subtype = GRUB_EFI_FILE_PATH_DEVICE_PATH_SUBTYPE;
  dp->length[0] = (grub_efi_uint8_t) (size & 0xff);
  dp->length[1] = (grub_efi_uint8_t) (size >> 8);

  p = (grub_efi_char16_t *) (dp + 1);
  while (len)
    {
      *(p++) = *(name++);
      len--;
    }

  next = GRUB_EFI_NEXT_DEVICE_PATH (dp);
  next->type = GRUB_EFI_END_DEVICE_PATH_TYPE;
  next->subtype = GRUB_EFI_END_ENTIRE_DEVICE_PATH_SUBTYPE;
  next->length[0] = sizeof (*next);
  next->length[1] = 0;

  buffer_size = 0;
  efi_call_5 (ifs[id]->load_file, ifs[id], dp, 0, &buffer_size, NULL);
  if (buffer_size == 0)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND, "file not found");

  buffer = grub_malloc (buffer_size);
  if (! buffer)
    return grub_errno;

  if (efi_call_5 (ifs[id]->load_file, ifs[id], dp, 0, &buffer_size, buffer))
    {
      grub_free (buffer);
      return grub_error (GRUB_ERR_BAD_FS, "read fails");
    }

  file->size = buffer_size;
  file->data = buffer;

  return GRUB_ERR_NONE;
}

static grub_ssize_t
grub_netfs_read (grub_file_t file, char *buf, grub_size_t len)
{
  char *data = file->data;
  grub_memcpy (buf, data + file->offset, len);
  return len;
}

static grub_err_t
grub_netfs_close (grub_file_t file)
{
  grub_free (file->data);
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_netfs_label (grub_device_t device __attribute ((unused)),
		   char **label __attribute ((unused)))
{
  *label = 0;
  return GRUB_ERR_NONE;
}

static struct grub_fs grub_netfs_fs =
  {
    .name = "netfs",
    .dir = grub_netfs_dir,
    .open = grub_netfs_open,
    .read = grub_netfs_read,
    .close = grub_netfs_close,
    .label = grub_netfs_label,
    .next = 0
  };

GRUB_MOD_INIT(netfs)
{
  find_handles ();
  if (num_ifs)
    {
      grub_disk_dev_register (&grub_net_dev);
      grub_fs_register (&grub_netfs_fs);
    }
}

GRUB_MOD_FINI(netfs)
{
  if (num_ifs)
    {
      grub_disk_dev_unregister (&grub_net_dev);
      grub_fs_unregister (&grub_netfs_fs);
      grub_free (ifs);
    }
}
