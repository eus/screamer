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
 * @file scream.h                                                     
 * @brief UDP flooding program.
 * @author Tobias Heer <heer@cs.rwth-aachen.de>
 * @author Tadeus Prastowo <eus@member.fsf.org>
 *
 * This file provides the base functions for the UDP flooder screamer.
 ******************************************************************************/

#ifndef SCREAM_H
#define SCREAM_H

#include <netinet/in.h> /* sockets */
#include "scream-common.h" /* common headers and definitions */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The timeout for receiving ::scream_packet_ack after sending
 * ::scream_packet_register and for receiving ::scream_packet_result after
 * sending ::scream_packet_reset.
 */
#define TIMEOUT 1000000ULL

/**
 * Scream base data structure.
 * This structure holds the state information for a scream run.
 */
struct scream_base_data_s
{
  int sock; /**< the communication socket. */
  struct sockaddr_in dest_addr; /**< the listener address. */
  int num_packets; /**< the number of scream_packet_type::SC_PACKET_FLOOD
                    *   packets that has been sent. */
};

/**
 * Opaque type for scream_base_data_s.
 */
typedef struct scream_base_data_s scream_base_data;

/**
 * Initialize the memory of a ::scream_base_data and the internal program state.
 *
 * @param [out] state basic connection state information of a screamer.
 *
 * @return An error code.
 */
err_code
scream_init (scream_base_data *state);

/**
 * Create a UDP socket to initialize scream_base_data::sock and resolve the
 * host name to initialize scream_base_data::dest_addr.
 *
 * @param [out] state basic connection state information of a screamer.
 * @param [in] host_name the name of the destination host.
 * @param [in] port the destination port number.
 *
 * @return An error code.
 */
err_code
scream_set_dest (scream_base_data *state,
		 const char *host_name,
		 uint16_t port);

/**
 * Send a scream_packet_type::SC_PACKET_REGISTER packet to the destination.
 * This will block until the registration is successful as indicated by
 * receiving a scream_packet_type::SC_PACKET_ACK packet.
 * 
 * @param [in] state basic connection state information of a screamer.
 * @param [in] sleep_time the delay between sends in microsecond.
 * @param [in] iterations the number of scream_packet_type::SC_PACKET_FLOOD
 *                        packets to be sent.
 *
 * @return   An error code.
 */
err_code
scream_register (const scream_base_data *state,
		 unsigned long long sleep_time,
		 int iterations);

/**
 * Loop, generate FLOOD data and send FLOOD data to destination.
 *
 * @param [in] state basic connection state information of a screamer.
 * @param [in] sleep_time delay between loops (in microseconds).
 * @param [in] flood_size size of the FLOOD data to generate (in bytes).
 * @param [in] iterations the number of loops (0 means infinite loops).
 * @param [in] test_mode activate an intentional packet drop and reordering to
 *                       test the listener.
 *
 * @return An error code.
 */
err_code
scream_pause_loop (scream_base_data *state,
		   int sleep_time,
		   size_t flood_size,
		   int iterations,
		   bool test_mode);

/**
 * Send a UDP packet to the destination specified in
 * scream_base_data::dest_addr.
 *
 * @param [in] state basic connection state information of a screamer.
 * @param [in] buffer data to be sent.
 * @param [in] buffer_size the size of the buffer in bytes.
 *
 * @return An error code.
 *
 */
err_code
scream_send (const scream_base_data *state,
	     const void *buffer,
	     size_t buffer_size);

/**
 * Receive a UDP packet from the source specified in
 * scream_base_data::dest_addr through scream_base_data::sock.
 *
 * @param [in] state basic connection state information of a screamer.
 * @param [out] buffer the buffer to receive data.
 * @param [in] buffer_size the size of the buffer in bytes.
 *
 * @return err_code::SC_ERR_COMM if no packet is received until timeout,
 *         err_code::SC_ERR_WRONGSENDER if a packet is not received from the
 *         specified source, err_code::SC_ERR_SUCCESS if a packet is received
 *         from the specified source before timeout, or else
 *         err_code::SC_ERR_RECV.
 */
err_code
scream_recv (const scream_base_data *state,
	     void *buffer,
	     size_t buffer_size);

/**
 * Keep resending a particular message to the destination specified in
 * scream_base_data::dest_addr through scream_base_data::sock every time
 * a timeout happens until a particular expected message is received from
 * the destination.
 *
 * @param [in] send_what the data sent to the destination.
 * @param [in] send_what_len the length of the data sent to the destination.
 * @param [in,out] wait_for_what the expected scream packet type must be
 *                               specified in scream_packet_general::type and
 *                               the received packet will be written to this
 *                               buffer.
 * @param [in] wait_for_what_len the actual length of the result buffer.
 * @param [in] wait_msg the message that should be printed everytime the
 *                      particular message is sent to the destination.
 * @param [in] state basic connection state information of a screamer.
 * @param [in] timeout the timeout after which the particular message is resent.
 *
 * @return An error code.
 */
err_code
scream_send_and_wait_for (const void *send_what,
			  size_t send_what_len,
			  scream_packet_general *wait_for_what,
			  size_t wait_for_what_len,
			  const char *wait_msg,
			  const scream_base_data *state,
			  const struct timeval *timeout);
	

/**
 * Send a scream_packet_type::SC_PACKET_RESET packet to the destination.
 * This will block until a reset is successful as indicated by
 * receiving a scream_packet_type::SC_PACKET_RESULT.
 * 
 * @param [in] state basic connection state information of a screamer.
 * @param [out] result the result sent back by the server.
 *
 * @return An error code.
 */
err_code
scream_reset (const scream_base_data *state,
	      scream_packet_result *result);

/**
 * Close socket in state.
 *
 * @param [in] state basic connection state information of a screamer.
 *
 * @return An error code.
 */
err_code
scream_close (scream_base_data *state);

/**
 * Print the statistics of screaming.
 *
 * @param [in] result the result received from the server.
 */
void
print_result (const scream_packet_result *result);

#ifdef __cplusplus
}
#endif

#endif /* SCREAM_H */
