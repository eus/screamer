/******************************************************************************
 * Copyright (C) 2010  Tadeus Prastowo <eus@member.fsf.org>                   *
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <unistd.h>
#include <errno.h>
#include "scream-common.h"

int
create_todo (sqlite3 *db)
{
  char title[128];
  char deadline[32];
  char priority[32];
  char status[32];
  char description[1024];
  sqlite3_stmt *insert_stmt;
  int rc = -1;

  printf ("Title: ");
  fgets (title, sizeof (title), stdin);
  printf ("Deadline: ");
  fgets (deadline, sizeof (deadline), stdin);
  printf ("Priority: ");
  fgets (priority, sizeof (priority), stdin);
  printf ("Status: ");
  fgets (status, sizeof (status), stdin);
  printf ("Description: ");
  fgets (description, sizeof (description), stdin);

  if (sqlite3_prepare_v2 (db,
			  "insert or replace into todo values (null, ?, ?, ?, ?, ?, -1)",
			  -1,
			  &insert_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare insert statement");
      return -1;
    }
  if (sqlite3_bind_text (insert_stmt, 1, title, strlen (title) - 1, SQLITE_STATIC))
    {
      sqlite3_perror (db, "Cannot bind title");
      goto error_db;
    }
  if (sqlite3_bind_text (insert_stmt, 2, deadline, strlen (deadline) - 1, SQLITE_STATIC))
    {
      sqlite3_perror (db, "Cannot bind deadline");
      goto error_db;
    }
  if (sqlite3_bind_int (insert_stmt, 3, atoi (priority)))
    {
      sqlite3_perror (db, "Cannot bind priority");
      goto error_db;
    }
  if (sqlite3_bind_text (insert_stmt, 4, status, strlen (status) - 1, SQLITE_STATIC))
    {
      sqlite3_perror (db, "Cannot bind status");
      goto error_db;
    }
  if (sqlite3_bind_text (insert_stmt, 5, description, strlen (description) - 1, SQLITE_STATIC))
    {
      sqlite3_perror (db, "Cannot bind description");
      goto error_db;
    }

  if (sqlite3_step (insert_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute insert");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (insert_stmt))
    {
      sqlite3_perror (db, "Cannot finalize insert statement");
    }

  return rc;
}

int
delete_todo (sqlite3 *db)
{
  char id[32];
  sqlite3_stmt *stmt;
  int rc = -1;
  int revision;
  int step_result;

  printf ("Delete ID (-1 to cancel): ");
  fgets (id, sizeof (id), stdin);
  if (atoi (id) < 0)
    {
      return 0;
    }

  if (sqlite3_prepare_v2 (db,
			  "select revision from todo where id = ?",
			  -1,
			  &stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare revision select statement");
      return -1;
    }
  if (sqlite3_bind_int (stmt, 1, atoi (id)))
    {
      sqlite3_perror (db, "Cannot bind ID");
      goto error_db;
    }

  if ((step_result = sqlite3_step (stmt)) != SQLITE_ROW)
    {
      if (step_result == SQLITE_DONE)
	{
	  goto bailout;
	}

      sqlite3_perror (db, "Cannot execute select");
      goto error_db;
    }
  revision = sqlite3_column_int (stmt, 0);
  if (sqlite3_finalize (stmt))
    {
      sqlite3_perror (db, "Cannot finalize select statement");
      goto error;
    }

  if (revision == -1)
    {
      if (sqlite3_prepare_v2 (db,
			      "delete from todo where id = ?",
			      -1,
			      &stmt,
			      NULL))
	{
	  sqlite3_perror (db, "Cannot prepare delete statement");
	  goto error;
	}
    }
  else
    {
      if (sqlite3_prepare_v2 (db,
			      "insert into deleted_todo (id) values (?)",
			      -1,
			      &stmt,
			      NULL))
	{
	  sqlite3_perror (db, "Cannot prepare insert statement");
	  goto error;
	}
    }

  if (sqlite3_bind_int (stmt, 1, atoi (id)))
    {
      sqlite3_perror (db, "Cannot bind ID");
      goto error_db;
    }

  if (sqlite3_step (stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute statement");
      goto error_db;
    }

 bailout:

  rc = 0;

 error_db:
  if (sqlite3_finalize (stmt))
    {
      sqlite3_perror (db, "Cannot finalize statement");
    }

 error:
  return rc;
}

int
edit_todo (sqlite3 *db)
{
  char title[128];
  char deadline[32];
  char priority[32];
  char status[32];
  char description[1024];
  char id[32];
  sqlite3_stmt *stmt;
  int rc = -1;
  size_t input_len;

  printf ("Edit ID (-1 to cancel): ");
  fgets (id, sizeof (id), stdin);
  if (atoi (id) < 0)
    {
      return 0;
    }
  printf ("Title: ");
  fgets (title, sizeof (title), stdin);
  printf ("Deadline: ");
  fgets (deadline, sizeof (deadline), stdin);
  printf ("Priority: ");
  fgets (priority, sizeof (priority), stdin);
  printf ("Status: ");
  fgets (status, sizeof (status), stdin);
  printf ("Description: ");
  fgets (description, sizeof (description), stdin);

  if (sqlite3_prepare_v2 (db,
			  "update todo set"
			  " title = coalesce (?, title),"
			  " deadline = coalesce (?, deadline),"
			  " priority = coalesce (?, priority),"
			  " status = coalesce (?, status),"
			  " description = coalesce (?, description)"
			  " where id = ?",
			  -1,
			  &stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare edit todo statement");
      return -1;
    }
  input_len = strlen (title) - 1;
  if (input_len && sqlite3_bind_text (stmt, 1, title, input_len, SQLITE_STATIC))
    {
      sqlite3_perror (db, "Cannot bind title");
      goto error_db;
    }
  input_len = strlen (deadline) - 1;
  if (input_len && sqlite3_bind_text (stmt, 2, deadline, input_len, SQLITE_STATIC))
    {
      sqlite3_perror (db, "Cannot bind deadline");
      goto error_db;
    }
  input_len = strlen (priority) - 1;
  if (input_len && sqlite3_bind_int (stmt, 3, atoi (priority)))
    {
      sqlite3_perror (db, "Cannot bind priority");
      goto error_db;
    }
  input_len = strlen (status) - 1;
  if (input_len && sqlite3_bind_text (stmt, 4, status, input_len, SQLITE_STATIC))
    {
      sqlite3_perror (db, "Cannot bind status");
      goto error_db;
    }
  input_len = strlen (description) - 1;
  if (input_len && sqlite3_bind_text (stmt, 5, description, input_len, SQLITE_STATIC))
    {
      sqlite3_perror (db, "Cannot bind description");
      goto error_db;
    }
  if (sqlite3_bind_int (stmt, 6, atoi (id)))
    {
      sqlite3_perror (db, "Cannot bind ID");
      goto error_db;
    }

  if (sqlite3_step (stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute update statement");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (stmt))
    {
      sqlite3_perror (db, "Cannot finalize update statement");
    }

 error:
  return rc;
}

int
list_todo (sqlite3 *db)
{
  int rc = -1;
  int step_result;
  sqlite3_stmt *select_stmt;

  if (sqlite3_prepare_v2 (db,
			  "select id, title, deadline, priority, status, description, revision"
			  " from todo"
			  " where id not in (select id from deleted_todo)",
			  -1,
			  &select_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare select statement");
      return -1;
    }
  while ((step_result = sqlite3_step (select_stmt)) == SQLITE_ROW)
    {
      printf ("\n"
	      "ID: %d\n"
	      "Title: %s\n"
	      "Deadline: %s\n"
	      "Priority: %d\n"
	      "Status: %s\n"
	      "Description: %s\n"
	      "Revision: %d\n",
	      sqlite3_column_int (select_stmt, 0),
	      sqlite3_column_text (select_stmt, 1),
	      sqlite3_column_text (select_stmt, 2),
	      sqlite3_column_int (select_stmt, 3),
	      sqlite3_column_text (select_stmt, 4),
	      sqlite3_column_text (select_stmt, 5),
	      sqlite3_column_int (select_stmt, 6));
    }
  if (step_result != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error in retrieving todo");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (select_stmt))
    {
      sqlite3_perror (db, "Cannot finalize select statement");
    }

  return rc;
}

int
process_todos (sqlite3 *db,
	       scream_packet_chunk *todos,
	       scream_packet_chunk *todos_end)
{
  int rc = -1;
  sqlite3_stmt *insert_stmt;
  scream_packet_chunk *next_chunk;

  if (sqlite3_prepare_v2 (db,
			  "insert into sync values (?, ?, ?, ?, ?, ?, ?)",
			  -1,
			  &insert_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare insert statement");
      return -1;
    }

  next_chunk = todos;
  while (next_chunk < todos_end)
    {
      scream_packet_chunk *todo_field;

      if (next_chunk->type != CHUNK_TODO)
	{
	  fprintf (stderr, "Invalid chunk data: Expected CHUNK_TODO\n");
	  goto error_db;
	}

      todo_field = (scream_packet_chunk *) next_chunk->datum;
      next_chunk = (scream_packet_chunk *) (((char *) next_chunk)
					    + sizeof (scream_packet_chunk)
					    + ntohs (next_chunk->len));

      while (todo_field < next_chunk)
	{
	  uint16_t todo_field_len = ntohs (todo_field->len);

	  switch (todo_field->type)
	    {
	    case CHUNK_TODO_ID:
	      if (sqlite3_bind_int (insert_stmt, 1, ntohl (*((int *) todo_field->datum))))
		{
		  sqlite3_perror (db, "Cannot bind ID");
		  goto error_db;
		}
	      break;
	    case CHUNK_TODO_TITLE:
	      if (sqlite3_bind_text (insert_stmt, 2, todo_field->datum, todo_field_len, NULL))
		{
		  sqlite3_perror (db, "Cannot bind title");
		  goto error_db;
		}
	      break;
	    case CHUNK_TODO_DEADLINE:
	      if (sqlite3_bind_text (insert_stmt, 3, todo_field->datum, todo_field_len, NULL))
		{
		  sqlite3_perror (db, "Cannot bind deadline");
		  goto error_db;
		}
	      break;
	    case CHUNK_TODO_PRIORITY:
	      if (sqlite3_bind_int (insert_stmt, 4, ntohl (*((int *) todo_field->datum))))
		{
		  sqlite3_perror (db, "Cannot bind priority");
		  goto error_db;
		}
	      break;
	    case CHUNK_TODO_STATUS:
	      if (sqlite3_bind_text (insert_stmt, 5, todo_field->datum, todo_field_len, NULL))
		{
		  sqlite3_perror (db, "Cannot bind title");
		  goto error_db;
		}
	      break;
	    case CHUNK_TODO_DESCRIPTION:
	      if (sqlite3_bind_text (insert_stmt, 6, todo_field->datum, todo_field_len, NULL))
		{
		  sqlite3_perror (db, "Cannot bind title");
		  goto error_db;
		}
	      break;
	    case CHUNK_TODO_REVISION:
	      if (sqlite3_bind_int (insert_stmt, 7, ntohl (*((int *) todo_field->datum))))
		{
		  sqlite3_perror (db, "Cannot bind revision");
		  goto error_db;
		}
	      break;
	    default:
	      fprintf (stderr, "Invalid chunk data: Expected CHUNK_TODO_{"
		       "ID,TITLE,DEADLINE,PRIORITY,STATUS,DESCRIPTION,REVISION}\n");
	      goto error_db;
	    }

	  todo_field = (scream_packet_chunk *) (((char *) todo_field)
						+ sizeof (scream_packet_chunk)
						+ todo_field_len);
	}

      if (sqlite3_step (insert_stmt) != SQLITE_DONE)
	{
	  sqlite3_perror (db, "Cannot insert a sync record");
	  goto error_db;
	}

      if (sqlite3_reset (insert_stmt))
	{
	  sqlite3_perror (db, "Cannot reset insert statement");
	  goto error_db;
	}
      if (sqlite3_clear_bindings (insert_stmt))
	{
	  sqlite3_perror (db, "Cannot clear bindings of insert statement");
	  goto error_db;
	}
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (insert_stmt))
    {
      sqlite3_perror (db, "Cannot finalize insert statement");
    }

  return rc;
}

int
create_sync_data (sqlite3 *db,
		  scream_packet_client_server_data **client_server_data,
		  size_t *client_server_data_len)
{
  scream_packet_chunk *chunk;
  int rc = -1;
  int step_result;
  sqlite3_stmt *select;
  const char select_new[] =
    "select id, title, deadline, priority, status, description, revision"
    " from sync"
    " where revision < 0";
  const char select_updated[] = 
    "select id, title, deadline, priority, status, description, revision"
    " from sync"
    " where (title is not null or deadline is not null"
    "        or priority is not null or status is not null"
    "        or description is not null)"
    " and revision is null";
  const char select_deleted[] =
    "select id"
    " from sync"
    " where title is null and deadline is null and priority is null"
    " and status is null and description is null and revision is null";
  size_t len = sizeof (scream_packet_client_server_data);

  /* calculate client_server_data_len */

  // new todo length calculation
  if (sqlite3_prepare_v2 (db, select_new, -1, &select, NULL))
    {
      sqlite3_perror (db, "Cannot prepare select statement");
      return -1;
    }
  while ((step_result = sqlite3_step (select)) == SQLITE_ROW)
    {
      int column_count = sqlite3_column_count (select);
      int i;

      len += sizeof (scream_packet_chunk);
      for (i = 0; i < column_count; i++)
	{
	  if (i != 6 || sqlite3_column_int (select, i) != -1) // revision
	    {
	      len += (sizeof (scream_packet_chunk)
		      + ((sqlite3_column_type (select, i) == SQLITE_INTEGER)
			 ? sizeof (uint32_t)
			 : sqlite3_column_bytes (select, i)));
	    }
	}
    }
  if (step_result != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error in retrieving new todo");
      goto error_db;
    }
  if (sqlite3_finalize (select))
    {
      sqlite3_perror (db, "Cannot finalize select statement");
      goto error;
    }

  // deleted todo length calculation
  if (sqlite3_prepare_v2 (db, select_deleted, -1, &select, NULL))
    {
      sqlite3_perror (db, "Cannot prepare select statement");
      return -1;
    }
  while ((step_result = sqlite3_step (select)) == SQLITE_ROW)
    {
      len += 2 * sizeof (scream_packet_chunk) + sizeof (uint32_t);
    }
  if (step_result != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error in retrieving deleted todo");
      goto error_db;
    }
  if (sqlite3_finalize (select))
    {
      sqlite3_perror (db, "Cannot finalize select statement");
      goto error;
    }

  // updated todo length calculation
  if (sqlite3_prepare_v2 (db, select_updated, -1, &select, NULL))
    {
      sqlite3_perror (db, "Cannot prepare select statement");
      return -1;
    }
  while ((step_result = sqlite3_step (select)) == SQLITE_ROW)
    {
      int column_count = sqlite3_column_count (select);
      int i;

      len += sizeof (scream_packet_chunk);
      for (i = 0; i < column_count; i++)
	{
	  if (sqlite3_column_type (select, i) != SQLITE_NULL)
	    {
	      len += (sizeof (scream_packet_chunk)
		      + ((sqlite3_column_type (select, i) == SQLITE_INTEGER)
			 ? sizeof (uint32_t)
			 : sqlite3_column_bytes (select, i)));
	    }
	}
    }
  if (step_result != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error in retrieving updated todo");
      goto error_db;
    }
  if (sqlite3_finalize (select))
    {
      sqlite3_perror (db, "Cannot finalize select statement");
      goto error;
    }

  /* Fill client_server_data */
  *client_server_data_len = len;
  *client_server_data = malloc (len);
  if (*client_server_data == NULL)
    {
      fprintf (stderr, "Insufficient memory to allocate client-server data\n");
      goto error;
    }
  (*client_server_data)->type = SC_PACKET_CLIENT_SERVER_DATA;
  chunk = (*client_server_data)->data;

  // fill in new todos
  if (sqlite3_prepare_v2 (db, select_new, -1, &select, NULL))
    {
      sqlite3_perror (db, "Cannot prepare select statement");
      goto error;
    }
  while ((step_result = sqlite3_step (select)) == SQLITE_ROW)
    {
      int column_count = sqlite3_column_count (select);
      int i;
      short chunk_len = 0;
      scream_packet_chunk *datum = (scream_packet_chunk *) chunk->datum;
      short datum_len;

      chunk->type = CHUNK_NEW_TODO;
      for (i = 0; i < column_count; i++)
	{
	  if (i != 6 || sqlite3_column_int (select, i) != -1) // revision
	    {
	      int int_val;

	      datum_len = ((sqlite3_column_type (select, i) == SQLITE_INTEGER)
			   ? sizeof (uint32_t)
			   : sqlite3_column_bytes (select, i));
	      switch (i)
		{
		case 0:
		  datum->type = CHUNK_TODO_ID;
		  break;
		case 1:
		  datum->type = CHUNK_TODO_TITLE;
		  break;
		case 2:
		  datum->type = CHUNK_TODO_DEADLINE;
		  break;
		case 3:
		  datum->type = CHUNK_TODO_PRIORITY;
		  break;
		case 4:
		  datum->type = CHUNK_TODO_STATUS;
		  break;
		case 5:
		  datum->type = CHUNK_TODO_DESCRIPTION;
		  break;
		case 6:
		  datum->type = CHUNK_TODO_REVISION;
		  break;
		default:
		  fprintf (stderr, "Invalid index for a CHUNK_NEW_TODO\n");
		  goto error_db;
		}
	      switch (sqlite3_column_type (select, i))
		{
		case SQLITE_INTEGER:
		  int_val = sqlite3_column_int (select, i);
		  if (i == 6)
		    {
		      int_val = -1 * (int_val + 2);
		    }
		  int_val = htonl (int_val);
		  memcpy (datum->datum, &int_val, sizeof (int_val));
		  break;
		case SQLITE_TEXT:
		  memcpy (datum->datum,
			  sqlite3_column_text (select, i),
			  sqlite3_column_bytes (select, i));
		  break;
		default:
		  fprintf (stderr, "Invalid column type for a CHUNK_NEW_TODO\n");
		  goto error_db;
		}
	      datum->len = htons (datum_len);
	      datum = (scream_packet_chunk *) (((char *) datum)
					       + sizeof (*datum) + datum_len);
	      chunk_len += sizeof (scream_packet_chunk) + datum_len;
	    }
	}
      chunk->len = htons (chunk_len);
      chunk = (scream_packet_chunk *) (((char *) chunk) + sizeof (*chunk) + chunk_len);
    }
  if (step_result != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error in retrieving new todo");
      goto error_db;
    }
  if (sqlite3_finalize (select))
    {
      sqlite3_perror (db, "Cannot finalize select statement");
      goto error;
    }

  // fill in deleted todo
  if (sqlite3_prepare_v2 (db, select_deleted, -1, &select, NULL))
    {
      sqlite3_perror (db, "Cannot prepare select statement");
      return -1;
    }
  while ((step_result = sqlite3_step (select)) == SQLITE_ROW)
    {
      int id_len = sizeof (uint32_t);
      int int_val;
      scream_packet_chunk *datum = (scream_packet_chunk *) chunk->datum;

      chunk->type = CHUNK_DELETE_TODO;
      chunk->len = htons (sizeof (scream_packet_chunk) + id_len);
      datum->type = CHUNK_TODO_ID;
      datum->len = htons (id_len);
      int_val = htonl (sqlite3_column_int (select, 0));
      memcpy (datum->datum, &int_val, sizeof (int_val));

      chunk = (scream_packet_chunk *) (((char *) chunk) + 2 * sizeof (*chunk) + id_len);      
    }
  if (step_result != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error in retrieving deleted todo");
      goto error_db;
    }
  if (sqlite3_finalize (select))
    {
      sqlite3_perror (db, "Cannot finalize select statement");
      goto error;
    }

  // fill in updated todo
  if (sqlite3_prepare_v2 (db, select_updated, -1, &select, NULL))
    {
      sqlite3_perror (db, "Cannot prepare select statement");
      return -1;
    }
  while ((step_result = sqlite3_step (select)) == SQLITE_ROW)
    {
      int column_count = sqlite3_column_count (select);
      int i;
      short chunk_len = 0;
      scream_packet_chunk *datum = (scream_packet_chunk *) chunk->datum;
      short datum_len;

      chunk->type = CHUNK_UPDATE_TODO;
      for (i = 0; i < column_count; i++)
	{
	  if (sqlite3_column_type (select, i) != SQLITE_NULL)
	    {
	      int int_val;

	      datum_len = ((sqlite3_column_type (select, i) == SQLITE_INTEGER)
			   ? sizeof (uint32_t)
			   : sqlite3_column_bytes (select, i));
	      switch (i)
		{
		case 0:
		  datum->type = CHUNK_TODO_ID;
		  break;
		case 1:
		  datum->type = CHUNK_TODO_TITLE;
		  break;
		case 2:
		  datum->type = CHUNK_TODO_DEADLINE;
		  break;
		case 3:
		  datum->type = CHUNK_TODO_PRIORITY;
		  break;
		case 4:
		  datum->type = CHUNK_TODO_STATUS;
		  break;
		case 5:
		  datum->type = CHUNK_TODO_DESCRIPTION;
		  break;
		case 6:
		  datum->type = CHUNK_TODO_REVISION;
		  break;
		default:
		  fprintf (stderr, "Invalid index for a CHUNK_UPDATE_TODO\n");
		  goto error_db;
		}
	      switch (sqlite3_column_type (select, i))
		{
		case SQLITE_INTEGER:
		  int_val = htonl (sqlite3_column_int (select, i));
		  memcpy (datum->datum, &int_val, sizeof (int_val));
		  break;
		case SQLITE_TEXT:
		  memcpy (datum->datum,
			  sqlite3_column_text (select, i),
			  sqlite3_column_bytes (select, i));
		  break;
		default:
		  fprintf (stderr, "Invalid column type for a CHUNK_UPDATE_TODO\n");
		  goto error_db;
		}
	      datum->len = htons (datum_len);
	      datum = (scream_packet_chunk *) (((char *) datum)
					       + sizeof (*datum) + datum_len);
	      chunk_len += sizeof (scream_packet_chunk) + datum_len;
	    }
	}
      chunk->len = htons (chunk_len);
      chunk = (scream_packet_chunk *) (((char *) chunk) + sizeof (*chunk) + chunk_len);
    }
  if (step_result != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error in retrieving updated todo");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (select))
    {
      sqlite3_perror (db, "Cannot finalize select statement");
    }

 error:
  return rc;
}

