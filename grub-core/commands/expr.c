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
# include <unistd.h>
# include <stdlib.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] =
  {
    {"set", 's', 0, N_("Store the result in a variable."), N_("VARNAME"), ARG_TYPE_STRING},
    {0, 0, 0, 0, 0, 0}
  };

int parse_nbr(char **ps);
int eval_expr_0(char **ps);
int eval_expr_1(char **ps);

static int do_op(int lhs, int rhs, char op)
{
    if (op == '+')
        return (lhs + rhs);
    else if (op == '-')
        return (lhs - rhs);
    else if (op == '*')
        return (lhs * rhs);
    else if (op == '/')
        return (lhs / rhs);
    else if (op == '%')
        return (lhs % rhs);
    else
        return (0);
}

static char *suppr_spaces(char *str)
{
  int i;
  int j;
  char *str2;

  i = 0;
  j = 0;
  str2 = malloc(sizeof(str) + 1);
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

int parse_nbr(char **ps)
{
  int nbr;
  int sign;

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
    nbr = eval_expr_0(ps);
    if ((*ps)[0] == ')')
      *ps = *ps + 1;
    return (sign * nbr);
  }
  while ('0' <= (*ps)[0] && (*ps)[0] <= '9')
  {
    nbr = (nbr * 10) + (*ps)[0] - '0';
    *ps = *ps + 1;
  }
  return (sign * nbr);
}

int eval_expr_0(char **ps)
{
  int lhs;
  int rhs;
  char op;

  lhs = parse_nbr(ps);
  while ((*ps)[0] != '\0' && (*ps)[0] != ')')
  {
    op = (*ps)[0];
    *ps = *ps + 1;
    if (op == '+' || op == '-')
      rhs = eval_expr_1(ps);
    else
      rhs = parse_nbr(ps);
    lhs = do_op(lhs, rhs, op);
  }
  return (lhs);
}

int eval_expr_1(char **ps)
{
  int lhs;
  int rhs;
  char op;

  lhs = parse_nbr(ps);
  while ((*ps)[0] == '*' || (*ps)[0] == '/' || (*ps)[0] == '%')
  {
    op = (*ps)[0];
    *ps = *ps + 1;
    rhs = parse_nbr(ps);
    lhs = do_op(lhs, rhs, op);
  }
  return (lhs);
}

static int eval_expr(char *str)
{
  str = suppr_spaces(str);
  if (str[0] == '\0')
    return (0);
  return (eval_expr_0(&str));
}
  
static grub_err_t
grub_cmd_expr (grub_extcmd_context_t ctxt, int argc, char **args)
{
  struct grub_arg_list *state = ctxt->state;
  int result;
  char str[10];
  
  if (argc < 1)
  return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("expression expected"));
  result = eval_expr(args[0]);
  
  if (state[0].set)
  {
    grub_snprintf (str, 10, "%d", result);
    grub_env_set (state[0].arg, str);
  }
  else
    grub_printf ("%d\n", result);

  return 0;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(expr)
{
  cmd = grub_register_extcmd ("expr", grub_cmd_expr, 0,
			      N_("EXPRESSION"), N_("Evaluate math expressions."),
			      options);
}

GRUB_MOD_FINI(expr)
{
  grub_unregister_extcmd (cmd);
}
