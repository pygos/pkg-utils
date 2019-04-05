/* SPDX-License-Identifier: ISC */
#include "pkg2sqfs.h"

static struct option long_opts[] = {
	{ "block-size", required_argument, NULL, 'b' },
	{ "force", no_argument, NULL, 'f' },
	{ "version", no_argument, NULL, 'V' },
	{ "help", no_argument, NULL, 'h' },
};

static const char *short_opts = "b:fhV";

extern char *__progname;

static const char *version_string =
"%s (Pygos %s) %s\n"
"Copyright (c) 2019 David Oberhollenzer\n"
"License ISC <https://opensource.org/licenses/ISC>\n"
"This is free software: you are free to change and redistribute it.\n"
"There is NO WARRANTY, to the extent permitted by law.\n"
"\n"
"Written by David Oberhollenzer.\n";

static const char *help_string =
"Usage: %s [OPTIONS] <package-file> <squashfs-file>\n"
"\n"
"Convert a package file into a squashfs image.\n"
"\n"
"Possible options:\n"
"\n"
"  --block-size, -b <size>  Block size to use for Squashfs image.\n"
"                           Defaults to %u.\n"
"  --force, -f              Overwrite the output file if it already exists.\n"
"  --help, -h               Print help text and exit.\n"
"  --version, -V            Print version information and exit.\n"
"\n";

static void print_tree(int level, node_t *n)
{
	int i;

	for (i = 0; i < (level - 1); ++i)
		fputs("|  ", stdout);

	switch (n->mode & S_IFMT) {
	case S_IFDIR:
		printf("%s%s/ (%u, %o)\n", level ? "+- " : "", n->name,
		       n->inode_num, (unsigned int)n->mode);

		n = n->data.dir->children;
		++level;

		while (n != NULL) {
			print_tree(level, n);
			n = n->next;
		}
		break;
	case S_IFLNK:
		printf("+- %s (%u) -> %s\n", n->name, n->inode_num,
		       n->data.symlink);
		break;
	case S_IFBLK:
		printf("+- %s (%u) b %lu\n", n->name, n->inode_num,
		       n->data.device);
		break;
	case S_IFCHR:
		printf("+- %s (%u) c %lu\n", n->name, n->inode_num,
		       n->data.device);
		break;
	default:
		printf("+- %s (%u)\n", n->name, n->inode_num);
		break;
	}
}

int main(int argc, char **argv)
{
	uint32_t blocksize = SQFS_DEFAULT_BLOCK_SIZE, timestamp = 0;
	int i, outmode = O_WRONLY | O_CREAT | O_EXCL;
	const char *infile, *outfile, *errstr;
	int status = EXIT_FAILURE;
	sqfs_info_t info;

	memset(&info, 0, sizeof(info));

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'b':
			blocksize = strtonum(optarg, 1024, 0xFFFFFFFF, &errstr);
			if (errstr != NULL) {
				fprintf(stderr, "Block size '%s': %s\n",
					optarg, errstr);
				return EXIT_FAILURE;
			}
			break;
		case 'f':
			outmode = O_WRONLY | O_CREAT | O_TRUNC;
			break;
		case 'h':
			printf(help_string, __progname,
			       SQFS_DEFAULT_BLOCK_SIZE);
			exit(EXIT_SUCCESS);
		case 'V':
			printf(version_string, __progname,
			       PACKAGE_NAME, PACKAGE_VERSION);
			exit(EXIT_SUCCESS);
		default:
			goto fail_arg;
		}
	}

	if ((optind + 1) >= argc) {
		fputs("Missing arguments: input and output files.\n", stderr);
		goto fail_arg;
	}

	infile = argv[optind++];
	outfile = argv[optind++];

	info.rd = pkg_reader_open(infile);
	if (info.rd == NULL)
		return EXIT_FAILURE;

	info.outfd = open(outfile, outmode, 0644);
	if (info.outfd < 0) {
		perror(outfile);
		goto out_pkg_close;
	}

	if (sqfs_super_init(&info.super, timestamp, blocksize, SQFS_COMP_GZIP))
		goto out_close;

	info.block = malloc(info.super.block_size * 3);
	if (info.block == NULL) {
		perror("malloc");
		goto out_close;
	}

	info.scratch = (char *)info.block + info.super.block_size;
	info.fragment = (char *)info.block + 2 * info.super.block_size;

	info.fs.default_uid = 0;
	info.fs.default_gid = 0;
	info.fs.default_mode = 0755;

	if (create_vfs_tree(&info))
		goto out_buffer;

	print_tree(0, info.fs.root);

	if (sqfs_super_write(&info))
		goto out_tree;

	if (pkg_data_to_sqfs(&info))
		goto out_fragments;

	free(info.block);
	info.block = NULL;

	if (sqfs_write_inodes(&info))
		goto out_fragments;

	if (sqfs_write_fragment_table(&info))
		goto out_fragments;

	if (sqfs_write_ids(&info))
		goto out_fragments;

	if (sqfs_super_write(&info))
		goto out_fragments;

	status = EXIT_SUCCESS;
out_fragments:
	free(info.fragments);
out_tree:
	destroy_vfs_tree(&info.fs);
out_buffer:
	free(info.block);
out_close:
	close(info.outfd);
out_pkg_close:
	pkg_reader_close(info.rd);
	return status;
fail_arg:
	fprintf(stderr, "Try `%s --help' for more information.\n", __progname);
	return EXIT_FAILURE;
}
