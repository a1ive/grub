 /*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019,2020  Free Software Foundation, Inc.
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
 *
 */

#include <grub/types.h>
#include <grub/misc.h>
#include <grub/err.h>
#include <grub/i386/pc/int.h>

#include <misc.h>
#include <vfat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <wimboot.h>
#include <bootapp.h>
#include <int13.h>

static grub_uint8_t cmdline_gui;

static inline void
grub_interrupt_wrapper (struct bootapp_callback_params *params)
{
  grub_uint8_t intno;
  struct grub_bios_int_registers regs;
  intno = params->vector.interrupt;
  regs.flags = GRUB_CPU_INT_FLAGS_DEFAULT;
  regs.es = params->es;
  regs.ds = params->ds;
  regs.eax = params->eax;
  regs.ebx = params->ebx;
  regs.ecx = params->ecx;
  regs.edx = params->edx;
  regs.edi = params->edi;
  regs.esi = params->esi;
  grub_bios_interrupt (intno, &regs);
  params->es = regs.es;
  params->ds = regs.ds;
  params->eax = regs.eax;
  params->ebx = regs.ebx;
  params->ecx = regs.ecx;
  params->edx = regs.edx;
  params->edi = regs.edi;
  params->esi = regs.esi;
  params->eflags = regs.flags;
}

static void
call_interrupt_wrapper (struct bootapp_callback_params *params)
{
  uint16_t *attributes;
  printf ("call interrupt 0x%x\n", params->vector.interrupt);
  /* Handle/modify/pass-through interrupt as required */
  if (params->vector.interrupt == 0x13)
  {
    /* Intercept INT 13 calls for the emulated drive */
    emulate_int13 (params);
  }
  else if ((params->vector.interrupt == 0x10) &&
           (params->ax == 0x4f01) && (!cmdline_gui))
  {
    /* Mark all VESA video modes as unsupported */
    attributes = REAL_PTR (params->es, params->di);
    grub_interrupt_wrapper (params);
    *attributes &= ~0x0001;
  }
  else
  {
    /* Pass through interrupt */
    grub_interrupt_wrapper (params);
  }
}

static void
call_real_wrapper (struct bootapp_callback_params *params)
{
  struct grub_bios_int_registers regs;
  regs.flags = GRUB_CPU_INT_FLAGS_DEFAULT;
  regs.es = params->es;
  regs.ds = params->ds;
  regs.eax = params->eax;
  regs.ebx = params->ebx;
  regs.ecx = params->ecx;
  regs.edx = params->edx;
  regs.edi = params->edi;
  regs.esi = params->esi;
  printf ("call rell function %04x:%04x\n",
          params->vector.function.offset, params->vector.function.segment);
  grub_bios_call_real (params->vector.function.offset,
                       &regs, params->vector.function.segment);
  params->es = regs.es;
  params->ds = regs.ds;
  params->eax = regs.eax;
  params->ebx = regs.ebx;
  params->ecx = regs.ecx;
  params->edx = regs.edx;
  params->edi = regs.edi;
  params->esi = regs.esi;
  params->eflags = regs.flags;
}

/** Real-mode callback functions */
static struct bootapp_callback_functions callback_fns =
{
  .call_interrupt = call_interrupt_wrapper,
  .call_real = call_real_wrapper,
};

/** Real-mode callbacks */
struct bootapp_callback callback =
{
  .fns = &callback_fns,
};

grub_err_t grub_wimboot_install (struct wimboot_cmdline *cmd)
{
  /* Add INT 13 drive */
  cmdline_gui = cmd->gui;
  callback.drive = initialise_int13 ();
  return GRUB_ERR_NONE;
}
