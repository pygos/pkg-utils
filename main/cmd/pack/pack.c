#include "pack.h"

static const struct option long_opts[] = {
	{ "toc-compressor", required_argument, NULL, 't' },
	{ "file-compressor", required_argument, NULL, 'f' },
	{ "file-list", required_argument, NULL, 'l' },
	{ "output", required_argument, NULL, 'o' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "t:f:l:o:";

static compressor_t *get_default_compressor(void)
{
	compressor_t *cmp;

	cmp = compressor_by_id(PKG_COMPRESSION_ZLIB);
	if (cmp != NULL)
		return cmp;

	cmp = compressor_by_id(PKG_COMPRESSION_NONE);
	if (cmp != NULL)
		return cmp;

	return cmp;
}

static compressor_t *try_get_compressor(const char *name)
{
	compressor_t *cmp = compressor_by_name(name);

	if (cmp == NULL)
		fprintf(stderr, "unkown compressor: %s\n", name);

	return cmp;
}

static int alloc_file_ids(image_entry_t *list)
{
	image_entry_t *ent;
	uint64_t file_id = 0;

	for (ent = list; ent != NULL; ent = ent->next) {
		if (!S_ISREG(ent->mode))
			continue;

		if (file_id > 0x00000000FFFFFFFFUL) {
			fprintf(stderr, "too many input files\n");
			return -1;
		}

		ent->data.file.id = file_id++;
	}

	return 0;
}

static int cmd_pack(int argc, char **argv)
{
	const char *filelist = NULL, *filename = NULL;
	compressor_t *cmp_toc, *cmp_files;
	image_entry_t *list;
	pkg_writer_t *wr;
	int i;

	cmp_toc = get_default_compressor();
	cmp_files = cmp_toc;

	if (cmp_toc == NULL) {
		fputs("no compressor implementations available\n", stderr);
		return EXIT_FAILURE;
	}

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 't':
			cmp_toc = try_get_compressor(optarg);
			if (cmp_toc == NULL)
				return EXIT_FAILURE;
			break;
		case 'f':
			cmp_files = try_get_compressor(optarg);
			if (cmp_files == NULL)
				return EXIT_FAILURE;
			break;
		case 'l':
			filelist = optarg;
			break;
		case 'o':
			filename = optarg;
			break;
		default:
			tell_read_help(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (filename == NULL) {
		fputs("missing argument: output package file\n", stderr);
		tell_read_help(argv[0]);
		return EXIT_FAILURE;
	}

	if (filelist == NULL) {
		fputs("missing argument: input file list\n", stderr);
		tell_read_help(argv[0]);
		return EXIT_FAILURE;
	}

	if (optind < argc)
		fputs("warning: ignoring extra arguments\n", stderr);

	list = filelist_read(filelist);
	if (list == NULL)
		return EXIT_FAILURE;

	list = image_entry_sort(list);

	if (alloc_file_ids(list))
		goto fail_fp;

	wr = pkg_writer_open(filename);
	if (wr == NULL)
		goto fail_fp;

	if (write_toc(wr, list, cmp_toc))
		goto fail;

	if (write_files(wr, list, cmp_files))
		goto fail;

	pkg_writer_close(wr);
	image_entry_free_list(list);
	return EXIT_SUCCESS;
fail:
	pkg_writer_close(wr);
fail_fp:
	image_entry_free_list(list);
	return EXIT_FAILURE;
}

static command_t pack = {
	.cmd = "pack",
	.usage = "OPTIONS...",
	.s_desc = "generate a package file",
	.l_desc =
"Read a list of files from the given lists file and generate a package.\n"
"Possible options:\n"
"  --file-list, -l <path>  Specify a file containing a list of input files.\n"
"  --output, -o <path>     Specify the path of the resulting package file.\n"
"\n"
"  --toc-compressor, -t <compressor>\n"
"  --file-compressor, -f <compressor>\n"
"      Specify what compressor to use for compressing the table of contents,\n"
"      or for compressing files respectively.\n",
	.run_cmd = cmd_pack,
};

REGISTER_COMMAND(pack)
