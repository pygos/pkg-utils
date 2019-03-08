/* SPDX-License-Identifier: ISC */
#ifndef COMMAND_H
#define COMMAND_H

typedef struct command_t {
	struct command_t *next;

	const char *cmd;	/* command name */
	const char *usage;	/* list of possible arguments */
	const char *s_desc;	/* short description used by help */
	const char *l_desc;	/* long description used by help */

	int (*run_cmd)(int argc, char **argv);
} command_t;

void command_register(command_t *cmd);

command_t *command_by_name(const char *name);

void __attribute__((noreturn)) usage(int status);

void tell_read_help(const char *cmd);

int check_arguments(const char *cmd, int argc, int minc, int maxc);

#define REGISTER_COMMAND(cmd) \
	static void __attribute__((constructor)) register_##cmd(void) \
	{ \
		command_register((command_t *)&cmd); \
	}

#endif /* COMMAND_H */
