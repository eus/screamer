/******************************************************************************
 * Copyright (C) 2008 by Tobias Heer <heer@cs.rwth-aachen.de>                 *
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
 * Close socket in state.
 *
 * @param [in] state basic connection state information of a screamer.
 *
 * @return An error code.
 */
err_code
scream_close (scream_base_data *state);

#ifdef __cplusplus
}
#endif

#endif /* SCREAM_H */
