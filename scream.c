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

#include <sys/time.h> /* gettimeofday (...) */
#include <ifaddrs.h> /* getifaddrs (...) */
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

#ifndef __USE_ISOC99
#define __USE_ISOC99
#endif
#include <limits.h> /* ULLONG_MAX */

err_code
scream_init (scream_base_data *state)
{
  /* clear struct contents (set contents to 0)) */
  bzero (state, sizeof (scream_base_data));

  /* init random number generator */
  srand ((unsigned int) time (NULL));

  state->id = rand () % sizeof (uint32_t);

  printf ("Initializing basic connection state information of client %u\n",
	  state->id);

  pthread_mutex_init (&state->sock_lock, NULL);
  state->is_registered = FALSE;

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

  /* resolve host name to IP address */
  hp = gethostbyname (host_name);

  if (hp == NULL)
    {
      perror ("Cannot resolve host name");

      return SC_ERR_SOCK;
    }
    
  /* fill sockaddr struct in state struct */
  bzero (&state->dest_addr, sizeof (state->dest_addr));
  state->dest_addr.sin_family = AF_INET;
  state->dest_addr.sin_port = htons (port);
  memcpy (&state->dest_addr.sin_addr, hp->h_addr, hp->h_length);

  printf ("Send to: %s:%d\n", inet_ntoa (state->dest_addr.sin_addr), port);

  return SC_ERR_SUCCESS;
}

err_code
scream_register (scream_base_data *state,
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
    .tv_sec = SEC_PART (REGISTER_TIMEOUT),
    .tv_usec = USEC_PART (REGISTER_TIMEOUT),
  };

  register_packet.id = htonl (state->id);

  /* hoping that any underlying socket error can eventually be resolved by the 
   * manager thread through the selection of a new channel
   */
  while (scream_send_and_wait_for (&register_packet,
				   sizeof (register_packet),
				   &ack,
				   sizeof (ack),
				   "Registering to",
				   state->sock,
				   &state->sock_lock,
				   &state->dest_addr,
				   &timeout,
				   REGISTER_REPETITION) != SC_ERR_SUCCESS)
    {
      sleep (SEC_PART (REGISTER_TIMEOUT));
    }

  return SC_ERR_SUCCESS;
}

err_code
scream_pause_loop (scream_base_data *state,
		   int sleep_time,
		   size_t flood_size,
		   int iterations,
		   bool test_mode)
{
  err_code err = SC_ERR_SUCCESS;
  int i = 0, j;
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
  while (iterations == 0 || i < iterations)
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

	  /* disregarding any underlying socket error because of hoping that the
	   * manager thread can eventually find the right channel
	   */
	  err = scream_send (state->sock,
			     &state->sock_lock,
			     &state->dest_addr,
			     packet,
			     packet_size);
	}

      printf ("Sleep for %d us\n", sleep_time);
      usleep(sleep_time);

      /* sleep for sleep_time milliseconds */
      if (err == SC_ERR_SUCCESS)
	{
	  state->num_packets++;
	  i++;
	}
    }

  free (packet);

  return err;
}

err_code
scream_reset (scream_base_data *state,
	      scream_packet_result *result)
{
  scream_packet_reset reset = { .type = SC_PACKET_RESET };
  struct timeval timeout = {
    .tv_sec = SEC_PART (RESET_TIMEOUT),
    .tv_usec = USEC_PART (RESET_TIMEOUT),
  };

  result->type = SC_PACKET_RESULT;

  /* hoping that any underlying socket error can eventually be resolved by the 
   * manager thread through the selection of a new channel
   */
  while (scream_send_and_wait_for (&reset,
				   sizeof (reset),
				   (scream_packet_general *) result,
				   sizeof (*result),
				   "Reset",
				   state->sock,
				   &state->sock_lock,
				   &state->dest_addr,
				   &timeout,
				   RESET_REPETITION) != SC_ERR_SUCCESS)
    {
      sleep (SEC_PART (RESET_TIMEOUT));
    }

  return SC_ERR_SUCCESS;
}

