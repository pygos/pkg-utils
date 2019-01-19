#include "pack.h"

static char *skipspace(char *str)
{
	while (isspace(*str))
		++str;
	return str;
}

static void complain(const input_file_t *f, const char *msg)
{
	fprintf(stderr, "%s: %zu: %s\n", f->filename, f->linenum, msg);
}

static void oom(input_file_t *f)
{
	complain(f, "out of memory");
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
		complain(f, "expected space after file name");
		goto fail;
	}

	ent->name = calloc(1, i + 1);
	if (ent->name == NULL) {
		oom(f);
		goto fail;
	}

	memcpy(ent->name, line, i);
	if (canonicalize_name(ent->name)) {
		complain(f, "invalid file name");
		goto fail;
	}

	if (ent->name[0] == '\0') {
		complain(f, "refusing to add entry for '/'");
		goto fail;
	}

	if (strlen(ent->name) > 0xFFFF) {
		complain(f, "name too long");
		goto fail;
	}

	line = skipspace(line + i);

	/* mode */
	if (!isdigit(*line)) {
		complain(f, "expected numeric mode after file name");
		goto fail;
	}

	while (isdigit(*line)) {
		if (*line > '7') {
			complain(f, "mode must be octal number");
			goto fail;
		}
		ent->mode = (ent->mode << 3) | (*(line++) - '0');
	}

	if (!isspace(*line)) {
		complain(f, "expected space after file mode");
		goto fail;
	}

	line = skipspace(line);

	ent->mode = (ent->mode & ~S_IFMT) | filetype;

	/* uid */
	if (!isdigit(*line)) {
		complain(f, "expected numeric UID after mode");
		goto fail;
	}

	while (isdigit(*line))
		ent->uid = (ent->uid * 10) + (*(line++) - '0');

	if (!isspace(*line)) {
		complain(f, "expected space after UID");
		goto fail;
	}

	line = skipspace(line);

	/* gid */
	if (!isdigit(*line)) {
		complain(f, "expected numeric GID after UID");
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

image_entry_t *filelist_mkdir(input_file_t *f)
{
	return filelist_mkentry(f, S_IFDIR);
}

image_entry_t *filelist_mkslink(input_file_t *f)
{
	image_entry_t *ent = filelist_mkentry(f, S_IFLNK);

	if (ent == NULL)
		return NULL;

	ent->data.symlink.target = strdup(f->line);
	if (ent->data.symlink.target == NULL) {
		oom(f);
		goto fail;
	}

	if (strlen(ent->data.symlink.target) > 0xFFFF) {
		complain(f, "symlink target too long");
		goto fail;
	}

	return ent;
fail:
	image_entry_free(ent);
	return NULL;
}

image_entry_t *filelist_mkfile(input_file_t *f)
{
	image_entry_t *ent = filelist_mkentry(f, S_IFREG);
	struct stat sb;

	if (ent == NULL)
		return NULL;

	ent->data.file.location = strdup(f->line);
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
		complain(f, "input file is too big");
		goto fail;
	}

	ent->data.file.size = sb.st_size;
	return ent;
fail:
	image_entry_free(ent);
	return NULL;
}
