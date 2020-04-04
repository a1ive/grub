/* lzmaio.c - lzma decompression support for GRUB 2 */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2020  Free Software Foundation, Inc.
 *  Copyright (C) 2010-2019 Miray Software <oss@miray.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/types.h>
#include <grub/fs.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/dl.h>

#include <grub/lib/LzmaDec.h>

GRUB_MOD_LICENSE ("GPLv3+");

#define INBUFSIZE 0x1000
#define OUTBUFSIZE 512
#define HEADERSIZE (LZMA_PROPS_SIZE + 8)

// Try to decode TESTSIZE bytes to see if file is lzma compressed
#define TESTSIZE 100

static void *SzAlloc(void *p __attribute__((unused)), size_t size) { return grub_malloc(size); }
static void SzFree(void *p __attribute__((unused)), void *address) { grub_free(address); }
static ISzAlloc g_Alloc = { SzAlloc, SzFree };

struct grub_lzmaio {
   grub_file_t file;
   grub_uint8_t header[HEADERSIZE];
   grub_uint8_t inbuf[INBUFSIZE];
   grub_ssize_t inbuf_offset;
   grub_ssize_t inbuf_len;
   grub_off_t   saved_offset;
   CLzmaDec state;
};
typedef struct grub_lzmaio *grub_lzmaio_p;

static struct grub_fs grub_lzmaio_fs;

static int is_lzma_file(grub_file_t file)
{
   SRes res;
   grub_lzmaio_p lzmaio = file->data;
   Byte outbuf[TESTSIZE];
   ELzmaStatus status;
   grub_size_t outSize = TESTSIZE;
   grub_size_t inSize = lzmaio->inbuf_len - lzmaio->inbuf_offset;

   /* lzma has no file magic, so try to decode a bit at the start */
   if (file->size == 0)
      return 0;

   res = LzmaDec_DecodeToBuf(&lzmaio->state, outbuf, &outSize, &lzmaio->inbuf[lzmaio->inbuf_offset], &inSize, LZMA_FINISH_ANY, &status);

   if (res != SZ_OK)
      return 0;

   return 1;
}

static grub_ssize_t
grub_lzmaio_fill_inbuf(grub_lzmaio_p lzmaio)
{
   grub_ssize_t ret = grub_file_read(lzmaio->file, lzmaio->inbuf, INBUFSIZE);
 
   lzmaio->inbuf_offset = 0;
   lzmaio->inbuf_len = ret > 0 ? ret : 0;

#ifdef DEBUG
   // Actually this should never happen
   if (ret > INBUFSIZE) 
   {
      lzmaio->inbuf_len = 0;
      grub_error(GRUB_ERR_OUT_OF_RANGE, "lzma: access after end of buffer");
      return -1;
   }
#endif

   return ret;
}

static grub_err_t
grub_lzmaio_seek (grub_file_t file)
{
   grub_lzmaio_p lzmaio = file->data;
   SRes res;

   if (file->offset < lzmaio->saved_offset || file->offset == 0)
   {
      grub_file_seek(lzmaio->file, 0);
      lzmaio->saved_offset = 0;
      if (grub_lzmaio_fill_inbuf(lzmaio) < 0) return grub_errno;
      if (lzmaio->inbuf_len < HEADERSIZE) return grub_error(GRUB_ERR_READ_ERROR, "no lzma header: file too small");
      lzmaio->inbuf_offset = HEADERSIZE;

      LzmaDec_Init(&lzmaio->state);
   }

   while (file->offset > lzmaio->saved_offset)
   {
      Byte outbuf[OUTBUFSIZE];
      grub_size_t outSize = file->offset - lzmaio->saved_offset > OUTBUFSIZE ? OUTBUFSIZE : file->offset - lzmaio->saved_offset ;
      grub_size_t inSize = lzmaio->inbuf_len - lzmaio->inbuf_offset;
      ELzmaStatus status;

      res = LzmaDec_DecodeToBuf(&lzmaio->state, outbuf, &outSize,
                                &lzmaio->inbuf[lzmaio->inbuf_offset],
                                &inSize, LZMA_FINISH_ANY, &status);

      lzmaio->inbuf_offset += inSize;
      lzmaio->saved_offset += outSize;

      if (res != SZ_OK)
      {
         grub_dprintf ("lzmaio", "res: %d, status: %d\n", res, status);
         return grub_error (GRUB_ERR_READ_ERROR, "error decompressing file");
      }
      if (status == LZMA_STATUS_FINISHED_WITH_MARK)
      {
       grub_dprintf("lzmaio", "\n");
       if (lzmaio->saved_offset != file->offset)
           return grub_error (GRUB_ERR_OUT_OF_RANGE, "attempt to read after end of file");
       else
          break;
      }
      if (status == LZMA_STATUS_NEEDS_MORE_INPUT)
      {
         if (grub_lzmaio_fill_inbuf(lzmaio) < 0) return grub_errno;
         if (lzmaio->inbuf_len == 0)
            return grub_error (GRUB_ERR_READ_ERROR, "file too short");
      }
   }
   return GRUB_ERR_NONE;
}


