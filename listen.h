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
#define CLIENT_MAX_NUM 16

/**
 * The time in second after sending a ::scream_packet_result at which the client
 * should be disassociated.
 */
#define TIME_TO_DEATH 100

/** The record of the client book-keeping structure. */
struct client_record
{
  time_t died_at; /**< Unix epoch time at which the client dies. */
  uint32_t id; /**< The means to identify a client for an address update. */
  struct sockaddr_in client_addr; /**< The primary client address. */
  unsigned long long sleep_time; /**< The sleep time in microsecond. */
  unsigned amount; /**< Number of FLOOD packets to be received. */
  int max_gap; /**< Maximum length of a gap. */
  int num_of_gaps; /**< Number of gaps. */
  bool is_out_of_order; /**< 1 or more FLOOD packet is out of order. */
  int recvd_packets; /**< Number of FLOOD packets received. */
  struct
  {
    bool is_set; /**< Not yet set. */
    unsigned long long delta; /**< Time diff in microsecond. */
  } min_latency; /**< Minimum time diff between prev & curr FLOOD. */
  struct
  {
    bool is_set; /**< Not yet set. */
    unsigned long long delta; /**< Time diff in microsecond. */
  } max_latency; /**< Maximum time diff between prev & curr FLOOD. */
  unsigned long long total_latency; /**<
                                     * The sum of all time diffs between two
                                     * FLOOD.
                                     */
  struct
  {
    unsigned long long ts; /**< Timestamp of previous FLOOD packet. */
    uint16_t seq; /**< Sequence of previous FLOOD packet. */
  } prev_packet; /**< The previous FLOOD packet. */
}; 

/** An indication that a client record is still in use. */
#define DIE_AT_ANOTHER_TIME (-1)

/** The client book-keeping structure. */
struct client_db
{
  size_t len; /**< The number of records. */
  struct client_record *recs; /**< The client records. */
};

/**
 * Handle a scream packet according to the state machine.
 *
 * @param [in] client_addr the address of the client who sent the packet.
 * @param [in] packet a valid ::scream_packet_general.
 * @param [in] len the length of the packet.
 * @param [in] sock the UDP socket on which the packet was received.
 * @param [in] db the client book-keeping data structure.
 *
 * @return An error code.
 */
err_code
listener_handle_packet (const struct sockaddr_in *client_addr,
			const scream_packet_general *packet,
			size_t len,
			int sock,
			struct client_db *db);

/**
 * Process a ::scream_packet_register.
 * If the client is not already registered, a new record is created. If the new
 * record cannot be created because the DB is full, the
 * ::scream_packet_register will be ignored. If the new record can be
 * created, the sleep time and the number of ::scream_packet_flood to be
 * sent are recorded. If the client is already registered, an
 * ::scream_packet_ack will be sent.
 *
 * @param [in] client_addr the address of the client.
 * @param [in] packet the ::scream_packet_register containing the
 *                    client's sleep time, the number of
 *                    ::scream_packet_flood to be sent and the client's ID.
 * @param [in] db the book-keeping structure to track the client.
 *
 * @return An error code.
 */
err_code
register_client (const struct sockaddr_in *client_addr,
		 const scream_packet_register *packet,
		 struct client_db *db);

/**
 * Update a client address.
 *
 * @param [in] client_addr the address of the client.
 * @param [in] packet the ::scream_packet_update_address containing the
 *                    client's ID, the new IPv4 address and the new port.
 * @param [in] db the book-keeping structure to track the client.
 *
 * @return An error code.
 */
err_code
update_client_address (const struct sockaddr_in *client_addr,
		       const scream_packet_update_address *packet,
		       struct client_db *db);

