/*
 * Copyright (c) 2000-2015 New Mexico Consortium
 * 
 * {{{EMULAB-LICENSE
 * 
 * This file is part of the Emulab network testbed software.
 * 
 * This file is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at
 * your option) any later version.
 * 
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this file.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * }}}
 *
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
#include <ipxe/settings.h>
#include <emulab/event.h>

/** @file
 *
 * Emulab event support
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
// emulab/event.c:167:5: error: ‘missing_errfile_declaration’ is deprecated (declared at include/errno.h:120) [-Werror=deprecated-declarations]
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

#define MAX_EVENT_STATE_SIZE 256
#define TMCD_PORT 7777

/** A event to send */
struct event_request {
	/** Reference counter */
	struct refcnt refcnt;
	/** Send request interface */
	struct interface request;
	/** Data transfer interface */
	struct interface socket;
	/** Retry timer */
	struct retry_timer timer;

	char state[MAX_EVENT_STATE_SIZE];
};

int event ( const char *dst_server,  char *state) {
	int rc;

	if ( ( rc = create_event ( &monojob, dst_server, state ) ) != 0 ) {
		printf ( "Could not create event request: %s\n", strerror ( rc ) );
		return rc;
	}

	/* Wait for bootinfo to complete */
	if ( ( rc = monojob_wait ( NULL, 0 ) ) != 0 ) {
		printf ( "event finished: %s\n", strerror ( rc ) );
		return rc;
	}

	return 0;
}

/**
 * Mark event request as complete
 *
 * @v event		Event request
 * @v rc		Return status code
 */
static void event_done ( struct event_request *event, int rc ) {
	/* Stop the retry timer */
	stop_timer ( &event->timer );

	/* Shut down interfaces */
	intf_shutdown ( &event->socket, rc );
	intf_shutdown ( &event->request, rc );
}

/**
 * Send event request
 *
 * @v event		Event request
 * @ret rc		Return status code
 */
static int event_send_packet ( struct event_request *event ) {
	/* Start retransmission timer */
	start_timer ( &event->timer );
	/* Send the data */
	return xfer_deliver_raw ( &event->socket, event->state, strlen ( event->state ) );
}

/**
 * Handle received data
 *
 * @v event		Event
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int event_xfer_deliver ( struct event_request *event,
				   struct io_buffer *iobuf __unused,
				   struct xfer_metadata *meta __unused ) {
	int rc = 0;
	event_done(event, rc);
	return 0;
}

/**
 * Handle event retransmission timer expiry
 *
 * @v timer		Retry timer
 * @v fail		Failure indicator
 */
static void event_timer_expired ( struct retry_timer *timer, int fail ) {
	struct event_request *event =
		container_of ( timer, struct event_request, timer );

	if ( fail ) {
		event_done ( event, -ETIMEDOUT );
	} else {
		event_send_packet ( event );
	}
}

/**
 * Receive new data
 *
 * @v event		Event request
 * @v rc		Reason for close
 */
static void event_xfer_close ( struct event_request *event, int rc) {
	if ( rc ) {
		printf ("yo\n");
		rc = -ECONNABORTED;
	}

	event_done ( event, rc );
}

/** event socket interface operations */
static struct interface_operation event_socket_operations[] = {
	INTF_OP ( xfer_deliver, struct event_request *, event_xfer_deliver ),
	INTF_OP ( intf_close, struct event_request *, event_xfer_close ),
};

/** event socket interface descriptor */
static struct interface_descriptor event_socket_desc =
	INTF_DESC ( struct event_request, socket, event_socket_operations );

/** event request interface operations */
static struct interface_operation event_request_op[] = {
	INTF_OP ( intf_close, struct event_request *, event_done ),
};

/** event request interface descriptor */
static struct interface_descriptor event_request_desc =
	INTF_DESC ( struct event_request, request, event_request_op );

/**
 * event query
 *
 * @v resolv		Name resolution interface
 * @v name		Name to resolve
 * @v sa		Socket address to fill in
 * @ret rc		Return status code
 */
int create_event ( struct interface * job, const char *dst_server,
		   char *state) {

	struct event_request *event;
	struct sockaddr_tcpip server;
	int rc;

	printf ("Sending event %s to %s\n", state, dst_server);

	/* Allocate event structure */
	event = zalloc ( sizeof ( *event ) );
	if ( ! event ) {
		rc = -ENOMEM;
		return rc;
	}

	ref_init ( &event->refcnt, NULL );
	intf_init ( &event->request, &event_request_desc, &event->refcnt );
	intf_init ( &event->socket, &event_socket_desc, &event->refcnt );
	timer_init ( &event->timer, event_timer_expired, &event->refcnt );

	// Set the state payload. One might be tempted to use vsnprintf
	// to build the payload, but for some reason it looks like the %s
	// at the end of the format string is ignored.
	// 
	// Instead, we use vsnprintf to set the header and strncpy to 
	// concatenate the state onto the end of the payload
	memset(event->state, 0, MAX_EVENT_STATE_SIZE);
	vsnprintf(event->state, MAX_EVENT_STATE_SIZE, "VERSION=39 state ", state);
	size_t offset = strlen(event->state);
	strncpy ( event->state + offset, state, MAX_EVENT_STATE_SIZE - offset );

	memset ( &server, 0, sizeof ( server ) );
	server.st_port = htons ( TMCD_PORT );

	/* Open UDP connection */
	if ( ( rc = xfer_open_named_socket ( &event->socket, SOCK_STREAM,
					     ( struct sockaddr * ) &server,
					     dst_server, NULL ) ) != 0 ) {
		printf ( "Could not open event socket on %s\n", dst_server);
		DBGC ( event, "event %p could not open socket: %s\n",
		       event, strerror ( rc ) );
		return rc;
	}

	/* Start timer to trigger first packet */
	start_timer_nodelay ( &event->timer );

	/* Attach parent interface, mortalise self, and return */
	intf_plug_plug ( &event->request, job );
	ref_put ( &event->refcnt );

	return 0;	
}

