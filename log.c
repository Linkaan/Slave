/*
 *  log.c
 *    Defines functions to log messages with vargs
 *****************************************************************************
 *  This file is part of Fågelmataren, an embedded project created to learn
 *  Linux and C. See <https://github.com/Linkaan/Fagelmatare>
 *  Copyright (C) 2015-2017 Linus Styrén
 *
 *  Fågelmataren is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the Licence, or
 *  (at your option) any later version.
 *
 *  Fågelmataren is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with Fågelmataren.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "log.h"

/* This function is prints a debug message with timestamp */
void
log_debug (const char *format, ...)
{
    char *timestamp = fetch_timestamp ();
    if (timestamp)
      {
		    fprintf (stdout, "[DEBUG: %s] ", timestamp);		
        free (timestamp);
      }

    va_list args;
    va_start (args, format);		
    vfprintf (stdout, format, args);
    va_end (args);
}