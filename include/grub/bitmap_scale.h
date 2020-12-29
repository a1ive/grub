/* bitmap_scale.h - Bitmap scaling functions. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2008,2009  Free Software Foundation, Inc.
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

#ifndef GRUB_BITMAP_SCALE_HEADER
#define GRUB_BITMAP_SCALE_HEADER 1

#include <grub/err.h>
#include <grub/types.h>
#include <grub/bitmap_scale.h>

/* Choose the fastest interpolation algorithm.  */
#define GRUB_VIDEO_BITMAP_SCALE_METHOD_FASTEST	0

/* Choose the highest quality interpolation algorithm.  */
#define GRUB_VIDEO_BITMAP_SCALE_METHOD_BEST	1

/* Specific algorithms:  */
/* Nearest neighbor interpolation.  */
#define GRUB_VIDEO_BITMAP_SCALE_METHOD_NEAREST	2

/* Bilinear interpolation.  */
#define GRUB_VIDEO_BITMAP_SCALE_METHOD_BILINEAR	3

#define GRUB_VIDEO_BITMAP_SCALE_METHOD_MASK	0xf

#define GRUB_VIDEO_BITMAP_SCALE_TYPE_NORMAL	0
#define GRUB_VIDEO_BITMAP_SCALE_TYPE_CENTER	0x10
#define GRUB_VIDEO_BITMAP_SCALE_TYPE_TILING	0x20
#define GRUB_VIDEO_BITMAP_SCALE_TYPE_MINFIT	0x30
#define GRUB_VIDEO_BITMAP_SCALE_TYPE_MAXFIT	0x40
#define GRUB_VIDEO_BITMAP_SCALE_TYPE_MASK	0xf0

grub_err_t
grub_video_bitmap_create_scaled (struct grub_video_bitmap **dst,
                                 int dst_width, int dst_height,
                                 struct grub_video_bitmap *src,
				 int scale, grub_video_color_t color);

#endif /* ! GRUB_BITMAP_SCALE_HEADER */
