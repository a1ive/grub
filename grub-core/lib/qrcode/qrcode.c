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
 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/term.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include "qr_encode.h"

GRUB_MOD_LICENSE ("GPLv3+");
GRUB_MOD_DUAL_LICENSE("MIT");

static const struct grub_arg_option options[] =
  {
    
    {0, 'l', 0, N_("QR LEVEL L"), 0, 0},
    {0, 'm', 0, N_("QR LEVEL M"), 0, 0},
    {0, 'q', 0, N_("QR LEVEL Q"), 0, 0},
    {0, 'h', 0, N_("QR LEVEL H"), 0, 0},
    {0, 0, 0, 0, 0, 0}
  };

static grub_err_t
grub_cmd_qrcode (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  int side, i, j, a, ecclevel;
  uint8_t bitdata[QR_MAX_BITDATA];

  ecclevel = QR_LEVEL_H;
  if (state[0].set)
    ecclevel = QR_LEVEL_L;
  if (state[1].set)
    ecclevel = QR_LEVEL_M;
  if (state[2].set)
    ecclevel = QR_LEVEL_Q;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("string expected"));

  if (strlen(args[0]) > 2047)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("string too long"));

  // remove newline
  if (args[0][strlen(args[0]) - 1] == '\n') {
    args[0][strlen(args[0]) - 1] = 0;
  }

  side = qr_encode(ecclevel, 0, args[0], 0, bitdata);

  grub_dprintf("qrcode", "side: %d\n", side);

  for (i = 0; i < side + 2; i++)
    grub_printf("██");
  grub_printf("\n");
  for (i = 0; i < side; i++) {
    grub_printf("██");
    for (j = 0; j < side; j++) {
      a = i * side + j;
      grub_printf((bitdata[a / 8] & (1 << (7 - a % 8))) ? "  " : "██");
    }
  	grub_printf("██");
  	grub_printf("\n");
  }
  for (i = 0; i < side + 2; i++)
      grub_printf("██");
  grub_printf("\n");

  return 0;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(qrcode)
{
  cmd = grub_register_extcmd ("qrcode", grub_cmd_qrcode,0,
			       N_("[OPTIONS] STRING"),
			       N_("Generate QR Code. "),
			       options);
}

GRUB_MOD_FINI(qrcode)
{
  grub_unregister_extcmd (cmd);
}