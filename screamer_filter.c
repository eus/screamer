/*****************************************************************************
 * Copyright (C) 2009 Tadeus Prastowo (eus@member.fsf.org)                   *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 3 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 **************************************************************************//**
 * @file screamer_filter.c                                                     
 * @brief Run this in an OpenWRT with Linux 2.4 to drop every 3rd FLOOD packet.
 *
 * This program will never forget the association pair of a screamer and a
 * listener. So, it may happen that a new association in the external world is
 * treated as an old one by this program.
 *
 * To setup the OpenWRT platform for ASUS WL500W, follows:
 * http://nuwiki.openwrt.org/oldwiki/openwrtdocs/hardware/asus/wl500w
 * (offline saved as doc/assignment4/building_from_source.html)
 * as well as doc/assignment4/assignment4.txt.
 *
 * Use doc/assignment4/dhcp and doc/assignment4/network to replace
 * /etc/config/dhcp and /etc/config/network in the OpenWRT root.
 *
 * This file has to be compiled with, for example,
 * kamikaze_8.09.1/staging_dir/toolchain-mipsel_gcc3.4.6/bin/mipsel-linux-gcc
 * -I `pwd`/kamikaze_8.09.1/staging_dir/mipsel/usr/include/
 * -L `pwd`/kamikaze_8.09.1/staging_dir/mipsel/usr/lib/
 * -o screamer_filter
 * screamer_filter_aug.c
 * -lipq
 *
 * Next, scp the resulting MIPS binary into OpenWRT.
 *
 * Install kamikaze_8.09.1/bin/packages/mipsel/kmod-ipt-queue_2.4.35.4-brcm-2.4-1_mipsel.ipk
 * to OpenWRT.
 *
 * Insert the following command into /etc/firewall.user:
 * iptables -I FORWARD 2 -p udp -j QUEUE
 *
 * Put doc/assignment4/screamer_filter into /etc/init.d and make symbolic links
 * to that file in /etc/rc.d: S98screamer_filter and K98screamer_filter.
 *
 * Restart the OpenWRT to complete the installation.
 *
 * @author Tadeus Prastowo <eus@member.fsf.org>
 ******************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <inttypes.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <libipq.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/netfilter.h>
#include <unistd.h>
#include "scream-common.h"

struct connection_record
{
  struct connection_record *next;
  struct connection_record *prev;
  uint32_t client_addr;
  uint32_t server_addr;
  unsigned packet_counter;
};

struct connection_db
{
  size_t db_len;
  struct connection_record *recs;
};

static struct connection_record *
add_db_record (uint32_t client_addr,
	       uint32_t server_addr,
	       struct connection_db *db)
{
  struct connection_record *rec = malloc (sizeof (struct connection_record));

  if (rec == NULL)
    {
      return NULL;
    }

  rec->client_addr = client_addr;
  rec->server_addr = server_addr;
  rec->packet_counter = 0;

  rec->next = db->recs;
  rec->prev = NULL;
  db->recs = rec;

  db->db_len++;

  return rec;
}

static struct connection_record *
get_db_record (uint32_t client_addr,
	       uint32_t server_addr,
	       const struct connection_db *db)
{
  struct connection_record *itr;

  for (itr = db->recs; itr != NULL; itr = itr->next)
    {
      if (itr->client_addr == client_addr
	  && itr->server_addr == server_addr)
	{
	  return itr;
	}
    }

  return NULL;
}

static void
remove_db_record (struct connection_record *rec, struct connection_db *db)
{
  struct connection_record *itr = db->recs;

  while (itr != NULL)
    {
      if (rec == itr)
	{
	  if (rec->next != NULL)
	    {
	      rec->next->prev = rec->prev;
	    }
	  if (rec == db->recs)
	    {
	      db->recs = rec->next;
	    }
	  if (rec->prev != NULL)
	    {
	      rec->prev->next = rec->next;
	    }

	  break;
	}

      itr = itr->next;
    }

  db->db_len--;
}

static void
free_connection_record (struct connection_record *rec)
{
  free (rec);
}

static void
free_connection_db (struct connection_db *db)
{
  struct connection_record *itr;

  for (itr = db->recs; itr != NULL; itr = db->recs)
    {
      remove_db_record (itr, db);
      free_connection_record (itr);
      db->db_len--;
    }
}

/** The size of an IPv4 header. */
#define IP_HDR_SIZE (15 * 4)

/** The size of a UDP header. */
#define UDP_HDR_SIZE 8

/**
 * The buffer size to inspect a queued packet.
 * This must be big enough so as not to truncate the original packet.
 */
#define BUFFER_LEN 2048

/** Drop every N-th ::scream_packet_flood of a screamer-listener pair. */
#define DROP_NTH_PACKET 3

