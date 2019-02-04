#ifndef PKG_FORMAT_H
#define PKG_FORMAT_H

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <endian.h>

typedef enum {
	PKG_MAGIC_HEADER = 0x21676B70,
	PKG_MAGIC_TOC = 0x21636F74,
	PKG_MAGIC_DATA = 0x21746164,
} PKG_MAGIC;

typedef enum {
	PKG_COMPRESSION_NONE = 0,
	PKG_COMPRESSION_ZLIB = 1,
	PKG_COMPRESSION_LZMA = 2,
} PKG_COMPRESSION;

typedef enum {
	PKG_DEPENDENCY_REQUIRES = 0,
} PKG_DEPENDENCY_TYPE;

typedef struct {
	uint32_t magic;
	uint8_t compression;
	uint8_t pad0;
	uint8_t pad1;
	uint8_t pad2;
	uint64_t compressed_size;
	uint64_t raw_size;

	/* uint8_t data[]; */
} record_t;

typedef struct {
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;

	uint16_t path_length;
	/* uint8_t path[]; */
} toc_entry_t;

typedef struct {
	uint16_t target_length;
	/* uint8_t target[]; */
} toc_symlink_extra_t;

typedef struct {
	uint64_t size;
	uint32_t id;
} toc_file_extra_t;

typedef struct {
	uint64_t devno;
} toc_device_extra_t;

typedef struct {
	uint32_t id;
	/* uint8_t data[]; */
} file_data_t;

typedef struct {
	uint16_t num_depends;
	/* pkg_dependency_t depends[]; */
} pkg_header_t;

typedef struct {
	uint8_t type;
	uint8_t name_length;
	/* uint8_t name[]; */
} pkg_dependency_t;

#endif /* PKG_FORMAT_H */