int
get_remote_todo (sqlite3 *db, int id, sqlite3_stmt **result)
{
  int rc = -1;

  if (*result == NULL)
    {
      if (sqlite3_prepare_v2 (db,
			      "select id, title, deadline, priority, status, description, revision"
			      " from sync"
			      " where id = ?",
			      -1,
			      result,
			      NULL))
	{
	  sqlite3_perror (db, "Cannot prepare sync todo select statement");
	  return -1;
	}      
    }
  else
    {
      if (sqlite3_reset (*result))
	{
	  sqlite3_perror (db, "Cannot reset sync todo select statement");
	  goto error_db;
	}
      if (sqlite3_clear_bindings (*result))
	{
	  sqlite3_perror (db, "Cannot clear bindings of sync todo select statement");
	  goto error_db;
	}
    }

  if (sqlite3_bind_int (*result, 1, id))
    {
      sqlite3_perror (db, "Cannot bind ID");
      goto error_db;
    }

  rc = sqlite3_step (*result);
  switch (rc)
    {
    case SQLITE_ROW:
    case SQLITE_DONE:
      return rc;
    default:
      sqlite3_perror (db, "Cannot execute sync todo select statement");
    }  

 error_db:
  if (sqlite3_finalize (*result))
    {
      sqlite3_perror (db, "Cannot finalize sync todo select statement");
    }

  return rc;
}

