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

// The maximum length for the bootwhat_type setting
#define MAX_BOOTWHAT_SETTING 256

extern int create_bootinfo_query ( struct interface *job, const char *hostname );
extern int bootinfo ( const char *hostname );

#endif /* _EMULAB_BOOTINFO_H */

