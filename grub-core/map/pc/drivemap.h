#ifndef _DRIVEMAP_HEADER
#define _DRIVEMAP_HEADER

#include <grub/types.h>
#include <grub/extcmd.h>

grub_err_t
grub_pcbios_drivemap_cmd (struct grub_extcmd_context *ctxt,
                          int argc, char **args);

#endif
