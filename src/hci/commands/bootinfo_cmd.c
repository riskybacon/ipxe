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
#include <emulab/bootinfo.h>
#include <getopt.h>

FILE_LICENCE ( GPL2_OR_LATER_OR_UBDL );

/** @file
 *
 * Bootinfo commands
 *
 */

/** "bootinfo" options */
struct bootinfo_options {};

/** "bootinfo" option list */
static struct option_descriptor bootinfo_opts[] = {};

/** "bootinfo" command descriptor */
static struct command_descriptor bootinfo_cmd =
	COMMAND_DESC ( struct bootinfo_options, bootinfo_opts, 1, 1, "hostname" );

int bootinfo(const char*);

/**
 * "bootinfo" command
 *
 * @v argc		Argument count
 * @v argv		Argument list
 * @ret rc		Return status code
 */
static int bootinfo_exec ( int argc, char **argv ) {
	struct bootinfo_options opts;
	int rc;
        char *hostname;

	/* Parse options */
	if ( ( rc = parse_options ( argc, argv, &bootinfo_cmd, &opts ) ) != 0 )
		return rc;

        /* Parse hostname */
        hostname = argv[optind];

        /* Bootinfo */
        if ( ( rc = bootinfo ( hostname ) ) != 0 )
                return rc;

	return 0;
}

/** Bootinfo commands */
struct command bootinfo_command __command = {
	.name = "bootinfo",
	.exec = bootinfo_exec,
};