#define PICK_LOCAL 1
#define PICK_REMOTE 2
#define ERROR 0
int
resolve_two_items_conflict (sqlite3_stmt *local_todo, sqlite3_stmt *remote_todo)
{
  char ans[3];

  printf ("[Conflict]\n"
	  "\tLocal item:\n"
	  "\t\tTitle: %s\n"
	  "\t\tDeadline: %s\n"
	  "\t\tPriority: %d\n"
	  "\t\tStatus: %s\n"
	  "\t\tDescription: %s\n"
	  "\tRemote item:\n"
	  "\t\tTitle: %s\n"
	  "\t\tDeadline: %s\n"
	  "\t\tPriority: %d\n"
	  "\t\tStatus: %s\n"
	  "\t\tDescription: %s\n"
	  "Pick which one (L/R)? ",
	  sqlite3_column_text (local_todo, 1),
	  sqlite3_column_text (local_todo, 2),
	  sqlite3_column_int (local_todo, 3),
	  sqlite3_column_text (local_todo, 4),
	  sqlite3_column_text (local_todo, 5),
	  sqlite3_column_text (remote_todo, 1),
	  sqlite3_column_text (remote_todo, 2),
	  sqlite3_column_int (remote_todo, 3),
	  sqlite3_column_text (remote_todo, 4),
	  sqlite3_column_text (remote_todo, 5));

  fgets (ans, sizeof (ans), stdin);
  switch (ans[0])
    {
    case 'L':
    case 'l':
      return PICK_LOCAL;
    case 'R':
    case 'r':
      return PICK_REMOTE;
    default:
      return ERROR;
    }
}

