/*
 * Copyright (C) 2013 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * You can also choose to distribute this program under the terms of
 * the Unmodified Binary Distribution Licence (as given in the file
 * COPYING.UBDL), provided that you have satisfied its requirements.
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/refcnt.h>
#include <ipxe/interface.h>
#include <ipxe/job.h>
#include <ipxe/xfer.h>
#include <ipxe/iobuf.h>
#include <ipxe/open.h>
#include <ipxe/socket.h>
#include <ipxe/retry.h>
#include <ipxe/monojob.h>
#include <ipxe/tcpip.h>
#include <emulab/bootinfo.h>
#include <emulab/bootwhat.h>

/** @file
 *
 * BOOTINFO query
 *
 */

/* Disambiguate the various error causes */
#define EPROTO_LEN __einfo_error ( EINFO_EPROTO_LEN )
#define EINFO_EPROTO_LEN __einfo_uniqify ( EINFO_EPROTO, 0x01, \
					   "Incorrect reply length" )
#define EPROTO_DATA __einfo_error ( EINFO_EPROTO_DATA )
#define EINFO_EPROTO_DATA __einfo_uniqify ( EINFO_EPROTO, 0x02, \
					    "Incorrect reply data" )
#define EPROTO_SEQ __einfo_error ( EINFO_EPROTO_SEQ )
#define EINFO_EPROTO_SEQ __einfo_uniqify ( EINFO_EPROTO, 0x03, \
					   "Delayed or out-of-sequence reply" )

// ERRNO defines
//
// I'm sure that these are defined in the iPXE code base somewhere.
// I would expect that if I include errno.h that they would be defined.
//
// However, I can't figure out how to define the ERRFILE and following
// the patterns in the iPXE code base does not help. I started with
// pinger.c and 
//
// I assume that they are defined elsewhere, but I cannot figure
// out how to avoid this error:
//
// emulab/bootinfo.c:167:5: error: ‘missing_errfile_declaration’ is deprecated (declared at include/errno.h:120) [-Werror=deprecated-declarations]
//   return -EINVAL;

// So, these values are defined here
//static int LOCAL_EINVAL = 22;

#undef EPROTO_DATA
#undef ETIMEDOUT
#undef EPROTO_LEN
#undef ENOMEM
#undef EINVAL
#undef EPROTO_SEQ
#undef ECONNABORTED

//static int EPROTO_DATA = 1;
static int ETIMEDOUT = 24;
//static int EPROTO_LEN = 25;
static int ENOMEM = 23;
//static int EINVAL = 26;
//static int EPROTO_SEQ = 26;
static int ECONNABORTED = 28;

/*
static int EPROTOLENT = 24;
static int EPROTOLENT = 24;
*/

/** A bootinfo request */
struct bootinfo_request {
	/** Reference counter */
	struct refcnt refcnt;
	/** Bootinfo request interface */
	struct interface request;
	/** Data transfer interface */
	struct interface socket;
	/** Retry timer */
	struct retry_timer timer;
	struct boot_info bootinfo;

};

int bootinfo (const char * hostname) {
	int rc;

	printf ("bootinfo(%s)\n", hostname);

	if ( ( rc = create_bootinfo_query ( &monojob, hostname ) ) != 0 ) {
		printf ( "Could not create bootinfo request: %s\n", strerror ( rc ) );
		return rc;
	}

	/* Wait for bootinfo to complete */
	if ( ( rc = monojob_wait ( NULL, 0 ) ) != 0 ) {
		printf ( "Bootinfo finished: %s\n", strerror ( rc ) );
		return rc;
	}


	return 0;
}

/**
 * Mark bootinfo request as complete
 *
 * @v bootinfo		Bootinfo request
 * @v rc		Return status code
 */
static void bootinfo_done ( struct bootinfo_request *bootinfo, int rc ) {
	printf ("bootinfo_done()\n");

	/* Stop the retry timer */
	stop_timer ( &bootinfo->timer );

	/* Shut down interfaces */
	intf_shutdown ( &bootinfo->socket, rc );
	intf_shutdown ( &bootinfo->request, rc );
}

/**
 * Send bootinfo request
 *
 * @v bootinfo		Bootinfo request
 * @ret rc		Return status code
 */
static int bootinfo_send_packet ( struct bootinfo_request *bootinfo ) {
	boot_info_t boot_info;
	printf ("bootinfo_send_packet()\n");
	/*
	 * Create a bootinfo request packet and send it.
	 */
	memset ( &boot_info, 0, sizeof(boot_info) );
	boot_info.version = BIVERSION_CURRENT;
	boot_info.opcode  = BIOPCODE_BOOTWHAT_REQUEST;
	/* Start retransmission timer */
	start_timer ( &bootinfo->timer );

	/* Send the data */
	return xfer_deliver_raw ( &bootinfo->socket, &boot_info, sizeof( boot_info ) );
}

