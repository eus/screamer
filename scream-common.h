/******************************************************************************
 * Copyright (C) 2008  Tobias Heer <heer@cs.rwth-aachen.de>                   *
 * Copyright (C) 2009  Tadeus Prastowo (eus@member.fsf.org)                   *
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
 * @author Tadeus Prastowo <eus@member.fsf.org>
 ******************************************************************************/

#ifndef SCREAM_COMMON_H
#define SCREAM_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h> /* uint_X types */
#include <netinet/in.h>

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

  } err_code;

/** Screamer packet/message types. */
typedef enum
  {
    SC_PACKET_UNKNOWN = 0, /**< Packet number 0 is reserved. Do not use. */
    SC_PACKET_REGISTER, /**< Register packet. */
    SC_PACKET_FLOOD, /**< Flood packet. */
    SC_PACKET_RESET, /**< Reset packet. */
    SC_PACKET_RESULT, /**< Result packet. */
    SC_PACKET_ACK, /**< Acknowledgement packet. */
    SC_PACKET_RETURN_ROUTABILITY, /**< Return-routability check packet. */
    SC_PACKET_RETURN_ROUTABILITY_ACK, /**<
				       * The acknowledgment packet for a return-
				       * routability check.
				       */
    SC_PACKET_UPDATE_ADDRESS, /**<
			       * A screamer updates its address for the
			       * subsequent communication
			       */
    SC_PACKET_UPDATE_ADDRESS_ACK, /**<
				   * The acknowledgment packet for an
				   * address update.
				   */
    SC_PACKET_MAX, /**< Maximum packet type number. */

  } scream_packet_type;

/** A general packet. */
typedef struct
{
  uint8_t type; /**< Packet type indicator. */
} __attribute__((__packed__)) scream_packet_general;

/**
 * A return routability packet.
 * The value of scream_packet_return_routability::type must be
 * scream_packet_type::SC_PACKET_RETURN_ROUTABILITY.
 */
typedef scream_packet_general scream_packet_return_routability;

/**
 * A return routability acknowledgment packet.
 * The value of scream_packet_return_routability_ack::type must be
 * scream_packet_type::SC_PACKET_RETURN_ROUTABILITY_ACK.
 */
typedef scream_packet_general scream_packet_return_routability_ack;

/** An update address packet. */
typedef struct
{
  uint8_t type; /**< Must be scream_packet_type::SC_PACKET_UPDATE_ADDRESS. */
  uint32_t id; /**< The client ID. */
  uint32_t sin_addr; /**< The new IPv4 address. */
  uint16_t sin_port; /**< The new port. */
} __attribute__((__packed__)) scream_packet_update_address;

/**
 * An update address acknowledgment packet.
 * The value of scream_packet_update_address_ack::type must be
 * scream_packet_type::SC_PACKET_UPDATE_ADDRESS_ACK.
 */
typedef scream_packet_general scream_packet_update_address_ack;

/**
 * An acknowledgement packet.
 * The value of scream_packet_ack::type must be
 * scream_packet_type::SC_PACKET_FLOOD.
 */
typedef scream_packet_general scream_packet_ack;

/**
 * A reset packet.
 * The value of scream_packet_ack::type must be
 * scream_packet_type::SC_PACKET_RESET.
 */
typedef scream_packet_general scream_packet_reset;

/** A flood packet. */
typedef struct
{
  uint8_t type; /**< Must be scream_packet_type::SC_PACKET_FLOOD. */
  uint16_t seq; /**< Sequence numbers. */
  uint8_t data[0]; /**< Flood data. */
} __attribute__((__packed__)) scream_packet_flood;

/** A register packet. */
typedef struct
{
  uint8_t type;        /**< Must be scream_packet_type::SC_PACKET_REGISTER. */
  struct
  {
    uint32_t sec; /**< The second part of the delay. */
    uint32_t usec; /**< The microsecond part of the delay. */
  } sleep_time; /**< Delay between FLOOD sends in microsecond. */
  uint32_t amount;     /**< The number of FLOOD packets to be sent. */
  uint32_t id; /**< The client ID. */
} __attribute__((__packed__)) scream_packet_register;

/** A result packet. */
typedef struct
{
  uint8_t type; /**< Must be scream_packet_type::SC_PACKET_RESULT. */
  uint32_t recvd_packets; /**< The number of received FLOOD packets. */
  uint32_t max_gap; /**< The number of lost packets in the widest gap. */
  uint32_t num_of_gaps; /**< The number of gaps. */
  uint8_t is_out_of_order; /**< There is an out-of-order delivery. */
  struct
  {
    uint32_t sec; /**< The second part of the latency. */
    uint32_t usec; /**< The microsecond part of the latency. */
  } max_latency;    /**< The maximum delay between two sends. */
  struct
  {
    uint32_t sec; /**< The second part of the latency. */
    uint32_t usec; /**< The microsecond part of the latency. */
  } min_latency; /**< The minimum delay between two sends. */
  struct
  {
    uint32_t sec; /**< The second part of the latency. */
    uint32_t usec; /**< The microsecond part of the latency. */
  } avg_latency; /**< The average delay between sends. */
} __attribute__((__packed__)) scream_packet_result;

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
is_scream_packet (const void *buffer, size_t len);

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
 * Send a ::scream_packet_ack to the destination.
 *
 * @param [in] sock the socket through which the packet is to be sent.
 * @param [in] dest the destination of the packet.
 *
 * @return An error code.
 */
err_code
send_ack (int sock, const struct sockaddr_in *dest);

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

#ifdef __cplusplus
}
#endif

#endif /* SCREAM_COMMON_H */
