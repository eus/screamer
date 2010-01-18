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
 **************************************************************************//**
 * @file listen.h
 * @brief UDP logging program functions.
 * @author Tobias Heer <heer@cs.rwth-aachen.de>
 * @author Tadeus Prastowo <eus@member.fsf.org>
 *                                                            
 * This file provides the base functions for the UDP listener.
 ******************************************************************************/

#ifndef LISTEN_H
#define LISTEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h> /* time (...)*/
#include <netinet/in.h>
#include "scream-common.h" /* common headers and definitions */

/** The maximum number of clients that the listener can have. */
#define CLIENT_MAX_NUM 10

/** The client record is still vacant. */
#define VACANT (-1)

/** The communication state of a synchronization session. */
enum comm_state
{
  CLOSED = 0, /**< The session is closed. */
  WAIT_SERVER_CLIENT_SYNC, /**< Wait for the server-to-client sync request. */
  WAIT_SERVER_CLIENT_RESP_ACK, /**<
				* Wait for the server-to-client sync response
				* acknowledgement.
				*/
  WAIT_CLIENT_SERVER_SYNC, /**< Wait for the client-to-server sync request. */
  WAIT_CLIENT_SERVER_DATA, /**< Wait for the client-to-server sync data. */
  WAIT_RESET, /**< Wait for reset. */
};

/** The record of the client book-keeping structure. */
struct client_record
{
  int32_t id; /**< The means to identify a client. */
  struct sockaddr_in client_addr; /**< The primary client address. */
  enum comm_state state; /**< The communication state. */
  void *data; /**<
	       * The server-to-client/client-to-server data to be
	       * sent/received.
	       */
  size_t len; /**< The total length of client_record::data. */
}; 

/** The client book-keeping structure. */
struct client_db
{
  size_t len; /**< The number of records. */
  struct client_record *recs; /**< The client records. */
  sqlite3 *todo_db; /**< The todo DB. */
};

/**
 * Handle a scream packet according to the state machine.
 *
 * @param [in] client_addr the address of the client who sent the packet.
 * @param [in] type the type of the first packet in the socket receiving queue.
 * @param [in] sock the UDP socket on which the packet was received.
 * @param [in] db the client book-keeping data structure.
 *
 * @return An error code.
 */
err_code
listener_handle_packet (const struct sockaddr_in *client_addr,
			scream_packet_type type,
			int sock,
			struct client_db *db);

/** The information passed by listener_handle_packet() to the callee. */
struct listener_info
{
  int sock; /**<
	     * The socket from which the callee must take one and only one
	     * packet.
	     */
  const struct sockaddr_in *client_addr; /**<
					  * The originating address of the
					  * packet (won't be NULL).
					  */
  struct client_record *client_db_rec; /**<
					* The DB record corresponds to
					* listener_info::client_addr (NULL if
					* the client is not yet maintained).
					*/
  struct client_db *db; /**< The book-keeping DB (won't be NULL). */
};

/**
 * Process a ::scream_packet_register.
 * If the client is not already registered, a new record is created. If the new
 * record cannot be created because the DB is full, the
 * ::scream_packet_register will be ignored.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
register_client (struct listener_info *info);

/**
 * Process a scream_packet_type::scream_packet_reset.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
reset_client (struct listener_info *info);

/**
 * Send a ::scream_packet_register_ack to the destination.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
send_register_ack (struct listener_info *info);

/**
 * Send a ::scream_packet_reset_ack to the destination.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
send_reset_ack (struct listener_info *info);

/**
 * Process a ::scream_packet_server_client_sync.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
server_client_sync (struct listener_info *info);

/**
 * Send a ::scream_packet_server_client_resp to the destination.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
send_server_client_resp (struct listener_info *info);

/**
 * Process a ::scream_packet_server_client_resp_ack.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
server_client_resp_ack (struct listener_info *info);

/**
 * Send a ::scream_packet_server_client_data to the destination.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
send_server_client_data (struct listener_info *info);

/**
 * Process a ::scream_packet_client_server_sync.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
client_server_sync (struct listener_info *info);

/**
 * Send a ::scream_packet_client_server_resp to the destination.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
send_client_server_resp (struct listener_info *info);

/**
 * Process a ::scream_packet_client_server_data.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
client_server_data (struct listener_info *info);

/**
 * Send a ::scream_packet_client_server_ack to the destination.
 *
 * @param [in] info the listener's info.
 *
 * @return An error code.
 */
