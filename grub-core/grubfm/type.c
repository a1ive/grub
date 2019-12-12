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
#include <grub/term.h>
 
#include "fm.h"

struct grubfm_file_ext grubfm_file_table[] =
{
  {".iso", "iso", ISO},
  {".isz", "iso", UNKNOWN},
  {".img", "img", DISK},
  {".ima", "img", DISK},
  {".vhd", "img", VHD},
  {".vhdx", "img", VHD},
  {".vmdk", "img", UNKNOWN}, /* TODO: add support for vmdk */
  {".fba", "img", FBA},
  {".jpg", "png", IMAGE},
  {".png", "png", IMAGE},
  {".tga", "png", IMAGE},
  {".bmp", "png", UNKNOWN},
  {".gif", "png", UNKNOWN},
  {".efi", "exe", EFI},
  {".lua", "lua", LUA},
  {".7z" , "7z" , UNKNOWN},
  {".rar", "7z" , UNKNOWN},
  {".zip", "7z" , UNKNOWN},
  {".lz" , "7z" , TAR},
  {".tar", "7z" , TAR},
  {".xz" , "7z" , TAR},
  {".gz" , "7z" , TAR},
  {".wim", "wim", WIM},
  {".is_", "wim", NT5},
  {".im_", "wim", NT5},
  {".exe", "exe", UNKNOWN},
  {".cfg", "cfg", CFG},
  {".pf2", "pf2", FONT},
  {".mod", "mod", MOD},
  {".mbr", "bin", MBR},
  {".nsh", "sh" , NSH},
  {".sh" , "sh" , UNKNOWN},
  {".bat", "sh" , UNKNOWN},
  {".lst", "cfg", LST},
  {".ipxe", "net", IPXE},
  {".c"  , "c"  , UNKNOWN},
  {".h"  , "c"  , UNKNOWN},
  {".cpp", "c"  , UNKNOWN},
  {".hpp", "c"  , UNKNOWN},
  {".mp3", "mp3", UNKNOWN},
  {".mp4", "mp4", UNKNOWN},
  {".flv", "mp4", UNKNOWN},
  {".doc", "doc", UNKNOWN},
  {".docx", "doc", UNKNOWN},
  {".wps", "doc", UNKNOWN},
  {".ppt", "doc", UNKNOWN},
  {".pptx", "doc", UNKNOWN},
  {".xls", "doc", UNKNOWN},
  {".xlsx", "doc", UNKNOWN},
  {".txt", "txt", UNKNOWN},
  {".ini", "txt", UNKNOWN},
  {".log", "txt", UNKNOWN},
  {".crt", "crt", UNKNOWN},
  {".cer", "crt", UNKNOWN},
  {".der", "crt", UNKNOWN},
  {".py" , "py" , PYTHON},
  {".pyc", "py" , PYTHON},
};

static char defaule_icon[] = "file";

char *
grubfm_get_file_type (struct grubfm_enum_file_info *info)
{
  char *icon = defaule_icon;
  if (!info || !info->name)
    goto ret;
  info->type = UNKNOWN;
  char *ext = grub_strrchr (info->name, '.');
  if (!ext)
    goto ret;
  int i;
  int n = sizeof (grubfm_file_table) / sizeof (struct grubfm_file_ext);
  for (i = 0; i < n; i++)
  {
    if (grub_strcmp (ext, grubfm_file_table[i].ext) == 0)
    {
      icon = grubfm_file_table[i].icon;
      info->type = grubfm_file_table[i].type;
      break;
    }
  }
ret:
  return icon;
}
