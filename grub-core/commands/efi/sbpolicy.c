/* sbpolicy.c - Install override security policy. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
 *  Copyright (C) 2019  a1ive@github
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
#include <grub/efi/api.h>
#include <grub/efi/efi.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] =
  {
    {"uninstall", 'u', 0, N_("Uninstall security policy"), 0, 0},
    {0, 0, 0, 0, 0, 0}
  };

typedef grub_efi_boolean_t (*policy_function)(void *data, grub_efi_uintn_t len);

struct _efi_security2_protocol;
struct _efi_security_protocol;

typedef struct _efi_security2_protocol grub_efi_security2_protocol;
typedef struct _efi_security_protocol grub_efi_security_protocol;

typedef grub_efi_status_t (*efi_security2_file_authentication) (
            const grub_efi_security2_protocol *this,
            const grub_efi_device_path_protocol_t *device_path,
            void *file_buffer,
            grub_efi_uintn_t file_size,
            grub_efi_boolean_t  boot_policy
                                     );
typedef grub_efi_status_t (*efi_security_file_authentication_state) (
            const grub_efi_security_protocol *this,
            grub_efi_uint32_t authentication_status,
            const grub_efi_device_path_protocol_t *file
                                     );

struct _efi_security2_protocol {
  efi_security2_file_authentication file_authentication;
};
struct _efi_security_protocol {
  efi_security_file_authentication_state  file_authentication_state;
};

static grub_efi_boolean_t 
security_policy_mok_override(void) {
  return 1;
}

static grub_efi_boolean_t 
security_policy_mok_deny(void *data __attribute__ ((unused)), grub_efi_uintn_t len __attribute__ ((unused))) {
  return 0;
}

static grub_efi_boolean_t 
security_policy_mok_allow(void *data __attribute__ ((unused)), grub_efi_uintn_t len __attribute__ ((unused))) {
  return 1;
}

static efi_security2_file_authentication es2fa = NULL;
static efi_security_file_authentication_state esfas = NULL;

static grub_efi_boolean_t(*sp_override)(void) = NULL;
static policy_function sp_allow = NULL;
static policy_function sp_deny = NULL;

static grub_efi_status_t
security2_policy_authentication (
    const grub_efi_security2_protocol *this,
    const grub_efi_device_path_protocol_t *device_path,
    void *file_buffer,
    grub_efi_uintn_t file_size,
    grub_efi_boolean_t  boot_policy
                 )
{
  grub_efi_status_t status;  

  if (sp_override && sp_override())
    return GRUB_EFI_SUCCESS;

  /* if policy would deny, fail now  */
  if (sp_deny && sp_deny(file_buffer, file_size))
    return GRUB_EFI_SECURITY_VIOLATION;

  /* Chain original security policy */
  status = es2fa(this, device_path, file_buffer, file_size, boot_policy);

  /* if OK, don't bother with allow check */
  if (status == GRUB_EFI_SUCCESS)
    return status;

  if (sp_allow && sp_allow(file_buffer, file_size))
    return GRUB_EFI_SUCCESS;

  return status;
}

static grub_efi_status_t
security_policy_authentication (
    const grub_efi_security_protocol *this,
    grub_efi_uint32_t authentication_status,
    const grub_efi_device_path_protocol_t *device_path_const
    )
{
  grub_efi_status_t status, fail_status;
  grub_efi_device_path_t *dev_path __attribute__ ((unused))
      = grub_efi_duplicate_device_path((grub_efi_device_path_t *)device_path_const);

  void *file_buffer = NULL;
  grub_efi_uintn_t file_size = 0;

  if (sp_override && sp_override())
    return GRUB_EFI_SUCCESS;

  /* Chain original security policy */
  status = esfas(this, authentication_status, device_path_const);

  /* capture failure status */
  fail_status = status;

  status = GRUB_EFI_SECURITY_VIOLATION;
  if (sp_deny && sp_deny(file_buffer, file_size))
    goto out;

  status = fail_status;
  if (status == GRUB_EFI_SUCCESS)
    goto out;

  /* fail status is platform security failure now */

  if (sp_allow && sp_allow(file_buffer, file_size))
    status = GRUB_EFI_SUCCESS;

out:
  return status;
}

