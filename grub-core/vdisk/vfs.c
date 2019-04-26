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

#include <grub/disk.h>
#include <grub/fs.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/dl.h>
#include "vfs.h"

#define VFS_DEVICE_NAME "vfs_"
#define VFS_DEVICE_LEN sizeof(VFS_DEVICE_NAME)

struct vfsdev_iterate_data
{
  void *orig_hook_data;
  grub_disk_dev_iterate_hook_t hook_func;
};

/* Look up vfs device based on device name */
static grub_err_t
lookup_device (const char *name, struct grub_vfs **found_vfs)
{
  const char *vfsdev_prefix = VFS_DEVICE_NAME;

  if (grub_strlen (name) < VFS_DEVICE_LEN - 1)
    {
      grub_dprintf ("vfs", "device name `%s' too short\n", name);
      grub_errno = GRUB_ERR_UNKNOWN_DEVICE;
      return GRUB_ERR_UNKNOWN_DEVICE;
    }

  for (unsigned int i = 0; i < VFS_DEVICE_LEN - 1 && name[i] != '\0'; i++)
    if (name[i] != vfsdev_prefix[i])
      {
	grub_dprintf ("vfs", "`%s' is not a vfs device\n", name);
	grub_errno = GRUB_ERR_UNKNOWN_DEVICE;
	return GRUB_ERR_UNKNOWN_DEVICE;
      }

  return grub_vfs_get (name + VFS_DEVICE_LEN - 1, found_vfs);
}

static int
grub_vfsdev_iterate_helper (struct grub_vfs *vfs, void *hook_data)
{
  struct vfsdev_iterate_data *_hook_data =
    (struct vfsdev_iterate_data *) hook_data;

  grub_dprintf ("vfs", "found vfs: %s\n", vfs->name);

  char *name = grub_malloc (grub_strlen (vfs->name) + VFS_DEVICE_LEN);

  if (!name)
    return 1;

  grub_strcpy (name, VFS_DEVICE_NAME);
  grub_strcpy (name + VFS_DEVICE_LEN - 1, vfs->name);

  grub_dprintf ("vfs", "device name: %s\n", name);
  if (_hook_data->hook_func (name, _hook_data->orig_hook_data))
    return 1;

  grub_free (name);
  return 0;
}

/* Iterate through devices */
static int
grub_vfsdev_iterate (grub_disk_dev_iterate_hook_t hook, void *hook_data,
		     grub_disk_pull_t pull)
{
  if (pull != GRUB_DISK_PULL_NONE)
    return 0;

  grub_dprintf ("vfs", "iterating through devices\n");

  struct vfsdev_iterate_data data =
  {
    .orig_hook_data = hook_data,
    .hook_func = hook
  };

  return grub_vfs_iterate_fs (grub_vfsdev_iterate_helper, &data);
}

/* Open our dummy disks */
static grub_err_t
grub_vfsdev_open (const char *name, grub_disk_t disk)
{
  /* Check whether the device name starts with vfs_ */
  struct grub_vfs *vfs;

  if (lookup_device (name, &vfs))
    return grub_errno;

  disk->total_sectors = 0;
  disk->id = 0;
  disk->data = 0;

  return GRUB_ERR_NONE;
}

static void
grub_vfsdev_close (grub_disk_t disk __attribute ((unused)))
{
}

static grub_err_t
grub_vfsdev_read (grub_disk_t disk __attribute ((unused)),
		  grub_disk_addr_t sector __attribute ((unused)),
		  grub_size_t size __attribute ((unused)),
		  char *buf __attribute ((unused)))
{
  grub_errno = GRUB_ERR_OUT_OF_RANGE;
  return GRUB_ERR_OUT_OF_RANGE;
}

static grub_err_t
grub_vfsdev_write (grub_disk_t disk __attribute ((unused)),
		   grub_disk_addr_t sector __attribute ((unused)),
		   grub_size_t size __attribute ((unused)),
		   const char *buf __attribute ((unused)))
{
  grub_errno = GRUB_ERR_OUT_OF_RANGE;
  return GRUB_ERR_OUT_OF_RANGE;
}

