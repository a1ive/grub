/* bootlog.c - prepare memory for logging or sync it after reboot */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2000,2001,2002,2003,2005,2006,2007,2008,2009  Free Software Foundation, Inc.
 *  Copyright (C) 2010,2011,2012 Miray Software <oss@miray.de>
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

#include <grub/memory.h>
#include <grub/machine/memory.h>
#include <grub/dl.h>
#include <grub/misc.h>
#include <grub/command.h>
#include <grub/miray_bootlog.h>
#include <grub/extcmd.h>
#include <grub/file.h>
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/device.h>
#include <grub/disk.h>
#include <grub/partition.h>

GRUB_MOD_LICENSE ("GPLv3+");


static const struct grub_arg_option options_bootlog_activate[] =
{
   {"set",             's', 0,
      ("Set a variable to return value."), "VAR", ARG_TYPE_STRING},
   {"file", 'f', 0,
      ("Filename to limit max size"), "VAR", ARG_TYPE_STRING},
   {0, 0, 0, 0, 0, 0}
};

static const struct grub_arg_option options_bootlog_write[] =
{
   {"file",             'f', 0,
      ("destination file"), "VAR", ARG_TYPE_STRING},
   {0, 0, 0, 0, 0, 0}
};

#ifdef USE_MIRAY_BOOTLOG

static const grub_uint64_t bootlog_min_size = sizeof(struct mirayBootlogHeader);


struct log_blocklist
{
  grub_disk_addr_t startsector;
  unsigned offset;
  grub_uint64_t length;
  struct log_blocklist *next;
   
};

static grub_file_t
open_bootlog_file (const char *filename)
{
   // TODO: handle case without filename?

   
   grub_file_filter_disable_compression ();
   return grub_file_open (filename);
}

struct read_hook_data
{
   struct log_blocklist * head;
   struct log_blocklist * tail;
   unsigned sectorSize;
};

static void read_hook (grub_disk_addr_t sector, unsigned offset, unsigned length, void * data)
{
   struct read_hook_data * hook_data = (struct read_hook_data *)data;
   
   if (hook_data->tail != 0 && hook_data->tail->startsector * hook_data->sectorSize + hook_data->tail->offset + hook_data->tail->length == sector * hook_data->sectorSize + offset)
   {
      hook_data->tail->length += length;
   }
   else
   {
      // Add new entry
      struct log_blocklist * new = (struct log_blocklist *)grub_malloc(sizeof(struct log_blocklist));
      new->startsector = sector;
      new->offset = offset;
      new->length = length;
      new->next = 0;

      if (hook_data->tail != 0)
         hook_data->tail->next = new;
      hook_data->tail = new;
      if (hook_data->head == 0)
      {
         hook_data->head = new;
      }
   }
}


static struct log_blocklist *
file_get_blocks(grub_file_t file)
{
   struct read_hook_data data = { .head = 0, .tail = 0 };
   grub_uint8_t * buffer = 0;

   if (file == 0)
   {
      grub_error(GRUB_ERR_BAD_ARGUMENT, "file == 0");
      return 0;
   }

   if (!file->device->disk)
   {
      grub_error (GRUB_ERR_BAD_DEVICE, "disk device required");
      return 0;
   }

   data.sectorSize = 1 << file->device->disk->log_sector_size;
   if (data.sectorSize < 512 || data.sectorSize > 4096)
   {
      grub_error (GRUB_ERR_BAD_DEVICE, "Invalid sector size %d", data.sectorSize);
      return 0;
   }


   buffer = grub_malloc(data.sectorSize);

   file->read_hook = read_hook;
   file->read_hook_data = &data;
   while (grub_file_read(file, buffer, data.sectorSize) > 0)
   {
   }
   grub_free(buffer);
   file->read_hook = 0;
   file->read_hook_data = 0;
   
   return data.head;
}

static void
free_blocklist(struct log_blocklist * list)
{
   struct log_blocklist * next = list;

   while (list != 0)
   {
      next = list->next;
      grub_free(list);
      list = next;
   }
}
   
