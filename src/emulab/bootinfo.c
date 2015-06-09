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
#include <ipxe/refcnt.h>
#include <ipxe/interface.h>
#include <ipxe/job.h>
#include <ipxe/xfer.h>
#include <ipxe/iobuf.h>
#include <ipxe/open.h>
#include <ipxe/socket.h>
#include <ipxe/retry.h>
#include <ipxe/monojob.h>
#include <emulab/bootinfo.h>

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

static int EPROTO_DATA = 1;
static int ETIMEDOUT = 24;
static int EPROTO_LEN = 25;
static int ENOMEM = 23;
static int EINVAL = 26;
static int EPROTO_SEQ = 26;

/*
static int EPROTOLENT = 24;
static int EPROTOLENT = 24;
*/

/** A bootinfo */
struct bootinfo {
	/** Reference count */
	struct refcnt refcnt;

	/** Job control interface */
	struct interface job;
	/** Data transfer interface */
	struct interface xfer;

	/** Timer */
	struct retry_timer timer;
	/** Timeout */
	unsigned long timeout;

	/** Payload length */
	size_t len;
	/** Current sequence number */
	uint16_t sequence;
	/** Response for current sequence number is still pending */
	int pending;
	/** Number of remaining expiry events (zero to continue indefinitely) */
	unsigned int remaining;
	/** Return status */
	int rc;

	/** Callback function
	 *
	 * @v src		Source socket address, or NULL
	 * @v sequence		Sequence number
	 * @v len		Payload length
	 * @v rc		Status code
	 */
	void ( * callback ) ( struct sockaddr *src, unsigned int sequence,
			      size_t len, int rc );
};

/**
 * Display bootinfo result
 *
 * @v src		Source socket address, or NULL
 * @v sequence		Sequence number
 * @v len		Payload length
 * @v rc		Status code
 */
static void bootinfo_callback ( struct sockaddr *peer, unsigned int sequence,
			    size_t len, int rc ) {

	/* Display ping response */
	printf ( "%zd bytes from %s: seq=%d",
		 len, ( peer ? sock_ntoa ( peer ) : "<none>" ), sequence );
	if ( rc != 0 )
		printf ( ": %s", strerror ( rc ) );
	printf ( "\n" );
}

int bootinfo (const char * hostname) {
	int rc;
	int quiet = 0;
	unsigned long timeout = 10;
	size_t len = 56;
	unsigned int count = 4;

	printf ("bootinfo 1\n");
	/* Create bootinfo job */
	if ( ( rc = create_bootinfo ( &monojob, hostname, timeout, len, count,
				    ( quiet ? NULL : bootinfo_callback ) ) ) != 0 ){
		printf ( "Could not start bootinfo: %s\n", strerror ( rc ) );
		return rc;
	}

	/* Wait for ping to complete */
	if ( ( rc = monojob_wait ( NULL, 0 ) ) != 0 ) {
		if ( ! quiet )
			printf ( "Finished: %s\n", strerror ( rc ) );
		return rc;
	}

	return 0;

  return 0;
}

/**
 * Generate payload
 *
 * @v bootinfo		Bootinfo
 * @v data		Data buffer
 */
static void bootinfo_generate ( struct bootinfo *bootinfo, void *data ) {
	uint8_t *bytes = data;
	unsigned int i;

	/* Generate byte sequence */
	for ( i = 0 ; i < bootinfo->len ; i++ )
		bytes[i] = ( i & 0xff );
}

/**
 * Verify payload
 *
 * @v bootinfo		Bootinfo
 * @v data		Data buffer
 * @ret rc		Return status code
 */
static int bootinfo_verify ( struct bootinfo *bootinfo, const void *data ) {
	const uint8_t *bytes = data;
	unsigned int i;

	/* Check byte sequence */
	for ( i = 0 ; i < bootinfo->len ; i++ ) {
		if ( bytes[i] != ( i & 0xff ) )
			return -EPROTO_DATA;
	}

	return 0;
}

/**
 * Close bootinfo
 *
 * @v bootinfo		Bootinfo
 * @v rc		Reason for close
 */
