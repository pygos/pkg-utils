#include "pack.h"

static char *skipspace(char *str)
{
	while (isspace(*str))
		++str;
	return str;
}

static void oom(input_file_t *f)
{
	input_file_complain(f, "out of memory");
}

static image_entry_t *filelist_mkentry(input_file_t *f, mode_t filetype)
{
	char *line = f->line;
	image_entry_t *ent;
	size_t i;

	ent = calloc(1, sizeof(*ent));
	if (ent == NULL) {
		oom(f);
		return NULL;
	}

	/* name */
	for (i = 0; !isspace(line[i]) && line[i] != '\0'; ++i)
		;

	if (!isspace(line[i])) {
		input_file_complain(f, "expected space after file name");
		goto fail;
	}

	ent->name = calloc(1, i + 1);
	if (ent->name == NULL) {
		oom(f);
		goto fail;
	}

	memcpy(ent->name, line, i);
	if (canonicalize_name(ent->name)) {
		input_file_complain(f, "invalid file name");
		goto fail;
	}

	if (ent->name[0] == '\0') {
		input_file_complain(f, "refusing to add entry for '/'");
		goto fail;
	}

	if (strlen(ent->name) > 0xFFFF) {
		input_file_complain(f, "name too long");
		goto fail;
	}

	line = skipspace(line + i);

	/* mode */
	if (!isdigit(*line)) {
		input_file_complain(f, "expected numeric mode after file name");
		goto fail;
	}

	while (isdigit(*line)) {
		if (*line > '7') {
			input_file_complain(f, "mode must be octal number");
			goto fail;
		}
		ent->mode = (ent->mode << 3) | (*(line++) - '0');
	}

	if (!isspace(*line)) {
		input_file_complain(f, "expected space after file mode");
		goto fail;
	}

	line = skipspace(line);

	ent->mode = (ent->mode & ~S_IFMT) | filetype;

	/* uid */
	if (!isdigit(*line)) {
		input_file_complain(f, "expected numeric UID after mode");
		goto fail;
	}

	while (isdigit(*line))
		ent->uid = (ent->uid * 10) + (*(line++) - '0');

	if (!isspace(*line)) {
		input_file_complain(f, "expected space after UID");
		goto fail;
	}

	line = skipspace(line);

	/* gid */
	if (!isdigit(*line)) {
		input_file_complain(f, "expected numeric GID after UID");
		goto fail;
	}

	while (isdigit(*line))
		ent->gid = (ent->gid * 10) + (*(line++) - '0');

	line = skipspace(line);

	/* remove processed data */
	memmove(f->line, line, strlen(line) + 1);
	return ent;
fail:
	free(ent->name);
	free(ent);
	return NULL;
}

int filelist_mkdir(input_file_t *f, void *obj)
{
	image_entry_t *ent = filelist_mkentry(f, S_IFDIR);
	image_entry_t **listptr = obj;

	if (ent == NULL)
		return -1;

	ent->next = *listptr;
	*listptr = ent;
	return 0;
}

int filelist_mkslink(input_file_t *f, void *obj)
{
	image_entry_t *ent = filelist_mkentry(f, S_IFLNK);
	image_entry_t **listptr = obj;

	if (ent == NULL)
		return -1;

	ent->data.symlink.target = strdup(f->line);
	if (ent->data.symlink.target == NULL) {
		oom(f);
		goto fail;
	}

	if (strlen(ent->data.symlink.target) > 0xFFFF) {
		input_file_complain(f, "symlink target too long");
		goto fail;
	}

	ent->next = *listptr;
	*listptr = ent;
	return 0;
fail:
	image_entry_free(ent);
	return -1;
}

int filelist_mkfile(input_file_t *f, void *obj)
{
	image_entry_t *ent = filelist_mkentry(f, S_IFREG);
	image_entry_t **listptr = obj;
	const char *ptr;
	struct stat sb;

	if (ent == NULL)
		return -1;

	if (f->line[0] == '\0') {
		ptr = strrchr(f->filename, '/');

		if (ptr == NULL) {
			ent->data.file.location = strdup(ent->name);
		} else {
			asprintf(&ent->data.file.location, "%.*s/%s",
				 (int)(ptr - f->filename), f->filename,
				 ent->name);
		}
	} else {
		ent->data.file.location = strdup(f->line);
	}

	if (ent->data.file.location == NULL) {
		oom(f);
		goto fail;
	}

	if (stat(ent->data.file.location, &sb) != 0) {
		perror(ent->data.file.location);
		goto fail;
	}

	if (sizeof(off_t) > sizeof(uint64_t) &&
	    sb.st_size > (off_t)(~((uint64_t)0))) {
		input_file_complain(f, "input file is too big");
		goto fail;
	}

	ent->data.file.size = sb.st_size;

	ent->next = *listptr;
	*listptr = ent;
	return 0;
fail:
	image_entry_free(ent);
	return -1;
}

static int filelist_mknod(input_file_t *f, void *obj)
{
	image_entry_t **listptr = obj, *ent;
	unsigned int maj, min;
	char *ptr;

	ent = filelist_mkentry(f, S_IFCHR);
	if (ent == NULL)
		return -1;

	switch (f->line[0]) {
	case 'c':
	case 'C':
		break;
	case 'b':
	case 'B':
		ent->mode = (ent->mode & (~S_IFMT)) | S_IFBLK;
		break;
	default:
		goto fail;
	}

	if (!isspace(f->line[1]))
		goto fail;

	ptr = f->line + 1;
	while (isspace(*ptr))
		++ptr;

	if (sscanf(ptr, "%u %u", &maj, &min) != 2)
		goto fail;

	ent->data.device.devno = makedev(maj, min);

	ent->next = *listptr;
	*listptr = ent;
	return 0;
fail:
	image_entry_free(ent);
	input_file_complain(f, "error in device specification");
	return -1;
}

static const keyword_handler_t line_hooks[] = {
	{ "file", filelist_mkfile },
	{ "dir", filelist_mkdir },
	{ "slink", filelist_mkslink },
	{ "nod", filelist_mknod },
};

#define NUM_LINE_HOOKS (sizeof(line_hooks) / sizeof(line_hooks[0]))

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

int filelist_read(const char *filename, image_entry_t **out)
{
	image_entry_t *list = NULL;

	if (process_file(filename, line_hooks, NUM_LINE_HOOKS, &list))
		goto fail;

	if (list != NULL) {
		list = image_entry_sort(list);

		if (alloc_file_ids(list))
			goto fail;
	}

	*out = list;
	return 0;
fail:
	image_entry_free_list(list);
	return -1;
}
