/* segmented_file.c - provide access to a segmented file  */
/*
 *  Module for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010 Miray Software <oss@miray.de>
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
#include <grub/misc.h>
#include <grub/mm.h>
#include <grub/file.h>
#include <grub/dl.h>
#include <grub/test.h>
#include <grub/disk.h>
#include <grub/partition.h>

GRUB_MOD_LICENSE ("GPLv3+");

/* defined in miray bootscreen module */
extern int miray_disk_nr_msg(int nr);
extern int miray_disk_retry_msg(void);

grub_file_t grub_segmented_file_open(grub_file_t in, const char *path);
static char * device_path (grub_device_t device, const char *filepath);

static struct grub_fs grub_segmented_file_fs;

struct grub_sf {
  grub_file_t cur_file;
  char * device_path;
  int pattern;
  int cur_file_num;
  int last_file;
  grub_off_t cur_file_base_offset;
};
typedef struct grub_sf *grub_sf_p;

static const struct {
  const char * filename;
  const char * pattern;
  const char * endpattern;
} filepatterns[] = {
  { .filename = "kernel.m00", .pattern = "kernel.m%02d", .endpattern = "kernel.f%02d" },
  { .filename = NULL, .pattern = NULL, .endpattern = NULL }
};

static void
disable_all_file_filters(void)
{
  grub_file_filter_id_t id;

  for (id = 0;
       id < GRUB_FILE_FILTER_MAX; id++)
    grub_file_filters_enabled[id] = 0;
}

static int
is_last_file(grub_sf_p pdata)
{
  return pdata->last_file;
}


static grub_err_t 
open_file_num(grub_sf_p pdata, int num)
{
   char * fmt;
   char * name;

   fmt = grub_xasprintf("%s%s", pdata->device_path, filepatterns[pdata->pattern].pattern);
   if (fmt == 0) return grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
  
   name = grub_xasprintf(fmt, num);
   grub_free(fmt);
   fmt = 0;
   if (name == 0) return grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");

   disable_all_file_filters();
   grub_dprintf("sf", "opening %s\n", name);
   pdata->cur_file = grub_file_open(name);
   grub_free(name);
  
   if (pdata->cur_file != 0) return GRUB_ERR_NONE;
   grub_dprintf("sf", "Not found\n");

   while (grub_error_pop()) {} // Clear error number


   fmt = grub_xasprintf("%s%s", pdata->device_path, filepatterns[pdata->pattern].endpattern);
   if (fmt == 0) return grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
  
   name = grub_xasprintf(fmt, num);
   grub_free(fmt);
   fmt = 0;
   if (name == 0) return grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");

   disable_all_file_filters();
   grub_dprintf("sf", "opening %s\n", name);
   pdata->cur_file = grub_file_open(name);
   grub_free(name);
  
   if (pdata->cur_file != 0) 
   {
      pdata->last_file = 1;
      return GRUB_ERR_NONE;
   }

   return grub_error(GRUB_ERR_FILE_NOT_FOUND, "File not found");
}

static grub_err_t
open_next_file(grub_sf_p pdata)
{
  char * fmt;
  char * name;

  if (is_last_file(pdata))
  {
    return grub_error (GRUB_ERR_OUT_OF_RANGE, "No next part");
  } 
 
  pdata->cur_file_num++;
  pdata->cur_file_base_offset += grub_file_size(pdata->cur_file);
  grub_file_close(pdata->cur_file);
  pdata->cur_file = 0;

  while (1)
  { 
      fmt = grub_xasprintf("%s%s", pdata->device_path, filepatterns[pdata->pattern].pattern);
      if (fmt == 0) return grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
  
      name = grub_xasprintf(fmt, pdata->cur_file_num);
      grub_free(fmt);
      fmt = 0;
      if (name == 0) return grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");

      disable_all_file_filters();
      grub_dprintf("sf", "opening %s\n", name);
      pdata->cur_file = grub_file_open(name);
      grub_free(name);
  
      if (pdata->cur_file != 0) return GRUB_ERR_NONE;
      grub_dprintf("sf", "Not found\n");

      while (grub_error_pop()) {} // Clear error number


      fmt = grub_xasprintf("%s%s", pdata->device_path, filepatterns[pdata->pattern].endpattern);
      if (fmt == 0) return grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
  
      name = grub_xasprintf(fmt, pdata->cur_file_num);
      grub_free(fmt);
      fmt = 0;
      if (name == 0) return grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");

      disable_all_file_filters();
      grub_dprintf("sf", "opening %s\n", name);
      pdata->cur_file = grub_file_open(name);
      grub_free(name);
  
      if (pdata->cur_file != 0) 
      {
	grub_dprintf("sf", "found\n");
        pdata->last_file = 1;
	return GRUB_ERR_NONE;
      }

      grub_dprintf("sf", "Not found\n");
      while (grub_error_pop()) {}; // Clear Error number

    int next_disk = miray_disk_nr_msg(pdata->cur_file_num + 1); 
    if (!next_disk)
    {
      grub_dprintf("sf", "User Abort\n");
      return grub_error(GRUB_ERR_READ_ERROR, "User abort");
    }

    grub_dprintf("sf", "retry with new disk\n");
  }
}

