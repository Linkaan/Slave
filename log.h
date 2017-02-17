/*
 *  log.h
 *    Some macros used for logging error, info and debug messages
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

#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"

extern void log_debug (const char *, ...);

/* If _DEBUG is not defined, we simply replace all _log_debug with nothing */
#ifndef _DEBUG
#define _log_debug(format, ...)
#else
#define _log_debug(format, ...) log_debug (format, ##__VA_ARGS__)
#endif

#define log_error_no_timestamp(msg)\
          fprintf (stderr, "%s: %s: %d: %s: %s\n", __progname,\
                   __FILE__, __LINE__, msg, strerror (errno))

/* The do { ... } while(0) part is a workaround for the issue of using these
   macros on a single line if statement without a body */

/* Simple macro used to print error messages with location */
#define log_error(msg)\
        do\
          {\
            char *timestamp = fetch_timestamp ();\
            if (timestamp)\
              {\
                fprintf (stderr, "[%s] %s: %s: %d: %s: %s\n", timestamp,\
                         __progname, __FILE__, __LINE__, msg,\
                         strerror (errno));\
                free (timestamp);\
              }\
            else\
                log_error_no_timestamp (msg);\
          } while(0)

/* Extension of macro above */
#define log_error_en(en, msg)\
        do { errno = en;log_error (msg); } while(0)

/* Helper function to get current time for logging messages */
static inline char *
fetch_timestamp ()
{    
    struct tm result;
    char *stime;

    stime = malloc (TIMESTAMP_MAX_LENGTH);
    if (stime == NULL)
        log_error_no_timestamp ("malloc failed for timestamp");
    else
      {
        time_t ltime;
        char *p;

        ltime = time (NULL);
        localtime_r (&ltime, &result);
        asctime_r (&result, stime);

        p = strchr (stime, '\n');
		    if (p != NULL)
			      *p = '\0';
      }
    return stime;
}

#endif /* _LOG_H_ */