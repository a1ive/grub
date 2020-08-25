/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
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
#include <grub/mm.h>
#include <grub/extcmd.h>
#include <grub/i18n.h>
#include <grub/env.h>
#include <unistd.h>
#include <stdlib.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] =
{
  {"set", 's', 0, N_("Store the result in a variable."),
    N_("VARNAME"), ARG_TYPE_STRING},
  {0, 0, 0, 0, 0, 0}
};

int64_t eval_expr_0 (char **ps);

static int64_t parse_sgn (int32_t sign, int64_t num)
{
  if (sign > 0)
    return num;
  else
    return (0 - num);
}

static int64_t div64s (int64_t n, int64_t d)
{
  if (!d)
  {
    grub_printf ("ERROR: division by zero.\n");
    return 1;
  }
  return grub_divmod64s (n, d, NULL);
}

static int64_t mod64s (int64_t n, int64_t d)
{
  int64_t r;
  if (!d)
  {
    grub_printf ("ERROR: division by zero.\n");
    return 1;
  }
  grub_divmod64s (n, d, &r);
  return r;
}

static int64_t do_op (int64_t lhs, int64_t rhs, char op)
{
  if (op == '+')
    return (lhs + rhs);
  else if (op == '-')
    return (lhs - rhs);
  else if (op == '*')
    return (lhs * rhs);
  else if (op == '/')
    return div64s (lhs, rhs);
  else if (op == '%')
    return mod64s (lhs, rhs);
  else
    return 0;
}

static char *suppr_spaces (char *str)
{
  int64_t i;
  int64_t j;
  char *str2 = NULL;

  i = 0;
  j = 0;
  str2 = malloc (sizeof (str) + 1);
  if (!str2)
    return NULL;
  while (str[i] != '\0')
  {
    if (str[i] != ' ')
    {
      str2[j] = str[i];
      j = j + 1;
    }
    i = i + 1;
  }
  str2[j] = '\0';
  return (str2);
}

static int64_t parse_nbr (char **ps)
{
  int64_t nbr;
  int32_t sign;

  nbr = 0;
  sign = 1;
  if ((*ps)[0] == '+' || (*ps)[0] == '-')
  {
    if ((*ps)[0] == '-')
      sign = -1;
    *ps = *ps + 1;
  }
  if ((*ps)[0] == '(')
  {
    *ps = *ps + 1;
    nbr = eval_expr_0 (ps);
    if ((*ps)[0] == ')')
      *ps = *ps + 1;
    return parse_sgn (sign, nbr);
  }
  while ('0' <= (*ps)[0] && (*ps)[0] <= '9')
  {
    nbr = (nbr * 10) + (*ps)[0] - '0';
    *ps = *ps + 1;
  }
  return parse_sgn (sign, nbr);
}

static int64_t eval_expr_1 (char **ps)
{
  int64_t lhs;
  int64_t rhs;
  char op;

  lhs = parse_nbr (ps);
  while ((*ps)[0] == '*' || (*ps)[0] == '/' || (*ps)[0] == '%')
  {
    op = (*ps)[0];
    *ps = *ps + 1;
    rhs = parse_nbr (ps);
    lhs = do_op (lhs, rhs, op);
  }
  return (lhs);
}

int64_t eval_expr_0 (char **ps)
{
  int64_t lhs;
  int64_t rhs;
  char op;

  lhs = parse_nbr(ps);
  while ((*ps)[0] != '\0' && (*ps)[0] != ')')
  {
    op = (*ps)[0];
    *ps = *ps + 1;
    if (op == '+' || op == '-')
      rhs = eval_expr_1 (ps);
    else
      rhs = parse_nbr (ps);
    lhs = do_op (lhs, rhs, op);
  }
  return (lhs);
}

static int64_t eval_expr (char *str)
{
  int64_t ret;
  char *str2 = NULL;
  str2 = suppr_spaces (str);
  if (!str2)
    return 0;
  str = str2;
  ret = eval_expr_0 (&str);
  grub_free (str2);
  return ret;
}

static grub_err_t
grub_cmd_expr (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  int64_t result;
  char str[32];

  if (argc < 1)
  return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("expression expected"));
  result = eval_expr (args[0]);

  if (state[0].set)
  {
    grub_snprintf (str, 32, "%lld", (long long)result);
    grub_env_set (state[0].arg, str);
  }
  else
    grub_printf ("%lld\n", (long long)result);

  return 0;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(expr)
{
  cmd = grub_register_extcmd ("expr", grub_cmd_expr, 0,
                              N_("[OPTIONS] EXPRESSION"),
                              N_("Evaluate math expressions."),
                              options);
}

GRUB_MOD_FINI(expr)
{
  grub_unregister_extcmd (cmd);
}
