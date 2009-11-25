/******************************************************************************
 * Copyright (C) 2008  Tobias Heer <heer@cs.rwth-aachen.de>                   *
 * Copyright (C) 2009  Tadeus Prastowo <eus@member.fsf.org>                   *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining      *
 * a copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including        *
 * without limitation the rights to use, copy, modify, merge, publish,        *
 * distribute, sublicense, and/or sell copies of the Software, and to         *
 * permit persons to whom the Software is furnished to do so, subject to      *
 * the following conditions:                                                  *
 *                                                                            *
 * The above copyright notice and this permission notice shall be             *
 * included in all copies or substantial portions of the Software.            *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,            *
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF         *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.     *
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR          *
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,      *
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR      *
 * OTHER DEALINGS IN THE SOFTWARE.                                            *
 ******************************************************************************/

#include <sys/types.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <stdlib.h> /* exit (...) */
#include <stdio.h> /* printf (...) */
#include <errno.h> /* errno (...) */
#include <unistd.h> /* getopt (...) */
#include "scream.h"

static void
usage (char *app_name)
{
  fprintf (stderr,
	   "Usage: %s -d destination -p port"
	   " [-i iterations] [-s sleep] [-b flood_size] [-l sloppy]\n"
	   "-d destination: IP address or hostname of destination host.\n"
	   "-p port       : destination port number.\n"
	   "-i iterations : number of packets to be sent (0 = infinite).\n"
	   "                    Default is 100 packets.\n"
	   "-s sleep      : sleep time between loops in microsecond.\n"
	   "                    Default is 100 ms.\n"
	   "-b flood_size : size of the packet payload in byte.\n"
	   "                    Default is 1000 bytes.\n"
	   "-t            : test mode (for testing screamer and the server).\n"
	   "-l            : use a sloppy manager.\n",
	   app_name);
}