/**
 * Handle received data
 *
 * @v bootinfo		Bootinfo
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int bootinfo_xfer_deliver ( struct bootinfo_request *bootinfo,
				   struct io_buffer *iobuf,
				   struct xfer_metadata *meta __unused ) {
	printf ("bootinfo_xfer_deliver()\n");
	boot_info_t *boot_reply = iobuf->data;
	boot_what_t *boot_whatp = (boot_what_t *) boot_reply->data;

	switch (boot_whatp->type) {
	case BIBOOTWHAT_TYPE_PART:
		printf ("BIBOOTWHAT_TYPE_PART\n");
		printf("partition:%d", boot_whatp->what.partition);
		if (boot_whatp->cmdline[0])
			printf(" %s", boot_whatp->cmdline);
		printf("\n");
		//goto done;
		break;
	case BIBOOTWHAT_TYPE_WAIT:
		printf ("BIBOOTWHAT_TYPE_WAIT\n");
		//		if (debug)
		//			printf("wait: now polling\n");
		//		pollmode = 1;
		//goto poll;
		break;
	case BIBOOTWHAT_TYPE_REBOOT:
		printf ("BIBOOTWHAT_TYPE_REBOOT\n");
		//		printf("reboot\n");
		//goto done;
		break;
	case BIBOOTWHAT_TYPE_AUTO:
		printf ("BIBOOTWHT_TYPE_AUTO\n");
		//		if (debug)
		//			printf("query: will query again\n");
		//goto again;
		break;
	case BIBOOTWHAT_TYPE_MFS:
		printf("BIBOOTWHAT_TYPE_MFS: mfs:%s\n", boot_whatp->what.mfs);
		//	goto done;
		break;
	}

	/* Stop the retry timer.  After this point, each code path
	 * must either restart the timer by calling dns_send_packet(),
	 * or mark the DNS operation as complete by calling
	 * dns_done()
	 */
	stop_timer ( &bootinfo->timer );

	
	return 0;
}

/**
 * Handle bootinfo retransmission timer expiry
 *
 * @v timer		Retry timer
 * @v fail		Failure indicator
 */
static void bootinfo_timer_expired ( struct retry_timer *timer __unused, int fail ) {
	printf ("bootinfo_timer_expired()\n");

	struct bootinfo_request *bootinfo =
		container_of ( timer, struct bootinfo_request, timer );

	if ( fail ) {
		printf ( "bootinfo_timer_expired: calling bootinfo_done\n" );
		bootinfo_done ( bootinfo, -ETIMEDOUT );
	} else {
		printf ( "bootinfo_timer_expired: calling bootinfo_send_packet\n" );
		bootinfo_send_packet ( bootinfo );
	}
}

/**
 * Receive new data
 *
 * @v bootinfo		Bootinfo request
 * @v rc		Reason for close
 */
static void bootinfo_xfer_close ( struct bootinfo_request *bootinfo __unused, int rc __unused) {
	printf ("bootinfo_xfer_close()\n");

	if ( ! rc )
		rc = -ECONNABORTED;

	bootinfo_done ( bootinfo, rc );
}

/** Bootinfo socket interface operations */
static struct interface_operation bootinfo_socket_operations[] = {
	INTF_OP ( xfer_deliver, struct bootinfo_request *, bootinfo_xfer_deliver ),
	INTF_OP ( intf_close, struct bootinfo_request *, bootinfo_xfer_close ),
};

/** Bootinfo socket interface descriptor */
static struct interface_descriptor bootinfo_socket_desc =
	INTF_DESC ( struct bootinfo_request, socket, bootinfo_socket_operations );

/** Bootinfo request interface operations */
static struct interface_operation bootinfo_request_op[] = {
	INTF_OP ( intf_close, struct bootinfo_request *, bootinfo_done ),
};

/** Bootinfo request interface descriptor */
static struct interface_descriptor bootinfo_request_desc =
	INTF_DESC ( struct bootinfo_request, request, bootinfo_request_op );

/**v
 * bootinfo query
 *
 * @v resolv		Name resolution interface
 * @v name		Name to resolve
 * @v sa		Socket address to fill in
 * @ret rc		Return status code
 */
int create_bootinfo_query ( struct interface * job, const char *hostname ) {

	struct bootinfo_request *bootinfo;
	struct sockaddr_tcpip server;
	struct sockaddr_tcpip local;
	int rc;

	printf ("Bootinfo query for host %s\n", hostname);

	/* Allocate DNS structure */
	bootinfo = zalloc ( sizeof ( *bootinfo ) );
	if ( ! bootinfo ) {
		rc = -ENOMEM;
		return rc;
	}

	printf ("create_bootinfo_query 1 v 0014\n");

	ref_init ( &bootinfo->refcnt, NULL );
	intf_init ( &bootinfo->request, &bootinfo_request_desc, &bootinfo->refcnt );
	intf_init ( &bootinfo->socket, &bootinfo_socket_desc, &bootinfo->refcnt );
	timer_init ( &bootinfo->timer, bootinfo_timer_expired, &bootinfo->refcnt );

	printf ("create_bootinfo_query 2\n");

	memset ( &server, 0, sizeof ( server ) );
	server.st_port = htons ( BOOTWHAT_DSTPORT );

	memset ( &local, 0, sizeof ( local ) );
	local.st_port = htons ( BOOTWHAT_SRCPORT );

	/* Open UDP connection */
	if ( ( rc = xfer_open_named_socket ( &bootinfo->socket, SOCK_DGRAM,
					     ( struct sockaddr * ) &server,
					     hostname,
					     ( struct sockaddr * ) &local ) ) != 0 ) {
		printf ( "Could not open bootinfo socket on %s\n", hostname);
		DBGC ( bootinfo, "bootinfo %p could not open socket: %s\n",
		       bootinfo, strerror ( rc ) );
		return rc;
	}

	printf ("create_bootinfo_query 3\n");
	/* Start timer to trigger first packet */
	start_timer_nodelay ( &bootinfo->timer );

	/* Attach parent interface, mortalise self, and return */
	intf_plug_plug ( &bootinfo->request, job );
	ref_put ( &bootinfo->refcnt );
	printf ("create_bootinfo_query 4\n");
	return 0;	
}
