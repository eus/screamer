/******************************************************************************
 * Copyright (C) 2008 by Tobias Heer <heer@cs.rwth-aachen.de>                 *
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

#include <errno.h> /* errno */
#include <stdio.h> /* printf (...) */
#include <stdlib.h> /* malloc (...) */
#include <string.h> /* memcpy (...), bzero (...) */
#include <unistd.h> /* usleep (...) */
#include <netdb.h> /* gethostbyname (...) */
#include <arpa/inet.h> /* inet_ntoa (...) */
#include <time.h> /* time (...) */
#include <assert.h> /* assert (...) */
#include "scream-common.h" /* common headers and definitions */
#include "scream.h"

err_code
scream_init (scream_base_data *state)
{
  printf ("Initializing basic connection state information\n");

  /* clear struct contents (set contents to 0)) */
  bzero (state, sizeof (scream_base_data));

  /* init random number generator */
  srand ((unsigned int) time (NULL));

  return SC_ERR_SUCCESS;
}

err_code
scream_set_dest (scream_base_data *state,
		 const char *host_name,
		 uint16_t port)
{
  struct hostent *hp = NULL;

  assert (state!= NULL);
  assert (host_name != NULL);

  if ((state->sock = socket (AF_INET, SOCK_DGRAM, 0)) == -1)
    {
      perror ("Cannot create socket");
      return SC_ERR_SOCK;
    }

  /* resolve host name to IP address */
  hp = gethostbyname (host_name);

  if (hp == NULL)
    {
      perror ("Cannot resolve host name");

      return SC_ERR_SOCK;
    }
    
  /* fill sockaddr struct in state struct */
  state->dest_addr.sin_family = AF_INET;
  state->dest_addr.sin_port = htons (port);
  memcpy (&state->dest_addr.sin_addr, hp->h_addr, hp->h_length);

  printf ("Send to: %s:%d\n", inet_ntoa (state->dest_addr.sin_addr), port);

  return SC_ERR_SUCCESS;
}

err_code
scream_register (const scream_base_data *state,
		 unsigned long long sleep_time,
		 int iterations)
{
  scream_packet_register register_packet =
    {
      .type = SC_PACKET_REGISTER,
      .sleep_time = {
	.sec = htonl (SEC_PART (sleep_time)),
	.usec = htonl (USEC_PART (sleep_time)),
      },
      .amount = htonl (iterations),
    };
  scream_packet_ack ack = {
    .type = SC_PACKET_ACK,
  };
  struct timeval timeout = {
    .tv_sec = SEC_PART (TIMEOUT),
    .tv_usec = USEC_PART (TIMEOUT),
  };

  return scream_send_and_wait_for (&register_packet,
				   sizeof (register_packet),
				   &ack,
				   sizeof (ack),
				   "Registering to",
				   state,
				   &timeout);
}

err_code
scream_pause_loop (scream_base_data *state,
		   int sleep_time,
		   size_t flood_size,
		   int iterations,
		   bool test_mode)
{
  err_code err = SC_ERR_SUCCESS;
  int i, j;
  size_t packet_size = sizeof (scream_packet_flood) + flood_size;
  scream_packet_flood *packet = malloc (packet_size);
  bool last_packet_reordered = FALSE; /* test mode modifiers */

  assert(state != NULL);

  if (packet == NULL)
    {
      fprintf (stderr, "Cannot allocate memory for FLOOD packet\n");
      return SC_ERR_NOMEM;
    }

  packet->type = SC_PACKET_FLOOD;
  packet->seq = 0;
  bzero (packet->data, flood_size); /* set packet data to 0*/

  /* loop for some iterations or loop infinitely (depending on iterations) */
  for (i = 0;
       (iterations == 0 || i < iterations) && err == SC_ERR_SUCCESS;
       i++)
    {
      if (test_mode == TRUE && (rand () % 4 == 1))
	{
	  /* test mode: drop packet with 1/4 chance */
	  printf ("Dropped packet %4d of %4d: ", i + 1, iterations);
	  err = SC_ERR_SUCCESS;
	}
      else
	{
	  /* we send something */
	  if (test_mode == TRUE)
	    {
	      if (last_packet_reordered == FALSE && (rand () % 4 == 1))
		{
		  /* reorder packets with 25% chance */
		  j = i + 1;
		  last_packet_reordered = TRUE;
		}
	      else if (last_packet_reordered == TRUE)
		{
		  j = i - 1;
		  last_packet_reordered = FALSE;
		}
	      else
		{
		  j = i; /* no reordering this time */
		}
	    }
	  else
	    {
	      j = i; /* normal operation*/
	    }

	  printf ("Sending packet %4d of %4d: ", j + 1, iterations);
	  packet->seq = htons (j);
	  err = scream_send (state, packet, packet_size);
	}

      /* sleep for sleep_time milliseconds */
      if (err == SC_ERR_SUCCESS)
	{
	  state->num_packets++;
	  printf ("Sleep for %d us\n", sleep_time);
	  usleep(sleep_time);
	}
    }

  free (packet);

  return err;
}