static void bootinfo_close ( struct bootinfo *bootinfo, int rc ) {

	/* Stop timer */
	stop_timer ( &bootinfo->timer );

	/* Shut down interfaces */
	intf_shutdown ( &bootinfo->xfer, rc );
	intf_shutdown ( &bootinfo->job, rc );
}

/**
 * Handle data transfer window change
 *
 * @v bootinfo		Bootinfo
 */
static void bootinfo_window_changed ( struct bootinfo *bootinfo ) {

	/* Do nothing if timer is already running */
	if ( timer_running ( &bootinfo->timer ) )
		return;

	/* Start timer when window opens for the first time */
	if ( xfer_window ( &bootinfo->xfer ) )
		start_timer_nodelay ( &bootinfo->timer );
}

/**
 * Handle timer expiry
 *
 * @v timer		Timer
 * @v over		Failure indicator
 */
static void bootinfo_expired ( struct retry_timer *timer, int over __unused ) {
	struct bootinfo *bootinfo = container_of ( timer, struct bootinfo, timer );
	struct xfer_metadata meta;
	struct io_buffer *iobuf;
	int rc;

	/* If no response has been received, notify the callback function */
	if ( bootinfo->pending && bootinfo->callback )
		bootinfo->callback ( NULL, bootinfo->sequence, 0, -ETIMEDOUT );

	/* Check for termination */
	if ( bootinfo->remaining && ( --bootinfo->remaining == 0 ) ) {
		bootinfo_close ( bootinfo, bootinfo->rc );
		return;
	}

	/* Increase sequence number */
	bootinfo->sequence++;

	/* Restart timer.  Do this before attempting to transmit, in
	 * case the transmission attempt fails.
	 */
	start_timer_fixed ( &bootinfo->timer, bootinfo->timeout );
	bootinfo->pending = 1;

	/* Allocate I/O buffer */
	iobuf = xfer_alloc_iob ( &bootinfo->xfer, bootinfo->len );
	if ( ! iobuf ) {
		DBGC ( bootinfo, "BOOTINFO %p could not allocate I/O buffer\n",
		       bootinfo );
		return;
	}

	/* Generate payload */
	bootinfo_generate ( bootinfo, iob_put ( iobuf, bootinfo->len ) );

	/* Generate metadata */
	memset ( &meta, 0, sizeof ( meta ) );
	meta.flags = XFER_FL_ABS_OFFSET;
	meta.offset = bootinfo->sequence;

	/* Transmit packet */
	if ( ( rc = xfer_deliver ( &bootinfo->xfer, iobuf, &meta ) ) != 0 ) {
		DBGC ( bootinfo, "BOOTINFO %p could not transmit: %s\n",
		       bootinfo, strerror ( rc ) );
		return;
	}
}

/**
 * Handle received data
 *
 * @v bootinfo		Bootinfo
 * @v iobuf		I/O buffer
 * @v meta		Data transfer metadata
 * @ret rc		Return status code
 */
static int bootinfo_deliver ( struct bootinfo *bootinfo, struct io_buffer *iobuf,
			    struct xfer_metadata *meta ) {
	size_t len = iob_len ( iobuf );
	uint16_t sequence = meta->offset;
	int terminate = 0;
	int rc;

	/* Clear response pending flag, if applicable */
	if ( sequence == bootinfo->sequence )
		bootinfo->pending = 0;

	/* Check for errors */
	if ( len != bootinfo->len ) {
		/* Incorrect length: terminate immediately if we are
		 * not pinging indefinitely.
		 */
		DBGC ( bootinfo, "BOOTINFO %p received incorrect length %zd "
		       "(expected %zd)\n", bootinfo, len, bootinfo->len );
		rc = -EPROTO_LEN;
		terminate = ( bootinfo->remaining != 0 );
	} else if ( ( rc = bootinfo_verify ( bootinfo, iobuf->data ) ) != 0 ) {
		/* Incorrect data: terminate immediately if we are not
		 * pinging indefinitely.
		 */
		DBGC ( bootinfo, "BOOTINFO %p received incorrect data:\n", bootinfo );
		DBGC_HDA ( bootinfo, 0, iobuf->data, iob_len ( iobuf ) );
		terminate = ( bootinfo->remaining != 0 );
	} else if ( sequence != bootinfo->sequence ) {
		/* Incorrect sequence number (probably a delayed response):
		 * report via callback but otherwise ignore.
		 */
		DBGC ( bootinfo, "BOOTINFO %p received sequence %d (expected %d)\n",
		       bootinfo, sequence, bootinfo->sequence );
		rc = -EPROTO_SEQ;
		terminate = 0;
	} else {
		/* Success: record that a packet was successfully received,
		 * and terminate if we expect to send no further packets.
		 */
		rc = 0;
		bootinfo->rc = 0;
		terminate = ( bootinfo->remaining == 1 );
	}

	/* Discard I/O buffer */
	free_iob ( iobuf );

	/* Notify callback function, if applicable */
	if ( bootinfo->callback )
		bootinfo->callback ( meta->src, sequence, len, rc );

	/* Terminate if applicable */
	if ( terminate )
		bootinfo_close ( bootinfo, rc );

	return rc;
}

