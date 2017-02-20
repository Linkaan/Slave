/*
 *  common.h
 *    Common macros and defs used in multiple source files
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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <errno.h>

#include <fgevents.h>

/* Define _GNU_SOURCE for pthread_timedjoin_np and asprintf */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/* Define _DEBUG to enable debug messages */
#define _DEBUG

#define TIMESTAMP_MAX_LENGTH 32

/* Temporary defs before config file is setup */
#define MASTER_IP "10.0.1.1"
#define MASTER_PORT 1337

/* String containing name the program is called with.
   To be initialized by main(). */
extern const char *__progname;

/* Common data structure used by threads */
struct thread_data {
    struct fg_events_data etdata;
};

#endif /* _COMMON_H_ */