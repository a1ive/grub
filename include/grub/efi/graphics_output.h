/* graphics_output.h - definitions of the graphics output protocol */
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

#ifndef GRUB_EFI_GRAPHICS_OUTPUT_HEADER
#define GRUB_EFI_GRAPHICS_OUTPUT_HEADER	1

#include <grub/efi/uga_draw.h>

#define GRUB_EFI_GRAPHICS_OUTPUT_GUID \
  { 0x9042a9de, 0x23dc, 0x4a38, { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a }}

struct grub_efi_pixel_bitmask
{
  grub_uint32_t red_mask;
  grub_uint32_t green_mask;
  grub_uint32_t blue_mask;
  grub_uint32_t reserved_mask;
};

enum grub_efi_graphics_pixel_format
  {
    GRUB_EFI_PIXEL_RGB_RESERVED_8BIT_PER_COLOR,
    GRUB_EFI_PIXEL_BGR_RESERVED_8BIT_PER_COLOR,
    GRUB_EFI_PIXEL_BIT_MASK,
    GRUB_EFI_PIXEL_BLT_ONLY,
    GRUB_EFI_PIXEL_FORMAT_MAX
  };

struct grub_efi_graphics_output_mode_information
{
  grub_uint32_t version;
  grub_uint32_t horizontal_resolution;
  grub_uint32_t vertical_resolution;
  enum grub_efi_graphics_pixel_format pixel_format;
  struct grub_efi_pixel_bitmask pixel_information;
  grub_uint32_t pixels_per_scan_line;
};

struct grub_efi_graphics_output_mode
{
  grub_uint32_t max_mode;
  grub_uint32_t mode;
  struct grub_efi_graphics_output_mode_information *info;
  grub_efi_uintn_t size_of_info;
  grub_efi_physical_address_t frame_buffer_base;
  grub_efi_uintn_t frame_buffer_size;
};

struct grub_efi_graphics_output_protocol
{
  grub_efi_status_t
  (*query_mode) (struct grub_efi_graphics_output_protocol *this,
		 grub_uint32_t mode_number,
		 grub_efi_uintn_t *size_of_info,
		 struct grub_efi_graphics_output_mode_information **info);

  grub_efi_status_t
  (*set_mode) (struct grub_efi_graphics_output_protocol *this,
	       grub_uint32_t mode_number);

  grub_efi_status_t
  (*blt) (struct grub_efi_uga_draw_protocol *this,
	  struct grub_efi_uga_pixel *blt_buffer,
	  enum grub_efi_uga_blt_operation blt_operation,
	  grub_efi_uintn_t src_x,
	  grub_efi_uintn_t src_y,
	  grub_efi_uintn_t dest_x,
	  grub_efi_uintn_t dest_y,
	  grub_efi_uintn_t width,
	  grub_efi_uintn_t height,
	  grub_efi_uintn_t delta);

  struct grub_efi_graphics_output_mode *mode;
};

#endif
