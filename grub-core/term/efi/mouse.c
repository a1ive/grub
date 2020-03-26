/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2006,2007,2008  Free Software Foundation, Inc.
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

#include <grub/term.h>
#include <grub/misc.h>
#include <grub/types.h>
#include <grub/err.h>
#include <grub/efi/efi.h>
#include <grub/efi/api.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define GRUB_EFI_SIMPLE_POINTER_GUID  \
  { 0x31878c87, 0x0b75, 0x11d5, \
    { 0x9a, 0x4f, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d } \
  }

typedef struct
{
  grub_efi_int32_t x;
  grub_efi_int32_t y;
  grub_efi_int32_t z;
  grub_efi_boolean_t left;
  grub_efi_boolean_t right;
} grub_efi_mouse_state;

static grub_efi_mouse_state last;

typedef struct
{
  grub_efi_uint64_t x;
  grub_efi_uint64_t y;
  grub_efi_uint64_t z;
  grub_efi_boolean_t left;
  grub_efi_boolean_t right;
} grub_efi_mouse_mode;

struct grub_efi_simple_pointer_protocol
{
  grub_efi_status_t (*reset) (struct grub_efi_simple_pointer_protocol *this,
                              grub_efi_boolean_t *extended_verification);
  grub_efi_status_t (*get_state) (struct grub_efi_simple_pointer_protocol *this,
                                  grub_efi_mouse_state *state);
  grub_efi_event_t *wait_for_input;
  grub_efi_mouse_mode *mode;
};
typedef struct grub_efi_simple_pointer_protocol grub_efi_simple_pointer_protocol_t;

static grub_err_t
grub_efi_mouse_input_init (struct grub_term_input *term)
{
  grub_efi_status_t status;
  grub_efi_guid_t mouse_guid = GRUB_EFI_SIMPLE_POINTER_GUID;
  grub_efi_simple_pointer_protocol_t *mouse_input = term->data;
  if (mouse_input)
    return 0;
  status = efi_call_3 (grub_efi_system_table->boot_services->locate_protocol,
                       &mouse_guid, NULL, &mouse_input);
  if (status != GRUB_EFI_SUCCESS || !mouse_input)
  {
    grub_printf ("ERROR: SimplePointerProtocol not found.\n");
    return GRUB_ERR_BAD_OS;
  }
  status = efi_call_2 (mouse_input->reset, mouse_input, TRUE);
  if (status != GRUB_EFI_SUCCESS)
  {
    grub_printf ("ERROR: SimplePointerProtocol reset failed.\n");
    return GRUB_ERR_BAD_OS;
  }
  term->data = (void *)mouse_input;

  return 0;
}

static int
grub_mouse_getkey (struct grub_term_input *term)
{
  grub_efi_mouse_state cur;
  grub_efi_simple_pointer_protocol_t *mouse = term->data;
  //int x;
  int y;
  if (!mouse)
    return GRUB_TERM_NO_KEY;
  efi_call_2 (mouse->get_state, mouse, &cur);
  if (grub_memcmp (&cur, &last, sizeof (grub_efi_mouse_state)) != 0)
  {
    //x = (int) grub_divmod64 (cur.x, mouse->mode->x, NULL);
    y = (int) grub_divmod64 (cur.y, mouse->mode->y, NULL);
    if (cur.left)
      return 0x0d;
    if (cur.right)
      return GRUB_TERM_ESC;
    if (y > 5)
      return GRUB_TERM_KEY_DOWN;
    if (y < -5)
      return GRUB_TERM_KEY_UP;
  }
  return GRUB_TERM_NO_KEY;
}

static struct grub_term_input grub_mouse_term_input =
{
  .name = "mouse",
  .getkey = grub_mouse_getkey,
  .init = grub_efi_mouse_input_init,
};

GRUB_MOD_INIT(efi_mouse)
{
  grub_term_register_input ("mouse", &grub_mouse_term_input);
}

GRUB_MOD_FINI(efi_mouse)
{
  grub_term_unregister_input (&grub_mouse_term_input);
}