#if 0
static grub_err_t
miray_cmd_bootlog_test(struct grub_command *cmd __attribute__ ((unused)),
      int argc __attribute__((unused)), char * argv[] __attribute__((unused)))
{
   grub_file_t file = 0;
   struct log_blocklist * list = 0;

   if (argc < 1)
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "missing filename");

   file = grub_file_open (argv[0]);

   if (file == 0)
      return grub_error(GRUB_ERR_FILE_NOT_FOUND, "No such file");

  if (! file->device->disk)
    {
      grub_file_close (file);
      return grub_error (GRUB_ERR_BAD_DEVICE, "disk device required");
    }

   list = file_get_blocks(file);
   if (list != 0)
   {
      struct log_blocklist * current = list;
      while (current != 0)
      {
         grub_printf("sector = 0x%" PRIxGRUB_UINT64_T ", offset = %d, length = 0x%" PRIxGRUB_UINT64_T "\n", current->startsector, current->offset, current->length);
         current = current->next;
      }
   }

   free_blocklist(list);

   grub_file_close(file);

   
   return GRUB_ERR_NONE;
}
#endif

static grub_err_t
miray_cmd_bootlog_activate(grub_extcmd_context_t ctxt,
      int argc __attribute__((unused)), char * argv[] __attribute__((unused)))
{
   struct grub_arg_list *state = ctxt->state;

   struct mirayBootlogHeader * header = 0;
   grub_uint64_t filesize = -1ULL;
   grub_size_t size = 0;

   grub_err_t ret;

   if (!state[0].set)
   {
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "Missing environment name");
   }

   if (state[1].set)
   {
      grub_file_t file = grub_file_open (state[1].arg);
      if (file != 0)
      {
         filesize = grub_file_size(file);
         grub_file_close(file);
      }
   }

   if (filesize < bootlog_min_size)
   {
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "Logfile too small");
   }

   ret = miray_getBootlogHeader((void **)&header, &size);
   if (ret != GRUB_ERR_NONE)
      return ret;

   filesize = filesize & (~(GRUB_DISK_SECTOR_SIZE - 1)); // TODO: Use disk block size?

   header->addr = (grub_uint64_t)(unsigned long)header;
   header->memsize = (filesize < size) ? filesize : size;
   header->datasize = 0; 
   grub_memmove(header->magic, mirayBootlogMemMagic, MIRAY_BOOTLOG_MAGIC_SIZE);
   grub_memset(miray_bootlog_dataAddr(header), ' ', miray_bootlog_memSize(header));


   // Mark memory as unavailable (HOLE)
   // Actually this should not be necessary, but we want to make sure it works
   { 
      char str_from[20];
      char str_to[20];
      char *func_args[] = { str_from, str_to };

      grub_snprintf(str_from, 20, "%" PRIuGRUB_UINT64_T, header->addr);
      grub_snprintf(str_to, 20, "%" PRIuGRUB_UINT64_T, header->addr + header->memsize - 1);
      
      grub_command_execute("cutmem", 2, func_args);
   }

   // Write the environment variable
   {
      #define ACTIVATE_MAX_STRING 50
      char tmp[ACTIVATE_MAX_STRING + 1];
      grub_snprintf(tmp, ACTIVATE_MAX_STRING, "0x%" PRIxGRUB_UINT64_T ",0x%" PRIxGRUB_UINT64_T, header->addr, header->memsize);
      tmp[ACTIVATE_MAX_STRING] = '\0';
      #undef ACTIVATE_MAX_STRING

      grub_env_set(state[0].arg, tmp);
   }

   grub_errno = GRUB_ERR_NONE;
   return GRUB_ERR_NONE;
}