err_code
scream_send (int sock,
	     pthread_mutex_t *sock_lock,
	     const struct sockaddr_in *dest_addr,
	     const void *buffer,
	     size_t buffer_size)
{
  err_code rc = SC_ERR_SUCCESS;

  assert (buffer != NULL);

  /* send packet to destination host and do some error handling */
  if (pthread_mutex_lock (sock_lock) != 0)
    {
      perror ("Cannot lock sock_lock for sending");
      return SC_ERR_LOCK;
    }
  if (sendto (sock,
	      buffer,
	      buffer_size,
	      0,
	      (struct sockaddr *) dest_addr,
	      sizeof (*dest_addr)) < 0)
    {
      printf ("Send failed: %s\n", strerror (errno));
      rc = SC_ERR_SEND;
    }
  if (pthread_mutex_unlock (sock_lock) != 0)
    {
      perror ("Cannot unlock sock_lock after sending");
      return SC_ERR_UNLOCK;
    }

  return rc;
}

err_code
scream_send_no_lock (int sock,
		     const struct sockaddr_in *dest_addr,
		     const void *buffer,
		     size_t buffer_size)
{
  assert (buffer != NULL);

  /* send packet to destination host and do some error handling */
  if (sendto (sock,
	      buffer,
	      buffer_size,
	      0,
	      (struct sockaddr *) dest_addr,
	      sizeof (*dest_addr)) < 0)
    {
      printf ("Send failed: %s\n", strerror (errno));
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
scream_recv (int sock,
	     pthread_mutex_t *sock_lock,
	     const struct sockaddr_in *dest_addr,
	     void *buffer,
	     size_t buffer_size)
{
  err_code rc = SC_ERR_SUCCESS;
  struct sockaddr_in send_from;
  socklen_t send_from_len;

  assert(buffer != NULL);

  /* receive packet from destination host and do some error handling */
  send_from_len = sizeof (send_from);

  if (pthread_mutex_lock (sock_lock) != 0)
    {
      perror ("Cannot lock sock_lock for receiving");
      return SC_ERR_LOCK;
    }
  if (recvfrom (sock,
		buffer,
		buffer_size,
		0,
		(struct sockaddr *) &send_from,
		&send_from_len) < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
	{
	  rc = SC_ERR_COMM;
	}
      else
	{
	  perror ("Receive failed");
	  rc = SC_ERR_RECV;
	}
    }
  if (pthread_mutex_unlock (sock_lock) != 0)
    {
      perror ("Cannot unlock sock_lock after receiving");
      return SC_ERR_UNLOCK;
    }

  if (rc != SC_ERR_SUCCESS)
    {
      return rc;
    }

  if (send_from_len != sizeof (send_from) /* invalid IPv4 address */
      || memcmp (&send_from, /* UDP packet not from corresponding server */
		 dest_addr,
		 sizeof (send_from)) != 0)
    {
      return SC_ERR_WRONGSENDER;
    }

  if (is_scream_packet (buffer, buffer_size) == FALSE)
    {
      return SC_ERR_PACKET;
    }

  printf ("Received %s packet from %s:%u\n",
	  get_scream_type_name (((scream_packet_general *) buffer)->type),
	  inet_ntoa (dest_addr->sin_addr),
	  ntohs (dest_addr->sin_port));

  return SC_ERR_SUCCESS;
}

err_code
scream_recv_no_lock (int sock,
		     const struct sockaddr_in *dest_addr,
		     void *buffer,
		     size_t buffer_size)
{
  struct sockaddr_in send_from;
  socklen_t send_from_len;

  assert(buffer != NULL);

  /* receive packet from destination host and do some error handling */
  send_from_len = sizeof (send_from);

  if (recvfrom (sock,
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
		 dest_addr,
		 sizeof (send_from)) != 0)
    {
      return SC_ERR_WRONGSENDER;
    }

  if (is_scream_packet (buffer, buffer_size) == FALSE)
    {
      return SC_ERR_PACKET;
    }

  printf ("Received %s packet from %s:%u\n",
	  get_scream_type_name (((scream_packet_general *) buffer)->type),
	  inet_ntoa (dest_addr->sin_addr),
	  ntohs (dest_addr->sin_port));

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
			  int sock,
			  pthread_mutex_t *sock_lock,
			  const struct sockaddr_in *dest_addr,
			  const struct timeval *timeout,
			  int repetition)
{
#define LOCK()						\
  do							\
    {							\
      if (pthread_mutex_lock (sock_lock) != 0)		\
	{						\
	  perror ("Cannot lock sock_lock");		\
	  return SC_ERR_LOCK;				\
	}						\
    }							\
  while (0)

#define UNLOCK(please_unset)					\
  do								\
    {								\
      if (pthread_mutex_unlock (sock_lock) != 0)		\
	{							\
	  perror ("Cannot unlock sock_lock");			\
	  return SC_ERR_UNLOCK;					\
	}							\
      if (please_unset)						\
	{							\
	  unset_timeout (sock);					\
	}							\
    }								\
  while (0)

  err_code rc = SC_ERR_SUCCESS;
  scream_packet_type expected_reply = wait_for_what->type;
  int i = 0;

  do
    {
      LOCK ();
      if ((rc = set_timeout (sock, timeout)) != SC_ERR_SUCCESS)
	{
	  UNLOCK (0);
	  return rc;
	}

      printf ("%s %s:%d ... ",
	      wait_msg,
	      inet_ntoa (dest_addr->sin_addr),
	      ntohs (dest_addr->sin_port));

      rc = scream_send_no_lock (sock,
				dest_addr,
				send_what,
				send_what_len);
      if (rc != SC_ERR_SUCCESS)
	{
	  UNLOCK (1);
	  return rc;
	}
      bzero (wait_for_what, wait_for_what_len);
      rc = scream_recv_no_lock (sock,
				dest_addr,
				wait_for_what,
				wait_for_what_len);
      if (rc == SC_ERR_COMM)
	{
	  printf ("[TIMEOUT]\n");
	  i++;
	}
      else if (rc == SC_ERR_PACKET
	       || rc == SC_ERR_WRONGSENDER
	       || (rc == SC_ERR_SUCCESS
		   && wait_for_what->type != expected_reply))
	{
	  printf ("[INVALID PACKET]\n");
	  i++;
	}
      else if (rc == SC_ERR_SUCCESS)
	{
	  printf ("[SUCCESS]\n");
	}
      else
	{
	  UNLOCK (1);
	  return rc;
	}

      UNLOCK (1);
    }
  while ((repetition == -1 || i < repetition)
	 && (rc != SC_ERR_SUCCESS || wait_for_what->type != expected_reply));

  if (i == repetition)
    {
      return SC_ERR_COMM;
    }
  
  return rc;

#undef LOCK
#undef UNLOCK
}

err_code
scream_send_and_wait_for_no_lock (const void *send_what,
				  size_t send_what_len,
				  scream_packet_general *wait_for_what,
				  size_t wait_for_what_len,
				  const char *wait_msg,
				  int sock,
				  const struct sockaddr_in *dest_addr,
				  const struct timeval *timeout,
				  int repetition)
{
  err_code rc = SC_ERR_SUCCESS;
  scream_packet_type expected_reply = wait_for_what->type;
  int i = 0;

  if ((rc = set_timeout (sock, timeout)) != SC_ERR_SUCCESS)
    {
      return rc;
    }

  do
    {
      printf ("%s %s:%d ... ",
	      wait_msg,
	      inet_ntoa (dest_addr->sin_addr),
	      ntohs (dest_addr->sin_port));

      rc = scream_send_no_lock (sock,
				dest_addr,
				send_what,
				send_what_len);
      if (rc != SC_ERR_SUCCESS)
	{
	  return rc;
	}
      bzero (wait_for_what, wait_for_what_len);
      rc = scream_recv_no_lock (sock,
				dest_addr,
				wait_for_what,
				wait_for_what_len);
      if (rc == SC_ERR_COMM)
	{
	  printf ("[TIMEOUT]\n");
	  i++;
	}
      else if (rc == SC_ERR_PACKET
	       || rc == SC_ERR_WRONGSENDER
	       || (rc == SC_ERR_SUCCESS
		   && wait_for_what->type != expected_reply))
	{
	  printf ("[INVALID PACKET]\n");
	  i++;
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
  while ((repetition == -1 || i < repetition)
	 && (rc != SC_ERR_SUCCESS || wait_for_what->type != expected_reply));

  rc = unset_timeout (sock);

  if (i == repetition)
    {
      return SC_ERR_COMM;
    }
  
  return rc;
}

void
store_db_record (struct channel_record *rec, struct channel_db *db)
{
  rec->next = db->recs;
  rec->prev = NULL;
  db->recs = rec;

  db->db_len++;
}

void
remove_db_record (struct channel_record *rec, struct channel_db *db)
{
  if (rec->next != NULL)
    {
      rec->next->prev = rec->prev;
    }
  if (db->recs == rec)
    {
      db->recs = rec->next;
    }
  else
    {
      rec->prev->next = rec->next;
    }

  db->db_len--;
}

struct channel_record *
get_channel_by_name (const char *name, const struct channel_db *db)
{
  struct channel_record *itr;
  
  for (itr = db->recs; itr != NULL; itr = itr->next)
    {
      if (strcmp (itr->if_name, name) == 0)
	{
	  return itr;
	}
    }

  return NULL;
}

bool
probe_ifs (struct channel_db *db)
{
  struct ifaddrs *addrs = NULL;
  struct ifaddrs *itr = NULL;
  struct channel_record *rec_itr = NULL;
  struct channel_record *channel;
  bool is_updated = FALSE;
  size_t num_of_expired_record = db->db_len;

  for (rec_itr = db->recs; rec_itr != NULL; rec_itr = rec_itr->next)
    {
      rec_itr->is_expired = TRUE;
    }

  if (getifaddrs (&addrs) == -1)
    {
      perror ("Cannot pool local network interfaces");

      freeifaddrs (addrs);

      return TRUE;
    }

  for (itr = addrs; itr != NULL; itr = itr->ifa_next)
    {
      /* drop non IPv4 address and loopback interface */
      if (itr->ifa_addr->sa_family != AF_INET
	  || strcmp (itr->ifa_name, "lo") == 0)
	{
	  continue;
	}

      channel = get_channel_by_name (itr->ifa_name, db);
      if (channel == NULL) /* New interface */
	{
	  channel = malloc (sizeof (*channel));
	  if (channel == NULL)
	    {
	      fprintf (stderr, "No memory to create a new channel record\n");
	      continue;
	    }
   
	  channel->next = NULL;
	  channel->prev = NULL;
	  channel->if_name = malloc (strlen (itr->ifa_name) + 1);
	  if (channel->if_name == NULL)
	    {
	      fprintf (stderr, "No memory to store an interface name\n");
	      free (channel);
	      continue;
	    }
	  strcpy (channel->if_name, itr->ifa_name);
	  memcpy (&channel->if_addr, itr->ifa_addr, sizeof (channel->if_addr));
	  channel->sock = -1;
	  channel->is_new = TRUE;
	  channel->is_expired = FALSE;
	  channel->is_modified = FALSE;
	  channel->is_connected = FALSE;

	  store_db_record (channel, db);

	  is_updated = TRUE;

	  printf ("\tNew interface found: %s (%s)\n",
		  channel->if_name,
		  inet_ntoa (channel->if_addr.sin_addr));

	  continue;
	}

      channel->is_expired = FALSE;
      num_of_expired_record--;

      /* Old interface */
      if (memcmp (itr->ifa_addr,
		  &channel->if_addr,
		  sizeof (channel->if_addr)) != 0) /* address change */
	{
	  printf ("\tInterface %s changes from %s to",
		  itr->ifa_name,
		  inet_ntoa (channel->if_addr.sin_addr));
	  printf (" %s\n",
		  inet_ntoa (((struct sockaddr_in *) itr->ifa_addr)->sin_addr));

	  memcpy (&channel->if_addr, itr->ifa_addr, sizeof (channel->if_addr));

	  channel->is_modified = TRUE;

	  is_updated = TRUE;
	}
    }

  freeifaddrs (addrs);

  if (num_of_expired_record != 0)
    {
      printf ("\tThere %s %d expired interface%s\n",
	      num_of_expired_record > 1 ? "are" : "is",
	      num_of_expired_record,
	      num_of_expired_record > 1 ? "s" : "");

      is_updated = TRUE;
    }

  if (is_updated == FALSE)
    {
      printf ("\tNo change in the interfaces\n");
    }

  return is_updated;
}

struct channel_record *
check_return_routability (const struct sockaddr_in *dest_addr,
			  const struct channel_db *db,
			  const struct comm_channel *main_channel)
{
  struct channel_record *itr;
  scream_packet_return_routability packet = {
    .type = SC_PACKET_RETURN_ROUTABILITY,
  };
  scream_packet_return_routability ack = {
    .type = SC_PACKET_RETURN_ROUTABILITY_ACK,
  };
  struct timeval timeout = {
    .tv_sec = SEC_PART (RETURN_ROUTABILITY_CHECK_TIMEOUT),
    .tv_usec = USEC_PART (RETURN_ROUTABILITY_CHECK_TIMEOUT),
  };
  struct timeval stopwatch;
  unsigned long long min_check_time = ULLONG_MAX;
  struct channel_record *best_channel = NULL;
  struct sockaddr_in addr = {
    .sin_family = AF_INET,
    .sin_port = 0,
  };

  for (itr = db->recs; itr != NULL; itr = itr->next)
    {
      itr->is_connected = FALSE;

      if (itr->is_expired == TRUE)
	{
	  continue;
	}

      if (itr->is_new == TRUE || itr->is_modified == TRUE)
	{
	  if (itr->is_modified == TRUE
	      && itr->sock != -1
	      && itr != main_channel->channel) /* mess not with main channel */
	    {
	      if (close (itr->sock) == -1)
		{
		  perror ("Close old channel socket by force");
		}
	      itr->sock = -1;
	    }

	  /* start creating and naming the socket */
	  /* the socket used by the screamer in its state is unaffected */
	  itr->sock = socket (AF_INET, SOCK_DGRAM, 0);
	  if (itr->sock == -1)
	    {
	      perror ("Cannot create a channel socket");
	      continue;
	    }

	  addr.sin_addr.s_addr = itr->if_addr.sin_addr.s_addr;
	  if (bind (itr->sock, (struct sockaddr *) &addr, sizeof (addr)) == -1)
	    {
	      perror ("Cannot bind a channel socket");
	      if (close (itr->sock) == -1)
		{
		  perror ("Close socket by force due to binding failure");
		}
	      itr->sock = -1;
	      continue;
	    }
	}

      printf ("On %s (%s):\n", itr->if_name, inet_ntoa (itr->if_addr.sin_addr));
      if (gettimeofday (&stopwatch, NULL) == -1)
	{
	  perror ("Cannot get stopwatch start time");
	  continue;
	}
      itr->checking_time = COMBINE_SEC_USEC (stopwatch.tv_sec,
					     stopwatch.tv_usec);
      if (scream_send_and_wait_for_no_lock (&packet,
					    sizeof (packet),
					    (scream_packet_general *) &ack,
					    sizeof (ack),
					    "\treturn-routability check to",
					    itr->sock,
					    dest_addr,
					    &timeout,
					    RETURN_ROUTABILITY_CHECK_REPETITION)
	  != SC_ERR_SUCCESS)
	{
	  continue;
	}
      if (gettimeofday (&stopwatch, NULL) == -1)
	{
	  perror ("Cannot get stopwatch end time");
	  continue;
	}
      itr->checking_time = (COMBINE_SEC_USEC (stopwatch.tv_sec,
					      stopwatch.tv_usec)
			    - itr->checking_time);

      if (itr->checking_time < min_check_time)
	{
	  min_check_time = itr->checking_time;
	  best_channel = itr;
	}

      printf ("\tChannel %s is connected with checking time %llu.%06llu s\n",
	      inet_ntoa (itr->if_addr.sin_addr),
	      SEC_PART (itr->checking_time),
	      USEC_PART (itr->checking_time));

      itr->is_connected = TRUE;
    }

  return best_channel;
}

void
reset_new_and_modified_flags (struct channel_db *db)
{
  struct channel_record *itr;

  for (itr = db->recs; itr != NULL; itr = itr->next)
    {
      itr->is_new = FALSE;
      itr->is_modified = FALSE;
    }
}

void
remove_expired_channels (struct channel_db *db)
{
  struct channel_record *itr;
  struct channel_db removed_channels = {
    .db_len = 0,
    .recs = NULL,
  };

  for (itr = db->recs; itr != NULL; itr = itr->next)
    {
      if (itr->is_expired == TRUE)
	{
	  store_db_record (itr, &removed_channels);
	  remove_db_record (itr, db);
	}
    }

  free_channel_db (&removed_channels);
}

void
free_channel (struct channel_record *channel)
{
  free (channel->if_name);
  free (channel);
}

void
free_channel_db (struct channel_db *db)
{
  struct channel_record *itr = db->recs;
  struct channel_record *removed_channel;

  while (itr != NULL)
    {
      remove_db_record (itr, db);
      removed_channel = itr;
      itr = itr->next;

      free_channel (removed_channel);
    }  
}

err_code
switch_comm_channel (struct comm_channel *main_channel,
		     struct channel_record *new_channel)	
{
  struct channel_record *old_channel;

  if (pthread_mutex_lock (main_channel->sock_lock) != 0)
    {
      perror ("Cannot lock main_channel->sock_lock");
      return SC_ERR_LOCK;
    }

  if (*main_channel->sock != -1)
    {
      if (close (*main_channel->sock) == -1)
	{
	  perror ("Forcibly close *main_channel->sock");
	}
    }
  old_channel = main_channel->channel;
  main_channel->channel = new_channel;
  if (old_channel != NULL)
    {
      free_channel (old_channel);
    }
  *main_channel->sock = new_channel->sock;

  if (pthread_mutex_unlock (main_channel->sock_lock) != 0)
    {
      perror ("Cannot unlock main_channel->sock_lock");
      return SC_ERR_UNLOCK;
    }

  return SC_ERR_SUCCESS;
}

err_code
switch_comm_channel_no_lock (struct comm_channel *main_channel,
			     struct channel_record *new_channel)
{
  struct channel_record *old_channel;

  if (*main_channel->sock != -1)
    {
      if (close (*main_channel->sock) == -1)
	{
	  perror ("Forcibly close *main_channel->sock");
	}
    }
  old_channel = main_channel->channel;
  main_channel->channel = new_channel;
  if (old_channel != NULL)
    {
      free_channel (old_channel);
    }
  *main_channel->sock = new_channel->sock;

  return SC_ERR_SUCCESS;
}

err_code
get_address (int sock, struct sockaddr_in *name)
{
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof (addr);

  if (getsockname (sock, (struct sockaddr *) &addr, &addr_len) == -1)
    {
      perror ("Cannot retrieve the name of the socket");
      return SC_ERR_NAME;
    }

  if (addr_len == sizeof (addr))
    {
      memcpy (name, &addr, sizeof (addr));
      return SC_ERR_SUCCESS;
    }

  return SC_ERR_NAME;
}

err_code
update_address (int sock,
		uint32_t id,
		const struct sockaddr_in *new_addr,
		const struct sockaddr_in *dest_addr,
		const struct comm_channel *main_channel)
{
  scream_packet_update_address packet = {
    .type = SC_PACKET_UPDATE_ADDRESS,
    .id = id,
    .sin_addr = new_addr->sin_addr.s_addr,
    .sin_port = new_addr->sin_port,
  };
  scream_packet_update_address_ack ack = {
    .type = SC_PACKET_UPDATE_ADDRESS_ACK,
  };
  struct timeval timeout = {
    .tv_sec = SEC_PART (UPDATE_ADDRESS_TIMEOUT),
    .tv_usec = USEC_PART (UPDATE_ADDRESS_TIMEOUT),
  };

  if (sock == *main_channel->sock)
    {
      return scream_send_and_wait_for (&packet,
				       sizeof (packet),
				       (scream_packet_general *) &ack,
				       sizeof (ack),
				       "\tupdate client address on",
				       sock,
				       main_channel->sock_lock,
				       dest_addr,
				       &timeout,
				       UPDATE_ADDRESS_REPETITION);
    }

  return scream_send_and_wait_for_no_lock (&packet,
					   sizeof (packet),
					   (scream_packet_general *) &ack,
					   sizeof (ack),
					   "\tupdate client address on",
					   sock,
					   dest_addr,
					   &timeout,
					   UPDATE_ADDRESS_REPETITION);
}

void
start_manager (bool is_careful,
	       const char *manager_name,
	       struct manager_data *data)
{
#define CAREFUL_MANAGER_SOCK_LOCK()					\
  do									\
    {									\
      if (is_careful == TRUE && is_sock_locked == FALSE)		\
	{								\
	  printf ("%s manager locks the channel\n", manager_name);	\
	  if (pthread_mutex_lock (data->main_channel->sock_lock) != 0)	\
	    {								\
	      perror ("Cannot lock data->main_channel->sock_lock");	\
	    }								\
	  is_sock_locked = TRUE;					\
	}								\
    }									\
  while (0)

#define CAREFUL_MANAGER_SOCK_UNLOCK()					\
  do									\
    {									\
    if (is_careful == TRUE)						\
      {									\
	printf ("%s manager unlocks the channel\n", manager_name);	\
	if (pthread_mutex_unlock (data->main_channel->sock_lock) != 0)	\
	  {								\
	    perror ("Cannot unlock data->main_channel->sock_lock");	\
	  }								\
	is_sock_locked = FALSE;						\
      }									\
    }									\
  while (0)

  struct channel_record *best_channel = NULL;
  struct sockaddr_in new_addr;
  bool is_sock_locked = FALSE;

  /* initialize the main communication channel */
  do
    {
      while (best_channel == NULL)
	{	  
	  printf ("%s manager probes local interfaces\n", manager_name);
	  probe_ifs (data->channels);
	  printf ("%s manager performs a return-routability check\n",
		  manager_name);
	  best_channel = check_return_routability (data->dest_addr,
						   data->channels,
						   data->main_channel);
	  reset_new_and_modified_flags (data->channels);
	  remove_expired_channels (data->channels);

	  if (best_channel == NULL)
	    {
	      printf ("No route to reach the listener..."
		      " sleeping for %d seconds\n",
		      MANAGER_POOL_RATE);
	      sleep (MANAGER_POOL_RATE);
	    }
	}
    }
  while (switch_comm_channel_no_lock (data->main_channel,
				      best_channel) != SC_ERR_SUCCESS); 

  while (data->is_stopped == FALSE)
    {	  
      reset_new_and_modified_flags (data->channels);
      remove_expired_channels (data->channels);
      sleep (MANAGER_POOL_RATE);

      printf ("%s manager probes local interfaces\n", manager_name);
      if (probe_ifs (data->channels) == TRUE)
	{
	  printf ("%s manager sees an interface update\n", manager_name);
	  CAREFUL_MANAGER_SOCK_LOCK ();

	  printf ("%s manager performs a return-routability check\n",
		  manager_name);
	  best_channel = check_return_routability (data->dest_addr,
						   data->channels,
						   data->main_channel);

	  if (data->main_channel->channel->is_expired == FALSE
	      && data->main_channel->channel->is_modified == FALSE
	      && data->main_channel->channel->is_connected == TRUE)
	    {
	      printf ("%s manager sees that the main channel is still OK\n",
		      manager_name);
	      CAREFUL_MANAGER_SOCK_UNLOCK ();
	      continue;
	    }

	  printf ("%s manager sees that the main channel has to change\n"
		  "\tbecause %s\n",
		  manager_name,
		  (data->main_channel->channel->is_expired
		   ? "it no longer exists"
		   : (data->main_channel->channel->is_modified
		      ? "it gets a new address"
		      : "it can't contact the listener")));

	  if (best_channel == NULL)
	    {
	      printf ("%s manager sees no route to reach the listener\n",
		      manager_name);
	      /* careful manager still locks the channel */
	      continue;
	    }

	  if (*data->main_channel->is_registered == TRUE)
	    {
	      if (get_address (best_channel->sock, &new_addr) != SC_ERR_SUCCESS)
		{
		  printf ("%s manager seeks for another channel due to failing"
			  " new channel\n", manager_name);
		  /* careful manager still locks the channel */
		  continue;
		}

	      printf ("%s manager is updating screamer's main address to"
		      " %s:%d\n",
		      manager_name,
		      inet_ntoa (new_addr.sin_addr),
		      ntohs (new_addr.sin_port));
	      if (update_address (best_channel->sock,
				  *data->id,
				  &new_addr,
				  data->dest_addr,
				  data->main_channel) != SC_ERR_SUCCESS)
		{
		  printf ("%s manager seeks for another channel due to failing"
			  " update\n", manager_name);
	      
		  /* careful manager still locks the channel */
		  continue;
		}
	    }

	  if (is_careful == TRUE)
	    {
	      if (switch_comm_channel_no_lock (data->main_channel,
					       best_channel) != SC_ERR_SUCCESS)
		{
		  printf ("%s manager seeks for another channel due to"
			  " failing change of main channel\n", manager_name);
		  /* careful manager still locks the channel */
		  continue;
		}

	      CAREFUL_MANAGER_SOCK_UNLOCK ();
	      /* update process complete */
	    }
	  else
	    {
	      if (switch_comm_channel (data->main_channel, best_channel)
		  != SC_ERR_SUCCESS)
		{
		  printf ("%s manager cannot change the main channel\n",
			  manager_name);
		}
	      /* update process complete */
	    }
	}
      else
	{
	  printf ("%s manager performs a return-routability check\n",
		  manager_name);
	  best_channel = check_return_routability (data->dest_addr,
						   data->channels,
						   data->main_channel);
	  if (data->main_channel->channel->is_connected == TRUE)
	    {
	      printf ("%s manager sees that the main channel is still OK\n",
		      manager_name);
	      CAREFUL_MANAGER_SOCK_UNLOCK ();
	    }
	  else
	    {
	      printf ("%s manager sees that the main channel is disconnected\n",
		      manager_name);
	      CAREFUL_MANAGER_SOCK_LOCK ();

	      if (best_channel == NULL)
		{
		  printf ("%s manager sees no route to reach the listener\n",
			  manager_name);
		  /* careful manager still locks the channel */
		  continue;
		}
	      
	      if (*data->main_channel->is_registered == TRUE)
		{
		  if (get_address (best_channel->sock, &new_addr)
		      != SC_ERR_SUCCESS)
		    {
		      printf ("%s manager seeks for another channel due to"
			      " failing new channel\n", manager_name);
		      /* careful manager still locks the channel */
		      continue;
		    }

		  printf ("%s manager is updating screamer's main address to"
			  " %s:%d\n",
			  manager_name,
			  inet_ntoa (new_addr.sin_addr),
			  ntohs (new_addr.sin_port));
		  if (update_address (best_channel->sock,
				      *data->id,
				      &new_addr,
				      data->dest_addr,
				      data->main_channel) != SC_ERR_SUCCESS)
		    {
		      printf ("%s manager seeks for another channel due to"
			      " failing update\n", manager_name);
		      /* careful manager still locks the channel */
		      continue;
		    }
		}

	      if (is_careful == TRUE)
		{
		  if (switch_comm_channel_no_lock (data->main_channel,
						   best_channel)
		      != SC_ERR_SUCCESS)
		    {
		      printf ("%s manager seeks for another channel due to"
			      " failing change of main channel\n",
			      manager_name);
		      /* careful manager still locks the channel */
		      continue;
		    }

		  CAREFUL_MANAGER_SOCK_UNLOCK ();
		  /* update process complete */
		}
	      else
		{
		  if (switch_comm_channel (data->main_channel, best_channel)
		      != SC_ERR_SUCCESS)
		    {
		      printf ("%s manager cannot change the main channel\n",
			      manager_name);
		    }
		  /* update process complete */
		}	      
	    }
	}
    }

#undef CAREFUL_MANAGER_SOCK_LOCK
#undef CAREFUL_MANAGER_SOCK_UNLOCK
}

void *
start_careful_manager (void *data)
{
  struct manager_data *manager_data = data;

  start_manager (TRUE, "Careful", manager_data);

  manager_data->exit_code = SC_ERR_SUCCESS;
  return &manager_data->exit_code;
}

void *
start_sloppy_manager (void *data)
{
  struct manager_data *manager_data = data;

  start_manager (FALSE, "Sloppy", manager_data);

  manager_data->exit_code = SC_ERR_SUCCESS;
  return &manager_data->exit_code;
}
