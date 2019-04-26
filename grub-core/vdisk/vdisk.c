/* vdisk.c - the core routine for virtual disk.  */
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

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/dl.h>
#include "vfs.h"

static struct grub_vfs *fs_list = NULL;

/* Check whether the name is available */
static int
check_vfs_name (const char *name)
{
  /* check whether there is at least one fs */
  if (fs_list == NULL)
    return 1;

  /* iterate the whole linked list */
  struct grub_vfs *start = fs_list;
  struct grub_vfs *current = start;

  do
    {
      if (grub_strcasecmp (name, current->name) == 0)
	return 0;

      current = current->next;
    }
  while (current != start);

  return 1;
}

/* Push the vfs into the linked list */
static void
push_vfs (struct grub_vfs *vfs)
{
  if (fs_list == NULL)
    {
      vfs->prev = vfs;
      vfs->next = vfs;
      fs_list = vfs;
    }
  else
    {
      vfs->prev = fs_list;
      vfs->next = fs_list->next;
      fs_list->next = vfs;
      fs_list->next->prev = vfs;
    }
}

/* Remove the vfs from the linked list */
static void
remove_vfs (struct grub_vfs *vfs)
{
  if (vfs->next == vfs)
    {
      vfs->next = NULL;
      vfs->prev = NULL;
      fs_list = NULL;
    }
  else
    {
      if (fs_list == vfs)
	fs_list = vfs->next;

      vfs->next->prev = vfs->prev;
      vfs->prev->next = vfs->next;
      vfs->next = NULL;
      vfs->prev = NULL;
    }
}

static int
check_node_name (struct grub_vfs_node *node, const char *name)
{
  /* check whether there is at least one fs */
  if (node->children == NULL)
    return 1;

  /* check whether the name is . or .. */
  if (grub_strcmp (name, ".") == 0 || grub_strcmp (name, "..") == 0)
    return 0;

  /* iterate the whole linked list */
  struct grub_vfs_node *start = node->children;
  struct grub_vfs_node *current = start;

  do
    {
      if (grub_strcasecmp (name, current->name) == 0)
	return 0;

      current = current->next;
    }
  while (current != start);

  return 1;
}

static void
push_node (struct grub_vfs_node *parent, struct grub_vfs_node *node)
{
  if (parent->children == NULL)
    {
      node->next = node;
      node->prev = node;
      parent->children = node;
    }
  else
    {
      node->prev = parent->children;
      node->next = parent->children->next;
      parent->children->next = node;
      parent->children->next->prev = node;
    }
  node->parent = parent;
}

static void
remove_node (struct grub_vfs_node *node)
{
  struct grub_vfs_node *parent = node->parent;
  if (node->next == node)
    {
      node->next = NULL;
      node->prev = NULL;
      if (parent)
	parent->children = NULL;
    }
  else
    {
      if (parent && parent->children == node)
	parent->children = node->next;

      node->next->prev = node->prev;
      node->prev->next = node->next;
      node->next = NULL;
      node->prev = NULL;
    }
}

/* Recursive destroy the node */
static void
destroy_node (struct grub_vfs_node *node)
{
  if (node->children != NULL)
    {
      /* if there is at least a child, restroy the children nodes recursively */
      while (node->children != NULL)
	destroy_node (node->children);
    }

  /* now remove the node */
  remove_node (node);

  if (node->name)
    grub_free (node->name);

  if (node->path_to_file)
    grub_free (node->path_to_file);

  if (node->data)
    grub_free (node->data);

  grub_free (node);
}

/* allocate the memory and make a new node */
static struct grub_vfs_node *
make_node (struct grub_vfs *vfs, int type, const char *name)
{
  struct grub_vfs_node *node = grub_zalloc (sizeof(struct grub_vfs_node));
  if (!node)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));
      return NULL;
    }

  node->name = grub_strdup (name);
  if (!node->name)
    {
      grub_free (node);
      return NULL;
    }

  node->vfs = vfs;
  node->type = type;

  return node;
}

