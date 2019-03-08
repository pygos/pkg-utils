/* SPDX-License-Identifier: ISC */
#include <stdlib.h>
#include <stdio.h>

#include "command.h"

int main(int argc, char **argv)
{
	command_t *cmd;

	if (argc < 2)
		usage(EXIT_SUCCESS);

	cmd = command_by_name(argv[1]);

	if (cmd == NULL) {
		fprintf(stderr, "Unknown command '%s'\n\n", argv[1]);
		usage(EXIT_FAILURE);
	}

	return cmd->run_cmd(argc - 1, argv + 1);
}
