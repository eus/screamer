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

#include <stdio.h> /* printf (...) */
#include <stdlib.h> /* malloc (...) */
#include <string.h> /* memcpy (...), bzero (...) */
#include <arpa/inet.h> /* inet_ntoa (...) */
#include <assert.h> /* assertions */
#include <errno.h>
#include "listen.h"

err_code
listener_handle_packet (const struct sockaddr_in *client_addr,
			const scream_packet_general *packet,
			size_t len,
			int sock,
			struct client_db *db)
{
  err_code err = SC_ERR_SUCCESS;

  assert (client_addr != NULL);
  assert (packet != NULL);

  printf ("Received %s", get_scream_type_name (packet->type));
  if (packet->type == SC_PACKET_FLOOD)
    {
      scream_packet_flood *flood = (scream_packet_flood *) packet;
      printf (" #%d", ntohs (flood->seq) + 1);
    }
  printf (" packet from %s:%u\n",
	  inet_ntoa (client_addr->sin_addr),
	  ntohs (client_addr->sin_port));

  switch (packet->type)
    {
    case SC_PACKET_REGISTER:
      if ((err = register_client (client_addr,
				  (scream_packet_register *) packet,
				  db)) == SC_ERR_SUCCESS)
	{
	  err = send_ack (sock, client_addr);
	}
      break;
    case SC_PACKET_FLOOD:
      err = record_packet (client_addr,
			   (scream_packet_flood *) packet,
			   get_last_packet_ts (sock),
			   db);
      break;
    case SC_PACKET_RESET:
      err = reset_client (sock, client_addr, db);
      break;
    case SC_PACKET_ACK:
      err = unregister_client (client_addr, db);
      break;
    case SC_PACKET_RETURN_ROUTABILITY:
      err = send_return_routability_ack (sock, client_addr);
      break;
    case SC_PACKET_UPDATE_ADDRESS:
      if ((err
	   = update_client_address (client_addr,
				    (scream_packet_update_address *) packet,
				    db)) == SC_ERR_SUCCESS)
	{
	  err = send_update_address_ack (sock, client_addr);
	}
      break;
    }

  return err;
}

err_code
register_client (const struct sockaddr_in *client_addr,
		 const scream_packet_register *packet,
		 struct client_db *db)
{
  struct client_record *empty_slot = get_client_record (client_addr, db);

  if (empty_slot != NULL)
    {
      return SC_ERR_SUCCESS;
    }

  empty_slot = get_empty_slot (client_addr, db);
  if (!empty_slot)
    {
      return SC_ERR_DB_FULL;
    }

  memset (empty_slot, 0, sizeof (*empty_slot));
  empty_slot->died_at = DIE_AT_ANOTHER_TIME;
  memcpy (&empty_slot->client_addr,
	  client_addr,
	  sizeof (empty_slot->client_addr));
  empty_slot->sleep_time = COMBINE_SEC_USEC (ntohl (packet->sleep_time.sec),
					     ntohl (packet->sleep_time.usec));
  empty_slot->amount = ntohl (packet->amount);
  empty_slot->id = ntohl (packet->id);
  empty_slot->is_out_of_order = FALSE;
  empty_slot->max_latency.is_set = FALSE;
  empty_slot->min_latency.is_set = FALSE;

  return SC_ERR_SUCCESS;
}

err_code
update_client_address (const struct sockaddr_in *client_addr,
		       const scream_packet_update_address *packet,
		       struct client_db *db)
{
  size_t db_len = db->len;
  size_t i;
  uint32_t client_id = packet->id;
  struct client_record *client = NULL;

  for (i = 0; i < db_len; i++)
    {
      if (db->recs[i].id == client_id)
	{
	  client = db->recs + i;
	  break;
	}
    }

  if (client == NULL)
    {
      fprintf (stderr,
	       "Cannot update the address of an unexisting client\n");      
      return SC_ERR_STATE;
    }

  client->client_addr.sin_addr.s_addr = packet->sin_addr;
  client->client_addr.sin_port = packet->sin_port;

  return SC_ERR_SUCCESS;
}

err_code
reset_client (int sock,
	      const struct sockaddr_in *client_addr,
	      struct client_db *db)
{
  err_code rc;
  struct client_record *rec = get_client_record (client_addr, db);

