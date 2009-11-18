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
 * @file scream-common.h                                                     
 * @brief Common functions for the scream protocol client and server.
 * @author Tobias Heer <heer@cs.rwth-aachen.de>
 ******************************************************************************/

#ifndef SCREAM_COMMON_H
#define SCREAM_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h> /* uint_X types */

/** Define boolean data type for C. */
typedef enum bool
  {
    FALSE = 0, /**< False. */
    TRUE, /**< True. */

  } bool;

/** Default port number. **/
#define SC_DEFAULT_PORT 50001

/** Maximum size for the packet buffer. **/
#define SC_MAX_BUFFER 2048

/** Number of retransmssions. */
#define SC_RETRANSMISSIONS 20

/** Timeout in seconds. */
#define SC_PACKET_TIMEO 2

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

  } err_code;

/** Screamer packet/message types. */
typedef enum
  {
    SC_PACKET_UNKNOWN = 0, /**< Packet number 0 is reserved. Do not use. */
    SC_PACKET_FLOOD = 1, /**< Flood packet. */
    SC_PACKET_MAX = 2, /**< Maximum packet type number. */

  } scream_packet_type;

/** A general packet. */
typedef struct
{
  uint8_t type; /**< Packet type indicator. **/
} __attribute__((__packed__)) scream_packet_general;

/** A flood packet. */
typedef struct
{
  uint8_t type; /**< Must be scream_packet_type::SC_PACKET_FLOOD. */
  uint16_t seq; /**< Sequence numbers. */
  uint8_t data[0]; /**< Flood data. */
} __attribute__((__packed__)) scream_packet_flood;

/* Common functions */

/**
 * Check if a piece of memory is a scream packet.
 *
 * @param [in] buffer piece of memory to be checked.
 * @param [in] len length of the buffer.
 *
 * @return #bool::TRUE if the packet is a ::scream_packet_general,
 *         #bool::FALSE otherwise.
 */
bool
is_scream_packet (const void *buffer, int len);

/**
 * Check if a piece of memory is of a certain scream packet.
 *
 * @param [in] packet a valid ::scream_packet_general. @see is_scream_packet
 * @param [in] len the length of the buffer.
 * @param [in] type the expected packet type. @see scream_packet_type
 *
 * @return #bool::TRUE if the packet is valid, #bool::FALSE otherwise.
 */
bool
check_scream_packet (const scream_packet_general *packet,
		     int len,
		     scream_packet_type type);

/**
 * Convert a type to a printable string.
 *
 * @param [in] type the packet type to be converted. @see scream_packet_type
 *
 * @return The string representing the given type.
 */
const char *
get_scream_type_name (scream_packet_type type);

#ifdef __cplusplus
}
#endif

#endif /* SCREAM_COMMON_H */
