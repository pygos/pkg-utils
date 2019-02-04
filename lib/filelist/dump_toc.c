#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#include "filelist/image_entry.h"

typedef int (*print_fun_t)(image_entry_t *ent, const char *root);

static int print_pretty(image_entry_t *ent, const char *root)
{
	(void)root;

	printf("%s\n", ent->name);
	printf("\towner: %d/%d\n", ent->uid, ent->gid);
	printf("\tpermissions: 0%04o\n", ent->mode & (~S_IFMT));

	fputs("\ttype: ", stdout);

	switch (ent->mode & S_IFMT) {
	case S_IFLNK:
		fputs("symlink\n", stdout);

		printf("\ttarget: %s\n\n", ent->data.symlink.target);
		break;
	case S_IFREG:
		fputs("file\n", stdout);

		printf("\tsize: %lu\n", (unsigned long)ent->data.file.size);
		printf("\tid: %u\n\n", (unsigned int)ent->data.file.id);
		break;
	case S_IFDIR:
		fputs("directory\n\n", stdout);
		break;
	case S_IFCHR:
		fputs("character device\n", stdout);
		printf("\tmajor: %u\n\tminor: %u\n\n",
		       major(ent->data.device.devno),
		       minor(ent->data.device.devno));
		break;
	case S_IFBLK:
		fputs("block device\n", stdout);
		printf("\tmajor: %u\n\tminor: %u\n\n",
		       major(ent->data.device.devno),
		       minor(ent->data.device.devno));
		break;
	default:
		fputs("unknown\n\n", stdout);
		goto fail_type;
	}
	return 0;
fail_type:
	fputs("unknown file type in table of contents\n", stderr);
	return -1;
}

static int print_sqfs(image_entry_t *ent, const char *root)
{
	mode_t mode = ent->mode & (~S_IFMT);
	(void)root;

	switch (ent->mode & S_IFMT) {
	case S_IFCHR:
		printf("%s c %o %u %u %u %u\n", ent->name, mode,
		       (unsigned int)ent->uid, (unsigned int)ent->gid,
		       major(ent->data.device.devno),
		       minor(ent->data.device.devno));
		break;
	case S_IFBLK:
		printf("%s b %o %u %u %u %u\n", ent->name, mode,
		       (unsigned int)ent->uid, (unsigned int)ent->gid,
		       major(ent->data.device.devno),
		       minor(ent->data.device.devno));
		break;
	case S_IFLNK:
		mode = 0777;
		/* fall-through */
	case S_IFDIR:
	case S_IFREG:
		printf("%s m %o %u %u\n", ent->name, mode,
		       (unsigned int)ent->uid, (unsigned int)ent->gid);
		break;
	default:
		fputs("unknown file type in table of contents\n", stderr);
		return -1;
	}

	return 0;
}

static int print_initrd(image_entry_t *ent, const char *root)
{
	mode_t mode = ent->mode & (~S_IFMT);

	switch (ent->mode & S_IFMT) {
	case S_IFCHR:
		printf("nod /%s 0%o %u %u c %u %u\n", ent->name, mode,
		       (unsigned int)ent->uid, (unsigned int)ent->gid,
		       major(ent->data.device.devno),
		       minor(ent->data.device.devno));
		return 0;
	case S_IFBLK:
		printf("nod /%s 0%o %u %u b %u %u\n", ent->name, mode,
		       (unsigned int)ent->uid, (unsigned int)ent->gid,
		       major(ent->data.device.devno),
		       minor(ent->data.device.devno));
		return 0;
	case S_IFLNK:
		printf("slink /%s %s", ent->name, ent->data.symlink.target);
		mode = 0777;
		break;
	case S_IFREG:
		printf("file /%s ", ent->name);

		if (root == NULL) {
			fputs(ent->name, stdout);
		} else {
			printf("%s/%s", root, ent->name);
		}
		break;
	case S_IFDIR:
		printf("dir /%s", ent->name);
		break;
	default:
		fputs("unknown file type in table of contents\n", stderr);
		return -1;
	}

	printf(" 0%o %u %u\n", mode,
	       (unsigned int)ent->uid, (unsigned int)ent->gid);
	return 0;
}

static print_fun_t printers[] = {
	[TOC_FORMAT_PRETTY] = print_pretty,
	[TOC_FORMAT_SQFS] = print_sqfs,
	[TOC_FORMAT_INITRD] = print_initrd,
};

int dump_toc(image_entry_t *list, const char *root, TOC_FORMAT format)
{
	while (list != NULL) {
		if (printers[format](list, root))
			return -1;

		list = list->next;
	}

	return 0;
}