static grub_err_t
miray_cmd_bootlog_has_log(struct grub_command *cmd __attribute__ ((unused)),
      int argc __attribute__((unused)), char * argv[] __attribute__((unused)))
{
   struct mirayBootlogHeader * header = 0;
   grub_size_t size = 0;

   grub_err_t ret;

   ret = miray_getBootlogHeader((void **)&header, &size);
   if (ret != GRUB_ERR_NONE)
   {
      return ret;
   }

   if (grub_memcmp(header->magic, mirayBootlogLogMagic, MIRAY_BOOTLOG_MAGIC_SIZE) != 0)
   {
      return grub_error(GRUB_ERR_BUG, "Invalid header");
   }

   if (header->datasize == 0)
   {
      return grub_error(GRUB_ERR_BUG, "No data available");
   }

   grub_errno = GRUB_ERR_NONE;
   return GRUB_ERR_NONE;
}
      

static grub_err_t
miray_cmd_bootlog_write(grub_extcmd_context_t ctxt,
      int argc __attribute__((unused)), char * argv[] __attribute__((unused)))
{
   struct grub_arg_list *state = ctxt->state;

   struct mirayBootlogHeader * header = 0;
   grub_size_t size = 0;

   grub_file_t file = 0;
   grub_size_t writesize = 0;
   grub_disk_addr_t part_start = 0;
   struct log_blocklist * blist = 0;


   grub_err_t ret;

   if (!state[0].set)
   {
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "Missing filename");
   }

   ret = miray_getBootlogHeader((void **)&header, &size);
   if (ret != GRUB_ERR_NONE)
      return ret;

   file = open_bootlog_file(state[0].arg);
   if (file == 0)
   {
      return grub_errno; /* File open has already set the error number */
   }

   /* Little bit of tricky calculation. Essentially write out data rounded up to SECTOR size, up to file size truncated to sector size */
   writesize = (((header->datasize + GRUB_DISK_SECTOR_SIZE <= grub_file_size(file)) ?  header->datasize + GRUB_DISK_SECTOR_SIZE : grub_file_size(file))) & (~(GRUB_DISK_SECTOR_SIZE - 1));

   if (writesize < bootlog_min_size)
   {
      return grub_error(GRUB_ERR_BAD_ARGUMENT, "File too small");
   }

   if (!file->device->disk)
   {
      grub_file_close (file);
      return grub_error (GRUB_ERR_BAD_DEVICE, "disk device required");
   }

   blist = file_get_blocks(file);
   if (blist == 0)
   {
      grub_file_close(file);
      return grub_errno;
   }

   part_start = grub_partition_get_start (file->device->disk->partition);