  if (rec == NULL)
    {
      fprintf (stderr, "Cannot reset an unexisting client\n");
      return SC_ERR_STATE;
    }

  if (rec->died_at == DIE_AT_ANOTHER_TIME) /* log this only once */
    {
      /* missing packets at the end of flooding */
      int gap = rec->amount - 1 - rec->prev_packet.seq;

      if (gap > 0)
	{
	  printf ("\t%d packets are either lost or out-of-order\n", gap);
	  if (rec->max_gap < gap)
	    {
	      rec->max_gap = gap;
	    }
	  rec->num_of_gaps++;
	}
    }
      
  if ((rc = send_result (sock, client_addr, db)) != SC_ERR_SUCCESS)
    {
      return rc;
    }

  return mark_client_for_unregistering (client_addr, db);
}

err_code
record_packet (const struct sockaddr_in *client_addr,
	       const scream_packet_flood *packet,
	       unsigned long long ts,
	       struct client_db *db)
{
  struct client_record *rec = get_client_record (client_addr, db);

  if (rec == NULL)
    {
      fprintf (stderr, "Cannot record a FLOOD of an unexisting client\n");
      return SC_ERR_STATE;
    }

  rec->recvd_packets++;

  if (rec->prev_packet.ts != 0) /* not the first FLOOD packet */
    {
      unsigned long long delta_ts = ts - rec->prev_packet.ts;

      printf ("\tDelay between the previous and current packet: %lu.%06lu s\n",
	      (unsigned long) SEC_PART (delta_ts),
	      (unsigned long) USEC_PART (delta_ts));
      rec->total_latency += delta_ts;

      if (rec->min_latency.is_set == FALSE)
	{
	  rec->min_latency.delta = delta_ts;
	  rec->min_latency.is_set = TRUE;
	}
      else if (rec->min_latency.delta > delta_ts)
	{
	  rec->min_latency.delta = delta_ts;
	}

      if (rec->max_latency.is_set == FALSE)
	{
	  rec->max_latency.delta = delta_ts;
	  rec->max_latency.is_set = TRUE;
	}
      else if (rec->max_latency.delta < delta_ts)
	{
	  rec->max_latency.delta = delta_ts;
	}

      if (rec->prev_packet.seq == ntohs (packet->seq))
	{
	  printf ("\tThe current packet is a duplicate of the previous one\n");
	}
      else if (rec->prev_packet.seq > ntohs (packet->seq))
	{
	  if (rec->is_out_of_order == FALSE) /* the first out-of-order */
	    {
	      rec->is_out_of_order = TRUE;
	    }
	  printf ("\tThe current packet is out-of-order\n");
	}
      else if (rec->prev_packet.seq + 1 != ntohs (packet->seq)
	       && rec->prev_packet.seq < ntohs (packet->seq))
	{
	  int gap = ntohs (packet->seq) - rec->prev_packet.seq - 1;

	  printf ("\t%d packets are either lost or out-of-order\n", gap);
	  if (rec->max_gap < gap)
	    {
	      rec->max_gap = gap;
	    }

	  rec->num_of_gaps++;
	}
    }
  else if (ntohs (packet->seq) != 0) /* packets missing at the beginning */
    {
      int gap = ntohs (packet->seq) - 1;

      printf ("\t%d packets are either lost or out-of-order\n", gap);
      rec->max_gap = gap;
      rec->num_of_gaps++;
    }

  rec->prev_packet.ts = ts;

  if (rec->prev_packet.seq < ntohs (packet->seq)) /* not out-of-order */
    {
      rec->prev_packet.seq = ntohs (packet->seq);
    }

  return SC_ERR_SUCCESS;
}

