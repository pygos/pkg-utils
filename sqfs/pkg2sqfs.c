/* SPDX-License-Identifier: ISC */
#include "pkg2sqfs.h"

static struct option long_opts[] = {
	{ "timestamp", required_argument, NULL, 't' },
	{ "compressor", required_argument, NULL, 'c' },
	{ "block-size", required_argument, NULL, 'b' },
	{ "dev-block-size", required_argument, NULL, 'B' },
	{ "default-uid", required_argument, NULL, 'u' },
	{ "default-gid", required_argument, NULL, 'g' },
	{ "default-mode", required_argument, NULL, 'm' },
	{ "force", no_argument, NULL, 'f' },
	{ "version", no_argument, NULL, 'V' },
	{ "help", no_argument, NULL, 'h' },
};

static const char *short_opts = "t:c:b:B:u:g:m:fhV";

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
"  --compressor, -c <name>     Select the compressor to use.\n"
"                              directories (defaults to 'xz').\n"
"  --timestamp, -t <value>     Set timestamp for all inodes and image file.\n"
"                              Defaults to 0 (Jan 1st, 1970).\n"
"  --block-size, -b <size>     Block size to use for Squashfs image.\n"
"                              Defaults to %u.\n"
"  --dev-block-size, -B <size> Device block size to padd the image to.\n"
"                              Defaults to %u.\n"
"  --default-uid, -u <valie>   Default user ID for implicitly created\n"
"                              directories (defaults to 0).\n"
"  --default-gid, -g <value>   Default group ID for implicitly created\n"
"                              directories (defaults to 0).\n"
"  --default-mode, -m <value>  Default permissions for implicitly created\n"
"                              directories (defaults to 0755).\n"
"  --force, -f                 Overwrite the output file if it exists.\n"
"  --help, -h                  Print help text and exit.\n"
"  --version, -V               Print version information and exit.\n"
"\n";

static const char *compressors[] = {
	[SQFS_COMP_GZIP] = "gzip",
	[SQFS_COMP_LZMA] = "lzma",
	[SQFS_COMP_LZO] = "lzo",
	[SQFS_COMP_XZ] = "xz",
	[SQFS_COMP_LZ4] = "lz4",
	[SQFS_COMP_ZSTD] = "zstd",
};

static long read_number(const char *name, const char *str, long min, long max)
{
	long base = 10, result = 0;
	int x;

	if (str[0] == '0') {
		if (str[1] == 'x' || str[1] == 'X') {
			base = 16;
			str += 2;
		} else {
			base = 8;
		}
	}

	if (!isxdigit(*str))
		goto fail_num;

	while (isxdigit(*str)) {
		x = *(str++);

		if (isupper(x)) {
			x = x - 'A' + 10;
		} else if (islower(x)) {
			x = x - 'a' + 10;
		} else {
			x -= '0';
		}

		if (x >= base)
			goto fail_num;

		if (result > (LONG_MAX - x) / base)
			goto fail_ov;

		result = result * base + x;
	}

	if (result < min)
		goto fail_uf;

	if (result > max)
		goto fail_ov;

	return result;
fail_num:
	fprintf(stderr, "%s: expected numeric value > 0\n", name);
	goto fail;
fail_uf:
	fprintf(stderr, "%s: number to small\n", name);
	goto fail;
fail_ov:
	fprintf(stderr, "%s: number to large\n", name);
	goto fail;
fail:
	exit(EXIT_FAILURE);
}