grub_err_t
grub_vfs_create (const char *name, struct grub_vfs **vfs)
{
  struct grub_vfs *fs;
  struct grub_vfs_node *root_node;
  fs = grub_zalloc (sizeof(struct grub_vfs));

  if (!fs)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));

  if (!check_vfs_name (name))
    return grub_error (GRUB_ERR_BAD_FILENAME, N_ ("The name has been taken."));

  fs->name = grub_strdup (name);
  if (!fs->name)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));

  /* create the root node */
  root_node = grub_zalloc (sizeof(struct grub_vfs_node));
  if (!root_node)
    {
      grub_free (fs->name);
      grub_free (fs);
      return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));
    }

  root_node->type = GRUB_VFS_NODE_TYPE_DIRECTORY;
  root_node->vfs = fs;
  fs->root = root_node;

  push_vfs (fs);
  *vfs = fs;
  return 0;
}

grub_err_t
grub_vfs_get (const char *name, struct grub_vfs **vfs)
{
  /* check whether there is at least one fs */
  if (fs_list == NULL)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND, N_ ("VFS is not found."));

  /* iterate the whole linked list */
  struct grub_vfs *start = fs_list;
  struct grub_vfs *current = start;

  do
    {
      if (grub_strcasecmp (name, current->name) == 0)
	{
	  *vfs = current;
	  return 0;
	}
      current = current->next;
    }
  while (current != start);

  return grub_error (GRUB_ERR_FILE_NOT_FOUND, N_ ("VFS is not found."));
}

void
grub_vfs_destroy (struct grub_vfs *vfs)
{
  remove_vfs (vfs);
  destroy_node (vfs->root);
  grub_free (vfs->name);
  grub_free (vfs);
}

int
grub_vfs_iterate_fs (grub_vfs_iterate_fs_hook_func_t hook, void *hook_data)
{
  if (fs_list == NULL)
    return 0;

  struct grub_vfs *start = fs_list;
  struct grub_vfs *current = start;

  do
    {
      if (hook (current, hook_data))
	/* if the hook function returns non-zero, we stop here */
	return 1;

      current = current->next;
    }
  while (current != start);

  return 0;
}

grub_err_t
grub_vfs_mkfile (struct grub_vfs_node *node, const char *filename,
		 const char *underlying_file)
{
  if (node->type != GRUB_VFS_NODE_TYPE_DIRECTORY)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
		       N_ ("Destination is not a directory"));

  struct grub_vfs_node *file_node;

  if (!check_node_name (node, filename))
    return grub_error (GRUB_ERR_BAD_FILENAME,
		       N_ ("The name `%s' has been taken."), filename);

  /* Check the file type */
  if (underlying_file)
    file_node = make_node (node->vfs, GRUB_VFS_NODE_TYPE_LINK_FILE, filename);
  else
    file_node =
      make_node (node->vfs, GRUB_VFS_NODE_TYPE_MEMORY_FILE, filename);

  if (!file_node)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));

  if (underlying_file != NULL)
    {
      grub_file_t file = grub_file_open (underlying_file, GRUB_FILE_TYPE_CAT);
      if (!file)
	return grub_error (GRUB_ERR_ACCESS_DENIED, N_ (
			     "unable to open the file"));
      file_node->len = grub_file_size (file);
      grub_file_close (file);

      file_node->path_to_file = grub_strdup (underlying_file);
      if (!file_node->path_to_file)
	{
	  destroy_node (file_node);
	  return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));
	}
    }

  push_node (node, file_node);
  return GRUB_ERR_NONE;
}

grub_err_t
grub_vfs_mkdir (struct grub_vfs_node *node, const char *name)
{
  if (node->type != GRUB_VFS_NODE_TYPE_DIRECTORY)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
		       N_ ("Destination is not a directory"));

  struct grub_vfs_node *dir_node;

  if (!check_node_name (node, name))
    return grub_error (GRUB_ERR_BAD_FILENAME, N_ ("The name has been taken."));

  dir_node = make_node (node->vfs, GRUB_VFS_NODE_TYPE_DIRECTORY, name);

  if (!dir_node)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));

  push_node (node, dir_node);
  return GRUB_ERR_NONE;
}