int
resolve_remote_deletion (sqlite3_stmt *local_todo)
{
  char ans[3];

  printf ("[Conflict]\n"
	  "\tLocal item:\n"
	  "\t\tTitle: %s\n"
	  "\t\tDeadline: %s\n"
	  "\t\tPriority: %d\n"
	  "\t\tStatus: %s\n"
	  "\t\tDescription: %s\n"
	  "\tRemote item is DELETED\n"
	  "Pick which one (L/R)? ",
	  sqlite3_column_text (local_todo, 1),
	  sqlite3_column_text (local_todo, 2),
	  sqlite3_column_int (local_todo, 3),
	  sqlite3_column_text (local_todo, 4),
	  sqlite3_column_text (local_todo, 5));

  fgets (ans, sizeof (ans), stdin);
  switch (ans[0])
    {
    case 'L':
    case 'l':
      return PICK_LOCAL;
    case 'R':
    case 'r':
      return PICK_REMOTE;
    default:
      return ERROR;
    }
}

int
resolve_local_deletion (sqlite3_stmt *remote_todo)
{
  char ans[3];

  printf ("[Conflict]\n"
	  "\tLocal item is DELETED\n"
	  "\tRemote item:\n"
	  "\t\tTitle: %s\n"
	  "\t\tDeadline: %s\n"
	  "\t\tPriority: %d\n"
	  "\t\tStatus: %s\n"
	  "\t\tDescription: %s\n"
	  "Pick which one (L/R)? ",
	  sqlite3_column_text (remote_todo, 1),
	  sqlite3_column_text (remote_todo, 2),
	  sqlite3_column_int (remote_todo, 3),
	  sqlite3_column_text (remote_todo, 4),
	  sqlite3_column_text (remote_todo, 5));

  fgets (ans, sizeof (ans), stdin);
  switch (ans[0])
    {
    case 'L':
    case 'l':
      return PICK_LOCAL;
    case 'R':
    case 'r':
      return PICK_REMOTE;
    default:
      return ERROR;
    }
}