grub_file_t
grub_segmented_file_open(grub_file_t io, const char *path)
{
  grub_file_t file = 0;
  grub_sf_p pdata = 0;
  unsigned int pattern = 0;

  if (path == 0 ||
      io == 0 || 
      io->device == 0 || 
      io->device->disk == 0 ||
      io->device->disk->partition != 0 )
    return io;

  const char * name = grub_strrchr (path, '/');
  if (name == 0) name = path;
  if (name[0] == '/') name++;

  while (filepatterns[pattern].filename != 0)
  {    
    if (grub_strcmp (name, filepatterns[pattern].filename) == 0) 
      break; /* found */
 
    pattern ++;
  }

  if (filepatterns[pattern].filename == 0)
    return io; /* Not found, no need to use this module */

  file = grub_malloc (sizeof(*file));
  if (! file)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
      return 0;
    }

  pdata = grub_zalloc (sizeof(*pdata));
  if (! pdata)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
      grub_free (file);
      return 0;
    }

  pdata->cur_file = io;
  pdata->device_path = device_path(io->device, path);
  if (! pdata->device_path)
    {
      grub_error (GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
      grub_free (pdata);
      grub_free (file);
      return 0;
    } 
  pdata->cur_file_num = 0;
  pdata->cur_file_base_offset = 0;
  pdata->last_file = 0;

  file->device = io->device;
  file->offset = 0;
  file->data = pdata;
  file->read_hook = 0;
  file->fs = &grub_segmented_file_fs;
  file->size = GRUB_FILE_SIZE_UNKNOWN;
  file->not_easily_seekable = 1;

  grub_dprintf("sf", "Using Segmented File filter\n");

  return file;

}

static grub_err_t
grub_segmented_file_close(struct grub_file *file)
{
  grub_dprintf("sf", "Closing Segmented File filter");
  grub_sf_p pdata = file->data;
  file->data = 0;

  if (pdata->cur_file != 0)
  {
    grub_file_close (pdata->cur_file);
  }
  grub_free(pdata->device_path);

  /* make sure we don't close the device twice */
  file->device = 0;

  return 0;
}

static grub_ssize_t
grub_segmented_file_read(struct grub_file *file, char *buf, grub_size_t len)
{
  grub_off_t cur_file_offset;
  grub_sf_p pdata = file->data;

  if (pdata->cur_file == 0)
  {
    if (open_file_num(pdata, pdata->cur_file_num) != GRUB_ERR_NONE)
    {
       grub_error (GRUB_ERR_READ_ERROR, "Filter has no base file");
       return -1;
    }
  }

  if (file->offset < pdata->cur_file_base_offset)
  {
    // Seeking backwards over disk bondries is not supported
    grub_error (GRUB_ERR_BAD_ARGUMENT, "Seek position not supported");
    return -1;
  }

  // seek to the correct position
  cur_file_offset = file->offset - pdata->cur_file_base_offset;
  while (cur_file_offset >= grub_file_size(pdata->cur_file))
  {
    if (is_last_file(pdata) && cur_file_offset == grub_file_size(pdata->cur_file)) 
    {
      file->size = pdata->cur_file_base_offset + cur_file_offset;
      return 0;
    }

    if (open_next_file(pdata) != GRUB_ERR_NONE)
    {
      // Set some end of file marker
      file->size = pdata->cur_file_base_offset - 1; 
      file->device = 0;
      // No nect
      return -1;
    }
    /* grub_test_assert(pdata->cur_file != 0); */
    file->device = pdata->cur_file->device;

    cur_file_offset = file->offset - pdata->cur_file_base_offset;
  }
  grub_file_seek(pdata->cur_file, cur_file_offset);


  grub_ssize_t ret;
#if 0
  while ((ret = grub_file_read(pdata->cur_file, buf, len)) < 0)
  {
     if (grub_errno == GRUB_ERR_READ_ERROR)
     {
        // Ask for Retry
        grub_file_close(pdata->cur_file);
        pdata->cur_file = 0;
        file->device = 0;

        int retry = miray_disk_retry_msg();   
 
        if (retry) 
        {
           while(grub_error_pop()) {}
           if (open_file_num(pdata, pdata->cur_file_num) == GRUB_ERR_NONE)
           {
              file->device = pdata->cur_file->device;
              continue;
           }
        }
     }
     break;
  }
#else
  ret = grub_file_read(pdata->cur_file, buf, len);
#endif

  return ret;
}

static struct grub_fs grub_segmented_file_fs =
  {
    .name = "segmented_file",
    .dir = 0,
    .open = 0,
    .read = grub_segmented_file_read,
    .close = grub_segmented_file_close,
    .label = 0,
    .next = 0,
  };

static inline const char * devicename (grub_device_t device)
{
  if (device->disk != 0)
  {
    return device->disk->name;
    // TODO: Accept Partitions
  }
  else return "";
}

static char *device_path (grub_device_t device, const char *filepath)
{
  int len = 0;
  char * ret;
  char * name; 

  len += grub_strlen(devicename(device));
  len += 2;
  len += grub_strlen(filepath);
  if (filepath[0] != '/') len++;

  ret = grub_malloc(len + 1); // Trailing \0

  ret[0] = '(';
  grub_strcpy(&ret[1], devicename(device));
  len = grub_strlen(ret);
  ret[len++] = ')';
  if (filepath[0] != '/')
  {
    ret[len++] = '/';
  }
  grub_strcpy(&(ret[len]), filepath);

  // get rid of filename
  name = grub_strrchr(ret, '/');
  name++; 
  *name = '\0';
  
  return ret;
}

GRUB_MOD_INIT (segmented_file)
{
  grub_file_filter_register (GRUB_FILE_FILTER_SEGMENTED_FILE, grub_segmented_file_open);
}

GRUB_MOD_FINI (segmented_file)
{
  grub_file_filter_unregister (GRUB_FILE_FILTER_SEGMENTED_FILE);
}