#if 0
   {
      grub_uint8_t tmp_buffer[GRUB_DISK_SECTOR_SIZE];
      grub_memcpy(tmp_buffer, header, GRUB_DISK_SECTOR_SIZE);
      grub_memset(tmp_buffer, ' ', sizeof(struct mirayBootlogHeader));
      
      grub_disk_write(file->device->disk, blist->startsector - part_start, blist->offset, GRUB_DISK_SECTOR_SIZE, tmp_buffer);
   }

   {
      struct log_blocklist * curlist = blist;
      grub_size_t curPos = GRUB_DISK_SECTOR_SIZE;
      grub_size_t pos = GRUB_DISK_SECTOR_SIZE;
      grub_uint8_t * buffer = (grub_uint8_t *) header;
      unsigned sectorSize = 1 << file->device->disk->log_sector_size;

      if (curPos >= curlist->length)
      {
         curlist = curlist->next;
         curPos = 0;
      }


      while (curlist != 0 && pos < writesize)
      {
         grub_disk_write(file->device->disk, curlist->startsector - part_start + ((curlist->offset + curPos) / sectorSize), (curlist->offset + curPos) % sectorSize, GRUB_DISK_SECTOR_SIZE, &buffer[pos]);
         curPos += GRUB_DISK_SECTOR_SIZE;
         pos += GRUB_DISK_SECTOR_SIZE;

         if (curPos >= curlist->length)
         {
            curlist = curlist->next;
            curPos = 0;
         }
      }      
   }
   #endif

   {
   }

   {
      grub_uint8_t tmp_buffer[GRUB_DISK_SECTOR_SIZE];

      struct log_blocklist * curlist = blist;
      grub_size_t curPos = GRUB_DISK_SECTOR_SIZE;
      grub_size_t pos = GRUB_DISK_SECTOR_SIZE;
      grub_uint8_t * buffer = (grub_uint8_t *) header;
      unsigned sectorSize = 1 << file->device->disk->log_sector_size;
      grub_size_t file_size = grub_file_size(file);


      grub_memcpy(tmp_buffer, header, GRUB_DISK_SECTOR_SIZE);
      grub_memset(tmp_buffer, ' ', sizeof(struct mirayBootlogHeader));
      
      grub_disk_write(file->device->disk, blist->startsector - part_start, blist->offset, GRUB_DISK_SECTOR_SIZE, tmp_buffer);
      grub_memset(tmp_buffer, ' ', GRUB_DISK_SECTOR_SIZE);


      if (curPos >= curlist->length)
      {
         curlist = curlist->next;
         curPos = 0;
      }


      while (curlist != 0 && pos < file_size)
      {
         if (pos < writesize)
         {
            grub_disk_write(file->device->disk, curlist->startsector - part_start + ((curlist->offset + curPos) / sectorSize), (curlist->offset + curPos) % sectorSize, GRUB_DISK_SECTOR_SIZE, &buffer[pos]);
         }
         else
         {
            // Fill remaining file with ' '
            grub_disk_write(file->device->disk, curlist->startsector - part_start + ((curlist->offset + curPos) / sectorSize), (curlist->offset + curPos) % sectorSize, GRUB_DISK_SECTOR_SIZE, tmp_buffer);
         }

         curPos += GRUB_DISK_SECTOR_SIZE;
         pos += GRUB_DISK_SECTOR_SIZE;

         if (curPos >= curlist->length)
         {
            curlist = curlist->next;
            curPos = 0;
         }
      }      
   }

   

   free_blocklist(blist);
   grub_file_close(file);
   
   return GRUB_ERR_NONE;
}


#else

// Dummy methods


static grub_err_t
miray_cmd_bootlog_activate(grub_extcmd_context_t ctxt __attribute__((unused)),
      int argc __attribute__((unused)), char * argv[] __attribute__((unused)))
{
   return GRUB_ERR_NONE;

}

static grub_err_t
miray_cmd_bootlog_has_log(struct grub_command *cmd __attribute__ ((unused)),
      int argc __attribute__((unused)), char * argv[] __attribute__((unused)))
{
   return grub_error(GRUB_ERR_BUG, "bootload not available");
}


static grub_err_t
miray_cmd_bootlog_write(grub_extcmd_context_t ctxt __attribute__((unused)),
      int argc __attribute__((unused)), char * argv[] __attribute__((unused)))
{
   return grub_error(GRUB_ERR_BUG, "bootlog not available");
}

#endif

static grub_extcmd_t cmd_activate;
static grub_extcmd_t cmd_write;
static grub_command_t cmd_has_log;

//static grub_command_t cmd_test;


GRUB_MOD_INIT(miray_bootlog)
{
  cmd_activate = grub_register_extcmd("miray_bootlog_activate",
                                       miray_cmd_bootlog_activate, 0,
                                       N_("-s envname"),
                                       N_("Activate bootlog"),
                                       options_bootlog_activate);

  cmd_write = grub_register_extcmd("miray_bootlog_write",
                                     miray_cmd_bootlog_write, 0,
                                     N_("-f FILE"),
                                     N_("Write bootlog to disk"),
                                     options_bootlog_write);

  cmd_has_log = grub_register_command("miray_bootlog_has_log",
				      miray_cmd_bootlog_has_log, 
				      0, N_("test if an previous log exists"));

#if 0
  cmd_test = grub_register_command("miray_bootlog_test",
				      miray_cmd_bootlog_test, 
				      0, N_("temporary test for bootlog"));
#endif
   
}

GRUB_MOD_FINI(miray_bootlog)
{
  grub_unregister_extcmd (cmd_activate);
  grub_unregister_extcmd (cmd_write);
  grub_unregister_command(cmd_has_log);
  
  //grub_unregister_command (cmd_test);
}

