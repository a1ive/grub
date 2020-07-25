#include <grub/dl.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/disk.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/charset.h>
#include <grub/eltorito.h>
#include <grub/err.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/i18n.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/net.h>
#include <grub/types.h>
#include <grub/term.h>
#include <grub/lua.h>

GRUB_MOD_LICENSE ("GPLv3+");

grub_efi_handle_t
grub_efi_bootpart (grub_efi_device_path_t *dp, const char *filename)
{
  char *text_dp = NULL;
  grub_efi_status_t status;
  grub_efi_handle_t image_handle = 0;
  grub_efi_device_path_t *boot_file = NULL;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  if (!dp)
  {
    grub_error (GRUB_ERR_BAD_OS, "Invalid device path");
    return NULL;
  }
  boot_file = grub_efi_file_device_path (dp, filename);
  if (!boot_file)
  {
    grub_error (GRUB_ERR_BAD_OS, "Invalid device path");
    return NULL;
  }
  status = efi_call_6 (b->load_image, TRUE, grub_efi_image_handle,
                       boot_file, NULL, 0, (void **)&image_handle);
  text_dp = grub_efi_device_path_to_str (boot_file);
  grub_free (boot_file);
  if (status != GRUB_EFI_SUCCESS)
  {
    grub_error (GRUB_ERR_BAD_OS, "Failed to load image %s", text_dp);
    grub_free (text_dp);
    return NULL;
  }
  grub_printf ("Will boot %s\n", text_dp);
  grub_free (text_dp);
  return image_handle;
}

grub_efi_handle_t
grub_efi_bootdisk (grub_efi_device_path_t *dp, const char *filename)
{
  grub_efi_status_t status;
  grub_efi_uintn_t count = 0;
  grub_efi_uintn_t i = 0;
  grub_efi_handle_t *buf = NULL;
  grub_efi_device_path_t *tmp_dp;
  grub_efi_handle_t image_handle = NULL;
  grub_efi_guid_t sfs_guid = GRUB_EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;

  status = efi_call_5 (b->locate_handle_buffer, GRUB_EFI_BY_PROTOCOL,
                       &sfs_guid, NULL, &count, &buf);
  if(status != GRUB_EFI_SUCCESS)
  {
    grub_error (GRUB_ERR_BAD_OS, "SimpleFileSystemProtocol not found.");
    return NULL;
  }
  for (i = 0; i < count; i++)
  {
    char *text_dp = NULL;
    tmp_dp = grub_efi_get_device_path (buf[i]);
    text_dp = grub_efi_device_path_to_str (tmp_dp);
    if (!grub_efi_is_child_dp (tmp_dp, dp))
      continue;
    grub_printf ("Child DP: %s\n", text_dp);
    grub_free (text_dp);

    image_handle = grub_efi_bootpart (tmp_dp, filename);
    if (image_handle)
      break;
  }
  return image_handle;
}

static grub_err_t
grub_cmd_dp (grub_extcmd_context_t ctxt __attribute__ ((unused)),
             int argc, char **args)
{
  if (argc != 1)
    return 0;
  char *text_dp = NULL;
  char *filename = NULL;
  char *devname = NULL;
  grub_device_t dev = 0;
  grub_efi_device_path_t *dp = NULL;
  grub_efi_device_path_t *file_dp = NULL;
  grub_efi_handle_t dev_handle = 0;
  int namelen = grub_strlen (args[0]);
  if (args[0][0] == '(' && args[0][namelen - 1] == ')')
  {
    args[0][namelen - 1] = 0;
    devname = &args[0][1];
  }
  else if (args[0][0] != '(' && args[0][0] != '/')
    devname = args[0];
  else
  {
    filename = args[0];
    devname = grub_file_get_device_name (filename);
  }
  dev = grub_device_open (devname);
  if (dev->disk)
    dev_handle = grub_efidisk_get_device_handle (dev->disk);
  else if (dev->net && dev->net->server)
  {
    grub_net_network_level_address_t addr;
    struct grub_net_network_level_interface *inf;
    grub_net_network_level_address_t gateway;
    grub_err_t err;
    err = grub_net_resolve_address (dev->net->server, &addr);
    if (err)
      goto out;
    err = grub_net_route_address (addr, &gateway, &inf);
    if (err)
      goto out;
    dev_handle = grub_efinet_get_device_handle (inf->card);
  }
  if (dev_handle)
    dp = grub_efi_get_device_path (dev_handle);
  if (filename)
    file_dp = grub_efi_file_device_path (dp, filename);
  else
    file_dp = grub_efi_duplicate_device_path (dp);
out:
  grub_printf ("DevicePath: ");
  if (!file_dp)
    grub_printf ("NULL\n");
  else
  {
    text_dp = grub_efi_device_path_to_str (file_dp);
    grub_printf ("%s\n", text_dp);
    if (text_dp)
      grub_free (text_dp);
    grub_free (file_dp);
  }

  if (dev)
    grub_device_close (dev);
  return 0;
}

