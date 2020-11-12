/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009,2020  Free Software Foundation, Inc.
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

#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/efi/api.h>
#include <grub/efi/efi.h>
#include <grub/efi/graphics_output.h>
#include <grub/efi/uga_draw.h>

#include "bios.h"

static video_display_info bios_display_info;

grub_efi_status_t
bios_display_init (void)
{
  grub_efi_status_t status;
  grub_efi_uint32_t tmp1, tmp2;
  grub_efi_boot_services_t *b = grub_efi_system_table->boot_services;
  grub_efi_guid_t gop_guid = GRUB_EFI_GOP_GUID;
  grub_efi_guid_t uga_guid = GRUB_EFI_UGA_DRAW_GUID;

  grub_memset (&bios_display_info, 0, sizeof(video_display_info));

  status = efi_call_3 (b->handle_protocol,
                       grub_efi_system_table->console_out_handler,
                       &gop_guid, (void **)&bios_display_info.gop);
  if (status != GRUB_EFI_SUCCESS)
    status = efi_call_3 (b->locate_protocol, &gop_guid,
                         NULL, (void **)&bios_display_info.gop);

  if (status == GRUB_EFI_SUCCESS)
  {
    grub_printf ("Found a GOP display adapter\n");

    bios_display_info.w = bios_display_info.gop->mode->info->width;
    bios_display_info.h = bios_display_info.gop->mode->info->height;
    bios_display_info.pixel_format
            = bios_display_info.gop->mode->info->pixel_format;
    bios_display_info.pixels_per_scanline
            = bios_display_info.gop->mode->info->pixels_per_scanline;
    bios_display_info.fb_base = bios_display_info.gop->mode->fb_base;
    // usually = pixels_per_scanline * height * BytesPerPixel
    // for MacBookAir7,2: 1536 * 900 * 4 = 5,529,600 bytes
    bios_display_info.fb_size = bios_display_info.gop->mode->fb_size;

    bios_display_info.type = VIDEO_GOP;
    bios_display_info.found = TRUE;
    goto exit;
  }
  else
    grub_printf ("GOP display adapter not found\n");

  bios_display_info.gop = NULL;
  status = efi_call_3 (b->handle_protocol,
                       grub_efi_system_table->console_out_handler,
                       &uga_guid, (void **)&bios_display_info.uga);
  if (status != GRUB_EFI_SUCCESS)
    status = efi_call_3 (b->locate_protocol, &uga_guid,
                         NULL, (void **)&bios_display_info.uga);

  if (status == GRUB_EFI_SUCCESS)
  {
    grub_printf ("Found a UGA display adapter\n");
    status = efi_call_5 (bios_display_info.uga->get_mode, bios_display_info.uga,
                         &bios_display_info.w, &bios_display_info.h, &tmp1, &tmp2);
    if (status != GRUB_EFI_SUCCESS)
    {
      grub_error (GRUB_ERR_BAD_OS, "unable to get current UGA mode");
      bios_display_info.uga = NULL;
      goto exit;
    }
    else
      grub_printf ("Received current UGA mode information\n");

    bios_display_info.pixel_format = GRUB_EFI_GOT_BGRA8; // default for UGA
    // TODO: find framebuffer base
    // TODO: find scanline length
    // https://github.com/coreos/grub/blob/master/grub-core%2Fvideo%2Fefi_uga.c
    bios_display_info.type = VIDEO_UGA;
    bios_display_info.found = TRUE;
  }
  else
    grub_printf ("UGA display adapter not found\n");

exit:
  if (!bios_display_info.found)
    grub_error (GRUB_ERR_BAD_OS, "no display adapters found");

  bios_display_info.fini = TRUE;
  return status;
}

static grub_efi_boolean_t
check_display (void)
{
  if (!bios_display_info.fini)
    bios_display_init ();

  return bios_display_info.found &&
         bios_display_info.type != VIDEO_NONE ? TRUE : FALSE;
}

