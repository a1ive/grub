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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/device.h>
#include <grub/file.h>
#include <grub/fs.h>
#include <grub/normal.h>
#include <grub/lib/sortlib.h>

#include "fm.h"

static void
grubfm_add_menu_parent (const char *dirname)
{
  char *parent_dir = NULL;
  char *src = NULL;
  parent_dir = grub_strdup (dirname);
  *grub_strrchr (parent_dir, '/') = 0;
  if (grub_strrchr (parent_dir, '/'))
    *grub_strrchr (parent_dir, '/') = 0;
  else if (parent_dir)
    parent_dir[0] = '\0';
  if (grub_strlen (parent_dir))
    src = grub_xasprintf ("grubfm \"%s/\"", parent_dir);
  else
    src = grub_xasprintf ("grubfm");

  grubfm_add_menu (_("Back"), "go-previous", NULL, src, 0);
  grub_free (src);
  if (parent_dir)
    grub_free (parent_dir);
}

static void
grubfm_add_menu_dir (const char *filename, char *pathname)
{
  char *title = NULL;
  title = grub_xasprintf ("%-10s [%s]", _("DIR"), filename);
  char *src = NULL;
  src = grub_xasprintf ("grubfm \"%s/\"", pathname);
  grubfm_add_menu (title, "dir", NULL, src, 0);
  grub_free (title);
  grub_free (src);
}

static void
grubfm_add_menu_file (struct grubfm_enum_file_info *file, char *pathname)
{
  char *title = NULL;
  title = grub_xasprintf ("%-10s %s", file->size, file->name);
  char *src = NULL;
  src = grub_xasprintf ("grubfm_open \"%s\"", pathname);
  char *icon = NULL;
  icon = grubfm_get_file_type (file);
  grubfm_add_menu (title, icon, NULL, src, 0);
  grub_free (title);
  grub_free (src);
}

static void
grubfm_enum_file_list_close (struct grubfm_enum_file_list *ctx)
{
  int i;
  if (!ctx->dir_list)
    goto free_f;
  for (i = 0; i < ctx->ndirs; i++)
  {
    if (ctx->dir_list[i].name)
      grub_free (ctx->dir_list[i].name);
  }
  grub_free (ctx->dir_list);
free_f:
  if (!ctx->file_list)
    return;
  for (i = 0; i < ctx->nfiles; i++)
  {
    if (ctx->file_list[i].name)
      grub_free (ctx->file_list[i].name);
    if (ctx->file_list[i].size)
    grub_free (ctx->file_list[i].size);
  }
  grub_free (ctx->file_list);
}

static int
grubfm_enum_device_iter (const char *name,
                         void *data __attribute__ ((unused)))
{
  if (grub_strcmp (name, "memdisk") == 0 ||
      grub_strcmp (name, "proc") == 0 ||
      grub_strcmp (name, "python") == 0)
    return 0;
  grub_device_t dev;

  dev = grub_device_open (name);
  if (dev)
  {
    grub_fs_t fs;

    fs = grub_fs_probe (dev);
    if (fs)
    {
      char *uuid = NULL;
      char *label = NULL;
      if (fs->fs_uuid)
      {
        int err;
        err = fs->fs_uuid (dev, &uuid);
        if (err)
        {
          grub_errno = 0;
          uuid = NULL;
        }
      }

      if (fs->fs_label)
      {
        int err;
        err = fs->fs_label (dev, &label);
        if (err)
        {
          grub_errno = 0;
          label = NULL;
        }
      }

      char *title = NULL;
      title = grub_xasprintf ("(%s) [%s] %s %s", name,
                grub_strlen(label)? label : "NO LABEL",
                fs->name, uuid? uuid : "");
      char *src = NULL;
      src = grub_xasprintf ("grubfm \"(%s)/\"", name);
      if (grub_strcmp (fs->name, "iso9660") == 0 ||
          grub_strcmp (fs->name, "udf") == 0)
        grubfm_add_menu (title, "iso", NULL, src, 0);
      else
        grubfm_add_menu (title, "hdd", NULL, src, 0);
      grub_free (title);
      grub_free (src);
      if (label)
        grub_free (label);
      if (uuid)
        grub_free (uuid);
    }
    else
      grub_errno = 0;
    grub_device_close (dev);
  }
  else
    grub_errno = 0;

  return 0;
}

int
grubfm_enum_device (void)
{
  grub_device_iterate (grubfm_enum_device_iter, NULL);
  return 0;
}

static int
grubfm_enum_file_count (const char *filename,
                        const struct grub_dirhook_info *info,
                        void *data)
{
  struct grubfm_enum_file_list *ctx = data;

  if (grub_strcmp (filename, ".") == 0 || grub_strcmp (filename, "..") == 0)
    return 0;

  if (info->dir)
    ctx->ndirs++;
  else
    ctx->nfiles++;
  return 0;
}

