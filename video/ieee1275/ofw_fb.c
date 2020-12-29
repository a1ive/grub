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

#define grub_video_render_target grub_video_fbrender_target

#include <grub/err.h>
#include <grub/types.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/video.h>
#include <grub/video_fb.h>
#include <grub/ieee1275/ieee1275.h>

#define SCREEN_DEVICE_NAME	"screen"

static struct
{
  struct grub_video_mode_info mode_info;
  struct grub_video_render_target *render_target;
  grub_uint8_t *ptr;
} framebuffer;

static grub_uint32_t fb_addr;
static grub_uint32_t fb_width;
static grub_uint32_t fb_height;
static grub_uint32_t fb_depth;
static grub_uint32_t fb_pitch;

static int
check_device (void)
{
  grub_ieee1275_phandle_t dev;

  if ((! grub_ieee1275_finddevice (SCREEN_DEVICE_NAME, &dev)) &&
      (! grub_ieee1275_get_integer_property (dev, "width", &fb_width,
					     sizeof fb_width, 0)) &&
      (! grub_ieee1275_get_integer_property (dev, "height", &fb_height,
					     sizeof fb_height, 0)) &&
      (! grub_ieee1275_get_integer_property (dev, "depth", &fb_depth,
					     sizeof fb_depth, 0)) &&
      (! grub_ieee1275_get_integer_property (dev, "linebytes", &fb_pitch,
					     sizeof fb_pitch, 0)) &&
      (! grub_ieee1275_get_integer_property (dev, "address", &fb_addr,
					     sizeof fb_addr, 0)))
    return 1;

  return 0;
}

static grub_err_t
grub_video_ofw_init (void)
{
  grub_memset (&framebuffer, 0, sizeof(framebuffer));
  return grub_video_fb_init ();
}

static grub_err_t
grub_video_ofw_set_palette (unsigned int start, unsigned int count,
			    struct grub_video_palette_data *palette_data)
{
  if (fb_depth == 8)
    {
      struct set_colors_args
      {
	struct grub_ieee1275_common_hdr common;
	grub_ieee1275_cell_t method;
	grub_ieee1275_cell_t ihandle;
	grub_ieee1275_cell_t number;
	grub_ieee1275_cell_t blue;
	grub_ieee1275_cell_t green;
	grub_ieee1275_cell_t red;
      } args;
      grub_ieee1275_ihandle_t dev_ih = 0;
      unsigned i;

      grub_ieee1275_open (SCREEN_DEVICE_NAME, &dev_ih);
      if (! dev_ih)
	return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "Can't open device");

      for (i = 0; i < count; i++)
	{
	  INIT_IEEE1275_COMMON (&args.common, "call-method", 6, 0);
	  args.method = (grub_ieee1275_cell_t) "color!";
	  args.ihandle = (grub_ieee1275_cell_t) dev_ih;
	  args.number = (grub_ieee1275_cell_t) start + i;
	  args.red = (grub_ieee1275_cell_t) palette_data[i].r;
	  args.green = (grub_ieee1275_cell_t) palette_data[i].g;
	  args.blue = (grub_ieee1275_cell_t) palette_data[i].b;

	  if (IEEE1275_CALL_ENTRY_FN (&args) == -1)
	    {
	      grub_ieee1275_close (dev_ih);
	      return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "color! fails");
	    }
	}

      grub_ieee1275_close (dev_ih);
    }

  return grub_video_fb_set_palette (start, count, palette_data);
}