grub_efi_status_t
bios_switch_video_mode (grub_efi_uintn_t width, grub_efi_uintn_t height)
{
  grub_efi_status_t status = GRUB_EFI_DEVICE_ERROR;
  grub_efi_uint32_t max_mode;
  grub_efi_uint32_t i;
  struct grub_efi_gop_mode_info *mode_info;
  grub_efi_uintn_t size_of_info;
  grub_efi_boolean_t match_found = FALSE;

  if (!check_display ())
  {
    grub_printf ("No display adapters found, unable to switch video mode.\n");
    return GRUB_EFI_DEVICE_ERROR;
  }

  if (bios_display_info.type != VIDEO_GOP)
  {
    grub_printf ("Video mode switching is not supported on UGA display adapters.\n");
    return GRUB_EFI_UNSUPPORTED;
  }

  // Try to switch to a desired resolution
  max_mode = bios_display_info.gop->mode->max_mode;
  for (i = 0; i < max_mode; i++)
  {
    status = efi_call_4 (bios_display_info.gop->query_mode,
                         bios_display_info.gop, i, &size_of_info, &mode_info);
    if (status == GRUB_EFI_SUCCESS)
    {
      if (mode_info->width == width && mode_info->height == height)
      {
        if (mode_info->pixel_format == GRUB_EFI_GOT_BGRA8 ||
            mode_info->pixel_format == GRUB_EFI_GOT_RGBA8)
        {
          match_found = TRUE;
          status = efi_call_2 (bios_display_info.gop->set_mode,
                               bios_display_info.gop, i);
          if (status != GRUB_EFI_SUCCESS)
            grub_error (GRUB_ERR_BAD_OS,
                        "Failed to switch to Mode %u %ux%u",
                        i, (unsigned)width, (unsigned)height);
          else
          {
            grub_printf ("Set mode %u %ux%u.\n", i,
                         (unsigned)width, (unsigned)height);
            break;
          }
        }
      }
    }
  }

  // Refresh bios_display_info
  bios_display_info.w = bios_display_info.gop->mode->info->width;
  bios_display_info.h = bios_display_info.gop->mode->info->height;
  bios_display_info.pixel_format
          = bios_display_info.gop->mode->info->pixel_format;
  bios_display_info.pixels_per_scanline
            = bios_display_info.gop->mode->info->pixels_per_scanline;
  bios_display_info.fb_base = bios_display_info.gop->mode->fb_base;
  bios_display_info.fb_size = bios_display_info.gop->mode->fb_size;

  efi_call_1 (grub_efi_system_table->con_out->clear_screen,
              grub_efi_system_table->con_out);

  if (!match_found)
    grub_error (GRUB_ERR_BAD_OS, "Resolution %ux%u not supported", width, height);

  return status;
}

grub_efi_status_t
bios_video_mode_hack (grub_efi_uintn_t width, grub_efi_uintn_t height)
{
  grub_efi_status_t status = GRUB_EFI_DEVICE_ERROR;
  grub_efi_uint32_t orig_pixels_per_scanline;
  grub_efi_uint32_t new_width;
  grub_efi_uint32_t new_height;
  grub_efi_uint32_t new_pixels_per_scanline;
  grub_efi_uint32_t new_fb_size;
  grub_efi_uint32_t scanline_scale = 1;

  if (!check_display ())
  {
    grub_printf ("No display adapters found, unable to switch video mode.\n");
    return GRUB_EFI_DEVICE_ERROR;
  }

  if (bios_display_info.type != VIDEO_GOP)
  {
    grub_printf ("Video mode switching is not supported on UGA display adapters.\n");
    return GRUB_EFI_UNSUPPORTED;
  }

  // Save old settings
  orig_pixels_per_scanline = bios_display_info.gop->mode->info->pixels_per_scanline;

  new_width = width;
  new_height = height;
  while (orig_pixels_per_scanline * scanline_scale < width)
    scanline_scale++;
  new_pixels_per_scanline = orig_pixels_per_scanline * scanline_scale;
  new_fb_size = new_pixels_per_scanline * new_height * 4;
  bios_display_info.gop->mode->info->width = new_width;
  bios_display_info.gop->mode->info->height = new_height;
  bios_display_info.gop->mode->info->pixels_per_scanline = new_pixels_per_scanline;
  bios_display_info.gop->mode->fb_size = new_fb_size;

  bios_display_info.w = bios_display_info.gop->mode->info->width;
  bios_display_info.h = bios_display_info.gop->mode->info->height;
  bios_display_info.pixel_format
          = bios_display_info.gop->mode->info->pixel_format;
  bios_display_info.pixels_per_scanline
          = bios_display_info.gop->mode->info->pixels_per_scanline;
  bios_display_info.fb_base = bios_display_info.gop->mode->fb_base;
  bios_display_info.fb_size = bios_display_info.gop->mode->fb_size;

  efi_call_1 (grub_efi_system_table->con_out->clear_screen,
              grub_efi_system_table->con_out);

  return status;
}