static grub_efi_status_t
security_policy_install(grub_efi_boolean_t (*override)(void), policy_function allow, policy_function deny)
{
  grub_efi_security2_protocol *security2_protocol = NULL;
  grub_efi_security_protocol *security_protocol;
  grub_efi_guid_t guid2 = GRUB_EFI_SECURITY2_PROTOCOL_GUID;
  grub_efi_guid_t guid = GRUB_EFI_SECURITY_PROTOCOL_GUID;

  sp_override = override;
  sp_allow = allow;
  sp_deny = deny;

  /* Don't bother with status here.  The call is allowed
   * to fail, since SECURITY2 was introduced in PI 1.2.1
   * If it fails, use security2_protocol == NULL as indicator */
  grub_printf ("Locate: EFI_SECURITY2_PROTOCOL\n");
    
  security2_protocol = grub_efi_locate_protocol (&guid2, NULL);

  if (security2_protocol)
  {
    grub_printf ("Try: EFI_SECURITY2_PROTOCOL\n");
    es2fa = security2_protocol->file_authentication;
    security2_protocol->file_authentication = 
        security2_policy_authentication;
    /* check for security policy in write protected memory */
    if (security2_protocol->file_authentication
        !=  security2_policy_authentication)
        return GRUB_EFI_ACCESS_DENIED;
    grub_printf ("OK: EFI_SECURITY2_PROTOCOL\n");
  }
  else
    grub_printf ("EFI_SECURITY2_PROTOCOL not found\n");
    
  grub_printf ("Locate: EFI_SECURITY_PROTOCOL\n");
  security_protocol = grub_efi_locate_protocol (&guid, NULL);
  if (!security_protocol)
  {
    /* This one is mandatory, so there's a serious problem */
    grub_printf ("EFI_SECURITY_PROTOCOL not found\n");
    return GRUB_EFI_NOT_FOUND;
  }
  
  grub_printf ("Try: EFI_SECURITY_PROTOCOL\n");
  esfas = security_protocol->file_authentication_state;
  security_protocol->file_authentication_state =
    security_policy_authentication;
  /* check for security policy in write protected memory */
  if (security_protocol->file_authentication_state
    !=  security_policy_authentication)
    return GRUB_EFI_ACCESS_DENIED;
  grub_printf ("OK: EFI_SECURITY_PROTOCOL\n");

  return GRUB_EFI_SUCCESS;
}

static grub_efi_status_t
security_policy_uninstall(void)
{
  grub_efi_guid_t guid2 = GRUB_EFI_SECURITY2_PROTOCOL_GUID;
  grub_efi_guid_t guid = GRUB_EFI_SECURITY_PROTOCOL_GUID;

  if (esfas)
  {
    grub_printf ("Uninstall: EFI_SECURITY_PROTOCOL\n");
    grub_efi_security_protocol *security_protocol;
    security_protocol = grub_efi_locate_protocol (&guid, NULL);
    if (!security_protocol)
      return GRUB_EFI_NOT_FOUND;
    security_protocol->file_authentication_state = esfas;
    esfas = NULL;
    grub_printf ("OK: EFI_SECURITY_PROTOCOL\n");
  }
  else
    grub_printf ("Skip: EFI_SECURITY_PROTOCOL\n");

  if (es2fa)
  {
    grub_printf ("Uninstall: EFI_SECURITY2_PROTOCOL\n");
    grub_efi_security2_protocol *security2_protocol;
    security2_protocol = grub_efi_locate_protocol (&guid2, NULL);
    if (!security2_protocol)
      return GRUB_EFI_NOT_FOUND;
    security2_protocol->file_authentication = es2fa;
    es2fa = NULL;
    grub_printf ("OK: EFI_SECURITY2_PROTOCOL\n");
  }
  else
      grub_printf ("Skip: EFI_SECURITY2_PROTOCOL\n");
  return GRUB_EFI_SUCCESS;
}


static grub_err_t
grub_cmd_sbpolicy (grub_extcmd_context_t ctxt,
        int argc __attribute__ ((unused)),
        char **args __attribute__ ((unused)))
{
  struct grub_arg_list *state = ctxt->state;
  grub_uint8_t secure_boot;
  grub_efi_status_t status;
  grub_size_t datasize = 0;
  char *data = NULL;
  grub_efi_guid_t global = GRUB_EFI_GLOBAL_VARIABLE_GUID;

  data = grub_efi_get_variable ("SecureBoot", &global,
                          &datasize);

  if (!data || !datasize)
    {
      grub_printf ("Not a Secure Boot Platform\n");
      grub_errno = GRUB_ERR_NONE;
      goto done;
    }

  secure_boot = *((grub_uint8_t *)data);
  if (secure_boot)
    {
      grub_printf ("SecureBoot Enabled\n");
      if (state[0].set)
      {
        status = security_policy_uninstall();
        if (status != GRUB_EFI_SUCCESS)
        {
          grub_error(GRUB_ERR_BAD_ARGUMENT,N_("Failed to uninstall security policy"));
          goto done;
        }
      }
      else
      {
        status = security_policy_install(security_policy_mok_override,
                     security_policy_mok_allow,
                     security_policy_mok_deny);
        if (status != GRUB_EFI_SUCCESS)
        {
          grub_error(GRUB_ERR_BAD_ARGUMENT,N_("Failed to install override security policy"));
          goto done;
        }
      }
    }
  else
    grub_printf ("SecureBoot Disabled\n");

  grub_errno = GRUB_ERR_NONE;

done:
  grub_free (data);
  return grub_errno;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(sbpolicy)
{
  cmd = grub_register_extcmd ("sbpolicy", grub_cmd_sbpolicy, 0, 0,
                  N_("Install override security policy."), options);
}

GRUB_MOD_FINI(sbpolicy)
{
  grub_unregister_extcmd (cmd);
}
