/*
 *  fagelmatare-slave
 *    Measures sensor values from sensehat and reports back to master
 *  slave.c
 *    Initialize communication with master and listen for events 
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
#include <signal.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <fgevents.h>

#include "common.h"
#include "log.h"

/* Forward declarations used in this file. */
static int fg_handle_event (void *, struct fgevent *, struct fgevent *);

/* Non-zero means we should exit the program as soon as possible */
static sem_t keep_going;

/* Signal handler for SIGINT, SIGHUP and SIGTERM */
static void
handle_sig (int signum)
{
    struct sigaction new_action;

    sem_post (&keep_going);

    new_action.sa_handler = handle_sig;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = SA_RESTART;

    sigaction (signum, &new_action, NULL);
}

/* Setup termination signals to exit gracefully */
static void
handle_signals ()
{
    struct sigaction new_action, old_action;

    /* Turn off buffering on stdout to directly write to log file */
    setvbuf (stdout, NULL, _IONBF, 0);

    /* Set up the structure to specify the new action. */
    new_action.sa_handler = handle_sig;
    sigemptyset (&new_action.sa_mask);
    new_action.sa_flags = SA_RESTART;
    
    /* Handle termination signals but avoid handling signals previously set
       to be ignored */
    sigaction (SIGINT, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN)
        sigaction (SIGINT, &new_action, NULL);
    sigaction (SIGHUP, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN)
        sigaction (SIGHUP, &new_action, NULL);
    sigaction (SIGTERM, NULL, &old_action);
    if (old_action.sa_handler != SIG_IGN)
        sigaction (SIGTERM, &new_action, NULL);
}

int
main (void)
{
	ssize_t s;
    struct fgevent fgev;
    struct thread_data tdata;

	/* Initialize keep_going as binary semaphore initially 0 */
    sem_init (&keep_going, 0, 0);

    memset (&tdata, 0, sizeof (tdata));

    handle_signals ();

    s = fg_events_client_init_inet (&tdata.etdata, &fg_handle_event, &tdata,
                                    MASTER_IP, MASTER_PORT);
    if (s != 0)
      {
        log_error ("error initializing fgevents");
        return 1;
      }

    fgev.id = 1;
    fgev.writeback = 1;
    fgev.length = 6;
    fgev.payload = malloc (sizeof (int32_t) * fgev.length);
    for (int i = 0; i < fgev.length; i++)
      {
        fgev.payload[i] = (i*3)/2;
      }
    fg_send_event (tdata.etdata.bev, &fgev);

    sem_wait (&keep_going);

    /* **************************************************************** */
    /*                                                                  */
    /*                      Begin shutdown sequence                     */
    /*                                                                  */
    /* **************************************************************** */
    sem_destroy (&keep_going);

    fg_events_client_shutdown (&tdata.etdata);

    return 0;
}

static int
fg_handle_event (void *arg, struct fgevent *fgev, struct fgevent *ansev)
{
    int i;
    struct thread_data *tdata = arg;

    /* Handle error in fgevent */
    if (fgev == NULL)
      {
        log_error_en (tdata->etdata.save_errno, tdata->etdata.error);
        return 0;
      }

    _log_debug ("eventid: %d\n", fgev->id);
    for (i = 0; i < fgev->length; i++)
      {
        _log_debug ("%d\n", fgev->payload[i]);
      }

    ansev->id = 5;
    ansev->writeback = 0;
    ansev->length = 0;
    return 1;    
}