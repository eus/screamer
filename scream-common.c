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
is_sane (const void *buffer, size_t len, size_t expected_len)
{
  const scream_packet_general *packet = buffer;

  if (len < expected_len)
    {
      fprintf (stderr, "Packet too small\n");
      return FALSE;
    }

  if (packet->type <= 0 || packet->type >= SC_PACKET_MAX)
    {
      fprintf (stderr, "Packet type out of bounds\n");
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
    case SC_PACKET_REGISTER_ACK:
      return "REGISTER ACK";
    case SC_PACKET_SERVER_CLIENT_SYNC:
      return "SERVER CLIENT SYNC";
    case SC_PACKET_SERVER_CLIENT_RESP:
      return "SERVER CLIENT RESP";
    case SC_PACKET_SERVER_CLIENT_RESP_ACK:
      return "SERVER CLIENT RESP ACK";
    case SC_PACKET_CLIENT_SERVER_SYNC:
      return "CLIENT SERVER SYNC";
    case SC_PACKET_CLIENT_SERVER_RESP:
      return "CLIENT SERVER RESP";
    case SC_PACKET_CLIENT_SERVER_ACK:
      return "CLIENT SERVER ACK";
    case SC_PACKET_RESET:
      return "RESET";
    case SC_PACKET_RESET_ACK:
      return "RESET ACK";
    case SC_PACKET_SERVER_CLIENT_DATA:
      return "SERVER CLIENT DATA";
    case SC_PACKET_CLIENT_SERVER_DATA:
      return "CLIENT SERVER DATA";
    default:
      return "UNKNOWN";
    }
}

void
remove_packet(int sock)
{
  char buffer;
  if (recvfrom (sock,
		&buffer,
		sizeof (buffer),
		0,
		NULL,
		NULL) == -1)
    {
		
      perror ("Error in removing packet");
    }
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

void
sqlite3_perror (sqlite3 *db, const char *message)
{
  fprintf (stderr, "%s: %s\n", message, sqlite3_errmsg(db));
}

void
sqlite3_clean_free (void *ptr_addr)
{
  void **p = (void **) ptr_addr;
  sqlite3_free (*p);
  *p = NULL;
}
