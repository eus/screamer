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
 * @file:   scream.c                                                       *
 * @brief:  UDP flooding program                                           *
 * @author: Tobias Heer <heer@cs.rwth-aachen.de>                           *
 *                                                                         *
 *  This file provides the base functions for the UDP                      *
 *  flooder screamer.                                                      *
 **************************************************************************/

#include <stdio.h> 		/* printf(...) */
#include <stdlib.h>		/* malloc(...)*/
#include <string.h>		/* memcpy(...), bzero(...) */
#include <unistd.h>		/* usleep(...) */
#include <netdb.h>		/* gethostbyname(...) */
#include <arpa/inet.h>	/* inet_ntoa(...) */
#include <time.h>		/* time(...) */
#include <assert.h>		/* assert(...) */

#include "scream.h"
#include "scream-common.h" /* common headers and definitions */

err_code scream_init(scream_base_data* state){

	printf("Initializing basic data structure\n");

	/* clear struct contents (set contents to 0))*/
	bzero((char*) state, (int) sizeof( scream_base_data ));

	/* init random number generator */
	srand((unsigned int) time(NULL) );

	return SC_ERR_SUCCESS;
}

err_code scream_set_dest(	scream_base_data* state,
							const char* host_name,
							const uint16_t port){


	struct hostent *hp = NULL;
	struct sockaddr_in client_addr;

	assert(state!= NULL);
	assert(port>0);
	assert(host_name != NULL);

	if((state->sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("Err: couldn't create socket");
		return SC_ERR_SOCK;
	}
			//sockaddr_in serverAddr;

	/* resolve host name to IP address */
	hp = gethostbyname(host_name);

	if(hp!= NULL){
		/* fill sockaddr struct in state struct */
		state->dest_addr.sin_family       = AF_INET;
		state->dest_addr.sin_port         = htons(port);
		memcpy((void*) &state->dest_addr.sin_addr, (void*) hp->h_addr, hp->h_length);


		/* bind to interface */
		/* bind to port */
		bzero((char*) &client_addr,sizeof(client_addr));
		client_addr.sin_family       = AF_INET;
		client_addr.sin_port         = htons(7100);
		client_addr.sin_addr.s_addr  = inet_addr("137.226.59.199");

		if(bind(state->sock,(struct sockaddr *) &client_addr,sizeof(client_addr)) == 0){
			return SC_ERR_SOCK;
		};
		perror("bind");

		printf("Send to:%s, %d\n", inet_ntoa(state->dest_addr.sin_addr), port);


		return SC_ERR_SUCCESS;
	}

	return SC_ERR_SOCK;
}

err_code scream_pause_loop(	scream_base_data* state,
							const int sleep_time,
							const int buffer_size,
							const int iterations,
							const BOOL test_mode){

	err_code err = SC_ERR_SUCCESS;
	int i, j;
	scream_packet_flood* packet = NULL;
	/* test mode modifiers */
	BOOL last_packet_reordered = FALSE;
	char buffer[buffer_size];

	assert(state != NULL);

	/* set buffer memory to 0*/
	bzero(buffer, sizeof(buffer));

	packet = (scream_packet_flood*) buffer;
	packet->type = SC_PACKET_FLOOD;

	/* loop for some iterations or loop infinitely (depending on iterations */
	for(i = 0; (iterations == 0 || i < iterations) && err == SC_ERR_SUCCESS; i++){

		if(test_mode == TRUE && (rand() % 4 == 1)){
			/*test mode: drop packet with 1/4 chance*/
			printf("Dropped packet %4d of %4d: ", i+1, iterations);
			err = SC_ERR_SUCCESS;
		}else{
			/* we send something */
			if(test_mode == TRUE){
				if(last_packet_reordered == FALSE && (rand() % 4 == 1)){
					/* reorder packets with 25% chance */
					j = i + 1;
					last_packet_reordered = TRUE;
				}else if(last_packet_reordered == TRUE){
					j = i - 1;
					last_packet_reordered = FALSE;
				}else{
					j = i; /* no reordering this time */
				}
			}else{
				j = i; /* normal operation*/
			}

			printf("Sending packet %4d of %4d: ", j+1, iterations);
			packet->seq  = htons(j);
			err = scream_send(state, buffer, buffer_size);
		}
		/*sleep for sleep_time milliseconds */
		if(err == SC_ERR_SUCCESS){
			state->num_packets++;
			printf("Sleep for %d\n", sleep_time);
			(void) usleep(sleep_time); /* we ignore what usleep says */
		}
	}
	return err;

}


err_code scream_send(	const scream_base_data* state,
						const char* const buffer,
						const size_t buffer_size){


	assert(state != NULL);
	assert(buffer != NULL);


	/*send packet to destination host and do some error handling */
	if(sendto(state->sock, buffer, buffer_size, 0, (struct sockaddr*) &state->dest_addr, sizeof(state->dest_addr)) < 0){
		perror("Send failed");
		return SC_ERR_SEND;
	}

	return SC_ERR_SUCCESS;
}


err_code scream_close(const scream_base_data* state){
	assert(state != NULL);

	if(close(state->sock) !=0 ) return SC_ERR_SOCK;
	return SC_ERR_SUCCESS;
}
