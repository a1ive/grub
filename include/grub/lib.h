/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2009  Free Software Foundation, Inc.
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

#ifndef GRUB_LIB_HEADER
#define GRUB_LIB_HEADER	1

#include <grub/types.h>
#include <grub/file.h>
#include <grub/err.h>
#include <grub/term.h>

/* The type of a completion item.  */
enum grub_completion_type
  {
    GRUB_COMPLETION_TYPE_COMMAND,
    GRUB_COMPLETION_TYPE_DEVICE,
    GRUB_COMPLETION_TYPE_PARTITION,
    GRUB_COMPLETION_TYPE_FILE,
    GRUB_COMPLETION_TYPE_ARGUMENT
  };
typedef enum grub_completion_type grub_completion_type_t;

/* Defined in `completion.c'.  */
char *grub_complete (char *buf, int *restore,
		       void (*hook) (const char *item,
				     grub_completion_type_t type,
				     int count));

/* Defined in `misc.c'.  */
void grub_wait_after_message (void);
grub_err_t grub_print_device_info (const char *name);

/* Defined in `print_ucs4.c'.  */
void grub_print_ucs4 (const grub_uint32_t * str,
		      const grub_uint32_t * last_position,
		      struct grub_term_output *term);

/* Defined in `md5_password.c'.  */
/* If CHECK is true, check a password for correctness. Returns 0
   if password was correct, and a value != 0 for error, similarly
   to strcmp.
   If CHECK is false, crypt KEY and save the result in CRYPTED.
   CRYPTED must have a salt.  */
int grub_md5_password (const char *key, char *crypted, int check);

/* For convenience.  */
#define grub_check_md5_password(key,crypted)	\
  grub_md5_password((key), (crypted), 1)
#define grub_make_md5_password(key,crypted)	\
  grub_md5_password((key), (crypted), 0)

/* Defined in `getline.c'.  */
char *grub_getline (grub_file_t file);

#define GRUB_DEFAULT_HISTORY_SIZE	50

/* Defined in `history.c'.  */
grub_err_t grub_history_init (int newsize);
grub_uint32_t *grub_history_get (int pos);
void grub_history_set (int pos, grub_uint32_t *s, grub_size_t len);
void grub_history_add (grub_uint32_t *s, grub_size_t len);
void grub_history_replace (int pos, grub_uint32_t *s, grub_size_t len);
int grub_history_used (void);

/* Defined in `crc.c'.  */
grub_uint32_t grub_getcrc32 (grub_uint32_t crc, void *buf, int size);

/* Defined in `hexdump.c'.  */
void hexdump (unsigned long bse,char* buf,int len);

/* Defined in `autolist.c'.  */
struct grub_autolist
{
  struct grub_autolist *next;
  char *name;
  char *value;
};
typedef struct grub_autolist *grub_autolist_t;

grub_autolist_t grub_autolist_load (const char *name);
extern grub_autolist_t grub_autolist_font;

#endif
