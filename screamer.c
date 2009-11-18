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
 * @file:   screamer.c                                                     *
 * @brief:  UDP flooding program                                           *
 * @author: Tobias Heer <heer@cs.rwth-aachen.de>                           *
 *                                                                         *
 *  This file provides the base functions for the UDP                      *
 *  flooder screamer.                                                      *
 **************************************************************************/

#include <stdlib.h> 	/* exit(...) */
#include <stdio.h> 		/* printf(...) */
#include <errno.h> 		/* errno(...) */
#include <unistd.h> 	/* getopt(...) */

#include "scream.h"

void static usage(char* app_name){

	printf( "Usage: %s -d destination -p port [-i iterations] [-s sleep] [-b bufsize]\n"
			"-d destination : IP address or hostname of destination host\n"
			"-p port        : destination port number\n"
			"-i iterations  : number of packets to be sent (0 = infinite). Default is 100\n"
			"-s sleep       : sleep time between loops. Default is 100\n"
			"-b bufsize     : size of the packet payload in byte. Default is 1000 byte\n"
			"-t             : test mode (for testing screamer and the server)\n"
			, app_name);
	exit(EXIT_SUCCESS);
}



int main (const int argc,  char * const argv[]) {


	char* host_name 	= NULL;
	uint16_t port 		= 0;
	int iterations 		= 100;
	int sleep_time  	= 100000; 	/* measured in microseconds:  100 ms, 100.000 us */
	int sleep_time_ms 	= 0;    	/* measured in milliseconds*/
	int buffer_size 	= 1000;
	BOOL test_mode		= FALSE;
	scream_base_data state;			/* basic scream data structure*/

	/* extract command line parameters (HINT: use getopt. > man getopt)*/
	int c;
	int err = SC_ERR_SUCCESS;

	while ((c=getopt(argc, argv, "hd:p:i:s:b:t")) != -1 && err == SC_ERR_SUCCESS){
		switch (c){
		case 'd':
			host_name = optarg;
			break;
		case 'p':
			port = (uint16_t) atoi(optarg);
			if(errno != 0){
				printf("Port number '%s' is invalid (errno: %d, port: %d)\n", optarg, errno, (int) port);
				err = SC_ERR_INPUT;
			}
			break;
		case 'i':
			iterations = atoi(optarg);
			if(errno != 0){
				printf("Loop number '%s' is invalid (errno: %d, iterations: %d)\n", optarg, errno, iterations);
				err = SC_ERR_INPUT;
			}
			break;
		case 's':
			sleep_time_ms = atoi(optarg);
			if(sleep_time_ms == 0 || errno != 0){
				printf("Sleep time' %s' is invalid (errno: %d, sleep time: %d)\n", optarg, errno, sleep_time);
				err = SC_ERR_INPUT;
			}
			sleep_time = sleep_time_ms * 1000;
			break;
		case 'b':
			buffer_size = atoi(optarg);
			if(buffer_size <= 0 || errno  != 0){
				printf("Buffer size '%s' is invalid (errno: %d, buffer size: %d)\n", optarg, errno, (int) buffer_size);
				err = SC_ERR_INPUT;
			}
			break;
		case 't':
			test_mode = TRUE;
			break;
		case 'h':
		default:
			usage(argv[0]);
			exit(EXIT_SUCCESS);
		}
	}

	/* Exit if input was invalid */
	if(err != SC_ERR_SUCCESS){
		printf("Error parsing input.\n");
		exit(EXIT_FAILURE);
	}


	/* check if user provided destination host and port */
	if(host_name == NULL){
		printf("Hostname must be set. Type \"%s -h\" for help.\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	if(port == 0){
		printf("Using default port %d.\n", SC_DEFAULT_PORT);
		port = (uint16_t) SC_DEFAULT_PORT;
	}

	/*Init screamer*/
	if(scream_init(&state ) != SC_ERR_SUCCESS){
		printf("Initialization failed\n");
		exit(EXIT_FAILURE);
	}

	/* fill the basic data structure */
	if(scream_set_dest(&state,host_name, port) != SC_ERR_SUCCESS){
		printf("Socket opetations failed\n");
		exit(EXIT_FAILURE);
	}

	/* start flood loop */
	if(scream_pause_loop(&state, sleep_time, buffer_size, iterations, test_mode) != SC_ERR_SUCCESS){
		printf("Loop or send error\n");
		exit(EXIT_FAILURE);
	}	
	
	/* close screamer */
	if(scream_close(&state) != SC_ERR_SUCCESS){
		printf("Closing socket failed\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

