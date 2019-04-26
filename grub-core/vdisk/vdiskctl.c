/* vdiskctl.c -- control the virtual disk */
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

#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/fshelp.h>
#include <grub/lib/arg.h>
#include <grub/extcmd.h>
#include <grub/dl.h>
#include "vfs.h"

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options_vdiskctl[] =
{
  { "create", 'c', 0, N_ ("create a new virtual file system"), 0, 0 },
  { "destory", 'd', 0, N_ ("destroy the virtual file system"), 0, 0 },
  { "add", 'a', 0, N_ ("add a file to the virtual file system"), 0, 0 },
  { "rm", 'r', 0, N_ ("remove the file from the virtual file system"), 0, 0 },
  { "mkdir", 'p', 0, N_ ("create an empty directory in the virtual file system"), 0, 0 },
  { "mkfile", 'f', 0, N_ ("create an empty file in the virtual file systemm"), 0, 0 },
  { 0, 0, 0, 0, 0, 0 }
};

enum options_vdiskctl
{
  VDISK_CREATE,
  VDISK_DESTORY,
  VDISK_ADD,
  VDISK_RM,
  VDISK_MKDIR,
  VDISK_MKFILE,
};

/* Split the PATH into directory name DIRNAME and file name FILENAME
   Caller is responsible to free DIRNAME and FILENAME returned by
   this function.
   Examples:
    '/path/to/dir/file' => '/path/to/dir', 'file'
    '/path/to/dir/' => '/path/to',  'dir'
 */
static grub_err_t
grub_fshelp_split_path (const char *path, char **dirname,
			char **filename)
{
  const char *ptr = path;
  const char *tmp = path;
  const char *next;
  grub_size_t dir_len = 0;
  char *_dirname = NULL;
  char *_filename = NULL;
  grub_err_t ret = GRUB_ERR_NONE;

  if (path[0] != '/')
    {
      return grub_error (GRUB_ERR_BAD_FILENAME, N_ ("invalid file path `%s'"),
			 path);
    }

  for (next = path;; tmp = next)
    {
      /* Remove all leading slashes. */
      while (*next == '/')
	next++;

      if (*next == '\0')
	break;

      ptr = tmp;

      /* looking for next part '/' or '\0' */
      while (*next != '/' && *next != '\0')
	next++;
    }

  /* extract dir name */
  if (dirname)
    {
      dir_len = ptr - path;
      if (dir_len > 0)
	{
	  _dirname = grub_malloc (dir_len + 1);
	  if (!_dirname)
	    {
	      ret = grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));
	      goto fail;
	    }

	  grub_memcpy (_dirname, path, dir_len);
	  _dirname[dir_len] = '\0';
	}
      else
	{
	  _dirname = grub_strdup ("/");
	  if (!_dirname)
	    {
	      ret = grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));
	      goto fail;
	    }
	}
    }

  /* extract file name */
  /* Remove all leading slashes. */
  while (*ptr == '/')
    ptr++;

  if (grub_strlen (ptr) == 0)
    {
      ret = grub_error (GRUB_ERR_BAD_FILENAME, N_ ("invalid file path `%s'"),
			path);
      goto fail;
    }

  if (filename)
    {
      char *tmp2;
      _filename = grub_strdup (ptr);

      if (!_filename)
	{
	  ret = grub_error (GRUB_ERR_OUT_OF_MEMORY, N_ ("out of memory"));
	  goto fail;
	}

      /* remove the slashes at the end */
      tmp2 = _filename;
      while (*tmp2 != '/' && *tmp2 != '\0')
	tmp2++;

      *tmp2 = '\0';
    }

  if (dirname)
    *dirname = _dirname;

  if (filename)
    *filename = _filename;

  return GRUB_ERR_NONE;

fail:
  grub_free (_dirname);
  grub_free (_filename);

  return ret;
}

static grub_err_t
grub_vdiskctl_create (int argc, char **args)
{
  struct grub_vfs *vfs;

  if (argc != 1)
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_ ("invalid argument"));

  const char *name = args[0];

  return grub_vfs_create (name, &vfs);
}


static grub_err_t
grub_vdiskctl_destroy (int argc, char **args)
{
  struct grub_vfs *vfs;

  if (argc != 1)
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_ ("invalid argument"));

  const char *name = args[0];

  if (grub_vfs_get (name, &vfs))
    return grub_errno;

  grub_vfs_destroy (vfs);
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_vdiskctl_add (int argc, char **args)
{
  struct grub_vfs *vfs;
  struct grub_vfs_node *node;
  char *dirname, *filename;

  if (argc != 3)
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_ ("invalid argument"));

  const char *fs_name = args[0];
  const char *target_name = args[1];
  const char *source_name = args[2];

  if (grub_vfs_get (fs_name, &vfs))
    return grub_errno;

  /* get the directory name and file name */
  if (grub_fshelp_split_path (target_name, &dirname, &filename))
    return grub_errno;

  if (grub_vfshelper_lookup_node (vfs, dirname, &node, GRUB_VFSHELPER_DIR))
    return grub_errno;

  if (grub_vfs_mkfile (node, filename, source_name))
    goto final;

  grub_errno = GRUB_ERR_NONE;

