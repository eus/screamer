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
 * @file:   listen.c                                                       *
 * @brief:  UDP logging program functions                                  *
 * @author: Tobias Heer <heer@cs.rwth-aachen.de>                           *
 *                                                                         *
 * This file provides the base functions for the UDP listener              *
 *                                                                         *
 **************************************************************************/

#include "listen.h"
#include <stdio.h> 		/* printf(...) */
#include <stdlib.h>		/* malloc(...)*/
#include <string.h>		/* memcpy(...), bzero(...) */
#include <arpa/inet.h>		/* inet_ntoa(...) */
#include <assert.h>	    	/* assertions */

err_code listener_handle_packet(const struct sockaddr_in* client_addr, const scream_packet_general* packet, const size_t len, const int sock){
	// you need different error codes in the next assignment
	int err = SC_ERR_SUCCESS;

	assert(client_addr != NULL);
	assert(packet      != NULL);

	printf("Received %s packet from %s:%u\n", scream_packet_name(packet->type), inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
	 return err;
}