/** Bootinfo data transfer interface operations */
static struct interface_operation bootinfo_xfer_op[] = {
	INTF_OP ( xfer_deliver, struct bootinfo *, bootinfo_deliver ),
	INTF_OP ( xfer_window_changed, struct bootinfo *, bootinfo_window_changed ),
	INTF_OP ( intf_close, struct bootinfo *, bootinfo_close ),
};

/** Bootinfo data transfer interface descriptor */
static struct interface_descriptor bootinfo_xfer_desc =
	INTF_DESC ( struct bootinfo, xfer, bootinfo_xfer_op );

/** Bootinfo job control interface operations */
static struct interface_operation bootinfo_job_op[] = {
	INTF_OP ( intf_close, struct bootinfo *, bootinfo_close ),
};

/** Bootinfo job control interface descriptor */
static struct interface_descriptor bootinfo_job_desc =
	INTF_DESC ( struct bootinfo, job, bootinfo_job_op );

/**
 * Create bootinfo
 *
 * @v job		Job control interface
 * @v hostname		Hostname to ping
 * @v timeout		Timeout (in ticks)
 * @v len		Payload length
 * @v count		Number of packets to send (or zero for no limit)
 * @v callback		Callback function (or NULL)
 * @ret rc		Return status code
 */
int create_bootinfo ( struct interface *job, const char *hostname,
		    unsigned long timeout, size_t len, unsigned int count,
		    void ( * callback ) ( struct sockaddr *src,
					  unsigned int sequence, size_t len,
					  int rc ) ) {
	struct bootinfo *bootinfo;
	int rc;

	printf ("create_bootinfo 1\n");

	/* Sanity check */
	if ( ! timeout )
		return -EINVAL;

	printf ("create_bootinfo 2\n");

	/* Allocate and initialise structure */
	bootinfo = zalloc ( sizeof ( *bootinfo ) );
	if ( ! bootinfo ) {
		return -ENOMEM;
	}

	printf ("create_bootinfo 3\n");

	ref_init ( &bootinfo->refcnt, NULL );
	intf_init ( &bootinfo->job, &bootinfo_job_desc, &bootinfo->refcnt );
	intf_init ( &bootinfo->xfer, &bootinfo_xfer_desc, &bootinfo->refcnt );
	timer_init ( &bootinfo->timer, bootinfo_expired, &bootinfo->refcnt );
	bootinfo->timeout = timeout;
	bootinfo->len = len;
	bootinfo->remaining = ( count ? ( count + 1 /* Initial packet */ ) : 0 );
	bootinfo->callback = callback;
	bootinfo->rc = -ETIMEDOUT;

	printf ("create_bootinfo 4\n");

	/* Open socket */
	if ( ( rc = xfer_open_named_socket ( &bootinfo->xfer, SOCK_ECHO, NULL,
					     hostname, NULL ) ) != 0 ) {
		DBGC ( bootinfo, "BOOTINFO %p could not open socket: %s\n",
		       bootinfo, strerror ( rc ) );
		goto err;
	}

	printf ("create_bootinfo 5\n");

	/* Attach parent interface, mortalise self, and return */
	intf_plug_plug ( &bootinfo->job, job );
	ref_put ( &bootinfo->refcnt );
	return 0;

 err:
	printf ("create_bootinfo 6\n");

	bootinfo_close ( bootinfo, rc );
	ref_put ( &bootinfo->refcnt );
	return rc;
}