final:
  if (dirname)
    grub_free (dirname);
  if (filename)
    grub_free (filename);

  return grub_errno;
}

static grub_err_t
grub_vdiskctl_rm (int argc, char **args)
{
  struct grub_vfs *vfs;
  struct grub_vfs_node *node;

  if (argc != 2)
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_ ("invalid argument"));

  const char *fs_name = args[0];
  const char *file_name = args[1];

  if (grub_vfs_get (fs_name, &vfs))
    return grub_errno;

  if (grub_vfshelper_lookup_node
	(vfs, file_name, &node, GRUB_VFSHELPER_UNKNOWN))
    return grub_errno;

  return grub_vfs_rm (node);
}

static grub_err_t
grub_vdiskctl_mkfile (int argc, char **args)
{
  struct grub_vfs *vfs;
  struct grub_vfs_node *node;
  char *dirname, *filename;

  if (argc != 2)
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_ ("invalid argument"));

  const char *fs_name = args[0];
  const char *file_name = args[1];

  if (grub_vfs_get (fs_name, &vfs))
    return grub_errno;

  /* get the directory name and file name */
  if (grub_fshelp_split_path (file_name, &dirname, &filename))
    return grub_errno;

  if (grub_vfshelper_lookup_node (vfs, dirname, &node, GRUB_VFSHELPER_DIR))
    return grub_errno;

  if (grub_vfs_mkfile (node, filename, 0))
    goto final;

  grub_errno = GRUB_ERR_NONE;

final:
  if (dirname)
    grub_free (dirname);
  if (filename)
    grub_free (filename);

  return grub_errno;
}

static grub_err_t
grub_vdiskctl_mkdir (int argc, char **args)
{
  struct grub_vfs *vfs;
  struct grub_vfs_node *node;
  char *dirname, *filename;

  if (argc != 2)
    grub_error (GRUB_ERR_BAD_ARGUMENT, N_ ("invalid argument"));

  const char *fs_name = args[0];
  const char *file_name = args[1];

  if (grub_vfs_get (fs_name, &vfs))
    return grub_errno;

  /* get the directory name and file name */
  if (grub_fshelp_split_path (file_name, &dirname, &filename))
    return grub_errno;

  if (grub_vfshelper_lookup_node (vfs, dirname, &node, GRUB_VFSHELPER_DIR))
    return grub_errno;

  if (grub_vfs_mkdir (node, filename))
    goto final;

  grub_errno = GRUB_ERR_NONE;

final:
  if (dirname)
    grub_free (dirname);
  if (filename)
    grub_free (filename);

  return grub_errno;
}

/*
 * options:
 *   -c --create  FS_NAME
 *   -d --destroy FS_NAME
 *   -a --add     FS_NAME PATH FILE
 *   -r --rm      FS_NAME PATH
 *   -p --mkdir   FS_NANE DIR
 *   -f --mkfile  FS_NAME FILE
 */

static grub_err_t
grub_vdiskctl (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;

  if (state[VDISK_CREATE].set)
    return grub_vdiskctl_create (argc, args);
  if (state[VDISK_DESTORY].set)
    return grub_vdiskctl_destroy (argc, args);
  if (state[VDISK_ADD].set)
    return grub_vdiskctl_add (argc, args);
  if (state[VDISK_RM].set)
    return grub_vdiskctl_rm (argc, args);
  if (state[VDISK_MKDIR].set)
    return grub_vdiskctl_mkdir (argc, args);
  if (state[VDISK_MKFILE].set)
    return grub_vdiskctl_mkfile (argc, args);

  return grub_error (GRUB_ERR_BAD_ARGUMENT, N_ ("unexpected arguments"));
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT (vdisk)
{
  vdisk_init ();
  const char *hlpstr =
    N_ ("-c --create  FS_NAME\n"
        "-d --destroy FS_NAME\n"
        "-a --add     FS_NAME PATH FILE\n"
        "-r --rm      FS_NAME PATH\n"
        "-p --mkdir   FS_NANE DIR\n"
        "-f --mkfile  FS_NAME FILE");

  const char *desc = "Control the virtual file system";
  cmd = grub_register_extcmd ("vdisk", grub_vdiskctl, 0, hlpstr, desc, options_vdiskctl);
}

GRUB_MOD_FINI (vdisk)
{
  vdisk_fini ();
  grub_unregister_extcmd (cmd);
}
