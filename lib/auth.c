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

#include <grub/auth.h>
#include <grub/list.h>
#include <grub/mm.h>
#include <grub/misc.h>
#include <grub/env.h>
#include <grub/time.h>

GRUB_EXPORT(grub_auth_authenticate);
GRUB_EXPORT(grub_auth_register_authentication);
GRUB_EXPORT(grub_auth_check_password);

struct grub_auth_user
{
  struct grub_auth_user *next;
  char *name;
  grub_auth_callback_t callback;
  void *arg;
  int authenticated;
};

struct grub_auth_user *users = NULL;

grub_err_t
grub_auth_register_authentication (const char *user,
				   grub_auth_callback_t callback,
				   void *arg)
{
  struct grub_auth_user *cur;

  cur = grub_named_list_find (GRUB_AS_NAMED_LIST (users), user);
  if (!cur)
    cur = grub_zalloc (sizeof (*cur));
  if (!cur)
    return grub_errno;
  cur->callback = callback;
  cur->arg = arg;
  if (! cur->name)
    {
      cur->name = grub_strdup (user);
      if (!cur->name)
	{
	  grub_free (cur);
	  return grub_errno;
	}
      grub_list_push (GRUB_AS_LIST_P (&users), GRUB_AS_LIST (cur));
    }
  return GRUB_ERR_NONE;
}

grub_err_t
grub_auth_unregister_authentication (const char *user)
{
  struct grub_auth_user *cur;
  cur = grub_named_list_find (GRUB_AS_NAMED_LIST (users), user);
  if (!cur)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "user '%s' not found", user);
  if (!cur->authenticated)
    {
      grub_free (cur->name);
      grub_list_remove (GRUB_AS_LIST_P (&users), GRUB_AS_LIST (cur));
      grub_free (cur);
    }
  else
    {
      cur->callback = NULL;
      cur->arg = NULL;
    }
  return GRUB_ERR_NONE;
}

grub_err_t
grub_auth_authenticate (const char *user)
{
  struct grub_auth_user *cur;

  cur = grub_named_list_find (GRUB_AS_NAMED_LIST (users), user);
  if (!cur)
    cur = grub_zalloc (sizeof (*cur));
  if (!cur)
    return grub_errno;

  cur->authenticated = 1;

  if (! cur->name)
    {
      cur->name = grub_strdup (user);
      if (!cur->name)
	{
	  grub_free (cur);
	  return grub_errno;
	}
      grub_list_push (GRUB_AS_LIST_P (&users), GRUB_AS_LIST (cur));
    }

  return GRUB_ERR_NONE;
}

grub_err_t
grub_auth_deauthenticate (const char *user)
{
  struct grub_auth_user *cur;
  cur = grub_named_list_find (GRUB_AS_NAMED_LIST (users), user);
  if (!cur)
    return grub_error (GRUB_ERR_BAD_ARGUMENT, "user '%s' not found", user);
  if (!cur->callback)
    {
      grub_free (cur->name);
      grub_list_remove (GRUB_AS_LIST_P (&users), GRUB_AS_LIST (cur));
      grub_free (cur);
    }
  else
    cur->authenticated = 0;
  return GRUB_ERR_NONE;
}

struct is_authenticated_closure
{
  const char *userlist;
  const char *superusers;
};

static int
is_authenticated_hook (grub_list_t item, void *closure)
{
  struct is_authenticated_closure *c = closure;
  const char *name;
  if (!((struct grub_auth_user *) item)->authenticated)
    return 0;
  name = ((struct grub_auth_user *) item)->name;

  return (c->userlist && grub_strword (c->userlist, name))
    || grub_strword (c->superusers, name);
}

static int
is_authenticated (const char *userlist)
{
  struct is_authenticated_closure c;

  c.userlist = userlist;
  c.superusers = grub_env_get ("superusers");

  if (!c.superusers)
    return 1;

  return grub_list_iterate (GRUB_AS_LIST (users), is_authenticated_hook, &c);
}

struct grub_auth_check_password_closure
{
  const char *login;
  struct grub_auth_user *cur;
};

static int
grub_auth_check_password_hook (grub_list_t item, void *closure)
{
  struct grub_auth_check_password_closure *c = closure;
  if (grub_strcmp (c->login, ((struct grub_auth_user *) item)->name) == 0)
    c->cur = (struct grub_auth_user *) item;
  return 0;
}

int
grub_auth_check_password (const char *userlist, const char *login,
			  const char *password)
{
  static unsigned long punishment_delay = 1;
  int result = 0;
  struct grub_auth_check_password_closure c;

  if (is_authenticated (userlist))
    {
      punishment_delay = 1;
      return 1;
    }

  if (! login)
    return 0;

  c.login = login;
  c.cur = NULL;
  grub_list_iterate (GRUB_AS_LIST (users), grub_auth_check_password_hook, &c);
  if ((c.cur) && (! c.cur->callback (login, password, c.cur->arg)))
    result = (is_authenticated (userlist));
  else
    grub_errno = 0;

  if (result)
    punishment_delay = 1;
  else
    {
      grub_sleep (punishment_delay);
      if (punishment_delay < GRUB_ULONG_MAX / 2)
	punishment_delay *= 2;
    }

  return result;
}