grub_efi_boolean_t
bios_match_cur_resolution (grub_efi_uintn_t width, grub_efi_uintn_t height)
{
  if (!check_display ())
  {
    grub_printf ("No display adapters found, unable to match video mode.\n");
    return FALSE;
  }
  if (bios_display_info.w == width && bios_display_info.h == height)
    return TRUE;
  else
    return FALSE;
}

grub_err_t
bios_fill_vesa_info (grub_efi_physical_address_t start,
                     grub_efi_physical_address_t *end)
{
  struct grub_vbe_info_block *vbe_info;
  struct grub_vbe_mode_info_block *vbe_mode_info;
  grub_uint8_t *ptr;
  grub_uint32_t x_offset_px;
  grub_uint32_t y_offset_px;
  grub_efi_physical_address_t fb_base_with_offset;

  if (!check_display ())
    return grub_error (GRUB_ERR_BAD_OS, "no display adapters found");

  vbe_info = (struct grub_vbe_info_block *)(grub_addr_t) start;
  ptr = vbe_info->reserved;
  grub_memcpy (vbe_info->signature, "VESA", 4);
  vbe_info->version = 0x0300;
  vbe_info->oem_string_ptr
          = (grub_uint32_t) start << 12 | (grub_uint16_t)(grub_addr_t) ptr;
  grub_memcpy (ptr, VBE_VENDOR_NAME, sizeof (VBE_VENDOR_NAME));
  ptr += sizeof (VBE_VENDOR_NAME);
  vbe_info->capabilities = GRUB_VBE_CAPABILITY_DACWIDTH;
  vbe_info->video_mode_ptr
          = (grub_uint32_t) start << 12 | (grub_uint16_t)(grub_addr_t) ptr;
  *(grub_uint16_t*) ptr = 0x00f1;
  ptr += 2;
  *(grub_uint16_t*) ptr = 0xFFFF;
  ptr += 2;
  vbe_info->total_memory
          = (grub_uint16_t)((bios_display_info.fb_size + 65535) / 65536);
  vbe_info->oem_software_rev = 0x0000;
  vbe_info->oem_vendor_name_ptr
          = (grub_uint32_t) start << 12 | (grub_uint16_t)(grub_addr_t) ptr;
  grub_memcpy (ptr, VBE_VENDOR_NAME, sizeof (VBE_VENDOR_NAME));
  ptr += sizeof (VBE_VENDOR_NAME);
  vbe_info->oem_product_name_ptr
          = (grub_uint32_t) start << 12 | (grub_uint16_t)(grub_addr_t) ptr;
  grub_memcpy (ptr, VBE_PRODUCT_NAME, sizeof (VBE_PRODUCT_NAME));
  ptr += sizeof (VBE_PRODUCT_NAME);
  vbe_info->oem_product_rev_ptr
          = (grub_uint32_t) start << 12 | (grub_uint16_t)(grub_addr_t) ptr;
  grub_memcpy (ptr, VBE_PRODUCT_VERSION, sizeof (VBE_PRODUCT_VERSION));
  ptr += sizeof (VBE_PRODUCT_VERSION);

  vbe_mode_info = (struct grub_vbe_mode_info_block *)vbe_info->oem_data;

  vbe_mode_info->mode_attributes = GRUB_VBE_MODEATTR_LFB_AVAIL |
                                   GRUB_VBE_MODEATTR_VGA_WINDOWED_AVAIL |
                                   GRUB_VBE_MODEATTR_VGA_COMPATIBLE |
                                   GRUB_VBE_MODEATTR_GRAPHICS |
                                   GRUB_VBE_MODEATTR_COLOR |
                                   GRUB_VBE_MODEATTR_RESERVED_1 |
                                   GRUB_VBE_MODEATTR_SUPPORTED;

  vbe_mode_info->x_resolution = 1024;
  vbe_mode_info->y_resolution = 768;
  vbe_mode_info->x_char_size = 8;
  vbe_mode_info->y_char_size = 16;

  x_offset_px = (bios_display_info.w - 1024) / 2;
  y_offset_px
        = (bios_display_info.h - 768) / 2 * bios_display_info.pixels_per_scanline;
  fb_base_with_offset
        = bios_display_info.fb_base + y_offset_px * 4 + x_offset_px * 4;

  vbe_mode_info->number_of_banks = 1;
  vbe_mode_info->bank_size = 0;
  vbe_mode_info->phys_base_addr = (grub_uint32_t) fb_base_with_offset;
  vbe_mode_info->lin_bytes_per_scan_line
        = bios_display_info.pixels_per_scanline * 4;
  vbe_mode_info->number_of_image_pages = 0;
  vbe_mode_info->lin_number_of_image_pages = 0;
  vbe_mode_info->win_func_ptr = 0x0;
  vbe_mode_info->win_a_attributes = 0x0;
  vbe_mode_info->win_b_attributes = 0x0;
  vbe_mode_info->win_granularity = 0x0;
  vbe_mode_info->win_size = 0x0;
  vbe_mode_info->win_a_segment = 0x0;
  vbe_mode_info->win_b_segment = 0x0;

  vbe_mode_info->number_of_planes = 1;
  vbe_mode_info->memory_model = GRUB_VBE_MEMORY_MODEL_DIRECT_COLOR;
  vbe_mode_info->direct_color_mode_info = 0x01;
  vbe_mode_info->bits_per_pixel = 32;
  vbe_mode_info->lin_blue_mask_size = 8;
  vbe_mode_info->lin_green_mask_size = 8;
  vbe_mode_info->lin_red_mask_size = 8;
  vbe_mode_info->lin_rsvd_mask_size = 8;

  if (bios_display_info.pixel_format == GRUB_EFI_GOT_BGRA8)
  {
    vbe_mode_info->lin_blue_field_position = 0;
    vbe_mode_info->lin_green_field_position = 8;
    vbe_mode_info->lin_red_field_position = 16;
    vbe_mode_info->lin_rsvd_field_position = 24;
  }
  else if (bios_display_info.pixel_format == GRUB_EFI_GOT_RGBA8)
  {
    vbe_mode_info->lin_red_field_position = 0;
    vbe_mode_info->lin_green_field_position = 8;
    vbe_mode_info->lin_blue_field_position = 16;
    vbe_mode_info->lin_rsvd_field_position = 24;
  }
  else
    return grub_error (GRUB_ERR_BAD_OS, "Unsupported value of pixel_format");

  vbe_mode_info->reserved2 = 0;
  vbe_mode_info->reserved3 = 0;
  vbe_mode_info->max_pixel_clock = 0;
  vbe_mode_info->reserved = 0x01;

  *end = (grub_addr_t)(vbe_mode_info + 1);
  return GRUB_ERR_NONE;
}