int
main (int argc, char *argv[])
{
  char *host_name = NULL;
  uint16_t port	= 0;
  size_t iterations = 100;
  unsigned long long sleep_time = 100000; /* measured in microseconds */
  size_t flood_size = 1000; /* measured in bytes */
  bool test_mode = FALSE;
  scream_base_data state; /* basic connection state information */
  scream_packet_result result;

  pthread_t manager_thread; /* responsible for monitoring NICs */
  err_code *manager_thread_rc;
  bool is_manager_careful = TRUE;
  struct channel_db db = {
    .db_len = 0,
    .recs = NULL,
  };
  struct comm_channel primary_channel = {
    .channel = NULL,
    .sock = &state.sock,
    .sock_lock = &state.sock_lock,
    .is_registered = &state.is_registered,
  };
  struct manager_data manager_data = {
    .main_channel = &primary_channel,
    .channels = &db,
    .is_stopped = FALSE,
    .id = &state.id,
    .dest_addr = &state.dest_addr,
  };

  /* extract command line parameters */
  int c;

  while ((c = getopt (argc, argv, "hd:p:i:s:b:tl")) != -1)
    {
      long strnum;
      int has_error;

      switch (c)
	{
	case 'd':
	  host_name = optarg;
	  break;
	case 'p':
	  strnum = eus_strtol (optarg, &has_error, "port number");
	  if (has_error)
	    {
	      exit (EXIT_FAILURE);
	    }
	  if (strnum < 0 || strnum > 65535)
	    {
	      fprintf (stderr,
		       "Error: port number must be between 0 and 65535\n");
	      exit (EXIT_FAILURE);
	    }
	  port = (uint16_t) strnum;
	  break;
	case 'i':
	  strnum = eus_strtol (optarg, &has_error, "iterations");
	  if (has_error)
	    {
	      exit (EXIT_FAILURE);
	    }
	  if (strnum < 0)
	    {
	      fprintf (stderr,
		       "Error: iterations must be a positive integer\n");
	      exit (EXIT_FAILURE);
	    }
	  iterations = (size_t) strnum;
	  break;
	case 's':
	  strnum = eus_strtol (optarg, &has_error, "sleep time");
	  if (has_error)
	    {
	      exit (EXIT_FAILURE);
	    }
	  if (strnum < 0)
	    {
	      fprintf (stderr,
		       "Error: sleep time must be a positive integer\n");
	      exit (EXIT_FAILURE);
	    }
	  sleep_time = strnum;
	  break;
	case 'b':
	  strnum = eus_strtol (optarg, &has_error, "flood size");
	  if (has_error)
	    {
	      exit (EXIT_FAILURE);
	    }
	  if (strnum < 0)
	    {
	      fprintf (stderr,
		       "Error: flood size must be a positive integer\n");
	      exit (EXIT_FAILURE);
	    }
	  flood_size = (size_t) strnum;
	  break;
	case 't':
	  test_mode = TRUE;
	  break;
	case 'l':
	  is_manager_careful = FALSE;
	  break;
	case 'h':
	default:
	  usage (argv[0]);
	  exit (EXIT_SUCCESS);
	}
    }

  /* check if user provided destination host and port */
  if (host_name == NULL)
    {
      fprintf (stderr,
	       "Hostname must be set. Type \"%s -h\" for help.\n",
	       argv[0]);
    
      exit (EXIT_FAILURE);
    }
  if (port == 0)
    {
      printf ("Using default port %d.\n", SC_DEFAULT_PORT);
      port = (uint16_t) SC_DEFAULT_PORT;
    }

  /* init screamer */
  if (scream_init (&state) != SC_ERR_SUCCESS)
    {
      fprintf (stderr, "Initialization failed.\n");
      exit (EXIT_FAILURE);
    }

  /* fill the destination structure */
  if (scream_set_dest (&state, host_name, port) != SC_ERR_SUCCESS)
    {
      fprintf (stderr, "Cannot set destination\n");
      exit (EXIT_FAILURE);
    }

  if (pthread_create (&manager_thread,
		      NULL,
		      (is_manager_careful == TRUE
		       ? start_careful_manager
		       : start_sloppy_manager),
		      &manager_data) != 0)
    {
      perror ("Cannot create the manager thread");
      exit (EXIT_FAILURE);
    }

  /* register to a server */
  if (scream_register (&state, sleep_time, iterations) != SC_ERR_SUCCESS)
    {
      fprintf (stderr, "Cannot register\n");
      exit (EXIT_FAILURE);
    }
  state.is_registered = TRUE;

  /* start flood loop */
  if (scream_pause_loop (&state, sleep_time, flood_size, iterations, test_mode)
      != SC_ERR_SUCCESS)
    {
      printf ("Loop or send error\n");
      exit (EXIT_FAILURE);
    }	

  if (scream_reset (&state, &result) != SC_ERR_SUCCESS)
    {
      fprintf (stderr, "Cannot reset\n");
      exit (EXIT_FAILURE);
    }

  /* send ACK */
  if (pthread_mutex_lock (&state.sock_lock) != 0)
    {
      perror ("Cannot lock state.sock_lock");
      exit (EXIT_FAILURE);
    }
  send_ack (state.sock, &state.dest_addr);
  if (pthread_mutex_unlock (&state.sock_lock) != 0)
    {
      perror ("Cannot unlock state.sock_lock");
      exit (EXIT_FAILURE);
    }

  /* stop manager thread */
  manager_data.is_stopped = TRUE;
	
  print_result (&result);

  pthread_join (manager_thread, (void **) &manager_thread_rc);

  free_channel_db (&db);

  if (*manager_thread_rc == SC_ERR_SUCCESS)
    {
      exit (EXIT_SUCCESS);
    }
  else
    {
      fprintf (stderr, "Manager thread exits with a failure: %d\n",
	       (int) *manager_thread_rc);
      exit (EXIT_FAILURE);
    }
}
