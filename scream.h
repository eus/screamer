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

#include <pthread.h>
#include <netinet/in.h> /* sockets */
#include "scream-common.h" /* common headers and definitions */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The timeout in microsecond for receiving ::scream_packet_ack after sending
 * ::scream_packet_register.
 */
#define REGISTER_TIMEOUT 1000000ULL

/**
 * The number of repetitions of send-recv cycle to receive
 * ::scream_packet_ack.
 */
#define REGISTER_REPETITION (-1)

/**
 * The timeout in microsecond for receiving for receiving
 * ::scream_packet_result after sending ::scream_packet_reset.
 */
#define RESET_TIMEOUT REGISTER_TIMEOUT

/**
 * The number of repetitions of send-recv cycle to receive
 * ::scream_packet_result.
 */
#define RESET_REPETITION (-1)

/**
 * The timeout in microsecond for receiving
 * ::scream_packet_return_routability_ack after sending
 * ::scream_packet_return_routability.
 */
#define RETURN_ROUTABILITY_CHECK_TIMEOUT REGISTER_TIMEOUT

/**
 * The number of repetitions of send-recv cycle to receive
 * ::scream_packet_return_routability_ack.
 */
#define RETURN_ROUTABILITY_CHECK_REPETITION 3

/**
 * The timeout in microsecond for receiving ::scream_packet_update_address_ack
 * after sending ::scream_packet_update_address.
 */
#define UPDATE_ADDRESS_TIMEOUT REGISTER_TIMEOUT

/**
 * The number of repetitions of send-recv cycle to receive
 * ::scream_packet_update_address_ack.
 */
#define UPDATE_ADDRESS_REPETITION 3

/**
 * The frequency in second of checking for an interface update that a manager
 * thread should perform.
 */
#define MANAGER_POOL_RATE 5

/**
 * Scream base data structure.
 * This structure holds the state information for a scream run.
 */
