#ifndef _EMULAB_EVENT_H
#define _EMULAB_EVENT_H


/** @file
 *
 * Emulab event state send
 *
 */

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

#include <stdint.h>
#include <ipxe/interface.h>
#include <ipxe/socket.h>


int create_event ( struct interface *job, const char *dst_server, char *state);
extern int event ( const char *hostname, char *state );

#endif /* _EMULAB_EVENT_H */