/* Open a new decompression object on top of IO. */
static grub_file_t
grub_lzmaio_open (grub_file_t io, enum grub_file_type type __attribute__ ((unused)))
{
   grub_file_t file;
   grub_lzmaio_p lzmaio;
   SRes res;

   if (type & GRUB_FILE_TYPE_NO_DECOMPRESS)
      return io;

   file = (grub_file_t) grub_zalloc (sizeof (*file));
   if (! file)
   {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
      return 0;
   }

   lzmaio = grub_zalloc (sizeof(*lzmaio));
   if (! lzmaio)
   {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
      grub_free (file);
      return 0;
   }

   lzmaio->file = io;

   file->device = io->device;
   file->offset = 0;
   file->data = lzmaio;
   file->read_hook = 0;
   file->fs = &grub_lzmaio_fs;
   file->not_easily_seekable = 1;

   if (grub_file_tell (lzmaio->file) != 0)
      grub_file_seek (lzmaio->file, 0);
   
   lzmaio->inbuf_len = grub_file_read (io, lzmaio->inbuf, INBUFSIZE);
   if (lzmaio->inbuf_len < HEADERSIZE)
   {
      grub_dprintf("filter", "inbug_len < HEADERSIZE");
      grub_errno = GRUB_ERR_NONE;
      grub_free (file);
      grub_free (lzmaio);
      grub_file_seek (io, 0);
      return io;
   }

   grub_memcpy(lzmaio->header, lzmaio->inbuf, HEADERSIZE);
   /* file->size = grub_le_to_cpu64(*((grub_uint64_t*)&lzmaio->header[LZMA_PROPS_SIZE])); <- gcc does not like this because of its 'antialiasing rules' ... */
   grub_memcpy(&file->size, &lzmaio->inbuf[LZMA_PROPS_SIZE], sizeof(grub_uint64_t));
   file->size = grub_le_to_cpu64(file->size);

   LzmaDec_Construct(&lzmaio->state);
   res = LzmaDec_Allocate(&lzmaio->state, lzmaio->header, LZMA_PROPS_SIZE, &g_Alloc);
   if (res != SZ_OK)
   {
      grub_dprintf("filter", "res != SZ_OK\n");
      grub_errno = GRUB_ERR_NONE;
      LzmaDec_Free(&lzmaio->state, &g_Alloc);
      grub_free (file);
      grub_free (lzmaio);
      grub_file_seek (io, 0);
      return io;
   }

   lzmaio->inbuf_offset = HEADERSIZE;
   LzmaDec_Init(&lzmaio->state);
   if (!is_lzma_file(file))
   {
      grub_dprintf("filter", "!is_lzma_file");
      grub_errno = GRUB_ERR_NONE;
      LzmaDec_Free(&lzmaio->state, &g_Alloc);
      grub_free (file);
      grub_free (lzmaio);
      grub_file_seek (io, 0);
      return io;
   }

   lzmaio->inbuf_offset = HEADERSIZE;

   LzmaDec_Init(&lzmaio->state);
   return file;
}


static grub_ssize_t
grub_lzmaio_read(struct grub_file *file, char *buf, grub_size_t len)
{
   grub_lzmaio_p lzmaio = file->data;
   SRes res;
   grub_ssize_t ret = 0;

   if (grub_lzmaio_seek (file) != GRUB_ERR_NONE)
   {
      grub_dprintf("lzmaio", "seek failed\n");
      return -1;
   }

   while (1)
   {
      grub_size_t outSize = len - ret;
      grub_size_t inSize = lzmaio->inbuf_len - lzmaio->inbuf_offset;
      ELzmaStatus status;

      res = LzmaDec_DecodeToBuf(&lzmaio->state, (Byte *)&buf[ret], &outSize, &lzmaio->inbuf[lzmaio->inbuf_offset], &inSize, LZMA_FINISH_ANY, &status);

      lzmaio->inbuf_offset += inSize;
      ret += outSize;


      if (res != SZ_OK)
      {
         grub_dprintf ("lzmaio", "res: %d, status: %d\n", res, status);
         grub_error(GRUB_ERR_FILE_READ_ERROR, "lzma decode failed");
         return -1;
      }
      if (status == LZMA_STATUS_FINISHED_WITH_MARK || (grub_size_t)ret == len)
      {
         lzmaio->saved_offset += ret;
         return ret;
      }
      if (status == LZMA_STATUS_NEEDS_MORE_INPUT)
      {
         grub_lzmaio_fill_inbuf(lzmaio);
         if (lzmaio->inbuf_len == 0)
         {
            grub_dprintf("lzmaio", "unexpected end of file\n");
            grub_error(GRUB_ERR_FILE_READ_ERROR, "unexpected end of file");
            return -1;
         }
      }
   }
}

static grub_err_t
grub_lzmaio_close(struct grub_file *file)
{
   grub_lzmaio_p lzmaio = file->data;
   file->data = 0;

   LzmaDec_Free(&lzmaio->state, &g_Alloc);
   grub_file_close (lzmaio->file);
   grub_free(lzmaio);

   /* Don't close device twice */
   file->device = 0;

   return 0;
}

static struct grub_fs grub_lzmaio_fs =
{
   .name = "lzmaio",
   .fs_dir = 0,
   .fs_open = 0,
   .fs_read = grub_lzmaio_read,
   .fs_close = grub_lzmaio_close,
   .fs_label = 0,
   .next = 0
};

GRUB_MOD_INIT (lzmaio)
{
   grub_file_filter_register (GRUB_FILE_FILTER_LZMAIO, grub_lzmaio_open);
}

GRUB_MOD_FINI (lzmaio)
{
   grub_file_filter_unregister (GRUB_FILE_FILTER_LZMAIO);
}