/* File system */
static grub_ssize_t
grub_vfsfs_read (grub_file_t file, char *buf, grub_size_t len)
{
  struct grub_vfs_ctx *ctx = (struct grub_vfs_ctx *) file->data;

  grub_vfs_seek (ctx, file->offset);

  return grub_vfs_read (ctx, buf, len);
}

static grub_err_t
grub_vfsfs_close (grub_file_t file)
{
  struct grub_vfs_ctx *ctx = (struct grub_vfs_ctx *) file->data;

  grub_vfs_close (ctx);

  return GRUB_ERR_NONE;
}

struct dir_helper_data
{
  grub_fs_dir_hook_t hook;
  void *hook_data;
};

static int
dir_helper (struct grub_vfs_node *node, void *hook_data)
{
  struct dir_helper_data *data = (struct dir_helper_data *) hook_data;
  struct grub_dirhook_info info =
  {
    .case_insensitive = 1,
    .mtimeset = 0,
    .inodeset = 0,
    .dir = node->type == GRUB_VFS_NODE_TYPE_DIRECTORY ? 1 : 0
  };

  return data->hook (node->name, &info, data->hook_data);
}

static grub_err_t
grub_vfsfs_dir (grub_device_t device, const char *path,
		grub_fs_dir_hook_t hook, void *hook_data)
{
  grub_disk_t dev = device->disk;
  struct grub_vfs_node *node;
  struct grub_vfs *vfs;
  struct dir_helper_data data =
  {
    hook = hook,
    hook_data = hook_data
  };

  if (!dev)
    return grub_error (GRUB_ERR_BAD_DEVICE, N_ ("invalid device"));

  if (lookup_device (dev->name, &vfs))
    {
      return grub_error (GRUB_ERR_BAD_FS, N_ ("not a vfsfs"));
    }

  if (grub_vfshelper_lookup_node (vfs, path, &node, GRUB_VFSHELPER_DIR))
    return grub_errno;

  return grub_vfs_dir (node, dir_helper, &data);
}

/* Read only open */
static grub_err_t
grub_vfsfs_open (struct grub_file *file, const char *path)
{
  grub_disk_t dev = file->device->disk;
  struct grub_vfs *vfs;
  struct grub_vfs_node *node;
  struct grub_vfs_ctx *ctx;

  if (!dev)
    return grub_error (GRUB_ERR_BAD_DEVICE, N_ ("invalid device"));

  if (lookup_device (dev->name, &vfs))
    {
      return grub_error (GRUB_ERR_BAD_FS, N_ ("not a vfsfs"));
    }

  if (grub_vfshelper_lookup_node (vfs, path, &node, GRUB_VFSHELPER_FILE)) {
    grub_dprintf ("vfs", "file `%s' not found\n", path);
    return grub_errno;
  }

  ctx = grub_vfs_open (node);
  if (!ctx)
    {
      grub_dprintf ("vfs", "unable to open the file `%s'\n", path);

      if (grub_errno != 0)
	return grub_errno;
      else
	return 1;
    }

  file->offset = 0;
  file->data = ctx;
  file->size = node->len;

  return GRUB_ERR_NONE;
}

static struct grub_disk_dev grub_vfs_dev =
{
  .name = "vfsdev",
  .id = GRUB_DISK_DEVICE_VFS_ID,
  .disk_iterate = grub_vfsdev_iterate,
  .disk_open = grub_vfsdev_open,
  .disk_close = grub_vfsdev_close,
  .disk_read = grub_vfsdev_read,
  .disk_write = grub_vfsdev_write,
  .next = 0
};

static struct grub_fs grub_vfs_fs =
{
  .name = "vfs",
  .fs_dir = grub_vfsfs_dir,
  .fs_open = grub_vfsfs_open,
  .fs_read = grub_vfsfs_read,
  .fs_close = grub_vfsfs_close,
  .next = 0
};

void vdisk_init (void)
{
  grub_disk_dev_register (&grub_vfs_dev);
  grub_fs_register (&grub_vfs_fs);
}

void vdisk_fini (void)
{
  grub_disk_dev_unregister (&grub_vfs_dev);
  grub_fs_unregister (&grub_vfs_fs);
}
