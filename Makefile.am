ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include -D_GNU_SOURCE
AM_CFLAGS = $(WARN_CFLAGS)

# helper libraries
libutil_a_SOURCES = lib/util/input_file.c lib/util/mkdir_p.c
libutil_a_SOURCES += lib/util/write_retry.c lib/util/read_retry.c
libutil_a_SOURCES += lib/util/canonicalize_name.c
libutil_a_SOURCES += include/util/util.h include/util/input_file.h

libfilelist_a_SOURCES = lib/filelist/dump_toc.c lib/filelist/image_entry.c
libfilelist_a_SOURCES += lib/filelist/image_entry_sort.c
libfilelist_a_SOURCES += include/filelist/image_entry.h

# application core
GLOBALHDR = include/pkgformat.h include/pkgreader.h
GLOBALHDR += include/compressor.h include/command.h include/pkgwriter.h
GLOBALHDR += include/pkgio.h

MAIN = main/pkg.c main/compressor.c main/command.c main/pkgreader.c
MAIN += main/pkgwriter.c main/pkgio_rd_image_entry.c main/pkg_unpack.c

pkg_SOURCES = $(GLOBALHDR) $(MAIN)
pkg_CFLAGS = $(AM_CFLAGS)
pkg_LDADD = libutil.a libfilelist.a

noinst_LIBRARIES = libutil.a libfilelist.a
bin_PROGRAMS = pkg

EXTRA_DIST = autogen.sh

##### commands #####

# pack command
pkg_SOURCES += main/cmd/pack/filelist.c
pkg_SOURCES += main/cmd/pack/write_toc.c main/cmd/pack/write_files.c
pkg_SOURCES += main/cmd/pack/pack.h main/cmd/pack/pack.c
pkg_SOURCES += main/cmd/pack/desc.c main/cmd/pack/write_hdr.c

# dump command
pkg_SOURCES += main/cmd/dump/dump.c main/cmd/dump/dump.h
pkg_SOURCES += main/cmd/dump/dump_header.c

# install command
pkg_SOURCES += main/cmd/install/collect.c main/cmd/install/install.c
pkg_SOURCES += main/cmd/install/install.h main/cmd/install/pkglist.c
pkg_SOURCES += main/cmd/install/tsort.c

# unpack command
pkg_SOURCES += main/cmd/unpack.c

# help command
pkg_SOURCES += main/cmd/help.c

##### compressors #####

# dummy compressor
pkg_SOURCES += main/compressors/none.c

# zlib compressor
if WITH_ZLIB
pkg_SOURCES += main/compressors/zlib.c

pkg_CFLAGS += $(ZLIB_CFLAGS)
pkg_LDADD += $(ZLIB_LIBS)
endif

# lzma compressor
if WITH_LZMA
pkg_SOURCES += main/compressors/lzma.c

pkg_CFLAGS += $(XZ_CFLAGS)
pkg_LDADD += $(XZ_LIBS)
endif