err_code
send_result (int sock,
	     const struct sockaddr_in *client_addr,
	     const struct client_db *db)
{
  struct client_record *rec = get_client_record (client_addr, db);
  scream_packet_result result = {0};
  unsigned long long avg_latency;

  if (rec == NULL)
    {
      fprintf (stderr, "Cannot compute result on an unexisting client\n");
      return SC_ERR_STATE;
    }

  avg_latency = rec->total_latency / (rec->recvd_packets - 1);

  result.type = SC_PACKET_RESULT;
  result.recvd_packets = htonl (rec->recvd_packets);
  result.max_gap = htonl (rec->max_gap);
  result.num_of_gaps = htonl (rec->num_of_gaps);
  result.is_out_of_order = rec->is_out_of_order == TRUE ? 1 : 0;
  if (rec->max_latency.is_set)
    {
      result.max_latency.sec = htonl (SEC_PART (rec->max_latency.delta));
      result.max_latency.usec = htonl (USEC_PART (rec->max_latency.delta));
    }
  if (rec->min_latency.is_set)
    {
      result.min_latency.sec = htonl (SEC_PART (rec->min_latency.delta));
      result.min_latency.usec = htonl (USEC_PART (rec->min_latency.delta));
    }
  result.avg_latency.sec = htonl (SEC_PART (avg_latency));
  result.avg_latency.usec = htonl (USEC_PART (avg_latency));

  if (sendto (sock, &result, sizeof (result), 0,
	      (struct sockaddr *) client_addr, sizeof (*client_addr)) == -1)
    {
      int last_errno = errno;

      printf ("Cannot send RESULT to %s:%d [%s]\n",
	      inet_ntoa (client_addr->sin_addr),
	      ntohs (client_addr->sin_port),
	      strerror (last_errno));

      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
send_return_routability_ack (int sock, const struct sockaddr_in *dest)
{
  scream_packet_return_routability_ack packet = {
    .type = SC_PACKET_RETURN_ROUTABILITY_ACK,
  };

  if (sendto (sock, &packet, sizeof (packet), 0,
	      (struct sockaddr *) dest, sizeof (*dest)) == -1)
    {
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
send_update_address_ack (int sock, const struct sockaddr_in *dest)
{
  scream_packet_update_address_ack packet = {
    .type = SC_PACKET_UPDATE_ADDRESS_ACK,
  };

  if (sendto (sock, &packet, sizeof (packet), 0,
	      (struct sockaddr *) dest, sizeof (*dest)) == -1)
    {
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
mark_client_for_unregistering (const struct sockaddr_in *client_addr,
			       struct client_db *db)
{
  struct client_record *rec = get_client_record (client_addr, db);

  if (rec == NULL)
    {
      fprintf (stderr, "Cannot unregister an unexisting client\n");
      return SC_ERR_STATE;
    }

  if (rec->died_at == DIE_AT_ANOTHER_TIME)
    {
      rec->died_at = time (NULL) + TIME_TO_DEATH;
    }

  return SC_ERR_SUCCESS;
}

err_code
unregister_client (const struct sockaddr_in *addr,
		   struct client_db *db)
{
  size_t db_len = db->len;
  size_t i;

  for (i = 0; i < db_len; i++)
    {
      if (memcmp (&db->recs[i].client_addr, addr, sizeof (*addr)) == 0
	  && db->recs[i].died_at > time (NULL))
	{
	  /* can't disassociate when not yet reset */
	  if (db->recs[i].died_at == DIE_AT_ANOTHER_TIME)
	    {
	      fprintf (stderr, "Cannot disassociate when not yet reset\n");
	      return SC_ERR_STATE;
	    }

	  db->recs[i].died_at = 0;
	}
    }

  return SC_ERR_SUCCESS;
}

struct client_record *
get_empty_slot (const struct sockaddr_in *client_addr,
		const struct client_db *db)
{
  size_t db_len = db->len;
  size_t i;

  for (i = 0; i < db_len; i++)
    {
      if (db->recs[i].died_at != DIE_AT_ANOTHER_TIME
	  && db->recs[i].died_at <= time (NULL))
	{
	  return db->recs + i;
	}
    }  

  return NULL;
}

struct client_record *
get_client_record (const struct sockaddr_in *addr,
		   const struct client_db *db)
{
  size_t db_len = db->len;
  size_t i;

  for (i = 0; i < db_len; i++)
    {
      if ((db->recs[i].died_at == DIE_AT_ANOTHER_TIME
	   || db->recs[i].died_at > time (NULL)) /* not yet disassociated */
	  && memcmp (&db->recs[i].client_addr, addr, sizeof (*addr)) == 0)
	{
	  return db->recs + i;
	}
    }

  return NULL;
}
