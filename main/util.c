#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "util.h"

int canonicalize_name(char *filename)
{
	char *ptr = filename;
	int i;

	while (*ptr == '/')
		++ptr;

	if (ptr != filename) {
		memmove(filename, ptr, strlen(ptr) + 1);
		ptr = filename;
	}

	while (*ptr != '\0') {
		if (*ptr == '/') {
			for (i = 0; ptr[i] == '/'; ++i)
				;

			if (i > 1)
				memmove(ptr + 1, ptr + i, strlen(ptr + i) + 1);
		}

		if (ptr[0] == '/' && ptr[1] == '\0') {
			*ptr = '\0';
			break;
		}

		++ptr;
	}

	ptr = filename;

	while (*ptr != '\0') {
		if (ptr[0] == '.') {
			if (ptr[1] == '/' || ptr[1] == '\0')
				return -1;

			if (ptr[1] == '.' &&
			    (ptr[2] == '/' || ptr[2] == '\0')) {
				return -1;
			}
		}

		while (*ptr != '\0' && *ptr != '/')
			++ptr;
		if (*ptr == '/')
			++ptr;
	}

	return 0;
}

ssize_t write_retry(int fd, void *data, size_t size)
{
	ssize_t ret, total = 0;

	while (size > 0) {
		ret = write(fd, data, size);
		if (ret == 0)
			break;
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}

		data = (char *)data + ret;
		size -= ret;
		total += ret;
	}

	return total;
}

ssize_t read_retry(int fd, void *buffer, size_t size)
{
	ssize_t ret, total = 0;

	while (size > 0) {
		ret = read(fd, buffer, size);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (ret == 0)
			break;

		total += ret;
		size -= ret;
		buffer = (char *)buffer + ret;
	}

	return total;
}
