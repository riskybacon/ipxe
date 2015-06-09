#ifndef _EMULAB_BOOTINFO_H
#define _EMULAB_BOOTINFO_H


/** @file
 *
 * Emulab BOOTINFO query
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/interface.h>
#include <ipxe/socket.h>

extern int create_bootinfo ( struct interface *job, const char *hostname,
			     unsigned long timeout, size_t len,
			     unsigned int count,
			     void ( * callback ) ( struct sockaddr *peer,
						   unsigned int sequence,
						   size_t len,
						   int rc ) );

extern int bootinfo ( const char *hostname );

#endif /* _EMULAB_BOOTINFO_H */

