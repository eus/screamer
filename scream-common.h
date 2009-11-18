/***************************************************************************
 *   Copyright (C) 2008 by Tobias Heer                                     *
 *   heer@cs.rwth-aachen.de                                                *
 *                                                                         *
 *   Permission is hereby granted, free of charge, to any person obtaining *
 *   a copy of this software and associated documentation files (the       *
 *   "Software"), to deal in the Software without restriction, including   *
 *   without limitation the rights to use, copy, modify, merge, publish,   *
 *   distribute, sublicense, and/or sell copies of the Software, and to    *
 *   permit persons to whom the Software is furnished to do so, subject to *
 *   the following conditions:                                             *
 *                                                                         *
 *   The above copyright notice and this permission notice shall be        *
 *   included in all copies or substantial portions of the Software.       *
 *                                                                         *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       *
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    *
 *   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*
 *   IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR     *
 *   OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, *
 *   ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR *
 *   OTHER DEALINGS IN THE SOFTWARE.                                       *
 ***************************************************************************
 * @file:   scream_common.h                                                *
 * @brief:  Common functions for the scream protocol client and server     *
 * @author: Tobias Heer <heer@cs.rwth-aachen.de>                           *
 **************************************************************************/
#ifndef SCREAM_COMMON_H
#define SCREAM_COMMON_H

#include <netinet/in.h>    /* uint_X types */

/* define booleans for C */
#define BOOL int
#define TRUE  1
#define FALSE 0

/** default port number **/
#define SC_DEFAULT_PORT 50001

/** maximum data packet buffer **/
#define SC_MAX_BUFFER   2048

/** number of retransmssions */
#define SC_RETRANSMISSIONS 20

/** timeout in seconds */
#define SC_PACKET_TIMEO 2

typedef enum {	SC_ERR_SUCCESS =   1, 	/* It worked! 		 */
				SC_ERR_NOMEM   =  -1,	/* Not enough memory */
				SC_ERR_SEND    =  -2,	/* Send failed		 */
				SC_ERR_SOCK	   =  -3,	/* Socket error */
				SC_ERR_INPUT   =  -4,	/* Wrong user input */
				SC_ERR_PACKET  =  -5,   /* Malformed packet (too small or wrong format) */
				SC_ERR_STATE   =  -6,   /* State error. Unexpected input */
				SC_ERR_DB_FULL =  -7,    /* Maximum number of clients reached */
				SC_ERR_COMM    =  -8    /* Communication error. host does not respond */

				} err_code;


typedef enum {	SC_PACKET_UNKNOWN =   0, /*packet number 0 is reserved. Do not use. */
		SC_PACKET_FLOOD     = 1,
		SC_PACKET_MAX      =  2 /* maximum packet type number */
		
		} scream_packet_type;


typedef struct {
	uint8_t  type;	/** @brief: packet type**/
} __attribute__((__packed__)) scream_packet_general;


/** @brief: packet  DATA**/
typedef struct {
	uint8_t  type;				/** @brief: packet type DATA**/
	uint16_t seq;				/** sequence numbers */
	uint8_t  data[0];			/** variable data field*/
} __attribute__((__packed__)) scream_packet_flood;


/* common functions */

/**
 * Check if a piece of memory is a scream packet
 *
 * @param	buffer			input byte array
 * @param	len				length of the buffer
 * @return					TRUE if the packet is a general scream packet, FALSE otherwise
 **/
BOOL scream_is_packet(const char* buffer, const int len);

/**
 * Check if a piece of memory is a scream packet
 *
 * @param	packet			a valid scream general packet @see: scream_is_packet
 * @param	len				length of the buffer
 * @param	len				the expected packet type @see: scream_packet_type
 * @return					TRUE if the packet is valid, FALSE otherwise
 **/
BOOL scream_check_packet(const scream_packet_general* packet, const int len, const scream_packet_type type);


/**
 * Return a string for a scream packet type
 *
 * @type	len				the expected packet type @see: scream_packet_type
 * @return					TRUE if the packet is valid, FALSE otherwise
 **/
const char* scream_packet_name(const scream_packet_type type);

#endif /* SCREAM_COMMON_H */
