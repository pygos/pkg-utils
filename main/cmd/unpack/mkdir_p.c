#include "unpack.h"

int mkdir_p(const char *path)
{
	size_t i, len;
	char *buffer;

	while (*path == '/')
		++path;

	if (*path == '\0')
		return 0;

	len = strlen(path) + 1;
	buffer = alloca(len);

	for (i = 0; i < len; ++i) {
		if (path[i] == '/' || path[i] == '\0') {
			buffer[i] = '\0';

			if (mkdir(buffer, 0755) != 0) {
				if (errno != EEXIST) {
					fprintf(stderr, "mkdir %s: %s\n",
						buffer, strerror(errno));
					return -1;
				}
			}
		}

		buffer[i] = path[i];
	}

	return 0;
}