grub_err_t
grub_vfs_dir (struct grub_vfs_node *node, grub_vfs_dir_hook_func_t hook,
	      void *hook_data)
{
  if (node->children == NULL)
    {
      return GRUB_ERR_NONE;
    }

  struct grub_vfs_node *start = node->children;
  struct grub_vfs_node *current = start;

  do
    {
      hook (current, hook_data);
      current = current->next;
    }
  while (current != start);

  return GRUB_ERR_NONE;
}

grub_err_t
grub_vfs_lookup (struct grub_vfs_node *node, const char *filename,
		 struct grub_vfs_node **found_node)
{
  if (node->children == NULL)
    return GRUB_ERR_FILE_NOT_FOUND;

  struct grub_vfs_node *start = node->children;
  struct grub_vfs_node *current = start;

  do
    {
      if (grub_strcasecmp (current->name, filename) == 0)
	{
	  *found_node = current;
	  return GRUB_ERR_NONE;
	}
      current = current->next;
    }
  while (current != start);

  grub_dprintf("vfs", "node `%s' not found\n", filename);
  return grub_error(GRUB_ERR_FILE_NOT_FOUND, "node `%s' not found\n", filename);
}

grub_err_t
grub_vfs_rm (struct grub_vfs_node *node)
{
  if (node->parent == 0)
    return grub_error (GRUB_ERR_BAD_FILE_TYPE,
		       N_ ("unable to remove root directory"));

  if (node->type == GRUB_VFS_NODE_TYPE_DIRECTORY && node->children != NULL)
    return grub_error (GRUB_ERR_BAD_FILE_TYPE, N_ ("directory is not empty"));

  destroy_node (node);
  return 0;
}

grub_err_t
grub_vfs_rm_recursively (struct grub_vfs_node *node)
{
  if (node->parent == 0)
    return grub_error (GRUB_ERR_BAD_FILE_TYPE,
		       N_ ("unable to remove root directory"));

  destroy_node (node);
  return 0;
}

grub_err_t
grub_vfs_mv (struct grub_vfs_node *src, struct grub_vfs_node *dest,
	     const char *filename)
{
  if (dest->type != GRUB_VFS_NODE_TYPE_DIRECTORY)
    return grub_error (GRUB_ERR_BAD_FILE_TYPE,
		       N_ ("Destination is not a directory"));

  if (!check_node_name (dest, filename))
    return grub_error (GRUB_ERR_BAD_FILENAME, N_ ("The name has been taken."));

  char *_name = grub_strdup (filename);
  if (!_name)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));

  grub_free (src->name);
  src->name = _name;

  remove_node (src);
  push_node (dest, src);

  return 0;
}

struct grub_vfs_ctx *
grub_vfs_open (struct grub_vfs_node *node)
{
  struct grub_vfs_ctx *_ctx = grub_zalloc (sizeof(struct grub_vfs_ctx));

  if (!_ctx)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));
      goto fail;
    }

  _ctx->node = node;
  if (node->path_to_file)
    {
      _ctx->file = grub_file_open (node->path_to_file, GRUB_FILE_TYPE_CAT);
      if (!_ctx->file)
	{
	  grub_dprintf ("vfs", "unable to open the file `%s'\n",
			node->path_to_file);
	  goto fail;
	}

      /* update the length of the file */
      node->len = grub_file_size (_ctx->file);
    }

  return _ctx;

fail:
  if (_ctx)
    grub_free (_ctx);

  return 0;
}

grub_off_t
grub_vfs_seek (struct grub_vfs_ctx *ctx, grub_off_t pos)
{
  grub_off_t old;
  if (ctx->file)
    return grub_file_seek (ctx->file, pos);

  if (pos > ctx->node->len)
    {
      grub_error (GRUB_ERR_OUT_OF_RANGE,
		  N_ ("attempt to seek outside of the file"));
      return -1;
    }

  old = ctx->pos;
  ctx->pos = pos;

  return old;
}

