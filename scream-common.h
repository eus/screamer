/******************************************************************************
 * Copyright (C) 2008  Tobias Heer <heer@cs.rwth-aachen.de>                   *
 * Copyright (C) 2009, 2010  Tadeus Prastowo (eus@member.fsf.org)             *
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
 * @file scream-common.h                                                     
 * @brief Common functions for the AndroidTodo synchronization server.
 * @author Tobias Heer <heer@cs.rwth-aachen.de>
 * @author Tadeus Prastowo <eus@member.fsf.org>
 ******************************************************************************/

#ifndef SCREAM_COMMON_H
#define SCREAM_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h> /* uint_X types */
#include <netinet/in.h>
#include <sqlite3.h>

/** Define boolean data type for C. */
typedef enum bool
  {
    FALSE = 0, /**< False. */
    TRUE, /**< True. */

  } bool;

/** Default port number. **/
#define SC_DEFAULT_PORT 50001

/** Timeout in seconds. */
#define SC_PACKET_TIMEO 2

/** Extract the second component of a time in microsecond. */
#define SEC_PART(t) (t / 1000000)

/** Extract the microsecond component of a time in microsecond. */
#define USEC_PART(t) (t % 1000000)

/** Form a time in microsecond given the second and the microsecond parts. */
#define COMBINE_SEC_USEC(sec, usec) ((sec * 1000000ULL) + usec)

/** Error code. */
typedef enum
  {
    SC_ERR_SUCCESS = 1, /**< It worked! */
    SC_ERR_NOMEM = -1, /**< Not enough memory. */
    SC_ERR_SEND = -2, /**< Send failed. */
    SC_ERR_SOCK = -3, /**< Socket error. */
    SC_ERR_INPUT = -4, /**< Wrong user input. */
    SC_ERR_PACKET = -5, /**< Malformed packet (too small or wrong format). */
    SC_ERR_STATE = -6, /**< State error. Unexpected input. */
    SC_ERR_DB_FULL = -7, /**< Maximum number of clients reached. */
    SC_ERR_COMM = -8, /**< Communication error. Host does not respond. */
    SC_ERR_RECV = -9, /**< Receive failed. */
    SC_ERR_SOCKOPT = -10, /**< Setting socket option failed. */
    SC_ERR_WRONGSENDER = -11, /**< Packet received from an unexpected host. */
    SC_ERR_LOCK = -12, /**< Screamer cannot lock the main channel. */
    SC_ERR_UNLOCK = -13, /**< Screamer cannot unlock the main channel. */
    SC_ERR_SIGNAL = -14, /**< Manager cannot signal the screamer. */
    SC_ERR_NAME = -15, /**< Socket has a problematic name. */
    SC_ERR_DB = -16, /**< DB error. */

  } err_code;

/**
 * AndroidTodo packet/message types.
 * WARNING: This must be kept in sync with AndroidTodo's TodoSyncCommunication
 * class.
 */
typedef enum
  {
    SC_PACKET_UNKNOWN = 0, /**< Packet number 0 is reserved. Do not use. */
    SC_PACKET_REGISTER, /**< Register packet. */
    SC_PACKET_REGISTER_ACK, /**< Register acknowledgement packet. */
    SC_PACKET_SERVER_CLIENT_SYNC, /**<
				   * Server-to-client synchronization
				   * request.
				   */
    SC_PACKET_SERVER_CLIENT_RESP, /**<
				   * Server-to-client synchronization
				   * response.
				   */
    SC_PACKET_SERVER_CLIENT_RESP_ACK, /**<
				       * Start server-to-client
				       * synchronization.
				       */
    SC_PACKET_CLIENT_SERVER_SYNC, /**<
				   * Client-to-server synchronization
				   * request.
				   */
    SC_PACKET_CLIENT_SERVER_RESP, /**< Start client-to-server synchronization. */
    SC_PACKET_CLIENT_SERVER_ACK, /**<
				  * Client-to-server synchronization
				  * completes.
				  */
    SC_PACKET_RESET, /**< Reset packet. */
    SC_PACKET_RESET_ACK, /**< Result acknowledgement packet. */
    SC_PACKET_SERVER_CLIENT_DATA, /**< Todo items from server. */
    SC_PACKET_CLIENT_SERVER_DATA, /**< Todo items from client. */
    SC_PACKET_MAX, /**< Maximum packet type number. */

  } scream_packet_type;

