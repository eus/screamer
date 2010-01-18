/******************************************************************************
 * Copyright (C) 2008  Tobias Heer <heer@cs.rwth-aachen.de>                   *
 * Copyright (C) 2009, 2010  Tadeus Prastowo <eus@member.fsf.org>             *
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

#include <stdlib.h> /* exit (...) */
#include <stdio.h> /* printf (...) */
#include <errno.h> /* errno (...) */
#include <unistd.h> /* getopt (...) */
#include <string.h> /* memcpy (...), bzero (...) */
#include <signal.h>
#include <sqlite3.h>
#include "listen.h"
#include "scream-common.h"

static void
usage (char *app_name)
{
  fprintf (stderr, "Usage: %s -d destination -p port"
	   " [-i iterations] [-s sleep] [-b bufsize]\n"
	   "        port: listen port.\n",
	   app_name);
}

static bool is_terminated = FALSE;

static void
terminate (int ignore)
{
  is_terminated = TRUE;
}

int
main (const int argc,  char * const argv[])
{
  struct sigaction sigint_action = { .sa_handler = terminate };
  struct client_record client_records[CLIENT_MAX_NUM];
  struct client_db db = {
    .len = CLIENT_MAX_NUM,
    .recs = client_records,
    .todo_db = NULL,
  };
  uint16_t port = 0;
  int err = SC_ERR_SUCCESS;
  int sock;
  int i;
  int c;
  char *err_msg = NULL;
  struct sockaddr_in server_addr, client_addr;

  bzero (client_records, sizeof (client_records));
  for (i = 0; i < CLIENT_MAX_NUM; i++)
    {
      client_records[i].id = VACANT;
    }

  if (sigaction (SIGINT, &sigint_action, NULL) == -1)
    {
      perror ("Cannot install SIGINT signal handler");
      exit (EXIT_FAILURE);
    }

  while ((c = getopt(argc, argv, "hp:")) != -1 && err == SC_ERR_SUCCESS)
    {
      switch (c)
	{
	  long strnum;
	  int has_error;

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
	case 'h':
	  usage (argv[0]);
	  exit (EXIT_SUCCESS);
	}
    }

  /* check if user provided destination host and port */
  if (port == 0)
    {
      printf ("Port number not set. Using default port number: %d\n",
	      SC_DEFAULT_PORT);
      port = (uint16_t) SC_DEFAULT_PORT;
    }

  /* open in-memory DB */
  if (sqlite3_open ("./db_repo", &db.todo_db))
    {
      sqlite3_perror (db.todo_db, "Cannot open DB");
      if (sqlite3_close (db.todo_db))
	{
	  sqlite3_perror (db.todo_db, "Cannot close DB");
	}
      exit (EXIT_FAILURE);
    }
  if (sqlite3_exec (db.todo_db,
		    "create table if not exists todo (client_id integer not null,"
		    " id integer not null primary key autoincrement, title blob not null,"
		    " deadline blob not null, priority integer not null,"
		    " status blob not null, description blob not null,"
		    " revision integer not null)",
		    NULL,
		    NULL,
		    &err_msg))
    {
      fprintf (stderr, "Cannot create todo table: %s\n", err_msg);
      sqlite3_free (err_msg);
      if (sqlite3_close (db.todo_db))
	{
	  sqlite3_perror (db.todo_db, "Cannot close DB");
	}
      exit (EXIT_FAILURE);
    }
  sqlite3_clean_free (&err_msg);

  /* open socket */
  if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) == -1)
    {
      perror ("Cannot create socket");
      exit (EXIT_FAILURE);
    }

  /* bind to port */
  bzero (&server_addr, sizeof (server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons (port);
  server_addr.sin_addr.s_addr = htonl (INADDR_ANY);

  if (bind (sock, (struct sockaddr *) &server_addr, sizeof (server_addr)) != 0)
    {
      perror ("Cannot bind to socket\n");
      exit (EXIT_FAILURE);
    }
  else
    {
      socklen_t len = sizeof (client_addr);
      scream_packet_general packet;
      ssize_t bytes_received;

      while (!is_terminated && err != SC_ERR_RECV)
	{
	  bytes_received = recvfrom (sock,
				     &packet,
				     sizeof (packet),
				     MSG_PEEK,
				     (struct sockaddr *) &client_addr,
				     &len);
	  
	  if (bytes_received == -1)
	    {
	      perror ("Error in retrieving packet");
	      err = SC_ERR_RECV;
	      continue;
	    }

	  if (len != sizeof (client_addr))
	    {
	      fprintf (stderr, "Invalid sender address\n");
	      err = SC_ERR_WRONGSENDER;
	      remove_packet(sock);
	      continue;
	    }

	  if (is_sane (&packet, bytes_received, sizeof (packet))
	      == FALSE)
	    {
	      err = SC_ERR_PACKET;
	      remove_packet(sock);
	      continue;
	    }

	  err = listener_handle_packet (&client_addr,
					packet.type,
					sock,
					&db);
	}
    }

  for (i = 0; i < CLIENT_MAX_NUM; i++)
    {
      if (client_records[i].data != NULL)
	{
	  free (client_records[i].data);
	  client_records[i].data = NULL;
	}
    }

  close (sock);
  if (sqlite3_close (db.todo_db))
    {
      sqlite3_perror (db.todo_db, "Cannot close DB");
    }

  exit (EXIT_SUCCESS);
}
