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
#include <poll.h>
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

#include <events.h>
#include <fgevents.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/event-config.h>
#include <event2/thread.h>

#include "sensors.h"
#include "common.h"
#include "log.h"

/* Forward declarations used in this file. */
static void exit_cb (evutil_socket_t, short, void *);
static void timer_cb (evutil_socket_t, short, void *);

static int start_timer_event (struct event_base *, struct thread_data *);

static int32_t query_temp (struct thread_data *);

static int fg_handle_event (void *, struct fgevent *, struct fgevent *);

/* flag set if sensors initialized */
static int is_sensors_enabled;

static struct event *exev;

/* Signal handler for SIGINT, SIGHUP and SIGTERM */
static void
handle_sig (int signum)
{
    struct sigaction new_action;

    if (exev != NULL)
        event_active (exev, EV_WRITE, 0);
    else
        exit (0);

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
    struct thread_data tdata;
    struct event_base *base;

    memset (&tdata, 0, sizeof (tdata));

    handle_signals ();

    evthread_use_pthreads ();

    tdata.valid_temp = false;

    s = sensors_init ();
    if (s != 0) is_sensors_enabled = 0;
    else is_sensors_enabled = 1;    

    struct event_config *config = event_config_new ();

    base = event_base_new_with_config (config);
    if (base == NULL)
      {
        log_error ("error event_base_new");
        return 1;
      }

    event_config_free (config);

    exev = event_new (base, -1, 0, exit_cb, base);
    if (!exev || event_add (exev, NULL) < 0)
        log_error ("could not create/add exit event");

    s = fg_events_client_init_inet (&tdata.etdata, &fg_handle_event, NULL,
                                &tdata, MASTER_IP, MASTER_PORT, FG_SLAVE);
    if (s != 0)
      {
        log_error_en (s, "error initializing fgevents");
      }

    s = start_timer_event (base, &tdata);
    if (s != 0)
      {
        log_error ("error in start_timer_event");
        return 1;
      }

    /* **************************************************************** */
    /*                                                                  */
    /*                      Begin shutdown sequence                     */
    /*                                                                  */
    /* **************************************************************** */

    fg_events_client_shutdown (&tdata.etdata);

    return 0;
}

static void
exit_cb (evutil_socket_t UNUSED(sig), short UNUSED(events), void *arg)
{
    struct event_base *base = arg;

    event_base_loopexit (base, NULL);
}

static void
timer_cb (evutil_socket_t UNUSED(fd), short UNUSED(what), void *arg)
{
    struct SensorData sensor_data;
    struct thread_data *tdata = arg;
    struct fgevent fgev;
    int sensors_avail;

    if (!is_sensors_enabled && sensors_init ())
      {
        is_sensors_enabled = 1;
      }

    sensors_avail = 0;
    if (is_sensors_enabled)
      {
        memset (&sensor_data, 0, sizeof (struct SensorData));

        // grab 8 samples with 100000 µs inbetween
        if (sensors_grab (&sensor_data, 8, 100000))
          {
            log_error ("failed to grab sensor data");
          }
        else
          {
            sensors_avail = 1;
          }
      }

    fgev.id = FG_SENSOR_DATA;
    fgev.receiver = FG_MASTER;
    fgev.writeback = 0;
    fgev.length = 8;
    fgev.payload = malloc (sizeof (int32_t) * fgev.length);

    memset (fgev.payload, 0, sizeof (int32_t) * fgev.length);

    int32_t tempx10 = query_temp (tdata);

    if (tdata->valid_temp && ++tdata->c_invalidate_temp < MAX_TEMP_AGE)
      {
        fgev.payload[0] = OUTTEMP;
        fgev.payload[1] = tempx10;
      }    

    if (sensors_avail)
      {
        fgev.payload[2] = INTEMP;
        fgev.payload[3] = (int32_t) sensor_data.temperature * 10.0;
        fgev.payload[4] = PRESSURE;
        fgev.payload[5] = (int32_t) sensor_data.pressure * 10.0;
        fgev.payload[6] = HUMIDITY;
        fgev.payload[7] = (int32_t) sensor_data.humidity * 10.0;        
      }

    fg_send_event (&tdata->etdata, &fgev);
}

static int32_t
query_temp (struct thread_data *tdata)
{
    struct fgevent fgev;
    fgev.id = FG_RETRIEVE_TEMP;
    fgev.receiver = FG_AVR;
    fgev.writeback = 1;
    fgev.length = 0;
    fg_send_event (&tdata->etdata, &fgev);
    return tdata->fetched_temp;
}

static int
start_timer_event (struct event_base *base, struct thread_data *tdata)
{
    struct timeval t = { 10, 0 };
    struct event *ev;

    ev = event_new (base, -1, EV_PERSIST, timer_cb, tdata);
    if (!ev || event_add (ev, &t) < 0)
        return -1;

    event_base_dispatch (base);
    return 0;
}

static int
fg_handle_event (void *arg, struct fgevent *fgev,
                 struct fgevent * UNUSED(ansev))
{
    struct thread_data *tdata = arg;

    /* Handle error in fgevent */
    if (fgev == NULL)
      {
        log_error_en (tdata->etdata.save_errno, tdata->etdata.error);
        return 0;
      }

    switch (fgev->id)
      {
        case FG_TEMP_RESULT:
            if (fgev->length > 0)
              {
                tdata->c_invalidate_temp = 0;
                tdata->valid_temp = true;
                tdata->fetched_temp = fgev->payload[0];                
              }
            break;
        default:
            _log_debug ("eventid: %d\n", fgev->id);
            break;                                                          
      }
      
    return 0;
}