err_code
send_client_server_ack (struct listener_info *info);

/**
 * Get a vacant slot in the DB to track a client.
 *
 * @param [in] db the book-keeping data structure.
 *
 * @return The first empty slot in DB or NULL if the book-keeping DB is full.
 */
struct client_record *
get_empty_slot (const struct client_db *db);

/**
 * Get the book-keeping record of a client.
 *
 * @param [in] id the client ID.
 * @param [in] db the book-keeping data structure.
 *
 * @return The client book-keeping record or NULL if the client has none.
 */
struct client_record *
get_client_record_on_id (int32_t id,
			 const struct client_db *db);

/**
 * Get the book-keeping record of a client that has not been disassociated.
 *
 * @param [in] addr the client address.
 * @param [in] db the book-keeping data structure.
 *
 * @return The client book-keeping record or NULL if the client has none.
 */
struct client_record *
get_client_record (const struct sockaddr_in *addr,
		   const struct client_db *db);

/**
 * Populate field client_record::data and client_record::len of
 * listener_info::client_db_rec with the user's todo items.
 *
 * @param [in,out] info the fields to be populated as well as the information
 *                      needed to populate them.
 *
 * @return An error code.
 */
err_code
retrieve_todos (struct listener_info *info);

/**
 * Sync the user's todo items with data provided in field client_record::data of
 * listener_info::client_db_rec.
 *
 * @param [in] info the information needed to sync the user's todo.
 *
 * @return An error code.
 */
err_code
sync_todos (struct listener_info *info);

/**
 * Parse client-server data chunk whose type is ::CHUNK_NEW_TODO.
 *
 * @param [in] db the synchronization DB.
 * @param [in] insert_stmt the prepared statement used to insert a new todo.
 *                         This will be reset and cleared of the bindings
 *                         upon return.
 * @param [in] chunk_len the length of the ::CHUNK_NEW_TODO.
 * @param [in] chunk the chunk whose fields are to be parsed.
 *
 * @return An error code.
 */
err_code
sync_create_todo (sqlite3 *db, sqlite3_stmt *insert_stmt, uint16_t chunk_len,
		  void *chunk);

/**
 * Parse client-server data chunk whose type is ::CHUNK_DELETE_TODO.
 *
 * @param [in] db the synchronization DB.
 * @param [in] delete_stmt the prepared statement used to delete a todo.
 *                         This will be reset and cleared of the bindings
 *                         upon return.
 * @param [in] chunk_len the length of the ::CHUNK_DELETE_TODO.
 * @param [in] chunk the chunk whose fields are to be parsed.
 *
 * @return An error code.
 */
err_code
sync_delete_todo (sqlite3 *db, sqlite3_stmt *delete_stmt, uint16_t chunk_len,
		  void *chunk);

/**
 * Parse client-server data chunk whose type is ::CHUNK_UPDATE_TODO.
 *
 * @param [in] db the synchronization DB.
 * @param [in] update_stmt the prepared statement used to update a todo.
 *                         This will be reset and cleared of the bindings
 *                         upon return.
 * @param [in] chunk_len the length of the ::CHUNK_UPDATE_TODO.
 * @param [in] chunk the chunk whose fields are to be parsed.
 *
 * @return An error code.
 */
err_code
sync_update_todo (sqlite3 *db, sqlite3_stmt *update_stmt, uint16_t chunk_len,
		  void *chunk);

#ifdef __cplusplus
}
#endif

#endif /* LISTEN_H */
