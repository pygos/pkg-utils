# Package File Format

## Record Structure

A package file consists of a series of records with possibly compressed
payload. Each record has a header, indicating the type of record, raw and
compressed size of the payload data and the compression algorithm used.

All multi byte integers are stored in little endian byte order.

Knowledge of the payload size lets a decoder skip unknown record types inserted
by an encoder that supports a newer version of the format, allowing for some
degree of backwards compatibility.

The diagram below illustrates what a record header looks like. The horizontally
arranged numbers indicate a byte offset inside a 32 bit word and the vertical
numbers count 32 bit words from the start of the header.

          0       1       2       3
      +-------+-------+-------+-------+
	0 |             magic             |
	  +-------+-------+-------+-------+
	1 | comp  |reserved for future use|
      +-------+-------+-------+-------+
	2 |                               |
	  |        compressed size        |
	3 |                               |
	  +-------+-------+-------+-------+
	4 |                               |
	  |       uncompressed size       |
	5 |                               |
	  +-------+-------+-------+-------+
	6 |                               |
	. |            payload            |
	. |                               |
	  |/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/

       /\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/|
	  |                               |
	  |_______________________________|


The field `magic` holds a 32 bit magic number indicating the chunk type.

Currently, the following chunk types are supported:

* `PKG_MAGIC_HEADER` with the value `0x21676B70` (ASCII "pkg!"). The overall
  package header with information about the package.
* `PKG_MAGIC_TOC` with the value `0x21636F74` (ASCII "toc!"). The table of
  contents record.
* `PKG_MAGIC_DATA` with the value `0x21746164` (ASCII "dat!"). The package data
  record.

The byte labeled `comp` holds a compression algorithm identifier. Currently, the
following compression algorithms are supported:

* `PKG_COMPRESSION_NONE` with the value 0. The payload is uncompressed. The
  compressed size of the record payload must equal the uncompressed size.
* `PKG_COMPRESSION_ZLIB` with the value 1. The record payload area contains a
  raw zlib stream.
* `PKG_COMPRESSION_LZMA` with the value 2. The record payload area contains
  lzma compressed data.

The compressor ID is padded with 3 bytes that **must be set to zero** by an
encoder and are currently reserved for future use.

## Package Header Record

The header record must be present in ever package and must always be the first
record in a package file. If a decoder encounters a file which does not start
with the magic value of the header record, it must reject the file.

Future versions of the format that break backwards compatibility can simply
introduce a new magic value for the (possibly altered) header record. Older
decoders are expected to reject a file with the newer format, while newer
decoders can implement different behavior depending on what magic value they
find at the start.

The payload of header record is currently only used to store package
dependencies. It is byte aligned and contains a 16 bit integer, indicating the
number of dependencies, followed by a sequence of dependent packages.

Each dependent package is encoded as follows:

         0         1         2         length - 1
    +---------+---------+-------     -+---------+
    |  type   | length  | name   ...  |         |
    +---------+---------+-------     -+---------+


The type field must be set to 0, indicating that a dependency is required by a
package.

The length byte indicates the length of the package name that follows, allowing
for up to 255 bytes of package name afterwards.

If all dependencies have been processed, but there is still payload data left
in the header record, a decoder must ignore the extra data and skip to the end
of the record.

## Table of Contents Record

For each file, directory, symlink, et cetera contained in the package, the
table of contents contains an entry with the following common structure:

          0       1       2       3
      +-------+-------+-------+-------+
    0 |     mode      |    user id    |
      +-------+-------+-------+-------+
    1 |   group id    |  path length  |
      +-------+-------+-------+-------+

The mode field contains standard UNIX permissions. The user ID and group ID
fields contain the numeric IDs of the user and group respectively that own
the file. The path length field indicates the length of the byte aligned,
absolute file name that follows.

The file path is expected to neither start nor end with a slash, contain no
sequences of more than one slash and not contain the components `.` or `..`.

On the bit level, the mode field is structured as follows:

    1 1 1 1 1 1
    5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
      type |U|G|S|r|w|x|r|w|x|r|w|x|
        |   | | | | | | | | | | | |
        |   | | | | | | | | | | | +--- others execute permission flag
        |   | | | | | | | | | | +----- others write permission flag
        |   | | | | | | | | | +------- others read permission flag
        |   | | | | | | | | +--------- group execute permission flag
        |   | | | | | | | +----------- group write permission flag
        |   | | | | | | +------------- group read permission flag
        |   | | | | | +--------------- owner execute permission flag
        |   | | | | +----------------- owner write permission flag
        |   | | | +------------------- owner read permission flag
        |   | | +--------------------- sticky bit
        |   | +----------------------- set GID bit
        |   +------------------------- set UID bit
        +----------------------------- file type

Currently, the following file types are supported:

* `S_IFCHR` with a value of 2. The entry defines a character device.
* `S_IFDIR` with a value of 4. The entry defines a directory.
* `S_IFBLK` with a value of 6. The entry defines a block device.
* `S_IFREG` with a value of 8. The entry defines a regular file.
* `S_IFLNK` with a value of 10. The entry defines a symbolic link.


For character and block devices, the file path is followed by a byte aligned,
64 bit device number.

For regular files, the path is followed by a byte aligned, 64 bit integer
indicating the total size of the file in bytes, followed by a byte aligned,
32 bit integer containing a unique file ID.

For symlinks, the path is followed by a byte aligned, 16 bit integer holding
the length of the target path, followed by the byte aligned symlink target.

## Data Record

A package may contain multiple data records holding raw file data. Each data
record contains a sequence of a byte aligned, 32 bit file ID, followed by raw
data until the file is filled (size indicated by table of contents is reached).

A file may not span across multiple data records. A file ID must not occur
more than once in a data record and must only occur in a single data record.
