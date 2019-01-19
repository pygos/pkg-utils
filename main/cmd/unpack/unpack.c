#include "unpack.h"


static const struct option long_opts[] = {
	{ "root", required_argument, NULL, 'r' },
	{ "no-chown", required_argument, NULL, 'o' },
	{ "no-chmod", required_argument, NULL, 'm' },
	{ NULL, 0, NULL, 0 },
};

static const char *short_opts = "r:om";

static int set_root(const char *path)
{
	if (mkdir_p(path))
		return -1;

	if (chdir(path)) {
		fprintf(stderr, "cd %s: %s\n", path, strerror(errno));
		return -1;
	}

	return 0;
}

static image_entry_t *get_file_entry(image_entry_t *list, uint32_t id)
{
	while (list != NULL) {
		if (S_ISREG(list->mode) && list->data.file.id == id)
			return list;

		list = list->next;
	}

	return NULL;
}

static int unpack_files(image_entry_t *list, pkg_reader_t *rd)
{
	uint8_t buffer[2048];
	image_entry_t *meta;
	file_data_t frec;
	ssize_t ret;
	size_t diff;
	uint64_t i;
	int fd;

	for (;;) {
		ret = pkg_reader_read_payload(rd, &frec, sizeof(frec));
		if (ret == 0)
			break;
		if (ret < 0)
			return -1;
		if ((size_t)ret < sizeof(frec))
			goto fail_trunc;

		frec.id = le32toh(frec.id);

		meta = get_file_entry(list, frec.id);
		if (meta == NULL) {
			fprintf(stderr, "%s: missing meta information for "
				"file %u\n", pkg_reader_get_filename(rd),
				(unsigned int)frec.id);
			return -1;
		}

		fd = open(meta->name, O_WRONLY | O_CREAT | O_EXCL, 0644);
		if (fd < 0) {
			perror(meta->name);
			return -1;
		}

		for (i = 0; i < meta->data.file.size; i += ret) {
			if ((meta->data.file.size - i) <
			    (uint64_t)sizeof(buffer)) {
				diff = meta->data.file.size - i;
			} else {
				diff = sizeof(buffer);
			}

			ret = pkg_reader_read_payload(rd, buffer, diff);
			if (ret < 0)
				goto fail_fd;
			if ((size_t)ret < diff)
				goto fail_trunc;

			ret = write_retry(fd, buffer, diff);
			if (ret < 0) {
				perror(meta->name);
				goto fail_fd;
			}

			if ((size_t)ret < diff) {
				fprintf(stderr, "%s: truncated write\n",
					pkg_reader_get_filename(rd));
				goto fail_fd;
			}
		}

		close(fd);
	}

	return 0;
fail_fd:
	close(fd);
	return -1;
fail_trunc:
	fprintf(stderr, "%s: truncated file data record\n",
		pkg_reader_get_filename(rd));
	return -1;
}

static int change_permissions(image_entry_t *list, int flags)
{
	while (list != NULL) {
		if (S_ISLNK(list->mode)) {
			list = list->next;
			continue;
		}

		if (!(flags & FLAG_NO_CHMOD)) {
			if (chmod(list->name, list->mode)) {
				fprintf(stderr, "%s: chmod: %s\n", list->name,
					strerror(errno));
				return -1;
			}
		}

		if (!(flags & FLAG_NO_CHOWN)) {
			if (chown(list->name, list->uid, list->gid)) {
				fprintf(stderr, "%s: chown: %s\n", list->name,
					strerror(errno));
				return -1;
			}
		}

		list = list->next;
	}

	return 0;
}

static int cmd_unpack(int argc, char **argv)
{
	const char *root = NULL, *filename;
	image_entry_t *list = NULL;
	int i, ret, flags = 0;
	pkg_reader_t *rd;
	record_t *hdr;

	for (;;) {
		i = getopt_long(argc, argv, short_opts, long_opts, NULL);
		if (i == -1)
			break;

		switch (i) {
		case 'r':
			root = optarg;
			break;
		case 'o':
			flags |= FLAG_NO_CHOWN;
			break;
		case 'm':
			flags |= FLAG_NO_CHMOD;
			break;
		default:
			tell_read_help(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		fputs("missing argument: package file\n", stderr);
		return EXIT_FAILURE;
	}

	filename = argv[optind++];

	if (optind < argc)
		fputs("warning: ignoring extra arguments\n", stderr);

	rd = pkg_reader_open(filename);
	if (rd == NULL)
		return EXIT_FAILURE;

	list = image_entry_list_from_package(rd);
	if (list == NULL) {
		pkg_reader_close(rd);
		return EXIT_FAILURE;
	}

	list = image_entry_sort(list);

	if (pkg_reader_rewind(rd))
		goto fail;

	if (root != NULL && set_root(root) != 0)
		goto fail;

	if (create_hierarchy(list))
		goto fail;

	for (;;) {
		ret = pkg_reader_get_next_record(rd);
		if (ret == 0)
			break;
		if (ret < 0)
			goto fail;

		hdr = pkg_reader_current_record_header(rd);

		if (hdr->magic == PKG_MAGIC_DATA) {
			if (unpack_files(list, rd))
				goto fail;
		}
	}

	if (change_permissions(list, flags))
		goto fail;

	image_entry_free_list(list);
	pkg_reader_close(rd);
	return EXIT_SUCCESS;
fail:
	image_entry_free_list(list);
	pkg_reader_close(rd);
	return EXIT_FAILURE;
}

static command_t unpack = {
	.cmd = "unpack",
	.usage = "[OPTIONS...] <pkgfile>",
	.s_desc = "unpack the contents of a package file",
	.l_desc =
"The unpack command extracts the file hierarchy stored in a package into\n"
"a destination directory (default: current working directory).\n"
"\n"
"Possible options:\n"
"  --root, -r <director>   A root directory to unpack the package. Defaults\n"
"                          to the current working directory if not set.\n"
"  --no-chown, -o          Do not change ownership of the extracted data.\n"
"                          Keep the uid/gid of the user who runs the program.\n"
"  --no-chmod, -m          Do not change permission flags of the extarcted\n"
"                          data. Use 0644 for all files and 0755 for all\n"
"                          directories.\n",
	.run_cmd = cmd_unpack,
};

REGISTER_COMMAND(unpack)