static int sqfs_padd_file(sqfs_info_t *info)
{
	size_t padd_sz = info->super.bytes_used % info->dev_blk_size;
	uint8_t *buffer;
	ssize_t ret;
	off_t off;

	if (padd_sz == 0)
		return 0;

	off = lseek(info->outfd, 0, SEEK_END);
	if (off == (off_t)-1) {
		perror("seek on output file");
		return -1;
	}

	padd_sz = info->dev_blk_size - padd_sz;
	buffer = alloca(padd_sz);
	memset(buffer, 0, padd_sz);

	ret = write_retry(info->outfd, buffer, padd_sz);

	if (ret < 0) {
		perror("Error padding squashfs image to page size");
		return -1;
	}

	if (ret == 0) {
		fputs("Truncated write trying to padd squashfs image\n",
		      stderr);
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	uint32_t blocksize = SQFS_DEFAULT_BLOCK_SIZE, timestamp = 0;
	int i, outmode = O_WRONLY | O_CREAT | O_EXCL;
	E_SQFS_COMPRESSOR compressor = SQFS_COMP_XZ;
	const char *infile, *outfile;
	int status = EXIT_FAILURE;
	compressor_stream_t *cmp;
	sqfs_info_t info;

	memset(&info, 0, sizeof(info));
	info.dev_blk_size = SQFS_DEVBLK_SIZE;
	info.fs.default_uid = 0;
	info.fs.default_gid = 0;
	info.fs.default_mode = 0755;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 't':
			timestamp = read_number("Timestamp", optarg,
						0, 0xFFFFFFFF);
			break;
		case 'c':
			for (i = SQFS_COMP_MIN; i <= SQFS_COMP_MAX; ++i) {
				if (strcmp(compressors[i], optarg) == 0) {
					compressor = i;
					break;
				}
			}
			break;
		case 'b':
			blocksize = read_number("Block size", optarg,
						1024, 0xFFFFFFFF);
			break;
		case 'B':
			info.dev_blk_size = read_number("Device block size",
							optarg, 4096,
							0xFFFFFFFF);
			break;
		case 'u':
			info.fs.default_uid = read_number("User ID", optarg,
							  0, 0xFFFFFFFF);
			break;
		case 'g':
			info.fs.default_gid = read_number("Group ID", optarg,
							  0, 0xFFFFFFFF);
			break;
		case 'm':
			info.fs.default_mode = read_number("Permissions",
							   optarg, 0, 0777);
			break;
		case 'f':
			outmode = O_WRONLY | O_CREAT | O_TRUNC;
			break;
		case 'h':
			printf(help_string, __progname,
			       SQFS_DEFAULT_BLOCK_SIZE, SQFS_DEVBLK_SIZE);

			fputs("Available compressors:\n", stdout);

			for (i = SQFS_COMP_MIN; i <= SQFS_COMP_MAX; ++i)
				printf("\t%s\n", compressors[i]);

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

	if (sqfs_super_init(&info.super, timestamp, blocksize, compressor))
		goto out_close;

	cmp = sqfs_get_compressor(&info.super);
	if (cmp == NULL)
		goto out_close;

	info.block = malloc(info.super.block_size * 3);
	if (info.block == NULL) {
		perror("malloc");
		goto out_cmp;
	}

	info.scratch = (char *)info.block + info.super.block_size;
	info.fragment = (char *)info.block + 2 * info.super.block_size;

	if (create_vfs_tree(&info))
		goto out_buffer;

	if (sqfs_super_write(&info.super, info.outfd))
		goto out_tree;

	if (pkg_data_to_sqfs(&info, cmp))
		goto out_fragments;

	free(info.block);
	info.block = NULL;

	if (sqfs_write_inodes(&info, cmp))
		goto out_fragments;

	if (sqfs_write_fragment_table(info.outfd, &info.super,
				      info.fragments, info.num_fragments,
				      cmp))
		goto out_fragments;

	if (sqfs_write_ids(info.outfd, &info.super,
			   info.fs.id_tbl, info.fs.num_ids, cmp))
		goto out_fragments;

	if (sqfs_super_write(&info.super, info.outfd))
		goto out_fragments;

	if (sqfs_padd_file(&info))
		goto out_fragments;

	status = EXIT_SUCCESS;
out_fragments:
	free(info.fragments);
out_tree:
	destroy_vfs_tree(&info.fs);
out_buffer:
	free(info.block);
out_cmp:
	cmp->destroy(cmp);
out_close:
	close(info.outfd);
out_pkg_close:
	pkg_reader_close(info.rd);
	return status;
fail_arg:
	fprintf(stderr, "Try `%s --help' for more information.\n", __progname);
	return EXIT_FAILURE;
}
