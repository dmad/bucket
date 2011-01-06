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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "global.h"
#include "arguments.h"

#define DEF_FILE_NAME "bucket.out"
#define DEF_BACKUP_NUMBER 5
#define DEF_BUCKET_SIZE "1M"

struct arguments
{
  int src_fd;
  char dest_filename[128];
  boolean_t new_bucket;
  unsigned int backup_buckets;
  size_t overflow_bytesize;
  boolean_t stdout;
};

static
void
_print_usage_header (const char *command)
{
  printf ("Usage: %s [OPTION]... [FILE]\n"
          "Reads FILE (or stdin if no FILE given or when FILE is '-').\n"
          "and sends it to buckets (logrotate comes to mind).\n",
          command);
}

static 
size_t
_parse_size (const char *string)
{
  struct { char symbol; size_t multiplier; } quanta[] = {
    { 'k', 1024 }, { 'K', 1024 }, 
    { 'm', 1024 * 1024 }, { 'M', 1024 * 1024 },
    { 'g', 1024 * 1024 * 1024 }, { 'G', 1024 * 1024 * 1024 },
    { 0, 0 } 
  };
  const char *end = string;
  double value;

  value = NULL == string ? 0.0 : strtod (string, (char **) &end);
  if (end > string && '\0' != *end) { /* check for symbol */
    int i;
    
    for (i = 0;'\0' != quanta[i].symbol;++i) {
      if (*end == quanta[i].symbol) {
        value *= quanta[i].multiplier;
        break;
      }
    }
  }

  return (size_t) value;
}

static
void
_print_version ()
{
  printf ("%s %s\n"
          "\n"
          "Copyright 2010 by Dirk Dierckx <dirk.dierckx@gmail.com>\n"
          "This is free software; see the source for copying conditions.\n"
          "There is NO warranty; not even for MERCHANTABILITY or FITNESS\n"
          "FOR A PARTICULAR PURPOSE.\n",
          PACKAGE, VERSION);
}

static
boolean_t
_process_option (struct arguments_definition *def,
		 int opt,
		 const char *optarg,
		 int argc,
		 char *argv[])
{
  struct arguments *args = def->user_data;
  boolean_t go_on = TRUE;
  
  switch (opt) {
  case '?': /* invalid option */
    go_on = FALSE;
    break;
  case ':': /* invalid argument */
  case 'h':
    print_usage (def, argv[0]);
    go_on = FALSE;
    break;
  case 'V':
    _print_version ();
    go_on = FALSE;
    break;
  case 'f':
    strncpy (args->dest_filename, optarg, sizeof (args->dest_filename));
    args->dest_filename[sizeof (args->dest_filename) - 1] = '\0';
    break;
  case 'n':
    args->new_bucket = TRUE;
    break;
  case 'b':
    args->backup_buckets = atoi (optarg);
    break;
  case 's':
    args->overflow_bytesize = _parse_size (optarg);
    break;
  case 'c':
    args->stdout = TRUE;
    break;
  default: /* unhandled option */
    fprintf (stderr, "Unhandled option %d\n", opt);
    go_on = FALSE;
    break;
  }

  return go_on;
}

static
boolean_t
_process_non_options (struct arguments_definition *def,
		      int optind,
		      int argc,
		      char *argv[])
{
  struct arguments *args = def->user_data;
  boolean_t go_on = TRUE;
  
  if (0 != strcmp ("-", argv[optind])) {
    args->src_fd = open (argv[optind], O_RDONLY);
    if (-1 == args->src_fd) {
      fprintf (stderr, "Could not open '%s' because: %s\n",
               argv[optind], strerror (errno));
      return FALSE;
    }
  } else
    args->src_fd = STDIN_FILENO;
  
  return go_on;
}

static
boolean_t
_get_arguments (struct arguments *args, int argc, char *argv[])
{
  struct arguments_definition def;
  struct arguments_option options[] = {
    { "Output", 'f', "file", required_argument, "NAME",
      "filename of bucket file" },
    { "Output", 'n', "new-bucket", no_argument, NULL,
      "force creation of a new bucket file" },
    { "Output", 'b', "backup", required_argument, "NUMBER",
      "number of backup buckets" },
    { "Output", 's', "size", required_argument, "SIZE",
      "size of bucket in bytes" },
    { "Output", 'c', "stdout", no_argument, NULL,
      "write also on standard output" },
    { "Miscellaneous", 'V', "version", no_argument, NULL,
      "print version information and exit" },
    { "Miscellaneous", 'h', "help", no_argument, NULL,
      "display this help and exit" },
    { NULL, 0, NULL, 0, NULL, NULL }
  };
  
  memset (&def, 0, sizeof (def));
  
  def.print_usage_header = &_print_usage_header;
  def.process_option = &_process_option;
  def.process_non_options = &_process_non_options;
  def.options = options;
  
  memset (args, 0, sizeof (args));
  
  args->src_fd = STDIN_FILENO;
  strcpy (args->dest_filename, DEF_FILE_NAME);
  args->new_bucket = FALSE; /* append if possible */
  args->backup_buckets = DEF_BACKUP_NUMBER;
  args->overflow_bytesize = _parse_size (DEF_BUCKET_SIZE);
  args->stdout = FALSE;
  
  def.user_data = args;

  return get_arguments (&def, argc, argv);
}

