/* vfs.h - the core routine for virtual file system.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GRUB_VFS_HEADER
#define GRUB_VFS_HEADER

#include <grub/types.h>
#include <grub/file.h>
#define grub_fshelp_node grub_vfs_node
#include <grub/fshelp.h>

enum
{
  GRUB_VFSHELPER_DIR,
  GRUB_VFSHELPER_FILE,
  GRUB_VFSHELPER_UNKNOWN
};

enum {
  GRUB_VFS_NODE_TYPE_LINK_FILE,
  GRUB_VFS_NODE_TYPE_MEMORY_FILE,
  GRUB_VFS_NODE_TYPE_DIRECTORY
};

struct grub_vfs;

/*
 * Always modify this structure using the provided functions.
 */
struct grub_vfs_node {
  /* File or directory name */
  char *name;

  /* The type of the file */
  int type;

  /* If the type of the file is 0,
   * This variable contains the absolute path to the underlying file.
   */
  char *path_to_file;

  /* Point to the underlying data */
  grub_uint8_t *data;

  /* The length of the file */
  grub_size_t len;

  /* the parent node */
  struct grub_vfs_node *parent;

  /* the children nodes */
  struct grub_vfs_node *children;

  /* linked list related */
  struct grub_vfs_node *next;
  struct grub_vfs_node *prev;

  /* the vfs which this node belonged to */
  struct grub_vfs *vfs;
};

struct grub_vfs {
  /* The virtual file system name */
  char *name;

  /* The root directory */
  struct grub_vfs_node *root;

  /* linked list to other vfs */
  struct grub_vfs *next;
  struct grub_vfs *prev;
};

struct grub_vfs_ctx {
  /* The node */
  struct grub_vfs_node *node;

  /* The underlying file */
  grub_file_t file;

  /* position */
  grub_off_t pos;
};

typedef int (*grub_vfs_iterate_fs_hook_func_t) (struct grub_vfs * vfs,
						void *hook_data);

typedef int (*grub_vfs_dir_hook_func_t) (struct grub_vfs_node * node,
					 void *hook_data);

/* Create a vritual file system */
grub_err_t grub_vfs_create (const char *name, struct grub_vfs **vfs);

/* Retrieve an existing virtual file system */
grub_err_t grub_vfs_get (const char *name, struct grub_vfs **vfs);

/* Destroy the virtual file system */
void grub_vfs_destroy (struct grub_vfs *vfs);

/* Iterate through all virtual file systems */
int grub_vfs_iterate_fs (grub_vfs_iterate_fs_hook_func_t hook,
			 void *hook_data);

/* Make a new file under the NODE */
grub_err_t
grub_vfs_mkfile (struct grub_vfs_node *node, const char *filename,
		 const char *underlying_file);

/* Make a new directory under the NODE */
grub_err_t grub_vfs_mkdir (struct grub_vfs_node *node, const char *name);

/* Iterate all children nodes under the NODE */
grub_err_t
grub_vfs_dir (struct grub_vfs_node *node, grub_vfs_dir_hook_func_t hook,
	      void *hook_data);

/* Lookup a single node under the NODE by NAME */
grub_err_t
grub_vfs_lookup (struct grub_vfs_node *node, const char *filename,
		 struct grub_vfs_node **found_node);

/* remove the node recursively */
grub_err_t
grub_vfs_rm_recursively (struct grub_vfs_node * node);

/* remove a node */
grub_err_t grub_vfs_rm (struct grub_vfs_node *node);

/* move the source node to destination directory */
grub_err_t
grub_vfs_mv (struct grub_vfs_node *src, struct grub_vfs_node *dest,
	     const char *filename);

/* Open the file */
struct grub_vfs_ctx *
grub_vfs_open (struct grub_vfs_node *node);

/* Close the file */
void grub_vfs_close (struct grub_vfs_ctx *ctx);

/* Seek the file */
grub_off_t grub_vfs_seek (struct grub_vfs_ctx *ctx, grub_off_t pos);

/* read the file */
grub_ssize_t
grub_vfs_read (struct grub_vfs_ctx *ctx, char *buf, grub_size_t len);

/* write the file */
grub_ssize_t
grub_vfs_write (struct grub_vfs_ctx *ctx, char *buf, grub_size_t len);

/* truncate the file to LEN */
grub_err_t grub_vfs_trunc (struct grub_vfs_ctx * ctx, grub_size_t len);

grub_err_t
grub_vfshelper_lookup_node (struct grub_vfs *vfs, const char *path,
			    struct grub_vfs_node **found, int expecttype);
                
void vdisk_init (void);
void vdisk_fini (void);

#endif