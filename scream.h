#ifndef SCREAM_H
#define SCREAM_H
/**
 *  @file 	scream.h
 *  @brief 	UDP flooding program
 *  @author Tobias Heer <heer@cs.rwth-aachen.de>
 *
 *  Header files for scream.
 *
 *  ASSIGNMENT: Complete the file according to the usage
 * 				scenario
 *
 *
 **/
#include <netinet/in.h>    /* sockets */
#include "scream-common.h" /* common headers and definitions */


/** @brief: Scream base data structure. This structure holds the state information for a scream run. **/
struct scream_base_data_s{
    int sock ;				/** @brief: socket 					**/
    struct sockaddr_in dest_addr;	/** @brief: destination address		**/
    int num_packets;
    /* extend this struct to represent all state information */
};

typedef struct scream_base_data_s scream_base_data;


/**
 * Initialize the memory for the scream_base_data struct
 *
 *
 * @note: 	Right now this function is really stupid. Feel
 * 			free to extend it.
 *
 * @param	state	basic state information for scream.
 *
 * @return 	scream_base_data struct
 */
err_code scream_init(/*@out@*/ scream_base_data* state);


/** Resolve destination name to IP and fill state->dest_addr
 *
 *  This function opens an UDP socket, stores the socket
 *  descriptor in this->baseSocket.
 *
 *  state->dest_addr is filled with the address family, the
 *  IP address, and port number. All values should be in
 *  network byte order.
 *
 * @note: mind the network byte order for multi-byte data types
 *
 * @param	state		basic state information for scream
 * @param 	host_name 	name of the destination host
 * @param 	port    	destination port number in host byte order
 * @return				error code
 **/
err_code scream_set_dest(	scream_base_data* state,
							const char* host_name,
							const uint16_t port);
/**
 * Loop, generate buffer, and send buffer to destination
 *
 * @param	state			basic state information for scream
 * @param	sleep_time		time between loops (in us - microseconds)
 * @param	buffer_size 	size of the buffer to generate (in bytes)
 * @param	iterations		number of loops (0 means infinite loops)
 * @return					error code
 *
 **/
err_code scream_pause_loop(	 scream_base_data* state,
							const int sleep_time,
							const int buffer_size,
							const int iterations,
							const BOOL test_mode);

/**
 * Send UDP packet to the destination specified
 * in state->dest_addr.
 *
 * @param	state			basic state information for scream
 * @param	buffer			buffer to be sent
 * @param	buffer_size 	size of the buffer in bytes
 * @return					error code
 *
 **/
err_code scream_send(	const scream_base_data* state,
						const char* buffer,
						const size_t buffer_size);

/**
 * Close socket and free memory for data
 *
 * @param	state	basic state information for scream
 * @return			error code
 *
 **/
err_code scream_close(	const scream_base_data* state);

#endif /* #ifndef SCREAM_H */
