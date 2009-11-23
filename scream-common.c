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
 ******************************************************************************/

#include <stdio.h> /* fprintf (...) */
#include <assert.h> /* assert (...) */
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "scream-common.h"

bool
is_scream_packet (const void *buffer, size_t len)
{
  const scream_packet_general *packet = buffer;
  size_t expected_size = sizeof (scream_packet_general);

  assert (buffer != NULL);

  /* check packet for consistency */
  if (len < expected_size)
    {
      fprintf (stderr, "Packet too small\n");
      return FALSE;
    }

  if (packet->type <= 0 || packet->type >= SC_PACKET_MAX)
    {
      fprintf (stderr, "Packet type out of bounds\n");
      return FALSE;
    }

  switch (packet->type)
    {
    case SC_PACKET_REGISTER:
      expected_size = sizeof (scream_packet_register);
      break;
    case SC_PACKET_FLOOD:
      expected_size = sizeof (scream_packet_flood);
      break;
    case SC_PACKET_RESET:
      expected_size = sizeof (scream_packet_reset);
      break;
    case SC_PACKET_RESULT:
      expected_size = sizeof (scream_packet_result);
      break;
    case SC_PACKET_ACK:
      expected_size = sizeof (scream_packet_ack);
      break;
    case SC_PACKET_RETURN_ROUTABILITY:
      expected_size = sizeof (scream_packet_return_routability);
      break;
    case SC_PACKET_RETURN_ROUTABILITY_ACK:
      expected_size = sizeof (scream_packet_return_routability_ack);
      break;
    case SC_PACKET_UPDATE_ADDRESS:
      expected_size = sizeof (scream_packet_update_address);
      break;
    case SC_PACKET_UPDATE_ADDRESS_ACK:
      expected_size = sizeof (scream_packet_update_address_ack);
      break;
    }

  if (len < expected_size)
    {
      fprintf (stderr, "Packet too small\n");
      return FALSE;
    }

  return TRUE;
}

const char *
get_scream_type_name (const scream_packet_type type)
{
  switch (type)
    {
    case SC_PACKET_REGISTER:
      return "REGISTER";
    case SC_PACKET_FLOOD:
      return "FLOOD";
    case SC_PACKET_RESET:
      return "RESET";
    case SC_PACKET_ACK:
      return "ACK";
    case SC_PACKET_RETURN_ROUTABILITY:
      return "RETURN ROUTABILITY";
    case SC_PACKET_RETURN_ROUTABILITY_ACK:
      return "RETURN ROUTABILITY ACK";
    case SC_PACKET_UPDATE_ADDRESS:
      return "UPDATE ADDRESS";
    case SC_PACKET_UPDATE_ADDRESS_ACK:
      return "UPDATE ADDRESS ACK";
    case SC_PACKET_RESULT:
      return "RESULT";
    default:
      return "UNKNOWN";
    }
}

err_code
send_ack (int sock, const struct sockaddr_in *dest)
{
  scream_packet_ack packet = { .type = SC_PACKET_ACK };

  if (sendto (sock, &packet, sizeof (packet), 0,
	      (struct sockaddr *) dest, sizeof (*dest)) == -1)
    {
      return SC_ERR_SEND;
    }

  return SC_ERR_SUCCESS;
}

unsigned long long
get_last_packet_ts (int sockfd)
{
  struct timeval curr_time;

  if (ioctl (sockfd, SIOCGSTAMP, &curr_time) == -1)
    {
      return 0;
    }

  return curr_time.tv_sec * 1000000ULL + curr_time.tv_usec;
}

unsigned long
eus_strtoul (const char *str, int *has_error, const char *what_is_str)
{
  unsigned long result;

  errno = 0;
  result = strtoul (str, NULL, 10);
  switch (errno)
    {
    case EINVAL:
      fprintf (stderr, "Error: %s is not an integer\n", what_is_str);
      *has_error = 1;
      return 0;
    case ERANGE:
      fprintf (stderr, "Error: %s is too big\n", what_is_str);
      *has_error = 1;
      return 0;
    }

  *has_error = 0;
  return result;
}

unsigned long long
eus_strtoull (const char *str, int *has_error, const char *what_is_str)
{
  unsigned long long result;

  errno = 0;

  result = strtoull (str, NULL, 10);
  switch (errno)
    {
    case EINVAL:
      fprintf (stderr, "Error: %s is not an integer\n", what_is_str);
      *has_error = 1;
      return 0;
    case ERANGE:
      fprintf (stderr, "Error: %s is too big\n", what_is_str);
      *has_error = 1;
      return 0;
    }

  *has_error = 0;
  return result;
}

long
eus_strtol (const char *str, int *has_error, const char *what_is_str)
{
  long result;

  errno = 0;
  result = strtol (str, NULL, 10);
  switch (errno)
    {
    case EINVAL:
      fprintf (stderr, "Error: %s is not an integer\n", what_is_str);
      *has_error = 1;
      return 0;
    case ERANGE:
      fprintf (stderr, "Error: %s is too big\n", what_is_str);
      *has_error = 1;
      return 0;
    }

  *has_error = 0;
  return result;
}

long long
eus_strtoll (const char *str, int *has_error, const char *what_is_str)
{
  long long result;

  errno = 0;
  result = strtoll (str, NULL, 10);
  switch (errno)
    {
    case EINVAL:
      fprintf (stderr, "Error: %s is not an integer\n", what_is_str);
      *has_error = 1;
      return 0;
    case ERANGE:
      fprintf (stderr, "Error: %s is too big\n", what_is_str);
      *has_error = 1;
      return 0;
    }

  *has_error = 0;
  return result;
}

err_code
set_timeout (int socket, const struct timeval *timeout)
{
  if (setsockopt (socket, SOL_SOCKET, SO_RCVTIMEO, timeout,
		  sizeof (*timeout)) == -1)
    {
      perror ("Cannot set timeout on socket");
      return SC_ERR_SOCKOPT;
    }

  return SC_ERR_SUCCESS;
}

err_code
unset_timeout (int socket)
{
  struct timeval timeout = {0};

  return set_timeout (socket, &timeout);
}
