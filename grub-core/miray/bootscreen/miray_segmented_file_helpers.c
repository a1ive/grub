/*
 *  Extention for GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2010,2011 Miray Software <oss@miray.de>
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


#include <grub/err.h>
#include <grub/term.h>
#include <grub/command.h>
#include <grub/mm.h>
#include <grub/i18n.h>
#include "miray_bootscreen.h"
#include "miray_screen.h"

#include <grub/time.h>

int miray_disk_nr_msg(int nr);
int miray_disk_retry_msg(void);
char ** miray_clone_stringlist(char ** list);
void miray_free_stringlist(char ** list);

static const char * _disk_next_msg_default[] =
{
  "Loading has to continue from the next disk.",
  "",
  ">>         PLEASE INSERT DISK #%d         <<",
  "",
  "Press <Enter> to continue or <ESC> to abort.",
  NULL
};

static const char * _disk_error_msg_default[] = 
{
   "A disk read error has occured.",
   "How do you want to proceed?",
   "",
   "Press <ESC> to abort or <Enter> to retry.",
   NULL
};

static char ** _disk_next_msg = 0;
static char ** _disk_error_msg = 0;

int miray_disk_nr_msg(int nr)
{
  int i;
  int count = 0; 
  char ** msg;

  const char ** msg_tpl = _disk_next_msg != 0 ? (const char **)_disk_next_msg
                                              : _disk_next_msg_default;

  while (msg_tpl[count] != 0) count++;
  if (count == 0) return grub_error(GRUB_ERR_BAD_ARGUMENT, "Invalid message template");

  msg = grub_malloc((count + 1) * sizeof(char *));
  if(msg == 0) return grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of memory");

  for (i = 0; i < count; ++i)
  {
    msg[i] = grub_xasprintf(msg_tpl[i], nr);
  }
  msg[count] = 0;

  miray_screen_message_box((const char **)msg, 0);
  miray_free_stringlist(msg);

  int ret = 0;
  while (1)
  {
    int c = grub_getkey_noblock();
    if (c != GRUB_TERM_NO_KEY)
    {
       switch (c) {
       case '\n':
       case '\r':
         ret = 1;
         goto done; 
       case '\e':
         ret = 0;
         goto done;
       }; 
    }
  }

done:
  miray_screen_redraw_text(1);

  return ret;

} 

int miray_disk_retry_msg(void)
{
   const char ** message = _disk_error_msg != 0 ? (const char **)_disk_error_msg : _disk_error_msg_default; 


   while (grub_getkey_noblock() != GRUB_TERM_NO_KEY) {} // Clear keyboard buffer
   miray_screen_message_box(message, 0);

  int ret = 0;
  while (1)
  {
    int c = grub_getkey_noblock();
    if (c != GRUB_TERM_NO_KEY)
    {
       switch (c) {
       case '\n':
       case '\r':
         ret = 1;
         goto done; 
       case '\e':
         ret = 0;
         goto done;
       }; 
    }
  }

done:
   miray_screen_redraw_text(1);

   return ret;
}


/* Creates a copy of list, list must be terminated with NULL */
char ** miray_clone_stringlist(char **list)
{
  int i;
  int count = 0;
  char ** ret;

  if (list == 0)
  {
    grub_error(GRUB_ERR_BAD_ARGUMENT, "NULL pointer");
    return 0;
  }

  while(list[count] != 0) count++; /* count number of lines */

  ret = grub_malloc((count + 1) * sizeof(char *));
  if (ret == 0)
  {
    grub_error(GRUB_ERR_OUT_OF_MEMORY, "Out of memory");
    return 0; 
  } 

  
  for (i = 0; i < count; i++)
  {
    ret[i] = grub_strdup(list[i]);
  }

  ret[count] = 0;
  return ret;
}


void miray_free_stringlist(char **list)
{
  int i = 0; 
  if (list == 0)
  {
    grub_dprintf("miray", "freeing null pointer");
    return;	
  }

  while (list[i] != 0)
  {
    grub_free(list[i]);
    i++;
  }
  
  grub_free(list);
}

static grub_err_t
miray_segfile_msg_next(struct grub_command *cmd __attribute__ ((unused)), 
   int argc , char *argv[])
{
  if (argc == 0) return grub_error(GRUB_ERR_BAD_ARGUMENT, "Missing argument");

  if (_disk_next_msg != 0) miray_free_stringlist(_disk_next_msg);
  _disk_next_msg = miray_clone_stringlist(argv);  

  return GRUB_ERR_NONE;
}

static grub_err_t
miray_segfile_msg_retry(struct grub_command *cmd __attribute__ ((unused)), 
   int argc , char *argv[])
{
  if (argc == 0) return grub_error(GRUB_ERR_BAD_ARGUMENT, "Missing argument");

  if (_disk_error_msg != 0) miray_free_stringlist(_disk_error_msg);
  _disk_error_msg = miray_clone_stringlist(argv);  

  return GRUB_ERR_NONE;
}

static grub_command_t _cmd_msg_next, _cmd_msg_retry;

grub_err_t miray_segfile_init(void)
{
   _cmd_msg_next = grub_register_command ("miray_floppy_msg_next",
      miray_segfile_msg_next, 0, N_("set next floppy message"));

   _cmd_msg_retry = grub_register_command ("miray_floppy_msg_error",
      miray_segfile_msg_retry, 0, N_("set floppy retry message"));

  return GRUB_ERR_NONE;
}

grub_err_t miray_segfile_fini(void)
{
  grub_unregister_command(_cmd_msg_next);
  grub_unregister_command(_cmd_msg_retry);

  if (_disk_next_msg != 0) miray_free_stringlist(_disk_next_msg);
  if (_disk_error_msg != 0) miray_free_stringlist(_disk_error_msg);

  _disk_next_msg = 0;
  _disk_error_msg = 0;

  return GRUB_ERR_NONE;
}

