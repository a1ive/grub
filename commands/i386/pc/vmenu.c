/*
 *  BURG - Brand-new Universal loadeR from GRUB
 *  Copyright 2009 Bean Lee - All Rights Reserved
 *
 *  BURG is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  BURG is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with BURG.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/env.h>
#include <grub/misc.h>
#include <grub/machine/init.h>
#include <grub/machine/vbe.h>
#include <grub/mm.h>
#include <grub/command.h>
#include <grub/i18n.h>
#include <grub/uitree.h>

static void *
real2pm (grub_vbe_farptr_t ptr)
{
  return (void *) ((((unsigned long) ptr & 0xFFFF0000) >> 12UL)
		   + ((unsigned long) ptr & 0x0000FFFF));
}

#define PREFIX_STR	"set gfxmode="
#define PREFIX_SIZE	(sizeof (PREFIX_STR) - 1)

static grub_err_t
grub_cmd_vmenu (grub_command_t cmd __attribute__ ((unused)),
		int argc, char **args)
{
  struct grub_vbe_info_block controller_info;
  struct grub_vbe_mode_info_block mode_info_tmp;
  grub_uint16_t *video_mode_list;
  grub_uint16_t *p;
  grub_uint16_t *saved_video_mode_list;
  grub_size_t video_mode_list_size;
  grub_err_t err;
  char *section_name;
  grub_uitree_t root;
  char buf[24];
  char *res_buf;

  if (argc < 1)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "section name required");

  section_name = args[0];

  err = grub_vbe_probe (&controller_info);
  if (err != GRUB_ERR_NONE)
    return err;

  /* Because the information on video modes is stored in a temporary place,
     it is better to copy it to somewhere safe.  */
  p = video_mode_list = real2pm (controller_info.video_mode_ptr);
  while (*p++ != 0xFFFF)
    ;

  video_mode_list_size = (grub_addr_t) p - (grub_addr_t) video_mode_list;
  saved_video_mode_list = grub_malloc (video_mode_list_size);
  if (! saved_video_mode_list)
    return grub_errno;

  grub_memcpy (saved_video_mode_list, video_mode_list, video_mode_list_size);

  root = grub_uitree_find (&grub_uitree_root, section_name);
  if (root)
    {
      grub_tree_remove_node (GRUB_AS_TREE (root));
      grub_uitree_free (root);
    }

  root = grub_uitree_create_node (section_name);
  if (! root)
    return grub_errno;
  grub_tree_add_child (GRUB_AS_TREE (&grub_uitree_root), GRUB_AS_TREE (root),
		       -1);

  grub_strcpy (buf, PREFIX_STR);
  res_buf = buf + PREFIX_SIZE;
  /* Walk through all video modes listed.  */
  for (p = saved_video_mode_list; *p != 0xFFFF; p++)
    {
      grub_uint32_t mode = (grub_uint32_t) *p;
      grub_uitree_t node;

      err = grub_vbe_get_video_mode_info (mode, &mode_info_tmp);
      if (err != GRUB_ERR_NONE)
	{
	  grub_errno = GRUB_ERR_NONE;
	  continue;
	}

      if ((mode_info_tmp.mode_attributes & GRUB_VBE_MODEATTR_SUPPORTED) == 0)
	/* If not available, skip it.  */
	continue;

      if ((mode_info_tmp.mode_attributes & GRUB_VBE_MODEATTR_RESERVED_1) == 0)
	/* Not enough information.  */
	continue;

      if ((mode_info_tmp.mode_attributes & GRUB_VBE_MODEATTR_COLOR) == 0)
	/* Monochrome is unusable.  */
	continue;

      if ((mode_info_tmp.mode_attributes & GRUB_VBE_MODEATTR_LFB_AVAIL) == 0)
	/* We support only linear frame buffer modes.  */
	continue;

      if ((mode_info_tmp.mode_attributes & GRUB_VBE_MODEATTR_GRAPHICS) == 0)
	/* We allow only graphical modes.  */
	continue;

      if ((mode_info_tmp.memory_model != GRUB_VBE_MEMORY_MODEL_PACKED_PIXEL) &&
	  (mode_info_tmp.memory_model != GRUB_VBE_MEMORY_MODEL_DIRECT_COLOR))
	continue;

      grub_snprintf (res_buf, sizeof (buf) - PREFIX_SIZE, "%dx%d",
		     mode_info_tmp.x_resolution, mode_info_tmp.y_resolution);

      node = grub_uitree_find (root, res_buf);
      if (node)
	continue;

      node = grub_uitree_create_node (res_buf);
      if (! root)
	return grub_errno;

      grub_uitree_set_prop (node, "command", buf);
      grub_tree_add_child (GRUB_AS_TREE (root), GRUB_AS_TREE (node), -1);
    }

  grub_free (saved_video_mode_list);

  return 0;
}

static grub_command_t cmd;

GRUB_MOD_INIT(vmenu)
{
  cmd = grub_register_command ("vmenu", grub_cmd_vmenu,
			       N_("SECTION_NAME"),
			       N_("Generate video mode menu."));
}

GRUB_MOD_FINI(vmenu)
{
  grub_unregister_command (cmd);
}
