/* SPDX-License-Identifier: ISC */
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#include "command.h"

extern char *__progname;

static void pretty_print(const char *str, int padd, int len)
{
	int i, brk;
next:
	while (isspace(*str) && *str != '\n')
		++str;

	if (!(*str))
		return;

	if (*str == '\n') {
		fputc('\n', stdout);
		++str;
		len = padd;
		goto next;
	}

	for (i = 0, brk = 0; str[i]; ++i) {
		if (str[i] == '<' || str[i] == '[')
			++brk;
		if (str[i] == '>' || str[i] == ']')
			--brk;
		if (!brk && isspace(str[i]))
			break;
	}

	if ((len + i) < 80) {
		fwrite(str, 1, i, stdout);
		str += i;
		len += i;

		if ((len + 1) < 80) {
			fputc(' ', stdout);
			++len;
			goto next;
		}
	}

	printf("\n%*s", padd, "");
	len = padd;
	goto next;
}

static void print_cmd_usage(const char *cmd, const char *usage)
{
	int padd;

	padd = printf("Usage: %s %s ", __progname, cmd);

	if ((strlen(usage) + padd) < 80) {
		fputs(usage, stdout);
		return;
	}

	pretty_print(usage, padd, padd);
}

static int cmd_help(int argc, char **argv)
{
	const char *help;
	command_t *cmd;

	if (argc < 2)
		usage(EXIT_SUCCESS);

	if (argc > 2) {
		fprintf(stderr, "Too many arguments\n\n"
				"Usage: %s help <command>", __progname);
		return EXIT_FAILURE;
	}

	cmd = command_by_name(argv[1]);

	if (cmd == NULL) {
		fprintf(stderr, "Unknown command '%s'\n\n"
			"Try `%s help' for a list of available commands\n",
			argv[1], __progname);
		return EXIT_FAILURE;
	}

	print_cmd_usage(cmd->cmd, cmd->usage);
	fputs("\n\n", stdout);

	if (cmd->l_desc != NULL) {
		fputs(cmd->l_desc, stdout);
	} else {
		help = cmd->s_desc;

		if (islower(*help)) {
			fputc(toupper(*(help++)), stdout);
			pretty_print(help, 0, 1);
		} else {
			pretty_print(help, 0, 0);
		}
	}

	fputc('\n', stdout);
	return EXIT_SUCCESS;
}

static command_t help = {
	.cmd = "help",
	.usage = "<command>",
	.s_desc = "print a help text for a command",
	.l_desc =
"Print a help text for a specified command. If no command is specified,\n"
"a generic help text and a list of available commands is shown.\n",
	.run_cmd = cmd_help,
};

REGISTER_COMMAND(help)