/** AndroidTodo chunk types. */
typedef enum
  {
    CHUNK_UNKNOWN = 0, /**< Chunk number 0 is reserved. Do not use. */
    CHUNK_NEW_TODO, /**< A new todo item to be stored. */
    CHUNK_UPDATE_TODO, /**< An update for a todo item. */
    CHUNK_DELETE_TODO, /**< Delete a todo item. */
    CHUNK_TODO, /**< A todo item as stored in the sync server. */
    CHUNK_TODO_ID, /**< The ID of a todo. */
    CHUNK_TODO_TITLE, /**< The title of a todo. */
    CHUNK_TODO_DEADLINE, /**< The deadline of a todo. */
    CHUNK_TODO_PRIORITY, /**< The priority of a todo. */
    CHUNK_TODO_STATUS, /**< The status of a todo. */
    CHUNK_TODO_DESCRIPTION, /**< The description of a todo. */
    CHUNK_TODO_REVISION, /**< The revision of a todo. */
    CHUNK_MAX, /**< Maximum chunk type number. */
  } chunk_type;

/** A chunk of datum. */
typedef struct
{
  uint8_t type; /**< The type of datum. */
  uint16_t len; /**< The length of datum. */
  char datum[0]; /**< The datum. */  
} __attribute__((__packed__)) scream_packet_chunk;

/** A general packet. */
typedef struct
{
  uint8_t type; /**< Packet type indicator. */
} __attribute__((__packed__)) scream_packet_general;

/**
 * A register acknowledgement packet.
 * The value of scream_packet_register_ack::type must be
 * scream_packet_type::SC_PACKET_REGISTER_ACK.
 */
typedef scream_packet_general scream_packet_register_ack;

/**
 * A reset packet.
 * The value of scream_packet_reset::type must be
 * scream_packet_type::SC_PACKET_RESET.
 */
typedef scream_packet_general scream_packet_reset;

/**
 * A reset acknowledgement packet.
 * The value of scream_packet_reset_ack::type must be
 * scream_packet_type::SC_PACKET_RESET_ACK.
 */
typedef scream_packet_general scream_packet_reset_ack;

/**
 * A server-to-client synchronization request packet.
 * The value of scream_packet_server_client_sync::type must be
 * scream_packet_type::SC_PACKET_SERVER_CLIENT_SYNC.
 */
typedef scream_packet_general scream_packet_server_client_sync;

/**
 * A server-to-client synchronization response packet.
 * The value of scream_packet_server_client_resp::type must be
 * scream_packet_type::SC_PACKET_SERVER_CLIENT_RESP.
 */
typedef struct
{
  uint8_t type; /**< Must be scream_packet_type::SC_PACKET_SERVER_CLIENT_RESP. */
  uint32_t len; /**< The length of items that follows. */
} __attribute__((__packed__)) scream_packet_server_client_resp;

/**
 * Start server-to-client synchronization packet.
 * The value of scream_packet_server_client_resp_ack::type must be
 * scream_packet_type::SC_PACKET_SERVER_CLIENT_RESP_ACK.
 */
typedef scream_packet_general scream_packet_server_client_resp_ack;

/**
 * A server-to-client synchronization data packet.
 * The value of scream_packet_server_client_data::type must be
 * scream_packet_type::SC_PACKET_SERVER_CLIENT_DATA.
 */
typedef struct
{
  uint8_t type; /**< Must be scream_packet_type::SC_PACKET_SERVER_CLIENT_DATA. */
  scream_packet_chunk data[0]; /**< The data that follows. */
} __attribute__((__packed__)) scream_packet_server_client_data;

/**
 * A client-to-server synchronization request packet.
 * The value of scream_packet_client_server_sync::type must be
 * scream_packet_type::SC_PACKET_CLIENT_SERVER_SYNC.
 */
typedef struct
{
  uint8_t type; /**< Must be scream_packet_type::SC_PACKET_CLIENT_SERVER_SYNC. */
  uint32_t len; /**< The length of items that follows. */
} __attribute__((__packed__)) scream_packet_client_server_sync;

/**
 * A client-to-server synchronization data packet.
 * The value of scream_packet_client_server_data::type must be
 * scream_packet_type::SC_PACKET_CLIENT_SERVER_DATA.
 */
typedef struct
{
  uint8_t type; /**< Must be scream_packet_type::SC_PACKET_CLIENT_SERVER_DATA. */
  scream_packet_chunk data[0]; /**< The data that follows. */
} __attribute__((__packed__)) scream_packet_client_server_data;

/**
 * Prevent the client to resend its sync data by acknowledging the received
 * data. The value of scream_packet_client_server_ack::type must be
 * scream_packet_type::SC_PACKET_CLIENT_SERVER_ACK.
 */