static grub_err_t
grub_video_ofw_setup (unsigned int width, unsigned int height,
		      unsigned int mode_type,
		      unsigned int mode_mask __attribute__ ((unused)))
{
  unsigned int depth;

  depth = (mode_type & GRUB_VIDEO_MODE_TYPE_DEPTH_MASK)
    >> GRUB_VIDEO_MODE_TYPE_DEPTH_POS;

  if (((! width) || (width == fb_width)) &&
      ((! height) || (height == fb_height)) &&
      ((! depth) || (depth == fb_depth)))
    {
      grub_err_t err;

      framebuffer.mode_info.width = fb_width;
      framebuffer.mode_info.height = fb_height;
      framebuffer.mode_info.pitch = fb_pitch;
      framebuffer.ptr = (grub_uint8_t *) (grub_target_addr_t) fb_addr;

      framebuffer.mode_info.number_of_colors = 256;
      if (fb_depth == 8)
	{
	  framebuffer.mode_info.mode_type = GRUB_VIDEO_MODE_TYPE_INDEX_COLOR;
	  framebuffer.mode_info.bpp = 8;
	  framebuffer.mode_info.bytes_per_pixel = 1;
	}
      else if (fb_depth == 16)
	{
	  framebuffer.mode_info.mode_type = GRUB_VIDEO_MODE_TYPE_RGB;
	  framebuffer.mode_info.bpp = 16;
	  framebuffer.mode_info.bytes_per_pixel = 2;
	  framebuffer.mode_info.red_mask_size = 5;
	  framebuffer.mode_info.red_field_pos = 11;
	  framebuffer.mode_info.green_mask_size = 6;
	  framebuffer.mode_info.green_field_pos = 5;
	  framebuffer.mode_info.blue_mask_size = 5;
	  framebuffer.mode_info.blue_field_pos = 0;
	}
      else
	return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "unsupported color depth");

      if ((((mode_type & GRUB_VIDEO_MODE_TYPE_INDEX_COLOR) != 0) &&
	   (framebuffer.mode_info.mode_type !=
	    GRUB_VIDEO_MODE_TYPE_INDEX_COLOR)) ||
	  (((mode_type & GRUB_VIDEO_MODE_TYPE_RGB) != 0) &&
	   (framebuffer.mode_info.mode_type != GRUB_VIDEO_MODE_TYPE_RGB)))
	return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "color mode mismatched");

      framebuffer.mode_info.blit_format =
	grub_video_get_blit_format (&framebuffer.mode_info);

      err = grub_video_fb_create_render_target_from_pointer
	(&framebuffer.render_target,
	 &framebuffer.mode_info,
	 framebuffer.ptr);

      if (err)
	return err;

      err = grub_video_fb_set_active_render_target
	(framebuffer.render_target);

      if (err)
	return err;

      err = grub_video_ofw_set_palette (0, GRUB_VIDEO_FBSTD_NUMCOLORS,
					grub_video_fbstd_colors);

      return err;
    }

  return grub_error (GRUB_ERR_UNKNOWN_DEVICE, "no matching mode found.");
}

static grub_err_t
grub_video_ofw_swap_buffers (void)
{
  /* TODO: Implement buffer swapping.  */
  return GRUB_ERR_NONE;
}

static grub_err_t
grub_video_ofw_set_active_render_target (struct grub_video_render_target *target)
{
  if (target == GRUB_VIDEO_RENDER_TARGET_DISPLAY)
    target = framebuffer.render_target;

  return grub_video_fb_set_active_render_target (target);
}

static grub_err_t
grub_video_ofw_get_info_and_fini (struct grub_video_mode_info *mode_info,
				  void **framebuf)
{
  grub_memcpy (mode_info, &(framebuffer.mode_info), sizeof (*mode_info));
  *framebuf = (char *) framebuffer.ptr;

  grub_video_fb_fini ();

  return GRUB_ERR_NONE;
}

static struct grub_video_adapter grub_video_ofw_adapter =
  {
    .name = "OpenFirmware frame buffer driver",

    .init = grub_video_ofw_init,
    .fini = grub_video_fb_fini,
    .setup = grub_video_ofw_setup,
    .get_info = grub_video_fb_get_info,
    .get_info_and_fini = grub_video_ofw_get_info_and_fini,
    .set_palette = grub_video_ofw_set_palette,
    .get_palette = grub_video_fb_get_palette,
    .set_viewport = grub_video_fb_set_viewport,
    .get_viewport = grub_video_fb_get_viewport,
    .map_color = grub_video_fb_map_color,
    .map_rgb = grub_video_fb_map_rgb,
    .map_rgba = grub_video_fb_map_rgba,
    .unmap_color = grub_video_fb_unmap_color,
    .fill_rect = grub_video_fb_fill_rect,
    .blit_bitmap = grub_video_fb_blit_bitmap,
    .blit_render_target = grub_video_fb_blit_render_target,
    .scroll = grub_video_fb_scroll,
    .swap_buffers = grub_video_ofw_swap_buffers,
    .create_render_target = grub_video_fb_create_render_target,
    .delete_render_target = grub_video_fb_delete_render_target,
    .set_active_render_target = grub_video_ofw_set_active_render_target,
    .get_active_render_target = grub_video_fb_get_active_render_target,

    .next = 0
  };

GRUB_MOD_INIT(ofw_fb)
{
  if (check_device ())
    grub_video_register (&grub_video_ofw_adapter);
}

GRUB_MOD_FINI(ofw_fb)
{
  if (fb_addr)
    grub_video_unregister (&grub_video_ofw_adapter);
}
