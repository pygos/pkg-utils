/* SPDX-License-Identifier: ISC */
#include "pack.h"

static const struct option long_opts[] = {
	{ "description", required_argument, NULL, 'd' },
	{ "file-list", required_argument, NULL, 'l' },
	{ "repo-dir", required_argument, NULL, 'r' },
	{ "force", no_argument, NULL, 'f' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "l:r:d:f";

static pkg_writer_t *open_writer(pkg_desc_t *desc, const char *repodir,
				 bool force)
{
	char *path;

	if (mkdir_p(repodir))
		return NULL;

	path = alloca(strlen(repodir) + strlen(desc->name) + 16);
	sprintf(path, "%s/%s.pkg", repodir, desc->name);

	return pkg_writer_open(path, force);
}

static int cmd_pack(int argc, char **argv)
{
	const char *filelist = NULL, *repodir = NULL, *descfile = NULL;
	image_entry_t *list = NULL;
	bool force = false;
	pkg_writer_t *wr;
	pkg_desc_t desc;
	int i;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'l':
			filelist = optarg;
			break;
		case 'd':
			descfile = optarg;
			break;
		case 'r':
			repodir = optarg;
			break;
		case 'f':
			force = true;
			break;
		default:
			tell_read_help(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (descfile == NULL) {
		fputs("missing argument: package description file\n", stderr);
		tell_read_help(argv[0]);
		return EXIT_FAILURE;
	}

	if (repodir == NULL) {
		repodir = REPODIR;
	}

	if (optind < argc)
		fputs("warning: ignoring extra arguments\n", stderr);

	if (desc_read(descfile, &desc))
		return EXIT_FAILURE;

	if (filelist != NULL && filelist_read(filelist, &list))
		goto fail_desc;

	wr = open_writer(&desc, repodir, force);
	if (wr == NULL)
		goto fail_fp;

	if (write_header_data(wr, &desc))
		goto fail;

	if (list != NULL) {
		if (write_toc(wr, list, desc.toccmp))
			goto fail;

		if (write_files(wr, list, desc.datacmp))
			goto fail;
	}

	pkg_writer_close(wr);
	image_entry_free_list(list);
	desc_free(&desc);
	return EXIT_SUCCESS;
fail:
	pkg_writer_close(wr);
fail_fp:
	image_entry_free_list(list);
fail_desc:
	desc_free(&desc);
	return EXIT_FAILURE;
}

static command_t pack = {
	.cmd = "pack",
	.usage = "OPTIONS...",
	.s_desc = "generate a package file",
	.l_desc =
"Read a list of files from the given lists file and generate a package.\n"
"Possible options:\n"
"  --file-list, -l <path>   Specify a file containing a list of input files.\n"
"  --repo-dir, -r <path>    Specify the output repository path to store the\n"
"                           package in.\n"
"                           If not set, defaults to " REPODIR ".\n"
"  --description, -d <path> Specify a file containing a description of the\n"
"                           package, including information such as package\n"
"                           dependencies, the actual package name, etc.\n"
"  --force, -f              If a package with the same name already exists,\n"
"                           overwrite it.\n"
"\n",
	.run_cmd = cmd_pack,
};

REGISTER_COMMAND(pack)