#if defined (__i386__) || defined (__x86_64__)

static grub_err_t
grub_cmd_elt (grub_extcmd_context_t ctxt __attribute__ ((unused)),
              int argc, char **args)
{
  const char *filename;
  grub_efi_status_t status;
  grub_disk_t disk = 0;
  grub_efi_handle_t dev_handle = 0;
  grub_efi_handle_t image_handle;
  grub_efi_device_path_t *dp = 0;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT,
                       N_("device name expected"));
  disk = grub_disk_open (args[0]);
  if (argc > 1)
    filename = args[1];
  else
    filename = EFI_REMOVABLE_MEDIA_FILE_NAME;
  if (disk)
    dev_handle = grub_efidisk_get_device_handle (disk);
  if (dev_handle)
    dp = grub_efi_get_device_path (dev_handle);
  if (!dp)
    goto fail;
  image_handle = grub_efi_bootdisk (dp, filename);
  if (! image_handle)
    goto fail;

  grub_script_execute_sourcecode ("terminal_output console");
  status = efi_call_3 (b->start_image, image_handle, 0, NULL);
  grub_printf ("StartImage returned 0x%lx\n", (unsigned long) status);
  status = efi_call_1 (b->unload_image, image_handle);
fail:
  if (disk)
    grub_disk_close (disk);
  return GRUB_ERR_NONE;
}

static grub_extcmd_t cmd_elt;

#endif

static grub_extcmd_t cmd_dp;

static int
lua_efi_vendor (lua_State *state)
{
  char *vendor;
  grub_uint16_t *vendor16, *fv = grub_efi_system_table->firmware_vendor;

  for (vendor16 = fv; *vendor16; vendor16++)
    ;
  vendor = grub_malloc (4 * (vendor16 - fv + 1));
  if (!vendor)
    return 0;
  *grub_utf16_to_utf8 ((grub_uint8_t *) vendor, fv, vendor16 - fv) = 0;
  lua_pushstring (state, vendor);
  grub_free (vendor);
  return 1;
}

static int
lua_efi_version (lua_State *state)
{
  char uefi_ver[11];
  grub_efi_uint16_t uefi_major_rev =
            grub_efi_system_table->hdr.revision >> 16;
  grub_efi_uint16_t uefi_minor_rev =
            grub_efi_system_table->hdr.revision & 0xffff;
  grub_efi_uint8_t uefi_minor_1 = uefi_minor_rev / 10;
  grub_efi_uint8_t uefi_minor_2 = uefi_minor_rev % 10;
  grub_snprintf (uefi_ver, 10, "%d.%d", uefi_major_rev, uefi_minor_1);
  if (uefi_minor_2)
    grub_snprintf (uefi_ver, 10, "%s.%d", uefi_ver, uefi_minor_2);
  lua_pushstring (state, uefi_ver);
  return 1;
}

static int
lua_efi_getdp (lua_State *state)
{
  grub_disk_t disk;
  grub_efi_handle_t handle = 0;
  grub_efi_device_path_t *dp = NULL;
  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  disk = lua_touserdata (state, 1);
  handle = grub_efidisk_get_device_handle (disk);
  if (!handle)
    return 0;
  dp = grub_efi_get_device_path (handle);
  if (!dp)
    return 0;
  lua_pushlightuserdata (state, dp);
  return 1;
}

static int
lua_efi_dptostr (lua_State *state)
{
  char *str = NULL;
  grub_efi_device_path_t *dp;
  luaL_checktype (state, 1, LUA_TLIGHTUSERDATA);
  dp = lua_touserdata (state, 1);
  str = grub_efi_device_path_to_str (dp);
  if (!str)
    return 0;
  lua_pushstring (state, str);
  grub_free (str);
  return 1;
}

static luaL_Reg efilib[] =
{
  {"vendor", lua_efi_vendor},
  {"version", lua_efi_version},
  {"getdp", lua_efi_getdp},
  {"dptostr", lua_efi_dptostr},
  {0, 0}
};

GRUB_MOD_INIT(dp)
{
  cmd_dp = grub_register_extcmd ("dp", grub_cmd_dp, 0, N_("DEVICE"),
                  N_("DevicePath."), 0);
#if defined (__i386__) || defined (__x86_64__)
  cmd_elt = grub_register_extcmd ("cdboot", grub_cmd_elt, 0, N_("DEVICE [FILE]"),
                  N_("Eltorito BOOT."), 0);
#endif
  if (grub_lua_global_state)
  {
    lua_gc (grub_lua_global_state, LUA_GCSTOP, 0);
    luaL_register (grub_lua_global_state, "efi", efilib);
    lua_gc (grub_lua_global_state, LUA_GCRESTART, 0);
  }
}

GRUB_MOD_FINI(dp)
{
  grub_unregister_extcmd (cmd_dp);
#if defined (__i386__) || defined (__x86_64__)
  grub_unregister_extcmd (cmd_elt);
#endif
}
