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
 * @file:   scream_common.c                                                *
 * @brief:  Common functions for the scream protocol client and server     *
 * @author: Tobias Heer <heer@cs.rwth-aachen.de>                           *
 **************************************************************************/

#include <stdio.h> 		/* printf(...) */
#include <assert.h>		/* assert(...) */
#include "scream-common.h"

BOOL scream_is_packet(const char* buffer, const int len){

	assert(buffer != NULL);

	/* check packet for consistency */
	if( len < sizeof(scream_packet_general)){
		printf("Packet too small.");
		return FALSE;
	}
	if(((scream_packet_general *) buffer)->type > 0  && ((scream_packet_general *) buffer)->type < SC_PACKET_MAX){
		return TRUE;
	}else{
		printf("Packet type out of bounds\n");
		return FALSE;
	}
}

BOOL scream_check_packet(const scream_packet_general* packet, const int len, const scream_packet_type type){

	err_code err = SC_ERR_SUCCESS;

	/* check the type */
	if(type != packet->type){
		printf("Packet type mismatch. Expected: %d  Given: %d",type, packet->type);
		return FALSE;
	}

	/* check the size of the packet */
	switch(packet->type){
		case SC_PACKET_FLOOD:
			if( len < sizeof(scream_packet_flood)){
				err =  SC_ERR_PACKET;
			}
			break;
		default:
			err = SC_ERR_PACKET;
			printf("Unknown packet type %d \n", packet->type);
	}

	/* handle errors */
	if(err != SC_ERR_SUCCESS){
		printf("Check packet: Malformed packet\n");
		return FALSE;
	}else{
		return TRUE;
	}
}


const char* scream_packet_name(const scream_packet_type type){
	switch(type){
		case SC_PACKET_FLOOD:    return "FLOOD";
		default:
			return "UNKNOWN";
	}
}
