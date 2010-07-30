/* OpenBSC Abis input driver for ip.access */

/* (C) 2009 by Harald Welte <laforge@gnumonks.org>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <osmocore/select.h>
#include <osmocore/msgb.h>
#include <osmocore/talloc.h>
#include <ipaccess.h>


#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#endif

#define TS1_ALLOC_SIZE	4096

static const uint8_t pong[] = { 0, 1, IPAC_PROTO_IPACCESS, IPAC_MSGT_PONG };
static const uint8_t id_ack[] = { 0, 1, IPAC_PROTO_IPACCESS, IPAC_MSGT_ID_ACK };
static const uint8_t id_req[] = { 0, 17, IPAC_PROTO_IPACCESS, IPAC_MSGT_ID_GET,
					0x01, IPAC_IDTAG_UNIT, 
					0x01, IPAC_IDTAG_MACADDR,
					0x01, IPAC_IDTAG_LOCATION1,
					0x01, IPAC_IDTAG_LOCATION2,
					0x01, IPAC_IDTAG_EQUIPVERS,
					0x01, IPAC_IDTAG_SWVERSION,
					0x01, IPAC_IDTAG_UNITNAME,
					0x01, IPAC_IDTAG_SERNR,
				};

static const char *idtag_names[] = {
	[IPAC_IDTAG_SERNR]	= "Serial_Number",
	[IPAC_IDTAG_UNITNAME]	= "Unit_Name",
	[IPAC_IDTAG_LOCATION1]	= "Location_1",
	[IPAC_IDTAG_LOCATION2]	= "Location_2",
	[IPAC_IDTAG_EQUIPVERS]	= "Equipment_Version",
	[IPAC_IDTAG_SWVERSION]	= "Software_Version",
	[IPAC_IDTAG_IPADDR]	= "IP_Address",
	[IPAC_IDTAG_MACADDR]	= "MAC_Address",
	[IPAC_IDTAG_UNIT]	= "Unit_ID",
};

static const char *ipac_idtag_name(int tag)
{
	if (tag >= ARRAY_SIZE(idtag_names))
		return "unknown";

	return idtag_names[tag];
}

/* send the id ack */
int ipaccess_send_id_ack(int fd)
{
	return write(fd, id_ack, sizeof(id_ack));
}

int ipaccess_send_id_req(int fd)
{
	return write(fd, id_req, sizeof(id_req));
}

/* base handling of the ip.access protocol */
int ipaccess_rcvmsg_base(struct msgb *msg,
			 struct bsc_fd *bfd)
{
	uint8_t msg_type = *(msg->l2h);
	int ret = 0;

	switch (msg_type) {
	case IPAC_MSGT_PING:
		ret = write(bfd->fd, pong, sizeof(pong));
		break;
	case IPAC_MSGT_PONG:
		break;
	case IPAC_MSGT_ID_ACK:
		ret = ipaccess_send_id_ack(bfd->fd);
		break;
	}
	return 0;
}

/*
 * read one ipa message from the socket
 * return NULL in case of error
 */
struct msgb *ipaccess_read_msg(struct bsc_fd *bfd, int *error)
{
	struct msgb *msg = msgb_alloc(TS1_ALLOC_SIZE, "Abis/IP");
	struct ipaccess_head *hh;
	int len, ret = 0;

	if (!msg) {
		*error = -ENOMEM;
		return NULL;
	}

	/* first read our 3-byte header */
	hh = (struct ipaccess_head *) msg->data;
	ret = recv(bfd->fd, msg->data, 3, 0);
	if (ret < 0) {
		msgb_free(msg);
		*error = ret;
		return NULL;
	} else if (ret == 0) {
		msgb_free(msg);
		*error = ret;
		return NULL;
	}

	msgb_put(msg, ret);

	/* then read te length as specified in header */
	msg->l2h = msg->data + sizeof(*hh);
	len = ntohs(hh->len);
	ret = recv(bfd->fd, msg->l2h, len, 0);
	if (ret < len) {
		msgb_free(msg);
		*error = -EIO;
		return NULL;
	}
	msgb_put(msg, ret);

	return msg;
}

void ipaccess_prepend_header(struct msgb *msg, int proto)
{
	struct ipaccess_head *hh;

	/* prepend the ip.access header */
	hh = (struct ipaccess_head *) msgb_push(msg, sizeof(*hh));
	hh->len = htons(msg->len - sizeof(*hh));
	hh->proto = proto;
}