int
update_todo_revision (sqlite3 *db, int id, int new_revision)
{
  int rc = -1;
  sqlite3_stmt *update_stmt;

  if (sqlite3_prepare_v2 (db,
			  "update todo set revision = ? where id = ?",
			  -1,
			  &update_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare update todo revision statement");
      return -1;
    }

  if (sqlite3_bind_int (update_stmt, 2, id))
    {
      sqlite3_perror (db, "Cannot bind ID to update todo revision statement");
      goto error_db;
    }
  if (sqlite3_bind_int (update_stmt, 1, new_revision))
    {
      sqlite3_perror (db, "Cannot bind new revision to update todo revision statement");
      goto error_db;
    }

  if (sqlite3_step (update_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute update todo revision statement");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (update_stmt))
    {
      sqlite3_perror (db, "Cannot finalize update todo revision statement");
    }

  return rc;
}

int
update_sync (sqlite3 *db, sqlite3_stmt *local_todo)
{
  int rc = -1;
  sqlite3_stmt *update_stmt;

  if (sqlite3_prepare_v2 (db,
			  "update sync set"
			  " title = case title when ? then NULL else ? end,"
			  " deadline = case deadline when ? then NULL else ? end,"
			  " priority = case priority when ? then NULL else ? end,"
			  " status = case status when ? then NULL else ? end,"
			  " description = case description when ? then NULL else ? end,"
			  " revision = NULL"
			  " where id = ?",
			  -1,
			  &update_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare sync update statement");
      return -1;
    }

  if (sqlite3_bind_text (update_stmt, 1, sqlite3_column_text (local_todo, 1), -1, SQLITE_TRANSIENT)
      || sqlite3_bind_text (update_stmt, 2, sqlite3_column_text (local_todo, 1), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind title to sync update statement");
      goto error_db;
    }
  if (sqlite3_bind_text (update_stmt, 3, sqlite3_column_text (local_todo, 2), -1, SQLITE_TRANSIENT)
      || sqlite3_bind_text (update_stmt, 4, sqlite3_column_text (local_todo, 2), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind deadline to sync update statement");
      goto error_db;
    }
  if (sqlite3_bind_int (update_stmt, 5, sqlite3_column_int (local_todo, 3))
      || sqlite3_bind_int (update_stmt, 6, sqlite3_column_int (local_todo, 3)))
    {
      sqlite3_perror (db, "Cannot bind priority to sync update statement");
      goto error_db;
    }
  if (sqlite3_bind_text (update_stmt, 7, sqlite3_column_text (local_todo, 4), -1, SQLITE_TRANSIENT)
      || sqlite3_bind_text (update_stmt, 8, sqlite3_column_text (local_todo, 4), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind status to sync update statement");
      goto error_db;
    }
  if (sqlite3_bind_text (update_stmt, 9, sqlite3_column_text (local_todo, 5), -1, SQLITE_TRANSIENT)
      || sqlite3_bind_text (update_stmt, 10, sqlite3_column_text (local_todo, 5), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind description to sync update statement");
      goto error_db;
    }
  if (sqlite3_bind_int (update_stmt, 11, sqlite3_column_int (local_todo, 0)))
    {
      sqlite3_perror (db, "Cannot bind ID to sync update statement");
      goto error_db;
    }

  if (sqlite3_step (update_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute sync update statement");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (update_stmt))
    {
      sqlite3_perror (db, "Cannot finalize sync update statement");
    }

  return rc;
}

int
replace_todo (sqlite3 *db, sqlite3_stmt *remote_todo)
{
  int rc = -1;
  sqlite3_stmt *replace_stmt;
  sqlite3_stmt *delete_stmt;
  int remote_todo_id = sqlite3_column_int (remote_todo, 0);

  if (sqlite3_prepare_v2 (db,
			  "update todo set"
			  " title = ?,"
			  " deadline = ?,"
			  " priority = ?,"
			  " status = ?,"
			  " description = ?,"
			  " revision = ? + 1"
			  " where id = ?",
			  -1,
			  &replace_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare replace todo statement");
      return -1;
    }

  if (sqlite3_bind_text (replace_stmt, 1, sqlite3_column_text (remote_todo, 1), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind title to replace todo statement");
      goto error_db;
    }
  if (sqlite3_bind_text (replace_stmt, 2, sqlite3_column_text (remote_todo, 2), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind deadline to replace todo statement");
      goto error_db;
    }
  if (sqlite3_bind_int (replace_stmt, 3, sqlite3_column_int (remote_todo, 3)))
    {
      sqlite3_perror (db, "Cannot bind priority to replace todo statement");
      goto error_db;
    }
  if (sqlite3_bind_text (replace_stmt, 4, sqlite3_column_text (remote_todo, 4), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind status to replace todo statement");
      goto error_db;
    }
  if (sqlite3_bind_text (replace_stmt, 5, sqlite3_column_text (remote_todo, 5), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind description to replace todo statement");
      goto error_db;
    }
  if (sqlite3_bind_int (replace_stmt, 6, sqlite3_column_int (remote_todo, 6)))
    {
      sqlite3_perror (db, "Cannot bind revision to replace todo statement");
      goto error_db;
    }
  if (sqlite3_bind_int (replace_stmt, 7, remote_todo_id))
    {
      sqlite3_perror (db, "Cannot bind ID to replace todo statement");
      goto error_db;
    }

  if (sqlite3_step (replace_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute replace todo statement");
      goto error_db;
    }

  if (sqlite3_finalize (replace_stmt))
    {
      sqlite3_perror (db, "Cannot finalize replace todo statement");
      goto error;
    }

  if (sqlite3_prepare_v2 (db,
			  "delete from deleted_todo where id = ?",
			  -1,
			  &delete_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare delete from deleted_todo statement");
      return -1;
    }

  if (sqlite3_bind_int (delete_stmt, 1, remote_todo_id))
    {
      sqlite3_perror (db, "Cannot bind ID to delete from deleted_todo statement");
      goto error_db_del;
    }

  if (sqlite3_step (delete_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute delete from deleted_todo statement");
      goto error_db_del;
    }

  rc = 0;

 error_db_del:
  if (sqlite3_finalize (delete_stmt))
    {
      sqlite3_perror (db, "Cannot finalize delete from deleted_todo statement");
    }

  return rc;

 error_db:
  if (sqlite3_finalize (replace_stmt))
    {
      sqlite3_perror (db, "Cannot finalize replace todo statement");
    }

 error:
  return rc;
}

int
insert_to_sync (sqlite3 *db, sqlite3_stmt *local_todo, bool new)
{
  int rc = -1;
  int revision;
  sqlite3_stmt *insert_stmt;

  if (sqlite3_prepare_v2 (db,
			  "insert into sync (id, title, deadline, priority, status, description, revision)"
			  " values (?, ?, ?, ?, ?, ?, ?)",
			  -1,
			  &insert_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare insert sync statement");
      return -1;
    }

  if (new != TRUE && sqlite3_bind_int (insert_stmt, 1, sqlite3_column_int (local_todo, 0)))
    {
      sqlite3_perror (db, "Cannot bind ID to insert sync statement");
      goto error_db;
    }
  if (sqlite3_bind_text (insert_stmt, 2, sqlite3_column_text (local_todo, 1), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind title to insert sync statement");
      goto error_db;
    }
  if (sqlite3_bind_text (insert_stmt, 3, sqlite3_column_text (local_todo, 2), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind deadline to insert sync statement");
      goto error_db;
    }
  if (sqlite3_bind_int (insert_stmt, 4, sqlite3_column_int (local_todo, 3)))
    {
      sqlite3_perror (db, "Cannot bind priority to insert sync statement");
      goto error_db;
    }
  if (sqlite3_bind_text (insert_stmt, 5, sqlite3_column_text (local_todo, 4), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind status to insert sync statement");
      goto error_db;
    }
  if (sqlite3_bind_text (insert_stmt, 6, sqlite3_column_text (local_todo, 5), -1, SQLITE_TRANSIENT))
    {
      sqlite3_perror (db, "Cannot bind description to insert sync statement");
      goto error_db;
    }
  revision = sqlite3_column_int (local_todo, 6);
  if (sqlite3_bind_int (insert_stmt, 7, new == TRUE ? -1 : -1 * (revision + 2)))
    {
      sqlite3_perror (db, "Cannot bind revision to insert sync statement");
      goto error_db;
    }

  if (sqlite3_step (insert_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute insert sync statement");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (insert_stmt))
    {
      sqlite3_perror (db, "Cannot finalize insert sync statement");
    }

  return rc;
}

int
insert_to_local (sqlite3 *db, int remote_id)
{
  int rc = -1;
  int revision;
  sqlite3_stmt *insert_stmt;

  if (sqlite3_prepare_v2 (db,
			  "insert into todo"
			  " select id, title, deadline, priority, status, description, revision + 1"
			  " from sync"
			  " where id = ?",
			  -1,
			  &insert_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare insert local statement");
      return -1;
    }

  if (sqlite3_bind_int (insert_stmt, 1, remote_id))
    {
      sqlite3_perror (db, "Cannot bind ID to insert local statement");
      goto error_db;
    }

  if (sqlite3_step (insert_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute insert local statement");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (insert_stmt))
    {
      sqlite3_perror (db, "Cannot finalize insert local statement");
    }

  return rc;
}

int
remove_sync (sqlite3 *db, int remote_todo_id)
{
  int rc = -1;
  sqlite3_stmt *remove_stmt;

  if (sqlite3_prepare_v2 (db,
			  "delete from sync where id = ?",
			  -1,
			  &remove_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare remove sync statement");
      return -1;
    }

  if (sqlite3_bind_int (remove_stmt, 1, remote_todo_id))
    {
      sqlite3_perror (db, "Cannot bind ID to remove sync statement");
      goto error_db;
    }

  if (sqlite3_step (remove_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute remove sync statement");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (remove_stmt))
    {
      sqlite3_perror (db, "Cannot finalize remove sync statement");
    }

  return rc;  
}

int
delete_local (sqlite3 *db, int local_todo_id)
{
  int rc = -1;
  sqlite3_stmt *delete_stmt;

  if (sqlite3_prepare_v2 (db,
			  "delete from todo where id = ?",
			  -1,
			  &delete_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare delete local statement");
      return -1;
    }

  if (sqlite3_bind_int (delete_stmt, 1, local_todo_id))
    {
      sqlite3_perror (db, "Cannot bind ID to delete local statement");
      goto error_db;
    }

  if (sqlite3_step (delete_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute delete local statement");
      goto error_db;
    }

  if (sqlite3_finalize (delete_stmt))
    {
      sqlite3_perror (db, "Cannot finalize delete local statement");
      return -1;
    }

  if (sqlite3_prepare_v2 (db,
			  "delete from deleted_todo where id = ?",
			  -1,
			  &delete_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare delete from deleted_todo statement");
      return -1;
    }

  if (sqlite3_bind_int (delete_stmt, 1, local_todo_id))
    {
      sqlite3_perror (db, "Cannot bind ID to delete from deleted_todo statement");
      goto error_db;
    }

  if (sqlite3_step (delete_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute delete from deleted_todo statement");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (delete_stmt))
    {
      sqlite3_perror (db, "Cannot finalize delete from deleted_todo statement");
    }

  return rc;
}

int
delete_sync (sqlite3 *db, int remote_todo_id)
{
  int rc = -1;
  sqlite3_stmt *delete_stmt;

  if (sqlite3_prepare_v2 (db,
			  "update sync set"
			  " title = NULL,"
			  " deadline = NULL,"
			  " priority = NULL,"
			  " status = NULL,"
			  " description = NULL,"
			  " revision = NULL"
			  " where id = ?",
			  -1,
			  &delete_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare delete sync statement");
      return -1;
    }

  if (sqlite3_bind_int (delete_stmt, 1, remote_todo_id))
    {
      sqlite3_perror (db, "Cannot bind ID to delete sync statement");
      goto error_db;
    }

  if (sqlite3_step (delete_stmt) != SQLITE_DONE)
    {
      sqlite3_perror (db, "Cannot execute delete sync statement");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (delete_stmt))
    {
      sqlite3_perror (db, "Cannot finalize delete sync statement");
    }

  return rc;
}

bool
are_todos_equal (sqlite3_stmt *local_todo, sqlite3_stmt *remote_todo)
{
  if ((strcmp (sqlite3_column_text (local_todo, 1), sqlite3_column_text (remote_todo, 1)) == 0)
      && (strcmp (sqlite3_column_text (local_todo, 2), sqlite3_column_text (remote_todo, 2)) == 0)
      && (sqlite3_column_int (local_todo, 3) == sqlite3_column_int (remote_todo, 3))
      && (strcmp (sqlite3_column_text (local_todo, 4), sqlite3_column_text (remote_todo, 4)) == 0)
      && (strcmp (sqlite3_column_text (local_todo, 5), sqlite3_column_text (remote_todo, 5)) == 0))
    {
      return TRUE;
    }

  return FALSE;
}

int
is_local_deleted (sqlite3 *db, int local_todo_id, bool *is_deleted)
{
  int rc = -1;
  int step_result;
  sqlite3_stmt *select_stmt;

  if (sqlite3_prepare_v2 (db,
			  "select id from deleted_todo where id = ?",
			  -1,
			  &select_stmt,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare select deleted_todo statement");
      return -1;
    }
  if (sqlite3_bind_int (select_stmt, 1, local_todo_id))
    {
      sqlite3_perror (db, "Cannot bind ID to select deleted_todo statement");
      return -1;
    }
  step_result = sqlite3_step (select_stmt);
  if (step_result == SQLITE_ROW)
    {
      *is_deleted = TRUE;
    }
  else if (step_result == SQLITE_DONE)
    {
      *is_deleted = FALSE;
    }
  else
    {
      sqlite3_perror (db, "Error in retrieving deleted_todo");
      goto error_db;
    }

  rc = 0;

 error_db:
  if (sqlite3_finalize (select_stmt))
    {
      sqlite3_perror (db, "Cannot finalize select deleted_todo statement");
    }

  return rc;
}

int
import_new_todos (sqlite3 *db)
{
  char *err_msg;

  if (sqlite3_exec (db,
		    "insert into todo"
		    " select id, title, deadline, priority, status, description, revision + 1"
		    " from sync"
		    " where id not in (select id from todo)"
		    " and title is not null and deadline is not null"
		    " and priority is not null and status is not null"
		    " and description is not null and revision is not null",
		    NULL,
		    NULL,
		    &err_msg))
    {
      fprintf (stderr, "Cannot initialize todo table: %s\n", err_msg);
      sqlite3_clean_free (&err_msg);
      return -1;
    }
  sqlite3_clean_free (&err_msg);

  if (sqlite3_changes (db))
    {
      fprintf (stderr, "[A16]\n");
    }

  return 0;
}

int
adjust_new_todo_ids_and_revisions (sqlite3 *db)
{
  char *err_msg;

  if (sqlite3_exec (db,
		    "delete from todo where revision = -1",
		    NULL,
		    NULL,
		    &err_msg))
    {
      fprintf (stderr, "Cannot delete new todos in todo table: %s\n", err_msg);
      sqlite3_clean_free (&err_msg);
      return -1;
    }
  sqlite3_clean_free (&err_msg);

  if (sqlite3_exec (db,
		    "insert into todo (id, title, deadline, priority, status, description, revision)"
		    " select id, title, deadline, priority, status, description, 1"
		    " from sync"
		    " where revision = -1",
		    NULL,
		    NULL,
		    &err_msg))
    {
      fprintf (stderr, "Cannot adjust new todo ids and revisions: %s\n", err_msg);
      sqlite3_clean_free (&err_msg);
      return -1;
    }
  sqlite3_clean_free (&err_msg);

  return 0;
}

int
sync_core (sqlite3 *db,
	   scream_packet_chunk *todos,
	   size_t server_client_data_len,
	   scream_packet_client_server_data **client_server_data,
	   size_t *client_server_data_len)
{
  int rc = -1;
  int step_result;
  sqlite3_stmt *local_todo;
  sqlite3_stmt *remote_todo = NULL;
  scream_packet_chunk *todos_end = (scream_packet_chunk *) (((char *) todos) + server_client_data_len);
  char *err_msg;

  if (sqlite3_exec (db,
		    "drop table if exists sync",
		    NULL,
		    NULL,
		    &err_msg))
    {
      fprintf (stderr, "Cannot drop sync table: %s\n", err_msg);
      sqlite3_clean_free (&err_msg);
      return -1;
    }
  sqlite3_clean_free (&err_msg);

  if (sqlite3_exec (db,
		    "create table if not exists sync ("
		    " id integer not null primary key autoincrement,"
		    " title text,"
		    " deadline text,"
		    " priority integer,"
		    " status text,"
		    " description text,"
		    " revision integer)",
		    NULL,
		    NULL,
		    &err_msg))
    {
      fprintf (stderr, "Cannot create sync table: %s\n", err_msg);
      sqlite3_clean_free (&err_msg);
      return -1;
    }
  sqlite3_clean_free (&err_msg);

  // Populating sync table
  if (todos != todos_end)
    {
      if (process_todos (db, todos, todos_end))
	{
	  return -1;
	}
    }

  // Synchronization logic
  if (sqlite3_prepare_v2 (db,
			  "select id, title, deadline, priority, status, description, revision from todo",
			  -1,
			  &local_todo,
			  NULL))
    {
      sqlite3_perror (db, "Cannot prepare local todo statement");
      goto error_db;
    }
  while ((step_result = sqlite3_step (local_todo)) == SQLITE_ROW)
    {
      int local_id = sqlite3_column_int (local_todo, 0);
      int local_rev = sqlite3_column_int (local_todo, 6);

      if (local_rev == -1) // [A1]
	{
	  if (insert_to_sync (db, local_todo, TRUE))
	    {
	      fprintf (stderr, "Failure in insert_to_sync\n");
	      goto error_db;
	    }
	  fprintf (stderr, "[A1]\n");
	}
      else
	{
	  int remote_todo_status;

	  if ((remote_todo_status = get_remote_todo (db, local_id, &remote_todo))
	      != SQLITE_ROW && remote_todo_status != SQLITE_DONE)
	    {
	      goto error_db;
	    }

	  if (remote_todo_status == SQLITE_ROW)
	    {
	      int remote_id = sqlite3_column_int (remote_todo, 0);
	      int remote_rev = sqlite3_column_int (remote_todo, 6);
	      bool is_deleted;

	      if (is_local_deleted (db, local_id, &is_deleted))
		{
		  goto error_db;
		}
	      if (is_deleted == TRUE)
		{
		  if (remote_rev > local_rev) // [A10]
		    {
		      if (replace_todo (db, remote_todo))
			{
			  fprintf (stderr, "Failure in replace_todo\n");
			  goto error_db;
			}
		      if (remove_sync (db, remote_id))
			{
			  fprintf (stderr, "Failure in remove_sync\n");
			  goto error_db;
			}
		      fprintf (stderr, "[A10]\n");
		    }
		  else if (remote_rev < local_rev) // [A14]
		    {
		      if (delete_local (db, local_id))
			{
			  fprintf (stderr, "Failure in delete_local\n");
			  goto error_db;
			}		  
		      if (delete_sync (db, remote_id))
			{
			  fprintf (stderr, "Failure in delete_sync\n");
			  goto error_db;
			}
		      fprintf (stderr, "[A14]\n");
		    }
		  else
		    {
		      if (are_todos_equal (local_todo, remote_todo) == TRUE) // [A11]
			{
			  if (delete_local (db, local_id))
			    {
			      fprintf (stderr, "Failure in delete_local\n");
			      goto error_db;
			    }		  
			  if (delete_sync (db, remote_id))
			    {
			      fprintf (stderr, "Failure in delete_sync\n");
			      goto error_db;
			    }
			  fprintf (stderr, "[A11]\n");
			}
		      else
			{
			  switch (resolve_local_deletion (remote_todo))
			    {
			    case PICK_LOCAL: // [A13]
			      if (delete_local (db, local_id))
				{
				  fprintf (stderr, "Failure in delete_local\n");
				  goto error_db;
				}		  
			      if (delete_sync (db, remote_id))
				{
				  fprintf (stderr, "Failure in delete_sync\n");
				  goto error_db;
				}
			      fprintf (stderr, "[A13]\n");
			      break;
			    case PICK_REMOTE: // [A12]
			      if (replace_todo (db, remote_todo))
				{
				  fprintf (stderr, "Failure in replace_todo\n");
				  goto error_db;
				}
			      if (remove_sync (db, remote_id))
				{
				  fprintf (stderr, "Failure in remove_sync\n");
				  goto error_db;
				}
			      fprintf (stderr, "[A12]\n");
			      break;
			    default:
			      fprintf (stderr, "Failure in resolve_local_deletion\n");
			      goto error_db;
			    }
			}
		    }
		}
	      else
		{
		  if (local_rev > remote_rev)
		    {
		      if (are_todos_equal (local_todo, remote_todo) == TRUE) // [A2]
			{
			  if (remove_sync (db, remote_id))
			    {
			      fprintf (stderr, "Failure in remove_sync\n");
			      goto error_db;
			    }
			  fprintf (stderr, "[A2]\n");
			}
		      else // [A3]
			{
			  if (update_sync (db, local_todo))
			    {
			      fprintf (stderr, "Failure in update_sync\n");
			      goto error_db;
			    }
			  if (update_todo_revision (db, local_id, local_rev + 1))
			    {
			      fprintf (stderr, "Failure in update_todo_revision\n");
			      goto error_db;
			    }
			  fprintf (stderr, "[A3]\n");
			}
		    }
		  else if (local_rev < remote_rev) // [A4]
		    {
		      if (replace_todo (db, remote_todo))
			{
			  fprintf (stderr, "Failure in replace_todo\n");
			  goto error_db;
			}
		      if (remove_sync (db, remote_id))
			{
			  fprintf (stderr, "Failure in remove_sync\n");
			  goto error_db;
			}
		      fprintf (stderr, "[A4]\n");
		    }
		  else
		    {
		      if (are_todos_equal (local_todo, remote_todo) == TRUE) // [A15]
			{
			  if (remove_sync (db, remote_id))
			    {
			      fprintf (stderr, "Failure in remove_sync\n");
			      goto error_db;
			    }
			  if (update_todo_revision (db, local_id, local_rev + 1))
			    {
			      fprintf (stderr, "Failure in update_todo_revision\n");
			      goto error_db;
			    }
			  fprintf (stderr, "[A15]\n");
			}
		      else
			{
			  switch (resolve_two_items_conflict (local_todo, remote_todo))
			    {
			    case PICK_LOCAL: // [A5]
			      if (update_todo_revision (db, local_id, local_rev + 2))
				{
				  fprintf (stderr, "Failure in update_todo_revision\n");
				  goto error_db;
				}      
			      if (update_sync (db, local_todo))
				{
				  fprintf (stderr, "Failure in update_sync\n");
				  goto error_db;
				}
			      fprintf (stderr, "[A5]\n");
			      break;
			    case PICK_REMOTE: // [A6]
			      if (replace_todo (db, remote_todo))
				{
				  fprintf (stderr, "Failure in replace_todo\n");
				  goto error_db;
				}
			      if (remove_sync (db, remote_id))
				{
				  fprintf (stderr, "Failure in remove_sync\n");
				  goto error_db;
				}
			      fprintf (stderr, "[A6]\n");
			      break;
			    default:
			      fprintf (stderr, "Failure in resolve_two_items_conflict\n");
			      goto error_db;
			    }
			}
		    }
		}
	    }
	  else
	    {
	      bool is_deleted;

	      if (is_local_deleted (db, local_id, &is_deleted))
		{
		  goto error_db;
		}
	      if (is_deleted == TRUE) // [A9]
		{
		  if (delete_local (db, local_id))
		    {
		      fprintf (stderr, "Failure in delete_local\n");
		      goto error_db;
		    }
		  fprintf (stderr, "[A9]\n");
		}
	      else
		{
		  switch (resolve_remote_deletion (local_todo))
		    {
		    case PICK_LOCAL: // [A8]
		      if (insert_to_sync (db, local_todo, FALSE))
			{
			  fprintf (stderr, "Failure in insert_to_sync\n");
			  goto error_db;
			}
		      if (update_todo_revision (db, local_id, local_rev + 1))
			{
			  fprintf (stderr, "Failure in update_todo_revision\n");
			  goto error_db;
			}
		      fprintf (stderr, "[A8]\n");
		      break;
		    case PICK_REMOTE: // [A7]
		      if (delete_local (db, local_id))
			{
			  fprintf (stderr, "Failure in delete_local\n");
			  goto error_db;
			}
		      fprintf (stderr, "[A7]\n");
		      break;
		    default:
		      fprintf (stderr, "Failure in resolve_remote_deletion\n");
		      goto error_db;
		    }
		}
	    }
	}
    }
  if (step_result != SQLITE_DONE)
    {
      sqlite3_perror (db, "Error in retrieving local todo");
      goto error_db;
    }

  // Adjust new todo IDs and revisions
  if (adjust_new_todo_ids_and_revisions (db))
    {
      goto error_db;
    }

  if (import_new_todos (db)) // [A16]
    {
      goto error_db;
    }

  // Send synchronization result
  if (create_sync_data (db, client_server_data, client_server_data_len))
    {
      goto error_db;
    }

  if (sqlite3_exec (db,
		    "delete from sync",
		    NULL,
		    NULL,
		    &err_msg))
    {
      fprintf (stderr, "Cannot empty sync table: %s\n", err_msg);
      sqlite3_clean_free (&err_msg);
      goto error_db;
    }
  sqlite3_clean_free (&err_msg);

  rc = 0;

 error_db:
  if (sqlite3_finalize (local_todo))
    {
      sqlite3_perror (db, "Cannot finalize local todo statement");
    }
  if (remote_todo != NULL && sqlite3_finalize (remote_todo))
    {
      sqlite3_perror (db, "Cannot finalize remote todo statement");
    }

  return rc;  
}

int
sync_todo (sqlite3 *db, int *sock, int user_id)
{
  int rc = -1;
  ssize_t bytes_sent;
  ssize_t bytes_received;
  scream_packet_register register_packet;
  scream_packet_register_ack register_ack_packet;
  scream_packet_server_client_sync server_client_sync;
  scream_packet_server_client_resp server_client_resp;
  scream_packet_server_client_data *server_client_data = NULL;  
  scream_packet_server_client_resp_ack server_client_resp_ack;
  size_t server_client_data_len;
  scream_packet_client_server_data *client_server_data = NULL;
  size_t client_server_data_len;
  scream_packet_client_server_sync client_server_sync;
  scream_packet_client_server_resp client_server_resp;
  scream_packet_client_server_ack client_server_ack;
  scream_packet_reset reset_packet;
  scream_packet_reset_ack reset_ack_packet;
  const int REGISTER_MAX_RETRY_COUNT = 5;
  struct sockaddr_in dest_addr;
  struct timeval timeout;
  int i;

  dest_addr.sin_family = AF_INET;
  inet_aton ("127.0.0.1", &dest_addr.sin_addr);
  dest_addr.sin_port = htons (SC_DEFAULT_PORT);

  *sock = socket (AF_INET, SOCK_DGRAM, 0);
  if (*sock == -1)
    {
      return -1;
    }

  register_packet.type = SC_PACKET_REGISTER;
  register_packet.id = htonl (user_id);
  
  for (i = 0; i < REGISTER_MAX_RETRY_COUNT; i++)
    {
      if ((bytes_sent = sendto (*sock,
				&register_packet,
				sizeof (register_packet),
				0,
				(struct sockaddr *) &dest_addr,
				sizeof (dest_addr))) == -1)
	{
	  perror ("Cannot send register packet");
	  goto error_sock;
	}
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      if (set_timeout (*sock, &timeout) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      do
	{
	  if ((bytes_received = recvfrom (*sock,
					  &register_ack_packet,
					  sizeof (register_ack_packet),
					  0,
					  NULL,
					  NULL)) == -1)
	    {
	      if (errno == EAGAIN)
		{
		  fprintf (stderr, "Registration fail...\n");
		  break;
		}
	      else
		{
		  perror ("Cannot receive register ack");
		  goto error_sock_unset_timeout;
		}
	    }
	}
      while (register_ack_packet.type != SC_PACKET_REGISTER_ACK
	     || bytes_received != sizeof (register_ack_packet));
      if (unset_timeout (*sock) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      if (register_ack_packet.type == SC_PACKET_REGISTER_ACK
	  && bytes_received == sizeof (register_ack_packet))
	{
	  break;
	}
      fprintf (stderr, "Registration fail...\n");
    }
  if (i == REGISTER_MAX_RETRY_COUNT)
    {
      goto success;
    }

  server_client_sync.type = SC_PACKET_SERVER_CLIENT_SYNC;
  while (1)
    {
      if ((bytes_sent = sendto (*sock,
				&server_client_sync,
				sizeof (server_client_sync),
				0,
				(struct sockaddr *) &dest_addr,
				sizeof (dest_addr))) == -1)
	{
	  perror ("Cannot send server-client sync packet");
	  goto error_sock;
	}
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      if (set_timeout (*sock, &timeout) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      do
	{
	  if ((bytes_received = recvfrom (*sock,
					  &server_client_resp,
					  sizeof (server_client_resp),
					  0,
					  NULL,
					  NULL)) == -1)
	    {
	      if (errno == EAGAIN)
		{
		  fprintf (stderr, "Server-to-client sync failed...\n");
		  break;
		}
	      else
		{
		  perror ("Cannot receive server client resp");
		  goto error_sock_unset_timeout;
		}
	    }
	}
      while (server_client_resp.type != SC_PACKET_SERVER_CLIENT_RESP
	     || bytes_received != sizeof (server_client_resp));
      if (unset_timeout (*sock) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      if (server_client_resp.type == SC_PACKET_SERVER_CLIENT_RESP
	  && bytes_received == sizeof (server_client_resp))
	{
	  break;
	}
      fprintf (stderr, "Server-to-client sync failed...\n");
    }
  server_client_data_len = ntohl (server_client_resp.len);

  server_client_data = malloc(server_client_data_len);
  if (server_client_data == NULL)
    {
      fprintf (stderr, "Insufficient memory to receive server-client data\n");
      goto error_sock;
    }

  server_client_resp_ack.type = SC_PACKET_SERVER_CLIENT_RESP_ACK;
  while (1)
    {
      if ((bytes_sent = sendto (*sock,
				&server_client_resp_ack,
				sizeof (server_client_resp_ack),
				0,
				(struct sockaddr *) &dest_addr,
				sizeof (dest_addr))) == -1)
	{
	  perror ("Cannot send server-client resp ack packet");
	  goto error_free_recv_data;
	}
      timeout.tv_sec = 10;
      timeout.tv_usec = 0;
      if (set_timeout (*sock, &timeout) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      do
	{
	  if ((bytes_received = recvfrom (*sock,
					  server_client_data,
					  server_client_data_len,
					  0,
					  NULL,
					  NULL)) == -1)
	    {
	      if (errno == EAGAIN)
		{
		  fprintf (stderr, "Server-to-client data failed...\n");
		  break;
		}
	      else
		{
		  perror ("Cannot receive server client data");
		  goto error_sock_unset_timeout;
		}
	    }
	}
      while (server_client_data->type != SC_PACKET_SERVER_CLIENT_DATA
	     || bytes_received != server_client_data_len);
      if (unset_timeout (*sock) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      if (server_client_data->type == SC_PACKET_SERVER_CLIENT_DATA
	  && bytes_received == server_client_data_len)
	{
	  break;
	}
      fprintf (stderr, "Server-to-client data failed...\n");
    }

  if (sync_core (db,
		 server_client_data->data,
		 server_client_data_len - sizeof (*server_client_data),
		 &client_server_data,
		 &client_server_data_len))
    {
      goto error_free_recv_data;
    }

  client_server_sync.type = SC_PACKET_CLIENT_SERVER_SYNC;
  client_server_sync.len = htonl (client_server_data_len);
  while (1)
    {
      if ((bytes_sent = sendto (*sock,
				&client_server_sync,
				sizeof (client_server_sync),
				0,
				(struct sockaddr *) &dest_addr,
				sizeof (dest_addr))) == -1)
	{
	  perror ("Cannot send client-server sync packet");
	  goto error_free_sent_data;
	}
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      if (set_timeout (*sock, &timeout) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      do
	{
	  if ((bytes_received = recvfrom (*sock,
					  &client_server_resp,
					  sizeof (client_server_resp),
					  0,
					  NULL,
					  NULL)) == -1)
	    {
	      if (errno == EAGAIN)
		{
		  fprintf (stderr, "Client-to-server resp failed...\n");
		  break;
		}
	      else
		{
		  perror ("Cannot receive client server resp");
		  goto error_sock_unset_timeout;
		}
	    }
	}
      while (client_server_resp.type != SC_PACKET_CLIENT_SERVER_RESP
	     || bytes_received != sizeof (client_server_resp));
      if (unset_timeout (*sock) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      if (client_server_resp.type == SC_PACKET_CLIENT_SERVER_RESP
	  && bytes_received == sizeof (client_server_resp))
	{
	  break;
	}
      fprintf (stderr, "Client-to-server resp failed...\n");
    }

  client_server_data->type = SC_PACKET_CLIENT_SERVER_DATA;
  while (1)
    {
      if ((bytes_sent = sendto (*sock,
				client_server_data,
				client_server_data_len,
				0,
				(struct sockaddr *) &dest_addr,
				sizeof (dest_addr))) == -1)
	{
	  perror ("Cannot send client-server data packet");
	  goto error_free_sent_data;
	}
      timeout.tv_sec = 10;
      timeout.tv_usec = 0;
      if (set_timeout (*sock, &timeout) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      do
	{
	  if ((bytes_received = recvfrom (*sock,
					  &client_server_ack,
					  sizeof (client_server_ack),
					  0,
					  NULL,
					  NULL)) == -1)
	    {
	      if (errno == EAGAIN)
		{
		  fprintf (stderr, "Client-to-server ack failed...\n");
		  break;
		}
	      else
		{
		  perror ("Cannot receive client server ack");
		  goto error_sock_unset_timeout;
		}
	    }
	}
      while (client_server_ack.type != SC_PACKET_CLIENT_SERVER_ACK
	     || bytes_received != sizeof (client_server_ack));
      if (unset_timeout (*sock) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      if (client_server_ack.type == SC_PACKET_CLIENT_SERVER_ACK
	  && bytes_received == sizeof (client_server_ack))
	{
	  break;
	}
      fprintf (stderr, "Client-to-server ack failed...\n");
    }

  reset_packet.type = SC_PACKET_RESET;
  while (1)
    {
      if ((bytes_sent = sendto (*sock,
				&reset_packet,
				sizeof (reset_packet),
				0,
				(struct sockaddr *) &dest_addr,
				sizeof (dest_addr))) == -1)
	{
	  perror ("Cannot send reset packet");
	  goto error_free_sent_data;
	}
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      if (set_timeout (*sock, &timeout) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      do
	{
	  if ((bytes_received = recvfrom (*sock,
					  &reset_ack_packet,
					  sizeof (reset_ack_packet),
					  0,
					  NULL,
					  NULL)) == -1)
	    {
	      if (errno == EAGAIN)
		{
		  fprintf (stderr, "Reset failed...\n");
		  break;
		}
	      else
		{
		  perror ("Cannot receive reset ack");
		  goto error_sock_unset_timeout;
		}
	    }
	}
      while (reset_ack_packet.type != SC_PACKET_RESET_ACK
	     || bytes_received != sizeof (reset_ack_packet));
      if (unset_timeout (*sock) != SC_ERR_SUCCESS)
	{
	  goto error_sock;
	}
      if (reset_ack_packet.type == SC_PACKET_RESET_ACK
	  && bytes_received == sizeof (reset_ack_packet))
	{
	  break;
	}
      fprintf (stderr, "Reset failed...\n");
    }

 success:
  rc = 0;

 error_sock_unset_timeout:
  unset_timeout (*sock);

 error_free_sent_data:
  if (client_server_data != NULL)
    {
      free (client_server_data);
    }

 error_free_recv_data:
  if (server_client_data != NULL)
    {
      free (server_client_data);
    }

 error_sock:
  if (close (*sock) == -1)
    {
      perror ("Cannot close socket");
    }
  *sock = -1;

  return rc;
}

err_code
setup_db(sqlite3 **db, const char *db_name)
{
  char *err_msg;

  if (sqlite3_open (db_name, db))
    {
      sqlite3_perror (*db, "Cannot open DB");
      return SC_ERR_DB;
    }

  if (sqlite3_exec (*db,
		    "create table if not exists todo"
		    " (id integer not null primary key autoincrement,"
		    "  title text,"
		    "  deadline text,"
		    "  priority integer,"
		    "  status text,"
		    "  description text,"
		    "  revision integer not null)",
		    NULL,
		    NULL,
		    &err_msg))
    {
      fprintf (stderr, "Cannot create todo table: %s\n", err_msg);
      sqlite3_clean_free (&err_msg);
      return SC_ERR_DB;
    }
  sqlite3_clean_free (&err_msg);

  if (sqlite3_exec (*db,
		    "create table if not exists deleted_todo"
		    " (id integer not null references todo (id)"
		    " on delete cascade on update cascade)",
		    NULL,
		    NULL,
		    &err_msg))
    {
      fprintf (stderr, "Cannot create deleted_todo table: %s\n", err_msg);
      sqlite3_clean_free (&err_msg);
      return SC_ERR_DB;
    }
  sqlite3_clean_free (&err_msg);

  return SC_ERR_SUCCESS;
}

size_t
get_user_input (char *buffer, size_t len, const char **err_msg)
{
  char *newline;

  fgets (buffer, len, stdin);
  if ((newline = strchr (buffer, '\n')) == NULL)
    {
      int next_char = getchar ();

      if (next_char == '\n' || next_char == EOF)
	{
	  return len - 1;
	}
      else
	{
	  *err_msg = "input too long";
	  return 0;
	}
    }

  *newline = '\0';

  return newline - buffer;
}

void
display_user_prompt (const char *prompt_msg, char *buffer, size_t len)
{
  const char *err_msg = NULL;

  do
    {
      if (err_msg != NULL)
	{
	  printf ("Error: %s\n", err_msg);
	  err_msg = NULL;
	}
      printf ("%s", prompt_msg);
    }
  while (get_user_input (buffer, len, &err_msg) == 0
	 || err_msg != NULL);
}

unsigned char
display_user_choice_prompt (const char *prompt_msg, unsigned char last_option)
{
  const char *err_msg = NULL;
  char ans[4];
  int num = 1;

  do
    {
      if (err_msg != NULL)
	{
	  printf ("Error: %s\n", err_msg);
	  err_msg = NULL;
	}
      if (num < 1 || num > last_option)
	{
	  printf ("Error: Invalid choice (%d)\n", num);
	}
      printf ("%s", prompt_msg);
    }
  while (get_user_input (ans, sizeof (ans), &err_msg) == 0
	 || err_msg != NULL
	 || (num = atoi (ans)) < 1
	 || num > last_option);

  return (unsigned char) num;
}

int
main (int argc, char **argv, char **envp)
{
  sqlite3 *db;
  bool is_terminated = FALSE;
  int sock = -1;
  int rc = EXIT_FAILURE;
  int user_id = -1;
  int dev_id = -1;
  char db_name[32];

  if (argc != 3)
    {
      fprintf (stderr, "Usage: %s USER_ID DEVICE_ID\n", argv[0]);
      exit (EXIT_FAILURE);
    }
  user_id = atoi (argv[1]);
  dev_id = atoi (argv[2]);
  snprintf (db_name, sizeof (db_name), "./todo_db_%d_dev_%d", user_id, dev_id);

  if (setup_db (&db, db_name) != SC_ERR_SUCCESS)
    {
      exit (EXIT_FAILURE);
    }

  do
    {
      printf ("[Menu (user_id = %d, dev_id = %d)]\n"
	      "1. List Todo\n"
	      "2. Create Todo\n"
	      "3. Edit Todo\n"
	      "4. Delete Todo\n"
	      "5. Sync Todo\n"
	      "\n"
	      "6. Quit\n"
	      "\n",
	      user_id, dev_id);

      switch (display_user_choice_prompt ("Choice (1-6)? ", 6))
	{
	case 1:
	  if (list_todo (db))
	    {
	      goto error_db;
	    }
	  break;
	case 2:
	  if (create_todo (db))
	    {
	      goto error_db;
	    }
	  break;
	case 3:
	  if (list_todo (db))
	    {
	      goto error_db;
	    }
	  if (edit_todo (db))
	    {
	      goto error_db;
	    }
	  break;
	case 4:
	  if (list_todo (db))
	    {
	      goto error_db;
	    }
	  if (delete_todo (db))
	    {
	      goto error_db;
	    }
	  break;
	case 5:
	  if (sync_todo (db, &sock, user_id))
	    {
	      goto error_sock;
	    }
	  break;
	case 6:
	  is_terminated = TRUE;
	  break;
	default:
	  fprintf (stderr, "Error: unexpected choice\n");
	  goto error_sock;
	}
    }
  while (is_terminated == FALSE);

  rc = EXIT_SUCCESS;

 error_sock:
  if (sock != -1 && close (sock))
    {
      perror ("Cannot close socket");
    }

 error_db:
  if (sqlite3_close (db))
    {
      sqlite3_perror (db, "Cannot close DB");
    }

  exit (rc);
}