static grub_ssize_t
list_compare (const void *f1,
              const void *f2)
{
  const struct grubfm_enum_file_info *d1 = f1;
  const struct grubfm_enum_file_info *d2 = f2;
  return (grub_strcmp(d1->name, d2->name));
}

static int
grubfm_enum_file_iter (const char *filename,
                       const struct grub_dirhook_info *info,
                       void *data)
{
  struct grubfm_enum_file_list *ctx = data;
  char *dirname = ctx->dirname;
  if (grub_strcmp (filename, ".") == 0 || grub_strcmp (filename, "..") == 0)
    return 0;
  char *pathname;

  if (dirname[grub_strlen (dirname) - 1] == '/')
    pathname = grub_xasprintf ("%s%s", dirname, filename);
  else
    pathname = grub_xasprintf ("%s/%s", dirname, filename);
  if (!pathname)
    return 1;
  if (info->dir)
  {
    ctx->dir_list[ctx->d].name = grub_strdup(filename);
    ctx->dir_list[ctx->d].type = DIR;
    ctx->d++;
  }
  else
  {
    grub_file_t file = 0;
    file = grub_file_open (pathname, GRUB_FILE_TYPE_GET_SIZE |
                           GRUB_FILE_TYPE_NO_DECOMPRESS);
    if (! file)
    {
      grub_errno = 0;
      grub_free (pathname);
      return 0;
    }
    ctx->file_list[ctx->f].name = grub_strdup (filename);
    ctx->file_list[ctx->f].size = grub_strdup (
        grub_get_human_size (file->size, GRUB_HUMAN_SIZE_SHORT));
    grub_file_close (file);
    ctx->f++;
  }
  grub_free (pathname);
  return 0;
}

int
grubfm_enum_file (char *dirname)
{
  char *device_name;
  grub_fs_t fs;
  const char *path;
  grub_device_t dev;

  grubfm_add_menu_parent (dirname);

  device_name = grub_file_get_device_name (dirname);
  dev = grub_device_open (device_name);
  if (!dev)
    goto fail;

  fs = grub_fs_probe (dev);
  path = grub_strchr (dirname, ')');
  if (!path)
    path = dirname;
  else
    path++;

  if (!path && !device_name)
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid argument");
    goto fail;
  }

  if (!*path)
  {
    if (grub_errno == GRUB_ERR_UNKNOWN_FS)
      grub_errno = GRUB_ERR_NONE;
    goto fail;
  }
  else if (fs)
  {
    struct grubfm_enum_file_list ctx = { 0, NULL, 0, NULL, NULL , 0, 0};
    ctx.dirname = dirname;
    (fs->fs_dir) (dev, path, grubfm_enum_file_count, &ctx);
    ctx.file_list = grub_zalloc (ctx.nfiles * sizeof (ctx.file_list[0]));
    if (!ctx.file_list)
      return 0;
    ctx.dir_list = grub_zalloc (ctx.ndirs * sizeof (ctx.dir_list[0]));
    if (!ctx.dir_list)
      return 0;
    (fs->fs_dir) (dev, path, grubfm_enum_file_iter, &ctx);
    ctx.ndirs = ctx.d;
    ctx.nfiles = ctx.f;
    perform_quick_sort (ctx.dir_list, ctx.ndirs,
                        sizeof(struct grubfm_enum_file_info), list_compare);
    for (ctx.d = 0; ctx.d < ctx.ndirs; ctx.d++)
    {
      char *pathname;
      if (dirname[grub_strlen (dirname) - 1] == '/')
        pathname = grub_xasprintf ("%s%s", dirname,
                                   ctx.dir_list[ctx.d].name);
      else
        pathname = grub_xasprintf ("%s/%s", dirname,
                                   ctx.dir_list[ctx.d].name);
      if (!pathname)
        return 1;
      grubfm_add_menu_dir (ctx.dir_list[ctx.d].name, pathname);
      grub_free (pathname);
    }
    perform_quick_sort (ctx.file_list, ctx.nfiles,
                        sizeof(struct grubfm_enum_file_info), list_compare);
    for (ctx.f = 0; ctx.f < ctx.nfiles; ctx.f++)
    {
      char *pathname;
      if (dirname[grub_strlen (dirname) - 1] == '/')
        pathname = grub_xasprintf ("%s%s", dirname,
                                   ctx.file_list[ctx.f].name);
      else
        pathname = grub_xasprintf ("%s/%s", dirname,
                                   ctx.file_list[ctx.f].name);
      if (!pathname)
        return 1;
      grubfm_add_menu_file (&ctx.file_list[ctx.f], pathname);
      grub_free (pathname);
    }
    grubfm_enum_file_list_close (&ctx);
  }

 fail:
  if (dev)
    grub_device_close (dev);

  grub_free (device_name);

  return 0;
}
