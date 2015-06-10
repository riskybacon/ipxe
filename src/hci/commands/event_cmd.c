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
 */

#include <string.h>
#include <stdio.h>
#include <ipxe/command.h>
#include <ipxe/parseopt.h>
#include <emulab/event.h>
#include <getopt.h>

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Emulab event sending commands
 *
 */

/** event options */
struct event_options {};

/** event option list */
static struct option_descriptor event_opts[] = {};

/** event command descriptor */
static struct command_descriptor event_cmd =
	COMMAND_DESC ( struct event_options, event_opts, 2, 2, "<dest server> <state>" );


/**
 * event command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int event_exec ( int argc, char **argv ) {
	struct event_options opts;
	int rc;
        char *dst_server;
	char *state;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &event_cmd, &opts ) ) != 0 )
		return rc;

        dst_server = argv[optind];
	state = argv[ optind + 1 ];

        /* Bootinfo */
        if ( ( rc = event ( dst_server, state ) ) != 0 )
                return rc;

	return 0;
}

/** event commands */
struct command event_command __command = {
	.name = "event",
	.exec = event_exec,
};

