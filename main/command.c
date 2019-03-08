/* SPDX-License-Identifier: ISC */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "command.h"

extern char *__progname;

static command_t *commands;

void command_register(command_t *cmd)
{
	cmd->next = commands;
	commands = cmd;
}

command_t *command_by_name(const char *name)
{
	command_t *cmd;

	for (cmd = commands; cmd != NULL; cmd = cmd->next) {
		if (strcmp(cmd->cmd, name) == 0)
			break;
	}

	return cmd;
}

void usage(int status)
{
	FILE *stream = (status == EXIT_SUCCESS ? stdout : stderr);
	int padd = 0, len;
	command_t *cmd;

	fprintf(stream, "usage: %s <command> [args...]\n\n"
			"Available commands:\n\n", __progname);

	for (cmd = commands; cmd != NULL; cmd = cmd->next) {
		len = strlen(cmd->cmd);

		padd = len > padd ? len : padd;
	}

	for (cmd = commands; cmd != NULL; cmd = cmd->next) {
		fprintf(stream, "%*s - %s\n",
			(int)padd + 1, cmd->cmd, cmd->s_desc);
	}

	fprintf(stream, "\nTry `%s help <command>' for more information "
			"on a specific command\n", __progname);
	exit(status);
}

void tell_read_help(const char *cmd)
{
	fprintf(stderr, "Try `%s help %s' for more information.\n",
		__progname, cmd);
}

int check_arguments(const char *cmd, int argc, int minc, int maxc)
{
	if (argc >= minc && argc <= maxc)
		return 0;

	fprintf(stderr, "Too %s arguments for `%s'\n",
			argc > maxc ? "many" : "few", cmd);
	tell_read_help(cmd);
	return -1;
}