static bool is_terminated = FALSE;

static void
terminate (int ignore)
{
  is_terminated = TRUE;
}

/**
 * The main entry point of this filter program.
 *
 * @param [in] argc the number of user-supplied arguments.
 * @param [in] argv the user-supplied arguments.
 * @param [in] envp the environment variables.
 *
 * @return an error code
 */
int
main (int argc, char **argv, char **envp)
{
  struct sigaction sigint_action = { .sa_handler = terminate };
  struct connection_db db = {
    .db_len = 0,
    .recs = NULL,
  };
  struct connection_record *rec;
  ipq_packet_msg_t *msg;
  struct iphdr *iphdr;
  struct udphdr *udphdr;
  scream_packet_general *screamhdr;
  struct ipq_handle *h;
  int verdict;
  struct in_addr in;
  unsigned char buffer[BUFFER_LEN];
  ssize_t bytes_rcvd;
  int msg_type;
  unsigned tot_hdr_len;

  if (sigaction (SIGINT, &sigint_action, NULL) == -1)
    {
      perror ("Cannot install SIGINT signal handler");
      exit (EXIT_FAILURE);
    }

  h = ipq_create_handle (0, PF_INET);
  if (h == NULL)
    {
      ipq_perror ("Cannot create IPQ handle");
      exit (EXIT_FAILURE);
    }

  if (ipq_set_mode (h, IPQ_COPY_PACKET, sizeof (buffer)) == -1)
    {
      ipq_perror ("Cannot set IPQ mode");
      ipq_destroy_handle (h);
      exit (EXIT_FAILURE);
    }

  while (is_terminated == FALSE)
    {
      memset (buffer, 0, sizeof (buffer));
      bytes_rcvd = ipq_read (h, buffer, sizeof (buffer), 0);
      if (bytes_rcvd == -1)
	{
	  ipq_perror ("Cannot retrieve packet");
	  continue;
	}

      msg_type = ipq_message_type (buffer);
      if (msg_type == NLMSG_ERROR)
	{
	  errno = ipq_get_msgerr (buffer);
	  perror ("Receive an error message");
	  verdict = NF_ACCEPT;
	  goto set_verdict_and_continue;
	}

      if (msg_type != IPQM_PACKET)
	{
	  verdict = NF_ACCEPT;
	  goto set_verdict_and_continue;
	}

      msg = ipq_get_packet (buffer);
      iphdr = (struct iphdr *) msg->payload;
      
      if (iphdr->protocol != IPPROTO_UDP)
	{
	  verdict = NF_ACCEPT;
	  goto set_verdict_and_continue;
	}

      udphdr = (struct udphdr *) (msg->payload + (iphdr->ihl << 2));

      if (ntohs (udphdr->dest) != SC_DEFAULT_PORT)
	{
	  verdict = NF_ACCEPT;
	  goto set_verdict_and_continue;
	}

      tot_hdr_len = (iphdr->ihl << 2) + UDP_HDR_SIZE;
      if (ntohs (iphdr->tot_len) - tot_hdr_len
	  < sizeof (scream_packet_general))
	{
	  verdict = NF_ACCEPT;
	  goto set_verdict_and_continue;
	}

      screamhdr = (scream_packet_general *) (msg->payload + tot_hdr_len);
      if (screamhdr->type != SC_PACKET_FLOOD)
	{
	  verdict = NF_ACCEPT;
	  goto set_verdict_and_continue;
	}

      rec = get_db_record (iphdr->saddr, iphdr->daddr, &db);
      if (rec == NULL)
	{
	  rec = add_db_record (iphdr->saddr, iphdr->daddr, &db);
	  if (rec == NULL)
	    {
	      fprintf (stderr, "Not enough memory to create table entry\n");
	      verdict = NF_ACCEPT;
	      goto set_verdict_and_continue;
	    }
	}

      in.s_addr = iphdr->saddr;
      printf ("FLOOD packet #%u from %s:%d",
	      rec->packet_counter + 1,
	      inet_ntoa (in),
	      ntohs (udphdr->source));

      in.s_addr = iphdr->daddr;
      printf (" to %s:%d: ",
	      inet_ntoa (in),
	      ntohs (udphdr->source));

      if (rec->packet_counter % DROP_NTH_PACKET == 2)
	{
	  printf ("DROP");
	  verdict = NF_DROP;
	}
      else
	{
	  printf ("ACCEPT");
	  verdict = NF_ACCEPT;
	}
      rec->packet_counter++;

      printf ("\n");

    set_verdict_and_continue:
      if (ipq_set_verdict(h, msg->packet_id, verdict, 0, NULL) == -1)
	{
	  ipq_perror ("Cannot set verdict");
	}
    }

  exit (EXIT_SUCCESS);
}