struct scream_base_data_s
{
  int sock; /**< The communication socket. */
  pthread_mutex_t sock_lock; /**< The communication socket lock. */
  struct sockaddr_in dest_addr; /**< The listener address. */
  int num_packets; /**< The number of scream_packet_type::SC_PACKET_FLOOD
                    *   packets that has been sent. */
  uint32_t id; /**< The client ID. */
  bool is_registered; /**< The screamer has successfully registered. */
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
 * Resolve the host name to initialize scream_base_data::dest_addr.
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
scream_register (scream_base_data *state,
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
 * Send a UDP packet to the destination specified in the destination address.
 *
 * @param [in] sock the socket to send the data.
 * @param [in] sock_lock the concurrent modification guard of the socket.
 * @param [in] dest_addr the destination address.
 * @param [in] buffer data to be sent.
 * @param [in] buffer_size the size of the buffer in bytes.
 *
 * @return An error code.
 *
 */
err_code
scream_send (int sock,
	     pthread_mutex_t *sock_lock,
	     const struct sockaddr_in *dest_addr,
	     const void *buffer,
	     size_t buffer_size);

/**
 * Send a UDP packet through the given socket to the specified destination
 * without locking the socket first.
 *
 * @param [in] sock the socket used to send the packet.
 * @param [in] dest_addr the address to which the packet is to be sent.
 * @param [in] buffer data to be sent.
 * @param [in] buffer_size the size of the buffer in bytes.
 *
 * @return An error code.
 *
 */
err_code
scream_send_no_lock (int sock,
		     const struct sockaddr_in *dest_addr,
		     const void *buffer,
		     size_t buffer_size);

/**
 * Receive a UDP packet from the source specified in
 * scream_base_data::dest_addr through scream_base_data::sock.
 *
 * @param [in] sock the socket from which the data is to be received.
 * @param [in] sock_lock the concurrent modification guard of the socket.
 * @param [in] dest_addr the address from which the data is to be received.
 * @param [out] buffer the buffer to receive data.
 * @param [in] buffer_size the size of the buffer in bytes.
 *
 * @return err_code::SC_ERR_COMM if no packet is received until timeout,
 *         err_code::SC_ERR_WRONGSENDER if the packet is not received from the
 *         specified source, err_code::SC_ERR_PACKET if the packet is not a
 *         scream packet, err_code::SC_ERR_SUCCESS if the packet is received
 *         from the specified source before timeout, or else
 *         err_code::SC_ERR_RECV.
 */
err_code
scream_recv (int sock,
	     pthread_mutex_t *sock_lock,
	     const struct sockaddr_in *dest_addr,
	     void *buffer,
	     size_t buffer_size);

/**
 * Receive a UDP packet from the specified address through the given socket
 * without locking the socket first.
 *
 * @param [in] sock the socket from which the packet is received.
 * @param [in] dest_addr from which address the packet must be received.
 * @param [out] buffer the buffer to receive data.
 * @param [in] buffer_size the size of the buffer in bytes.
 *
 * @return err_code::SC_ERR_COMM if no packet is received until timeout,
 *         err_code::SC_ERR_WRONGSENDER if the packet is not received from the
 *         specified source, err_code::SC_ERR_PACKET if the packet is not a
 *         scream packet, err_code::SC_ERR_SUCCESS if the packet is received
 *         from the specified source before timeout, or else
 *         err_code::SC_ERR_RECV.
 */
err_code
scream_recv_no_lock (int sock,
		     const struct sockaddr_in *dest_addr,
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
 * @param [in] sock the socket through which data are sent and received.
 * @param [in] sock_lock the concurrent modification guard of the socket.
 * @param [in] dest_addr the destination address from which data is received.
 * @param [in] timeout the timeout after which the particular message is resent.
 * @param [in] repetition how many times the send-recv process has to be
 *                        repeated if it gets a timeout or a wrong packet (-1
 *                        means keep repeating)
 *
 * @return err_code::SC_ERR_COMM if the send-recv process has been repeated
 *         as many as the specified repetition or another error code.
 */
err_code
scream_send_and_wait_for (const void *send_what,
			  size_t send_what_len,
			  scream_packet_general *wait_for_what,
			  size_t wait_for_what_len,
			  const char *wait_msg,
			  int sock,
			  pthread_mutex_t *sock_lock,
			  const struct sockaddr_in *dest_addr,
			  const struct timeval *timeout,
			  int repetition);

/**
 * Keep resending a particular message to the destination specified in
 * the given destination address through the given socket every time
 * a timeout happens or a wrong message is received until a particular expected
 * message is received from the destination without locking the socket first.
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
 * @param [in] sock the socket through which message is exchanged.
 * @param [in] dest_addr the destination address to/from which message is
 *                       sent/received.
 * @param [in] timeout the timeout after which the particular message is resent.
 * @param [in] repetition how many times the send-recv process has to be
 *                        repeated if it gets a timeout or a wrong packet (-1
 *                        means keep repeating)
 *
 * @return err_code::SC_ERR_COMM if the send-recv process has been repeated
 *         as many as the specified repetition or another error code.
 */
err_code
scream_send_and_wait_for_no_lock (const void *send_what,
				  size_t send_what_len,
				  scream_packet_general *wait_for_what,
				  size_t wait_for_what_len,
				  const char *wait_msg,
				  int sock,
				  const struct sockaddr_in *dest_addr,
				  const struct timeval *timeout,
				  int repetition);	

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
scream_reset (scream_base_data *state,
	      scream_packet_result *result);

/**
 * Print the statistics of screaming.
 *
 * @param [in] result the result received from the server.
 */
void
print_result (const scream_packet_result *result);

/** An interface that has an IPv4 associated address and socket. */
struct channel_record
{
  struct channel_record *next; /**< A link to the next record. */
  struct channel_record *prev; /**< A link to the previous record. */
  char *if_name; /**< The interface name. */
  struct sockaddr_in if_addr; /**< The associated IPv4 address. */
  int sock; /**< The associated socket. */
  bool is_new; /**< This channel is a new channel. */
  bool is_expired; /**< This channel no longer exists. */
  bool is_modified; /**< This channel has got an address change. */
  bool is_connected; /**< The listener can be contacted through this channel. */
  unsigned long long checking_time; /**<
				     * The duration of return-routability
				     * checking in microsecond.
				     */
};

/**
 * A collection of the available public IPv4 interfaces (i.e., ignoring
 * loopback).
 */
struct channel_db
{
  size_t db_len; /**< The number of the available channels. */
  struct channel_record *recs; /**< The available channels. */
};

/**
 * Store a channel record in the DB.
 *
 * @param [in] rec the record to be stored.
 * @param [in] db the DB in which the record is to be stored.
 */
void
store_db_record (struct channel_record *rec, struct channel_db *db);

/**
 * Remove a channel record from the DB. The removed channel still has its
 * channel_record::next and channel_record::prev pointers points to the
 * records in the DB so that it is save to invoke this function while iterating
 * over the DB.
 *
 * @param [in] rec the record to be removed.
 * @param [in] db the DB from which the record is to be removed.
 */
void
remove_db_record (struct channel_record *rec, struct channel_db *db);

/**
 * Get a channel record from that has the specified name from the DB.
 *
 * @param [in] name the name of the channel record to find.
 * @param [in] db a collection of channel records.
 *
 * @return NULL if the name matches no record or a channel_record having the
 *         specified name.
 */
struct channel_record *
get_channel_by_name (const char *name, const struct channel_db *db);

/**
 * A shared data indicating the channel through which all communication with
 * a listener should take place.
 */
struct comm_channel
{
  struct channel_record *channel; /**< The main communication channel. */
  int *sock; /**< The socket through which a screamer communicates with a
	      *   listener.
	      */
  pthread_mutex_t *sock_lock; /**< A concurrent modification guard for sock. */
  bool *is_registered; /**<
			* The screamer has registered itself so that
			* an update address packet can be sent.
			*/
};

/**
 * Check for an update in the available communication channels.
 *
 * @param [in] db the collection of available communication channels to be
 *                updated. Which record is new and which one is updated can be
 *                checked through channel_record::is_new,
 *                channel_record::is_expired and channel_record::is_modified.
 *
 * @return bool::TRUE if there is an update or bool::FALSE if there is none.
 */
bool
probe_ifs (struct channel_db *db);

/**
 * Check all possible communication channels by binding all non-expired
 * addresses in the DB to sockets stored in the corresponding records in the
 * DB. The channel_record::is_connected field of all records whose sockets can
 * be used to contact the listener is set to bool::TRUE, while that of other
 * records is set to bool::FALSE.
 *
 * @param [in] dest_addr the address of the listener.
 * @param [in] db all of the possible communication channels whose
 *                channel_record::is_connected is to be updated.
 * @param [in] main_channel the main channel of communication.
 *
 * @return The best communication channel based on checking time.
 */
struct channel_record *
check_return_routability (const struct sockaddr_in *dest_addr,
			  const struct channel_db *db,
			  const struct comm_channel *main_channel);

/**
 * Retrieve the name of a socket.
 *
 * @param [in] sock the socket whose name is to be retrieved.
 * @param [out] name the retrieved name of the socket.
 *
 * @return err_code::SC_ERR_NAME if there is a problem in retrieving the name
 *         or err_code::SC_ERR_SUCCESS if everything is okay.
 */
err_code
get_address (int sock, struct sockaddr_in *name);

/**
 * Inform the listener of this screamer's new address so that when the screamer
 * sends packet from this new address, the listener knows whose book-keeping
 * record it should update.
 *
 * @param [in] sock the socket through which the update notification is
 *                  delivered.
 * @param [in] id the ID of the client so that the listener can identify which
 *                old address needs to be updated.
 * @param [in] new_addr the new address of the client.
 * @param [in] dest_addr the address of the listener.
 * @param [in] main_channel the main communication channel.
 *
 * @return err_code::SC_ERR_COMM if the process of updating an address fails
 *         because the listener does not send acknowledgement,
 *         err_code::SC_ERR_SUCCESS if the update process completes successfully
 *         or another error code in case there is a problem with the socket.
 */
err_code
update_address (int sock,
		uint32_t id,
		const struct sockaddr_in *new_addr,
		const struct sockaddr_in *dest_addr,
		const struct comm_channel *main_channel);

/**
 * All expired channels in the DB are removed and freed accordingly.
 *
 * @param [in] db the DB whose expired channels are to be removed and freed.
 */
void
remove_expired_channels (struct channel_db *db);

/**
 * Free a channel record. 
 *
 * @param [in] channel the channel to be freed (no longer valid upon return).
 */
void
free_channel (struct channel_record *channel);

/**
 * Remove and free all channels in the DB.
 *
 * @param [in] db the DB whose channels are to be removed and freed.
 */
void
free_channel_db (struct channel_db *db);

/**
 * Replace the main communication socket by locking it with its corresponding
 * mutex lock.
 *
 * @param [in] main_channel the main channel of communication to be updated.
 * @param [in] new_channel the new channel of communication.
 *
 * @return An error code.
 */
err_code
switch_comm_channel (struct comm_channel *main_channel,
		     struct channel_record *new_channel);

/**
 * Replace the main communication socket without first locking it with its
 * corresponding mutex lock.
 *
 * @param [in] main_channel the main channel of communication to be updated.
 * @param [in] new_channel the new channel of communication.
 *
 * @return An error code.
 */
err_code
switch_comm_channel_no_lock (struct comm_channel *main_channel,
			     struct channel_record *new_channel);

/** The data that a manager thread needs for its operation. */
struct manager_data
{
  struct comm_channel *main_channel; /**< The main communication channel. */
  struct channel_db *channels; /**< The available channels. */
  bool is_stopped; /**< A signal to stop the manager thread graciously. */
  const uint32_t *id; /**< The client ID. */
  const struct sockaddr_in *dest_addr; /**< The listener address. */
  err_code exit_code; /**< The manager exit code. */
};

/**
 * Set channel_record::is_new and channel_record::is_modified to bool::FALSE.
 * This should be performed after a call to probe_ifs() that usually is
 * followed by a call to check_return_routability() and usually is followed by
 * some operations that use those flags to make some decisions. Those flags
 * need to be reset because after all operations is done, the flagged records
 * are no longer new or modified when they are passed to another call to
 * probe_ifs().
 *
 * @param [in] db the collection of available communication channels to be
 *                updated.
 */
void
reset_new_and_modified_flags (struct channel_db *db);

/**
 * Start the manager thread to monitor IPv4 addresses in all available public
 * interfaces (i.e,. ignoring the loopback interface). It can change the
 * communication channel to another one that is more desirable
 *
 * @param [in] is_careful will make the manager to behave as per the
 *                        specification of start_careful_manager() when it is
 *                        set to bool::TRUE. When it is set to bool::FALSE, it
 *                        will behave as per the specification of
 *                        start_sloppy_manager().
 * @param [in] manager_name the name of the manager.
 * @param [in] data the needed data to perform a management duty.
 */
void
start_manager (bool is_careful,
	       const char *manager_name,
	       struct manager_data *data);

/**
 * Start the manager thread to monitor IPv4 addresses in all available public
 * interfaces (i.e,. ignoring the loopback interface). It can change the
 * communication channel to another one that is more desirable. Spotting
 * an update, this careful manager will prevent anyone from using the
 * communication channel by holding a mutex lock on the channel.
 *
 * @param [in] data a manager_data object.
 *
 * @return NULL because a manager is not suppossed to quit until program exit.
 */
void *
start_careful_manager (void *data);

/**
 * Start the manager thread to monitor IPv4 addresses in all available public
 * interfaces (i.e,. ignoring the loopback interface). It can change the
 * communication channel to another one that is more desirable. This sloppy
 * manager will not prevent anyone from using the communication channel upon
 * spotting a need to update the communication channel. The manager will only
 * do so when it is replacing the communication channel.
 *
 * @param [in] data a manager_data object.
 *
 * @return NULL because a manager is not suppossed to quit until program exit.
 */
void *
start_sloppy_manager (void *data);

#ifdef __cplusplus
}
#endif

#endif /* SCREAM_H */