static
boolean_t
_backup_bucket (struct arguments *args)
{
  boolean_t retval = TRUE;
  char src[sizeof(args->dest_filename) + 64];
  char dest[sizeof(args->dest_filename) + 64];
  unsigned int dest_index;
  struct stat s;

  for (dest_index = args->backup_buckets;
       retval && dest_index > 0;
       --dest_index) {
      if (dest_index > 1)
	sprintf (src, "%s.%u", args->dest_filename, dest_index - 1);
      else
	strcpy (src, args->dest_filename);
      sprintf (dest, "%s.%u", args->dest_filename, dest_index);

      if (0 == stat (src, &s) && S_ISREG (s.st_mode)) { /* file exists */
	if (0 != rename (src, dest)) {
	  fprintf (stderr, "Could not rename '%s' to '%s' because: %s\n",
		   src, dest, strerror (errno));
	  retval = FALSE;
	}
      }
  }

  return retval;
}

int
main (int argc, char *argv[])
{
  struct arguments args;

  if (_get_arguments (&args, argc, argv)) {
    size_t buffer_size = 1024 * 32;
    int ofd = -1;
    boolean_t busy = TRUE;
    void *buffer;

    if (args.overflow_bytesize > 0)
      if (buffer_size > args.overflow_bytesize)
	buffer_size = args.overflow_bytesize;

    buffer = malloc (buffer_size);
    if (NULL == buffer) {
      fprintf (stderr, "Could not allocate read buffer because: %s\n",
	       strerror (errno));
      busy = FALSE;
    }

    while (busy) {
      size_t bucket_size = 0;
      boolean_t overflow = FALSE;
      
      {
	struct stat s;
	boolean_t new_bucket = TRUE;

	if (!args.new_bucket
	    && 0 == stat (args.dest_filename, &s) 
	    && S_ISREG (s.st_mode)
	    && (0 == args.overflow_bytesize /* no constraint on size */
		|| s.st_size < args.overflow_bytesize)) {
	  new_bucket = FALSE;
	  ofd = open (args.dest_filename, O_WRONLY);
	  if (-1 == ofd) { /* bail out with error */
	    fprintf (stderr, "Could not open '%s' for writing because: %s\n",
		     args.dest_filename, strerror (errno));
	    busy = FALSE;
	  } else if ((off_t) -1 == (bucket_size = lseek (ofd, 0, SEEK_END))) {
	    fprintf (stderr, 
		     "Could not seek to the end of '%s' because: %s\n",
		     args.dest_filename, strerror (errno));
	    busy = FALSE;
	  }
	}

	if (busy && new_bucket) {
	  if (!_backup_bucket (&args)) { /* bail out with error */
	    fprintf (stderr, "Failed to backup\n");
	    busy = FALSE;
	  } else {
	    ofd = open (args.dest_filename, O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
	    if (-1 == ofd) { /* bail out with error */
	      fprintf (stderr, "Could not open '%s' for writing because: %s\n", 
		       args.dest_filename, strerror (errno));
	      busy = FALSE;
	    }
	  }
	}
      }

      while (busy && !overflow) {
	size_t max_read_size;
	ssize_t read_size;

	if (0 == args.overflow_bytesize) /* no constraint on size */
	  max_read_size = buffer_size;
	else {
	  size_t remain = args.overflow_bytesize - bucket_size;
	  
	  max_read_size = (remain < buffer_size ? remain : buffer_size);
	}

	read_size = read (args.src_fd, buffer, max_read_size); 
	if (-1 == read_size) {
	  fprintf (stderr, "Could not read from input because: %s\n",
		   strerror (errno));
	  busy = FALSE;
	} else if (0 == read_size) { /* EOF */
	  busy = FALSE; 
	} else if (read_size > 0) {
	  
	  if (args.stdout) {
	    if (write (STDOUT_FILENO, buffer, read_size) != read_size) {
	      fprintf (stderr, "Could not write to stdout because: %s\n",
		       strerror (errno));
	      /* we do no terminate in this case !!! */
	    }
	  }
	      
	  if (write (ofd, buffer, read_size) != read_size) {
	    fprintf (stderr, "Could not write to '%s' because: %s\n",
		     args.dest_filename, strerror (errno));
	    busy = FALSE;
	  } else {
	    bucket_size += read_size;
	    overflow = (bucket_size >= args.overflow_bytesize);
	  }
	}
      }

      if (ofd > 0) {
	close (ofd);
	ofd = -1;
      }
    }

    if (STDIN_FILENO != args.src_fd)
      close (args.src_fd);

    if (NULL != buffer)
      free (buffer);
  }

  return 0;
}
