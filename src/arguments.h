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

#ifndef __ARGUMENTS_H__
#define __ARGUMENTS_H__

#include "global.h"

#include <stddef.h> /* size_t */

#define _GNU_SOURCE
#include <getopt.h> /* getopt_long, ... */

__BEGIN_CDECLS

struct arguments_option
{
  char *cat;
  int short_opt;
  char *long_opt;
  int has_arg;
  char *arg_name;
  char *desc;
};

struct arguments_definition
{
  void (*print_usage_header)(const char *command);
  boolean_t (*process_option)(struct arguments_definition *def, 
			      int opt, 
			      const char *optarg, 
			      int argc, 
			      char *argv[]);
  boolean_t (*process_non_options)(struct arguments_definition *def,
				   int optind,
				   int argc, 
				   char *argv[]);

  struct arguments_option *options;
  void *user_data;
};

boolean_t get_arguments (struct arguments_definition *def, 
			 int argc, 
			 char *argv[]);

__END_CDECLS

#endif /* #ifndef __ARGUMENTS_H__ */
