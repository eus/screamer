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

#include <stdio.h> /* printf (...) */
#include <stdlib.h> /* malloc (...) */
#include <string.h> /* memcpy (...), bzero (...) */
#include <arpa/inet.h> /* inet_ntoa (...) */
#include <assert.h> /* assertions */
#include <errno.h>
#include <sqlite3.h>
#include "listen.h"

err_code
listener_handle_packet (const struct sockaddr_in *client_addr,
			scream_packet_type type,
			int sock,
			struct client_db *db)
{
  err_code err = SC_ERR_SUCCESS;
  struct listener_info info = {
    .sock = sock,
    .client_addr = client_addr,
    .client_db_rec = get_client_record (client_addr, db),
    .db = db,
  };

  printf ("Received %s packet from %s:%u\n",
	  get_scream_type_name (type),
	  inet_ntoa (client_addr->sin_addr),
	  ntohs (client_addr->sin_port));

  switch (type)
    {
    case SC_PACKET_REGISTER:
      err = register_client(&info);
      break;
    case SC_PACKET_RESET:
      err = reset_client(&info);
      break;
    case SC_PACKET_SERVER_CLIENT_SYNC:
      err = server_client_sync(&info);
      break;
    case SC_PACKET_SERVER_CLIENT_RESP_ACK:
      err = server_client_resp_ack(&info);
      break;
    case SC_PACKET_CLIENT_SERVER_SYNC:
      err = client_server_sync(&info);
      break;
    case SC_PACKET_CLIENT_SERVER_DATA:
      err = client_server_data(&info);
      break;
    default:
      break;
    }

  return err;
}

err_code
register_client (struct listener_info *info)
{
  struct client_record *rec = info->client_db_rec;
  scream_packet_register packet;
  ssize_t byte_received = recvfrom (info->sock,
				    &packet,
				    sizeof(packet),
				    0,
				    NULL,
				    NULL);
  if (byte_received == -1)
    {
      perror ("Cannot retrieve a register packet");
      return SC_ERR_RECV;
    }
  if (is_sane (&packet, byte_received, sizeof (packet)) == FALSE)
    {
      return SC_ERR_PACKET;
    }

  if (rec == NULL)
    {
      rec = get_client_record_on_id (ntohl (packet.id), info->db);
      if (rec == NULL) // the client is not yet associated
	{
	  rec = get_empty_slot (info->db);
	  if (rec == NULL) // it's full!
	    {
	      return SC_ERR_DB_FULL;
	    }

	  rec->id = ntohl (packet.id);
	  memcpy (&rec->client_addr, info->client_addr, sizeof (rec->client_addr));
	  rec->state = CLOSED;
	  rec->data = NULL;
	  rec->len = 0;
	}
      else // start this client's sync session
	{
	  memcpy (&rec->client_addr, info->client_addr, sizeof (rec->client_addr));
	}
    }

  if (rec->state != CLOSED
      && rec->state != WAIT_SERVER_CLIENT_SYNC)
    {
      fprintf (stderr, "Unexpected register rejected\n");
      return SC_ERR_STATE;
    }

  if (rec->state == CLOSED)
    {
      rec->state = WAIT_SERVER_CLIENT_SYNC;
    }

  return send_register_ack(info);
}