/**
 * Process a ::scream_packet_flood. If this is the first
 * ::scream_packet_flood from the client, the timestamp and the sequence
 * number are simply recorded in the DB. If this is the subsequent
 * ::scream_packet_flood, the delta in the timestamp of the previous and
 * current ::scream_packet_flood are recorded in the form of maximum and
 * minimum latencies. The current ::scream_packet_flood's timestamp is
 * recorded as the previous timestamp. The current ::scream_packet_flood's
 * sequence number is only recorded if it is greater than the last
 * recorded sequence number. The out-of-order flag is set if the current
 * ::scream_packet_flood's sequence number is less than the last
 * recorded sequence number. Upon recording the sequence number of the current
 * ::scream_packet_flood, the delta of the current and last recorded
 * sequence number is calculated. If the delta is greater than 1, the total
 * number as well as the maximum width of the gap that has been so far is
 * updated.
 *
 * @param [in] client_addr the client address.
 * @param [in] packet the current ::scream_packet_flood.
 * @param [in] ts the timestamp of the current ::scream_packet_flood.
 * @param [in] db the book-keeping data structure.
 *
 * @return An error code.
 */
err_code
record_packet (const struct sockaddr_in *client_addr,
	       const scream_packet_flood *packet,
	       unsigned long long ts,
	       struct client_db *db);

/**
 * Process a scream_packet_type::scream_packet_reset.
 * After determining whether or not there are some missing/out-of-order packets
 * at the end, the flooding data of the client is sent to the client and the
 * corresponding book-keeping entry is marked to be disassociated within
 * #TIME_TO_DEATH.
 *
 * @param [in] sock the socket through which the ::scream_packet_result
 *                  is sent.
 * @param [in] client_addr the address of the client.
 * @param [in] db the book-keeping structure to track the client.
 *
 * @return An error code.
 */
err_code
reset_client (int sock,
	      const struct sockaddr_in *client_addr,
	      struct client_db *db);

/**
 * Send ::scream_packet_result to the client who sent
 * ::scream_packet_reset. The ::scream_packet_result contains the
 * number of received ::scream_packet_flood, the maximum width of the
 * gap, the number of gaps, out-of-order flag, minimum latency, maximum
 * latency, and average latency.
 *
 * @param [in] sock the socket through which the ::scream_packet_result
 *                  is sent.
 * @param [in] client_addr the client address.
 * @param [in] db the book-keeping data structure.
 *
 * @return An error code.
 */
err_code
send_result (int sock,
	     const struct sockaddr_in *client_addr,
	     const struct client_db *db);

/**
 * Send a ::scream_packet_return_routability_ack to the destination.
 *
 * @param [in] sock the socket through which the packet is to be sent.
 * @param [in] dest the destination of the packet.
 *
 * @return An error code.
 */
err_code
send_return_routability_ack (int sock, const struct sockaddr_in *dest);

/**
 * Send a ::scream_packet_update_address_ack to the destination.
 *
 * @param [in] sock the socket through which the packet is to be sent.
 * @param [in] dest the destination of the packet.
 *
 * @return An error code.
 */
err_code
send_update_address_ack (int sock, const struct sockaddr_in *dest);

/**
 * Set an absolute time after which the client slot in the book-keeping data
 * structure can be reused.
 *
 * @param [in] client_addr the client address.
 * @param [in] db the book-keeping data structure.
 *
 * @return An error code.
 */
err_code
mark_client_for_unregistering (const struct sockaddr_in *client_addr,
			       struct client_db *db);

/**
 * Disassociate a client upon receiving a ::scream_packet_ack from the client.
 *
 * @param [in] client_addr the client address.
 * @param [in] db the book-keeping data structure.
 *
 * @return An error code.
 */
err_code
unregister_client (const struct sockaddr_in *client_addr,
		   struct client_db *db);

/**
 * Get a vacant slot in the DB to track a client.
 *
 * @param [in] client_addr the client address.
 * @param [in] db the book-keeping data structure.
 *
 * @return The first empty slot in DB or NULL if the book-keeping DB is full.
 */
struct client_record *
get_empty_slot (const struct sockaddr_in *client_addr,
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

#ifdef __cplusplus
}
#endif

#endif /* LISTEN_H */
