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

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/err.h>
#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/procfs.h>
#include <grub/partition.h>
#include <grub/ventoy.h>
#include "vt_compatible.h"

static grub_err_t
grub_cmd_ventoy (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                 int argc, char **args)
{
  ventoy_os_param *os_param = NULL;
  os_param = grub_ventoy_get_osparam ();
  if (os_param)
    return grub_error (GRUB_ERR_FILE_NOT_FOUND, N_("Ventoy data not found"));

  if (argc > 0)
    grub_env_set (args[0], os_param->vtoy_img_path);
  else
    grub_printf ("%s\n", os_param->vtoy_img_path);

#ifdef GRUB_MACHINE_EFI
  grub_free (os_param);
#endif
  return GRUB_ERR_NONE;
}

static ventoy_os_param proc_vt_param;
static ventoy_img_chunk_list image_chunk_list;
static ventoy_img_chunk_list persist_chunk_list;

static char *
get_osparam (grub_size_t *sz)
{
  *sz = 0;
  char *ret = NULL;
  *sz = sizeof (ventoy_os_param);
  if (!*sz)
    return ret;
  ret = grub_malloc (*sz);
  if (!ret)
    return ret;
  grub_memcpy (ret, &proc_vt_param, *sz);
  return ret;
}

struct grub_procfs_entry proc_osparam =
{
  .name = "ventoy_os_param",
  .get_contents = get_osparam,
};

static char *
get_imgmap (grub_size_t *sz)
{
  *sz = 0;
  char *ret = NULL;
  *sz = image_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
  if (!*sz)
    return ret;
  ret = grub_malloc (*sz);
  if (!ret)
    return ret;
  grub_memcpy (ret, image_chunk_list.chunk, *sz);
  return ret;
}

struct grub_procfs_entry proc_imgmap =
{
  .name = "ventoy_image_map",
  .get_contents = get_imgmap,
};

static char *
get_persist (grub_size_t *sz)
{
  *sz = 0;
  char *ret = NULL;
  *sz = persist_chunk_list.cur_chunk * sizeof(ventoy_img_chunk);
  if (!*sz)
    return ret;
  ret = grub_malloc (*sz);
  if (!ret)
    return ret;
  grub_memcpy (ret, persist_chunk_list.chunk, *sz);
  return ret;
}

struct grub_procfs_entry proc_persist =
{
  .name = "ventoy_persistent_map",
  .get_contents = get_persist,
};

static grub_err_t
grub_cmd_osparam (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                  int argc, char **args)
{
  grub_file_t file = 0;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");
  file = grub_file_open (args[0], GRUB_FILE_TYPE_PRINT_BLOCKLIST);
  if (!file || !file->device || !file->device->disk)
  {
    grub_error (GRUB_ERR_BAD_ARGUMENT, "bad file");
    goto end;
  }

  grub_memset (&proc_vt_param, 0, sizeof (ventoy_os_param));
  grub_ventoy_fill_osparam (file, &proc_vt_param);

  if (image_chunk_list.chunk)
    grub_free(image_chunk_list.chunk);
  grub_memset(&image_chunk_list, 0, sizeof(image_chunk_list));
  image_chunk_list.chunk = grub_malloc (sizeof (ventoy_img_chunk)
                                        * DEFAULT_CHUNK_NUM);
  if (!image_chunk_list.chunk)
  {
    grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
    goto end;
  }
  image_chunk_list.max_chunk = DEFAULT_CHUNK_NUM;
  image_chunk_list.cur_chunk = 0;
  grub_ventoy_get_chunklist
      (grub_partition_get_start (file->device->disk->partition),
       file, &image_chunk_list);

end:
  if (file)
    grub_file_close (file);

  return GRUB_ERR_NONE;
}

static grub_err_t
grub_cmd_persist (grub_extcmd_context_t ctxt __attribute__ ((unused)),
                  int argc, char **args)
{
  grub_file_t file = 0;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad argument");
  file = grub_file_open (args[0], GRUB_FILE_TYPE_PRINT_BLOCKLIST);
  if (!file || !file->device || !file->device->disk)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "bad file");

  if (persist_chunk_list.chunk)
    grub_free(persist_chunk_list.chunk);
  grub_memset(&persist_chunk_list, 0, sizeof(persist_chunk_list));
  persist_chunk_list.chunk = grub_malloc (sizeof (ventoy_img_chunk)
                                          * DEFAULT_CHUNK_NUM);
  if (!persist_chunk_list.chunk)
  {
    grub_file_close (file);
    return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Can't allocate image chunk memoty\n");
  }
  persist_chunk_list.max_chunk = DEFAULT_CHUNK_NUM;
  persist_chunk_list.cur_chunk = 0;
  grub_ventoy_get_chunklist
    (grub_partition_get_start (file->device->disk->partition),
     file, &persist_chunk_list);

  grub_file_close (file);
  return GRUB_ERR_NONE;
}

static grub_extcmd_t cmd_vt, cmd_os, cmd_per;

void
vt_compatible_init (void)
{
  grub_procfs_register ("ventoy_os_param", &proc_osparam);
  grub_procfs_register ("ventoy_image_map", &proc_imgmap);
  grub_procfs_register ("ventoy_persistent_map", &proc_persist);
  cmd_vt = grub_register_extcmd ("ventoy", grub_cmd_ventoy, 0, N_("[VAR]"),
                                 N_("Get Ventoy information."), 0);
  cmd_os = grub_register_extcmd ("load_vt_param", grub_cmd_osparam, 0, N_("FILE"),
                                 N_("Update Ventoy OsParam."), 0);
  cmd_per = grub_register_extcmd ("load_vt_persist", grub_cmd_persist, 0, N_("FILE"),
                                 N_("Update Ventoy Persistent."), 0);
}

void
vt_compatible_fini (void)
{
  grub_procfs_unregister (&proc_osparam);
  grub_procfs_unregister (&proc_imgmap);
  grub_procfs_unregister (&proc_persist);
  grub_unregister_extcmd (cmd_vt);
  grub_unregister_extcmd (cmd_os);
  grub_unregister_extcmd (cmd_per);
}