err_code
scream_reset (const scream_base_data *state,
	      scream_packet_result *result)
{
  scream_packet_reset reset = { .type = SC_PACKET_RESET };
  struct timeval timeout = {
    .tv_sec = SEC_PART (TIMEOUT),
    .tv_usec = USEC_PART (TIMEOUT),
  };

  result->type = SC_PACKET_RESULT;

  return scream_send_and_wait_for (&reset,
				   sizeof (reset),
				   (scream_packet_general *) result,
				   sizeof (*result),
				   "Reset",
				   state,
				   &timeout);
}

err_code
scream_send (const scream_base_data *state,
	     const void *buffer,
	     size_t buffer_size)
{
  assert (state != NULL);
  assert (buffer != NULL);

  /* send packet to destination host and do some error handling */
  if (sendto (state->sock,
	      buffer,
	      buffer_size,
	      0,
	      (struct sockaddr *) &state->dest_addr,
	      sizeof (state->dest_addr)) < 0)
    {
      printf ("Send failed: %s\n", strerror (errno));
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
scream_recv (const scream_base_data *state,
	     void *buffer,
	     size_t buffer_size)
{
  struct sockaddr_in send_from;
  socklen_t send_from_len;

  assert(state != NULL);
  assert(buffer != NULL);

  /* receive packet from destination host and do some error handling */
  send_from_len = sizeof (send_from);

  if (recvfrom (state->sock,
		buffer,
		buffer_size,
		0,
		(struct sockaddr *) &send_from,
		&send_from_len) < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
	{
	  return SC_ERR_COMM;
	}
      else
	{
	  perror ("Receive failed");
	  return SC_ERR_RECV;
	}
    }

  if (send_from_len != sizeof (send_from) /* invalid IPv4 address */
      || memcmp (&send_from, /* UDP packet not from corresponding server */
		 &state->dest_addr,
		 sizeof (send_from)) != 0)
    {
      return SC_ERR_WRONGSENDER;
    }

  return SC_ERR_SUCCESS;
}

err_code
scream_close (scream_base_data *state)
{
  assert (state != NULL);

  if (close (state->sock) !=0 )
    {
      return SC_ERR_SOCK;
    }

  state->sock = -1;

  return SC_ERR_SUCCESS;
}

void
print_result (const scream_packet_result *result)
{
  printf ("Successful flood packets: %u\n"
	  "The widest gap          : %u\n"
	  "The number of gaps      : %u\n"
	  "Is out of order         : %s\n"
	  "Minimum latency         : %u.%06u s\n"
	  "Maximum latency         : %u.%06u s\n"
	  "Average latency         : %u.%06u s\n",
	  (unsigned) ntohl (result->recvd_packets),
	  (unsigned) ntohl (result->max_gap),
	  (unsigned) ntohl (result->num_of_gaps),
	  result->is_out_of_order ? "Yes" : "No",
	  (unsigned) ntohl (result->min_latency.sec),
	  (unsigned) ntohl (result->min_latency.usec),
	  (unsigned) ntohl (result->max_latency.sec),
	  (unsigned) ntohl (result->max_latency.usec),
	  (unsigned) ntohl (result->avg_latency.sec),
	  (unsigned) ntohl (result->avg_latency.usec));
}

err_code
scream_send_and_wait_for (const void *send_what,
			  size_t send_what_len,
			  scream_packet_general *wait_for_what,
			  size_t wait_for_what_len,
			  const char *wait_msg,
			  const scream_base_data *state,
			  const struct timeval *timeout)
{
  err_code rc = SC_ERR_SUCCESS;
  scream_packet_type expected_reply = wait_for_what->type;

  if ((rc = set_timeout (state->sock, timeout)) != SC_ERR_SUCCESS)
    {
      return rc;
    }

  do
    {
      printf ("%s %s:%d ... ",
	      wait_msg,
	      inet_ntoa (state->dest_addr.sin_addr),
	      ntohs (state->dest_addr.sin_port));

      rc = scream_send (state,
			send_what,
			send_what_len);
      if (rc != SC_ERR_SUCCESS)
	{
	  return rc;
	}
      bzero (wait_for_what, wait_for_what_len);
      rc = scream_recv (state, wait_for_what, wait_for_what_len);
      if (rc == SC_ERR_COMM)
	{
	  printf ("[TIMEOUT]\n");
	}
      else if (rc == SC_ERR_WRONGSENDER
	       || (rc == SC_ERR_SUCCESS
		   && wait_for_what->type != expected_reply))
	{
	  printf ("[INVALID PACKET]\n");
	}
      else if (rc == SC_ERR_SUCCESS)
	{
	  printf ("[SUCCESS]\n");
	}
      else
	{
	  return rc;
	}
    }
  while (rc != SC_ERR_SUCCESS || wait_for_what->type != expected_reply);

  rc = unset_timeout (state->sock);
  
  return rc;
}
