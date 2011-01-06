/* -*- mode: C; c-file-style: "gnu" -*- */
/*
 * Copyright (C) 2010 Dirk Dierckx <dirk.dierckx@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <assert.h> /* assert */
#include <errno.h>
#include <stdlib.h> /* atoi, strtod */
#include <stdio.h> /* printf, fprintf, sprintf */
#include <string.h> /* memset, strcmp, strlen */
#include <sys/types.h> /* open */
#include <sys/stat.h> /* open */
#include <fcntl.h> /* open */
#include <unistd.h> /* STDIN_FILENO */
#include <ctype.h> /* isalnum */

#include "arguments.h"

static
size_t
_print_option (char *buffer, struct arguments_option *option)
{
  boolean_t has_short = isalnum (option->short_opt);
  boolean_t has_long = NULL != option->long_opt;
  boolean_t has_arg = (no_argument != option->has_arg 
		       && NULL != option->arg_name);

  assert (has_short || has_long);

  sprintf (buffer, "%c%c%s%s%s%s%s",
	   (has_short ? '-' : ' '),
	   (has_short ? option->short_opt : ' '),
	   (has_short && has_long
	    ? ", "
	    : (has_long ? "  " : "")),
	   (has_long ? "--" : ""),
	   (has_long ? option->long_opt : ""),
	   (has_arg ? "=" : ""),
	   (has_arg ? option->arg_name : ""));

  return strlen (buffer);
}

void
print_usage (struct arguments_definition *def, 
	     const char *command)
{
  char *prev_cat = NULL;
  size_t i, len, max_len = 0;
  char option[80], padding[80];
  
  if (NULL != def->print_usage_header)
    def->print_usage_header (command);
  else if (NULL == def->options[0].cat)
    printf ("Usage: %s\n", command);
  else
    printf ("Usage: %s [OPTION]...\n", command);
  
  for (i = 0;NULL != def->options[i].cat;++i) {
    len = _print_option (option, def->options + i);
    if (len > max_len)
      max_len = len;
  }
  
  memset (padding, ' ', sizeof (padding));
  padding[sizeof (padding) - 1] = '\0';
  
  for (i = 0;NULL != def->options[i].cat;++i) {
    if (NULL == prev_cat || 0 != strcmp (prev_cat, def->options[i].cat)) {
      printf ("\n%s:\n", def->options[i].cat);
      prev_cat = def->options[i].cat;
    }
    len = _print_option (option, def->options + i);
    padding[max_len - len] = '\0';
    printf ("  %s%s  %s\n", option, padding, def->options[i].desc);
    padding[max_len - len] = ' ';
  }
  printf ("\n");
}

boolean_t
get_arguments (struct arguments_definition *def, int argc, char *argv[])
{
  boolean_t go_on = FALSE;
  size_t i, count = 0;
  struct option *opts;
  char *optstring;

  assert (NULL != def);

  for (i = 0;NULL != def->options[i].cat;++i)
    if (NULL != def->options[i].long_opt)
      ++count;

  opts = (struct option *) malloc (sizeof (struct option) * (count + 1));
  if (NULL == opts) {
    fprintf (stderr, "Could not allocate memory because: %s\n",
             strerror (errno));
  } else {
    memset (opts, 0, sizeof (struct option) * (count + 1));

    for (i = 0;NULL != def->options[i].cat;++i)
      if (isalnum (def->options[i].short_opt))
        ++count;
    optstring = (char *) malloc ((count + 1) * 3);
    if (NULL == optstring) {
      fprintf (stderr, "Could not allocate memory because: %s\n",
               strerror (errno));
    } else {
      size_t s = 0, l = 0;
      int opt;

      memset (optstring, 0, (count + 1) * 3);
      for (i = 0;NULL != def->options[i].cat;++i) {
        if (isalnum (def->options[i].short_opt)) {
          optstring[s++] = def->options[i].short_opt;
          switch (def->options[i].has_arg) {
          case optional_argument:
            optstring[s++] = ':';
          case required_argument:
            optstring[s++] = ':';
          case no_argument:
            break;
          default:
            assert (FALSE); /* invalid has_arg value */
            break;
          }
        }

        if (NULL != def->options[i].long_opt) {
          opts[l].name = def->options[i].long_opt;
          opts[l].has_arg = def->options[i].has_arg;
          opts[l].flag = NULL;
          opts[l].val = def->options[i].short_opt;
          ++l;
        }
      }

      for (go_on = TRUE; 
           go_on && -1 != (opt = getopt_long (argc, argv, optstring, opts, NULL));)
        go_on = def->process_option (def, opt, optarg, argc, argv);

      if (go_on && optind < argc)
        go_on = def->process_non_options (def, optind, argc, argv);

      free (optstring);
    }

    free (opts);
  }

  return go_on;
}