err_code
send_register_ack (struct listener_info *info)
{
  scream_packet_register_ack packet = {

    .type = SC_PACKET_REGISTER_ACK,
  };
  ssize_t bytes_sent = sendto (info->sock,
			       &packet,
			       sizeof (packet),
			       0,
			       (struct sockaddr *) info->client_addr,
			       sizeof (*info->client_addr));

  if (bytes_sent == -1)
    {
      perror ("Cannot send register ack\n");
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
server_client_sync (struct listener_info *info)
{
  err_code rc;
  struct client_record *rec = info->client_db_rec;
  scream_packet_server_client_sync packet;
  ssize_t byte_received = recvfrom (info->sock,
				    &packet,
				    sizeof(packet),
				    0,
				    NULL,
				    NULL);
  if (byte_received == -1)
    {
      perror ("Cannot retrieve a server-to-client sync request packet");
      return SC_ERR_RECV;
    }
  if (is_sane (&packet, byte_received, sizeof (packet)) == FALSE)
    {
      return SC_ERR_PACKET;
    }

  if (rec == NULL)
    {
      fprintf (stderr, "Session does not exist\n");
      return SC_ERR_STATE;
    }

  if (rec->state != WAIT_SERVER_CLIENT_SYNC
      && rec->state != WAIT_SERVER_CLIENT_RESP_ACK)
    {
      fprintf (stderr, "Unexpected server-to-client sync rejected\n");
      return SC_ERR_STATE;
    }

  if (rec->state == WAIT_SERVER_CLIENT_SYNC)
    {
      rc = retrieve_todos (info);
      if (rc != SC_ERR_SUCCESS)
	{
	  return rc;
	}
      rec->state = WAIT_SERVER_CLIENT_RESP_ACK;
    }

  return send_server_client_resp (info);
}

err_code
send_server_client_resp (struct listener_info *info)
{
  scream_packet_server_client_resp packet = {

    .type = SC_PACKET_SERVER_CLIENT_RESP,
  };
  ssize_t bytes_sent;

  packet.len = htonl (info->client_db_rec->len);
  bytes_sent = sendto (info->sock,
		       &packet,
		       sizeof (packet),
		       0,
		       (struct sockaddr *) info->client_addr,
		       sizeof (*info->client_addr));

  if (bytes_sent == -1)
    {
      perror ("Cannot send server-to-client resp\n");
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
server_client_resp_ack (struct listener_info *info)
{
  struct client_record *rec = info->client_db_rec;
  scream_packet_server_client_resp_ack packet;
  ssize_t byte_received = recvfrom (info->sock,
				    &packet,
				    sizeof(packet),
				    0,
				    NULL,
				    NULL);
  if (byte_received == -1)
    {
      perror ("Cannot retrieve a server-to-client resp ack packet");
      return SC_ERR_RECV;
    }
  if (is_sane (&packet, byte_received, sizeof (packet)) == FALSE)
    {
      return SC_ERR_PACKET;
    }

  if (rec == NULL)
    {
      fprintf (stderr, "Session does not exist\n");
      return SC_ERR_STATE;
    }

  if (rec->state != WAIT_SERVER_CLIENT_RESP_ACK
      && rec->state != WAIT_CLIENT_SERVER_SYNC)
    {
      fprintf (stderr, "Unexpected server-to-client resp ack rejected\n");
      return SC_ERR_STATE;
    }

  if (rec->state == WAIT_SERVER_CLIENT_RESP_ACK)
    {
      rec->state = WAIT_CLIENT_SERVER_SYNC;
    }

  return send_server_client_data (info);
}

err_code
send_server_client_data (struct listener_info *info)
{
  ssize_t bytes_sent = sendto (info->sock,
			       info->client_db_rec->data,
			       info->client_db_rec->len,
			       0,
			       (struct sockaddr *) info->client_addr,
			       sizeof (*info->client_addr));

  if (bytes_sent == -1)
    {
      perror ("Cannot send server-to-client data\n");
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
client_server_sync (struct listener_info *info)
{
  struct client_record *rec = info->client_db_rec;
  scream_packet_client_server_sync packet;
  ssize_t byte_received = recvfrom (info->sock,
				    &packet,
				    sizeof(packet),
				    0,
				    NULL,
				    NULL);
  if (byte_received == -1)
    {
      perror ("Cannot retrieve a client-to-server sync packet");
      return SC_ERR_RECV;
    }
  if (is_sane (&packet, byte_received, sizeof (packet)) == FALSE)
    {
      return SC_ERR_PACKET;
    }

  if (rec == NULL)
    {
      fprintf (stderr, "Session does not exist\n");
      return SC_ERR_STATE;
    }

  if (rec->state != WAIT_CLIENT_SERVER_SYNC
      && rec->state != WAIT_CLIENT_SERVER_DATA)
    {
      fprintf (stderr, "Unexpected client-to-server sync rejected\n");
      return SC_ERR_STATE;
    }

  if (rec->state == WAIT_CLIENT_SERVER_SYNC)
    {
      size_t len = ntohl(packet.len);

      free (rec->data);
      rec->data = malloc (len);
      if (rec->data == NULL)
	{
	  fprintf (stderr, "Insufficient memory for receiving sync data\n");
	  return SC_ERR_NOMEM;
	}
      rec->len = len;
      rec->state = WAIT_CLIENT_SERVER_DATA;
    }

  return send_client_server_resp (info);
}

err_code
send_client_server_resp (struct listener_info *info)
{
  scream_packet_client_server_resp packet = {

    .type = SC_PACKET_CLIENT_SERVER_RESP,
  };
  ssize_t bytes_sent = sendto (info->sock,
			       &packet,
			       sizeof (packet),
			       0,
			       (struct sockaddr *) info->client_addr,
			       sizeof (*info->client_addr));

  if (bytes_sent == -1)
    {
      perror ("Cannot send client-to-server resp\n");
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
client_server_data (struct listener_info *info)
{
  err_code rc;
  struct client_record *rec = info->client_db_rec;
  ssize_t byte_received;

  if (rec == NULL)
    {
      fprintf (stderr, "Session does not exist\n");
      remove_packet (info->sock);
      return SC_ERR_STATE;
    }

  if (rec->data == NULL)
    {
      remove_packet (info->sock);
    }
  else
    {
      byte_received = recvfrom (info->sock,
				rec->data,
				rec->len,
				0,
				NULL,
				NULL);
      if (byte_received == -1)
	{
	  perror ("Cannot retrieve a client-to-server data packet");
	  return SC_ERR_RECV;
	}
      if (is_sane (rec->data, byte_received, rec->len) == FALSE)
	{
	  return SC_ERR_PACKET;
	}
    }

  if (rec->state != WAIT_CLIENT_SERVER_DATA
      && rec->state != WAIT_RESET)
    {
      fprintf (stderr, "Unexpected client-to-server sync rejected\n");
      return SC_ERR_STATE;
    }

  if (rec->state == WAIT_CLIENT_SERVER_DATA)
    {
      rc = sync_todos (info);
      if (rc != SC_ERR_SUCCESS)
	{
	  return rc;
	}
      free (rec->data);
      rec->data = NULL;
      rec->state = WAIT_RESET;
    }

  return send_client_server_ack (info);  
}

err_code
send_client_server_ack (struct listener_info *info)
{
  scream_packet_client_server_ack packet = {

    .type = SC_PACKET_CLIENT_SERVER_ACK,
  };
  ssize_t bytes_sent = sendto (info->sock,
			       &packet,
			       sizeof (packet),
			       0,
			       (struct sockaddr *) info->client_addr,
			       sizeof (*info->client_addr));

  if (bytes_sent == -1)
    {
      perror ("Cannot send client-to-server ack\n");
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

err_code
reset_client (struct listener_info *info)
{
  struct client_record *rec = info->client_db_rec;
  scream_packet_reset packet;
  ssize_t byte_received = recvfrom (info->sock,
				    &packet,
				    sizeof(packet),
				    0,
				    NULL,
				    NULL);
  if (byte_received == -1)
    {
      perror ("Cannot retrieve a reset packet");
      return SC_ERR_RECV;
    }
  if (is_sane (&packet, byte_received, sizeof (packet)) == FALSE)
    {
      return SC_ERR_PACKET;
    }

  if (rec == NULL)
    {
      fprintf (stderr, "Session does not exist\n");
      return SC_ERR_STATE;
    }

  if (rec->state != CLOSED && rec->state != WAIT_RESET)
    {
      fprintf (stderr, "Unexpected reset rejected\n");
      return SC_ERR_STATE;
    }

  if (rec->state == WAIT_RESET)
    {
      bzero (&rec->client_addr, sizeof (rec->client_addr)); // close session      
      rec->state = CLOSED;
    }

  return send_reset_ack (info);
}

err_code
send_reset_ack (struct listener_info *info)
{
  scream_packet_reset_ack packet = {

    .type = SC_PACKET_RESET_ACK,
  };
  ssize_t bytes_sent = sendto (info->sock,
			       &packet,
			       sizeof (packet),
			       0,
			       (struct sockaddr *) info->client_addr,
			       sizeof (*info->client_addr));

  if (bytes_sent == -1)
    {
      perror ("Cannot send reset ack\n");
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

struct client_record *
get_empty_slot (const struct client_db *db)
{
  size_t db_len = db->len;
  size_t i;

  for (i = 0; i < db_len; i++)
    {
      if (db->recs[i].id == VACANT)
	{
	  return db->recs + i;
	}
    }  

  return NULL;
}

struct client_record *
get_client_record_on_id (int32_t id,
			 const struct client_db *db)
{
  size_t db_len = db->len;
  size_t i;

  for (i = 0; i < db_len; i++)
    {
      if (db->recs[i].id == id)
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
      if (memcmp (&db->recs[i].client_addr, addr, sizeof (*addr)) == 0)
	{
	  return db->recs + i;
	}
    }

  return NULL;
}

err_code
retrieve_todos (struct listener_info *info)
{
  int rc;
  sqlite3 *db = info->db->todo_db;
  sqlite3_stmt *query;
  size_t *len = &info->client_db_rec->len;
  scream_packet_server_client_data *packet;
  scream_packet_chunk *todo_chunk;
  int col_count;

  if (sqlite3_prepare (info->db->todo_db,		       
		       "select id, title, deadline, priority, status,"
		       " description, revision from todo where client_id = ?",
		       -1,
		       &query,
		       NULL))
    {
      sqlite3_perror (db, "Cannot prepare retrieval statement");
      return SC_ERR_DB;
    }
  col_count = sqlite3_column_count (query);

  if (sqlite3_bind_int (query, 1, info->client_db_rec->id))
    {
      sqlite3_perror (db, "Cannot bind ID to the retrieval statement");
      goto db_error;
    }

  /* calculate len */
  *len = sizeof (scream_packet_server_client_data);
  while ((rc = sqlite3_step (query)) == SQLITE_ROW)
    {
      int i;

      *len += sizeof (scream_packet_chunk);
      for (i = 0; i < col_count; i++)
	{
	  *len += sizeof (scream_packet_chunk);
	  switch (sqlite3_column_type (query, i))
	    {
	    case SQLITE_INTEGER:
	      *len += 4;
	      break;
	    case SQLITE_BLOB:
	      *len += sqlite3_column_bytes (query, i);
	      break;
	    default:
	      fprintf (stderr, "Unexpected column type during retrieval\n");
	      goto db_error;
	    }
	}
    }
  if (rc != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error during execution of retrieval statement");
      goto db_error; 
    }

  if (sqlite3_reset (query))
    {
      sqlite3_perror (db, "Cannot reset retrieval statement");
      goto db_error;
    }

  /* populate data */
  info->client_db_rec->data = malloc (*len);
  if (info->client_db_rec->data == NULL)
    {
      fprintf (stderr, "Insufficient memory to allocate server-to-client"
	       " todo packet\n");
      goto db_error;
    }
  packet = info->client_db_rec->data;
  packet->type = SC_PACKET_SERVER_CLIENT_DATA;
  todo_chunk = packet->data;
  while (sqlite3_step (query) == SQLITE_ROW)
    {
      uint16_t todo_chunk_len = 0;
      scream_packet_chunk *chunk;
      int i;

      todo_chunk->type = CHUNK_TODO;
      chunk = (scream_packet_chunk *) todo_chunk->datum;
      for (i = 0; i < col_count; i++)
	{
	  int int_val;
	  int chunk_len;

	  switch (i)
	    {
	    case 0:
	      chunk->type = CHUNK_TODO_ID;
	      break;
	    case 1:
	      chunk->type = CHUNK_TODO_TITLE;
	      break;
	    case 2:
	      chunk->type = CHUNK_TODO_DEADLINE;
	      break;
	    case 3:
	      chunk->type = CHUNK_TODO_PRIORITY;
	      break;
	    case 4:
	      chunk->type = CHUNK_TODO_STATUS;
	      break;
	    case 5:
	      chunk->type = CHUNK_TODO_DESCRIPTION;
	      break;
	    case 6:
	      chunk->type = CHUNK_TODO_REVISION;
	      break;
	    default:
	      fprintf (stderr, "Unexpected column index during retrieval\n");
	      goto mem_error;
	    }

	  switch (sqlite3_column_type (query, i))
	    {
	    case SQLITE_INTEGER:
	      chunk_len = 4;
	      int_val = htonl (sqlite3_column_int (query, i));
	      memcpy (chunk->datum, &int_val, chunk_len);
	      break;
	    case SQLITE_BLOB:
	      chunk_len = sqlite3_column_bytes (query, i);
	      memcpy (chunk->datum, sqlite3_column_blob (query, i), chunk_len);
	      break;
	    default:
	      fprintf (stderr, "Unexpected column type during retrieval\n");
	      goto mem_error;
	    }
	  chunk->len = htons (chunk_len);
	  todo_chunk_len += sizeof (*chunk) + chunk_len;
	  chunk = (scream_packet_chunk *) (((char *) chunk) + sizeof (*chunk)
					   + chunk_len);
	}
      todo_chunk->len = htons (todo_chunk_len);
      todo_chunk = (scream_packet_chunk *) (((char *) todo_chunk)
					    + sizeof (*todo_chunk)
					    + todo_chunk_len);
    }
  if (rc != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error during execution of retrieval statement");
      goto mem_error; 
    }

  if (sqlite3_finalize (query))
    {
      sqlite3_perror (db, "Cannot finalize retrieval statement");
    }

  return SC_ERR_SUCCESS;

 mem_error:
  free (info->client_db_rec->data);
  info->client_db_rec->data = NULL;

 db_error:
  if (sqlite3_finalize (query))
    {
      sqlite3_perror (db, "Cannot finalize retrieval statement");
    }
  return SC_ERR_DB;
}

err_code
sync_todos (struct listener_info *info)
{
  scream_packet_client_server_data *data = info->client_db_rec->data;
  scream_packet_chunk *chunk = data->data;
  scream_packet_chunk *chunk_end = (scream_packet_chunk *)
    (((char *) info->client_db_rec->data) + info->client_db_rec->len);
  sqlite3_stmt *insert_stmt;
  sqlite3_stmt *delete_stmt;
  sqlite3_stmt *update_stmt;

  if (data->type != SC_PACKET_CLIENT_SERVER_DATA)
    {
      return SC_ERR_PACKET;
    }

  if (sqlite3_prepare_v2 (info->db->todo_db,
			  "insert into todo (client_id, id, title, deadline,"
			  " priority, status, description, revision)"
			  " values (?, ?, ?, ?, ?, ?, ?, coalesce (?, 0))",
			  -1,
			  &insert_stmt,
			  NULL))
    {
      sqlite3_perror (info->db->todo_db, "Cannot prepare insert statement");
      goto err_db;
    }
  if (sqlite3_prepare_v2 (info->db->todo_db,
			  "delete from todo where id = ?",
			  -1,
			  &delete_stmt,
			  NULL))
    {
      sqlite3_perror (info->db->todo_db, "Cannot prepare delete statement");
      goto err_db_insert;
    }
  if (sqlite3_prepare_v2 (info->db->todo_db,
			  "update todo set"
			  " title = coalesce (?, title),"
			  " deadline = coalesce (?, deadline),"
			  " priority = coalesce (?, priority),"
			  " status = coalesce (?, status),"
			  " description = coalesce (?, description),"
			  " revision = revision + 1"
			  " where id = ?",
			  -1,
			  &update_stmt,
			  NULL))
    {
      sqlite3_perror (info->db->todo_db, "Cannot prepare update statement");
      goto err_db_delete;
    }

  while (chunk < chunk_end)
    {
      uint16_t chunk_len = ntohs (chunk->len);

      switch (chunk->type)
	{
	case CHUNK_NEW_TODO:
	  fprintf (stderr, "New todo detected\n");
	  if (sqlite3_bind_int (insert_stmt, 1, info->client_db_rec->id))
	    {
	      sqlite3_perror (info->db->todo_db,
			      "Cannot bind user id to sync insert statement");
	      goto err_db_update;
	    }
	  if (sync_create_todo (info->db->todo_db, insert_stmt,
				chunk_len, chunk->datum)
	      != SC_ERR_SUCCESS)
	    {
	      fprintf (stderr, "Failure in sync create\n");	      
	      goto err_db_update;
	    }
	  break;
	case CHUNK_DELETE_TODO:
	  fprintf (stderr, "Delete todo detected\n");
	  if (sync_delete_todo (info->db->todo_db, delete_stmt,
				chunk_len, chunk->datum)
	      != SC_ERR_SUCCESS)
	    {
	      fprintf (stderr, "Failure in sync delete\n");
	      goto err_db_update;
	    }
	  break;
	case CHUNK_UPDATE_TODO:
	  fprintf (stderr, "Update todo detected\n");
	  if (sync_update_todo (info->db->todo_db, update_stmt,
				chunk_len, chunk->datum)
	      != SC_ERR_SUCCESS)
	    {
	      fprintf (stderr, "Failure in sync update\n");
	      goto err_db_update;
	    }
	  break;
	default:
	  fprintf (stderr, "Invalid client-server data chunk type\n");
	  goto err_db_update;
	}

      chunk = (scream_packet_chunk *) (((char *) chunk) + sizeof (*chunk)
				       + chunk_len);
    }

  if (sqlite3_finalize (update_stmt))
    {
      sqlite3_perror (info->db->todo_db, "Cannot finalize update statement");
      goto err_db_delete;
    }
  if (sqlite3_finalize (delete_stmt))
    {
      sqlite3_perror (info->db->todo_db, "Cannot finalize delete statement");
      goto err_db_insert;
    }
  if (sqlite3_finalize (insert_stmt))
    {
      sqlite3_perror (info->db->todo_db, "Cannot finalize insert statement");
      goto err_db;
    }

  return SC_ERR_SUCCESS;

 err_db_update:
  if (sqlite3_finalize (update_stmt))
    {
      sqlite3_perror (info->db->todo_db, "Cannot finalize update statement");
    }

 err_db_delete:
  if (sqlite3_finalize (delete_stmt))
    {
      sqlite3_perror (info->db->todo_db, "Cannot finalize delete statement");
    }

 err_db_insert:
  if (sqlite3_finalize (insert_stmt))
    {
      sqlite3_perror (info->db->todo_db, "Cannot finalize insert statement");
    }

 err_db:
  return SC_ERR_DB;
}

err_code
sync_create_todo (sqlite3 *db, sqlite3_stmt *insert_stmt, uint16_t chunk_len,
		  void *chunk)
{
  scream_packet_chunk *field = chunk;
  scream_packet_chunk *end_of_field =
    (scream_packet_chunk *) (((char *) chunk) + chunk_len);
  int int_val;

  while (field < end_of_field)
    {
      uint16_t field_len = ntohs (field->len);

      switch (field->type)
	{
	case CHUNK_TODO_ID:
	  int_val = ntohl (*((int *) field->datum));
	  if (sqlite3_bind_int (insert_stmt, 2, int_val))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_ID");
	      goto err_db;
	    }
	  fprintf (stderr, "\tID: %d\n", int_val);
	  break;
	case CHUNK_TODO_TITLE:
	  if (sqlite3_bind_blob (insert_stmt, 3, field->datum,
				 field_len, SQLITE_STATIC))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_TITLE");
	      goto err_db;
	    }
	  break;
	case CHUNK_TODO_DEADLINE:
	  if (sqlite3_bind_blob (insert_stmt, 4, field->datum,
				 field_len, SQLITE_STATIC))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_DEADLINE");
	      goto err_db;
	    }
	  break;
	case CHUNK_TODO_PRIORITY:
	  int_val = ntohl (*((int *) field->datum));
	  if (sqlite3_bind_int (insert_stmt, 5, int_val))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_PRIORITY");
	      goto err_db;
	    }
	  break;
	case CHUNK_TODO_STATUS:
	  if (sqlite3_bind_blob (insert_stmt, 6, field->datum,
				 field_len, SQLITE_STATIC))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_STATUS");
	      goto err_db;
	    }
	  break;
	case CHUNK_TODO_DESCRIPTION:
	  if (sqlite3_bind_blob (insert_stmt, 7, field->datum,
				 field_len, SQLITE_STATIC))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_DESCRIPTION");
	      goto err_db;
	    }
	  break;
	case CHUNK_TODO_REVISION:
	  int_val = ntohl (*((int *) field->datum));
	  if (sqlite3_bind_int (insert_stmt, 8, int_val))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_REVISION");
	      goto err_db;
	    }
	  fprintf (stderr, "\tRevision: %d\n", int_val);
	  break;
	default:
	  fprintf (stderr, "Invalid field type for CHUNK_NEW_TODO\n");
	  goto err_db;
	}

      field = (scream_packet_chunk *)
	(((char *) field) + sizeof (*field) + field_len);
    }

  if (sqlite3_step (insert_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute insert statement");
      goto err_db;
    }

  if (sqlite3_reset (insert_stmt))
    {
      sqlite3_perror (db, "Cannot reset insert statement");
      goto err_db;
    }
  if (sqlite3_clear_bindings (insert_stmt))
    {
      sqlite3_perror (db, "Cannot clear bindings of insert statement");
      goto err_db;
    }

  return SC_ERR_SUCCESS;

 err_db:

  return SC_ERR_DB;
}

err_code
sync_delete_todo (sqlite3 *db, sqlite3_stmt *delete_stmt, uint16_t chunk_len,
		  void *chunk)
{
  scream_packet_chunk *field = chunk;
  scream_packet_chunk *end_of_field =
    (scream_packet_chunk *) (((char *) chunk) + chunk_len);
  int int_val;

  while (field < end_of_field)
    {
      uint16_t field_len = ntohs (field->len);

      switch (field->type)
	{
	case CHUNK_TODO_ID:
	  int_val = ntohl (*((int *) field->datum));
	  if (sqlite3_bind_int (delete_stmt, 1, int_val))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_ID");
	      goto err_db;
	    }
	  fprintf (stderr, "\tID: %d\n", int_val);
	  break;
	default:
	  fprintf (stderr, "Invalid field type for CHUNK_DELETE_TODO\n");
	  goto err_db;
	}

      field = (scream_packet_chunk *)
	(((char *) field) + sizeof (*field) + field_len);
    }

  if (sqlite3_step (delete_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute delete statement");
      goto err_db;
    }

  if (sqlite3_reset (delete_stmt))
    {
      sqlite3_perror (db, "Cannot reset delete statement");
      goto err_db;
    }
  if (sqlite3_clear_bindings (delete_stmt))
    {
      sqlite3_perror (db, "Cannot clear bindings of delete statement");
      goto err_db;
    }

  return SC_ERR_SUCCESS;

 err_db:

  return SC_ERR_DB;
}

err_code
sync_update_todo (sqlite3 *db, sqlite3_stmt *update_stmt, uint16_t chunk_len,
		  void *chunk)
{
  scream_packet_chunk *field = chunk;
  scream_packet_chunk *end_of_field =
    (scream_packet_chunk *) (((char *) chunk) + chunk_len);
  int int_val;

  while (field < end_of_field)
    {
      uint16_t field_len = ntohs (field->len);

      switch (field->type)
	{
	case CHUNK_TODO_ID:
	  int_val = ntohl (*((int *) field->datum));
	  if (sqlite3_bind_int (update_stmt, 6, int_val))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_ID");
	      goto err_db;
	    }
	  fprintf (stderr, "\tID: %d\n", int_val);
	  break;
	case CHUNK_TODO_TITLE:
	  if (sqlite3_bind_blob (update_stmt, 1, field->datum,
				 field_len, SQLITE_STATIC))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_TITLE");
	      goto err_db;
	    }
	  break;
	case CHUNK_TODO_DEADLINE:
	  if (sqlite3_bind_blob (update_stmt, 2, field->datum,
				 field_len, SQLITE_STATIC))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_DEADLINE");
	      goto err_db;
	    }
	  break;
	case CHUNK_TODO_PRIORITY:
	  int_val = ntohl (*((int *) field->datum));
	  if (sqlite3_bind_int (update_stmt, 3, int_val))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_PRIORITY");
	      goto err_db;
	    }
	  break;
	case CHUNK_TODO_STATUS:
	  if (sqlite3_bind_blob (update_stmt, 4, field->datum,
				 field_len, SQLITE_STATIC))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_STATUS");
	      goto err_db;
	    }
	  break;
	case CHUNK_TODO_DESCRIPTION:
	  if (sqlite3_bind_blob (update_stmt, 5, field->datum,
				 field_len, SQLITE_STATIC))
	    {
	      sqlite3_perror (db, "Cannot bind CHUNK_TODO_DESCRIPTION");
	      goto err_db;
	    }
	  break;
	default:
	  fprintf (stderr, "Invalid field type for CHUNK_UPDATE_TODO\n");
	  goto err_db;
	}

      field = (scream_packet_chunk *)
	(((char *) field) + sizeof (*field) + field_len);
    }

  if (sqlite3_step (update_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute update statement");
      goto err_db;
    }

  if (sqlite3_reset (update_stmt))
    {
      sqlite3_perror (db, "Cannot reset update statement");
      goto err_db;
    }
  if (sqlite3_clear_bindings (update_stmt))
    {
      sqlite3_perror (db, "Cannot clear bindings of update statement");
      goto err_db;
    }

  return SC_ERR_SUCCESS;

 err_db:

  return SC_ERR_DB;
}