typedef scream_packet_general scream_packet_client_server_ack;

/**
 * Start client-to-server synchronization response packet.
 * The value of scream_packet_client_server_resp::type must be
 * scream_packet_type::SC_PACKET_CLIENT_SERVER_RESP.
 */
typedef scream_packet_general scream_packet_client_server_resp;

/** A register packet. */
typedef struct
{
  uint8_t type; /**< Must be scream_packet_type::SC_PACKET_REGISTER. */
  uint32_t id; /**< The client ID. */
} __attribute__((__packed__)) scream_packet_register;

/* Common functions */

/**
 * Check if a piece of memory is a sane packet.
 *
 * @param [in] buffer piece of memory to be checked.
 * @param [in] len length of the buffer.
 * @param [in] expected_len the expected length of the buffer.
 *
 * @return #bool::TRUE if the packet has a recognized type and of size
 *         expected_len, #bool::FALSE otherwise.
 */
bool
is_sane (const void *buffer, size_t len, size_t expected_len);

/**
 * Convert a type to a printable string.
 *
 * @param [in] type the packet type to be converted. @see scream_packet_type
 *
 * @return The string representing the given type.
 */
const char *
get_scream_type_name (scream_packet_type type);

/**
 * Remove a UDP packet in the front of the buffer of sock.
 *
 * @param [in] sock the socket whose first UDP datagram in its buffer will be removed.
 */
void
remove_packet(int sock);

/**
 * Return the timestamp in microsecond of the last received packet.
 * The timestamp is measured since Unix epoch.
 *
 * @param [in] sockfd the socket that has the last-received packet.
 *
 * @return The timestamp in microsecond, or zero if there is an error.
 */
unsigned long long
get_last_packet_ts (int sockfd);

/**
 * Convert a string into an integer of type unsigned long.
 *
 * @param [in] str the string to be converted.
 * @param [out] has_error is set to zero if there is no conversion failure,
 *                        to one if there is a failure.
 * @param [in] what_is_str the context to be printed in the error message.
 *
 * @return The integer represented by the string.
 */
unsigned long
eus_strtoul (const char *str, int *has_error, const char *what_is_str);

/**
 * Convert a string into an integer of type unsigned long long.
 *
 * @param [in] str the string to be converted.
 * @param [out] has_error is set to zero if there is no conversion failure,
 *                        to one if there is a failure.
 * @param [in] what_is_str the context to be printed in the error message.
 *
 * @return The integer represented by the string.
 */
unsigned long long
eus_strtoull (const char *str, int *has_error, const char *what_is_str);

/**
 * Convert a string into an integer of type long.
 *
 * @param [in] str the string to be converted.
 * @param [out] has_error is set to zero if there is no conversion failure,
 *                        to one if there is a failure.
 * @param [in] what_is_str the context to be printed in the error message.
 *
 * @return The integer represented by the string.
 */
long
eus_strtol (const char *str, int *has_error, const char *what_is_str);

/**
 * Convert a string into an integer of type long long.
 *
 * @param [in] str the string to be converted.
 * @param [out] has_error is set to zero if there is no conversion failure,
 *                        to one if there is a failure.
 * @param [in] what_is_str the context to be printed in the error message.
 *
 * @return The integer represented by the string.
 */
long long
eus_strtoll (const char *str, int *has_error, const char *what_is_str);

/**
 * Set a receiving timeout on a socket.
 *
 * @param [in] socket the socket whose receiving timeout is to be set.
 * @param [in] timeout the timeout for receiving a packet from the socket.
 *
 * @return An error code.
 */
err_code
set_timeout (int socket, const struct timeval *timeout);

/**
 * Unset a receiving timeout on a socket.
 *
 * @param [in] socket the socket whose receiving timeout is to be unset.
 *
 * @return An error code.
 */
err_code
unset_timeout (int socket);

/**
 * Produces a nice error message identical to perror().
 *
 * @param [in] db the database whose latest error is to be produced.
 * @param [in] message the message to be printed before the error message.
 */
void
sqlite3_perror (sqlite3 *db, const char *message);

/**
 * Frees a pointer that is produced by either sqlite3_malloc() or
 * sqlite3_realloc() and NULLified the pointer so that it is safe for reuse.
 *
 * @param [in] ptr_addr the address of the pointer to be freed
 */
void
sqlite3_clean_free (void *ptr_addr);

#ifdef __cplusplus
}
#endif

#endif /* SCREAM_COMMON_H */