grub_ssize_t
grub_vfs_read (struct grub_vfs_ctx *ctx, char *buf, grub_size_t len)
{
  if (ctx->file)
    /* Read the underyling file */
    return grub_file_read (ctx->file, buf, len);
  else
    {
      /* Read our virtual file */
      if (ctx->pos > ctx->node->len)
	/* Attempt to read past the end of file */
	return -1;

      if (len > ctx->node->len - ctx->pos)
	len = ctx->node->len - ctx->pos;

      /* Prevent an overflow.  */
      if ((grub_ssize_t) len < 0)
	len >>= 1;

      if (len == 0)
	return 0;

      /* try to read the file */
      grub_memcpy (buf, ctx->node->data + ctx->pos, len);
      ctx->pos += len;
      return len;
    }
}

grub_ssize_t
grub_vfs_write (struct grub_vfs_ctx *ctx, char *buf, grub_size_t len)
{
  if (ctx->file)
    /* If the node is backed by a file, don't write it */
    return -1;

  if (len == 0)
    return 0;

  /* Write to our virtual file */
  if (ctx->pos + len > ctx->node->len)
    {
      void *new_mem = NULL;

      /* We need to realloc the memory */
      if (ctx->node->data)
	new_mem = grub_realloc (ctx->node->data, ctx->pos + len);
      else
	new_mem = grub_malloc (ctx->pos + len);

      if (!new_mem)
	goto fail;

      ctx->node->data = new_mem;
      ctx->node->len = ctx->pos + len;
    }

  grub_memcpy (ctx->node->data + ctx->pos, buf, len);
  ctx->pos += len;

  return len;
fail:
  return -1;
}

/* truncate the file to LEN */
grub_err_t
grub_vfs_trunc (struct grub_vfs_ctx *ctx, grub_size_t len)
{
  if (ctx->file)
    /* If the node is backed by a file, don't truncate it */
    return -1;

  void *new_mem = grub_realloc (ctx->node->data, len);
  if (!new_mem)
    return grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));

  ctx->node->data = new_mem;
  ctx->node->len = len;

  return 0;
}

/* Close the file */
void
grub_vfs_close (struct grub_vfs_ctx *ctx)
{
  if (ctx->file)
    grub_file_close (ctx->file);

  grub_free (ctx);
}

struct helper_data
{
  grub_fs_dir_hook_t hook;
  void *hook_data;
};

static grub_err_t
helper_hook_func (grub_fshelp_node_t dir,
		  const char *name,
		  grub_fshelp_node_t * foundnode,
		  enum grub_fshelp_filetype *foundtype)
{
  struct grub_vfs_node *node = (struct grub_vfs_node *) dir;
  struct grub_vfs_node *_foundnode;
  grub_err_t err;

  grub_error_push();
  err = grub_vfs_lookup (node, name, &_foundnode);
  grub_error_pop();

  if (err)
    return err;

  *foundtype =
    _foundnode->type ==
    GRUB_VFS_NODE_TYPE_DIRECTORY ? GRUB_FSHELP_DIR : GRUB_FSHELP_REG;

  *foundnode = _foundnode;

  return GRUB_ERR_NONE;
}

grub_err_t
grub_vfshelper_lookup_node (struct grub_vfs * vfs, const char *path,
		      struct grub_vfs_node ** found, int expecttype)
{
  int fshelp_filetype = GRUB_FSHELP_UNKNOWN;

  if (expecttype == GRUB_VFSHELPER_DIR)
    fshelp_filetype = GRUB_FSHELP_DIR;
  else if (expecttype == GRUB_VFSHELPER_FILE)
    fshelp_filetype = GRUB_FSHELP_REG;

  return grub_fshelp_find_file_lookup (path, vfs->root, found, helper_hook_func,
				       0, fshelp_filetype);
}
