ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/include -D_GNU_SOURCE
AM_CFLAGS = $(WARN_CFLAGS)

# application core
GLOBALHDR = include/pkgformat.h include/util.h include/pkgreader.h
GLOBALHDR += include/compressor.h include/command.h include/pkgwriter.h
GLOBALHDR += include/image_entry.h

MAIN = main/pkg.c main/util.c main/compressor.c main/command.c main/pkgreader.c
MAIN += main/pkgwriter.c main/image_entry.c main/image_entry_sort.c

pkg_SOURCES = $(GLOBALHDR) $(MAIN)
pkg_CFLAGS = $(AM_CFLAGS)
pkg_LDADD =

bin_PROGRAMS = pkg

EXTRA_DIST = autogen.sh

##### commands #####

# pack command
pkg_SOURCES += main/cmd/pack/filelist.c main/cmd/pack/filelist_read.c
pkg_SOURCES += main/cmd/pack/write_toc.c main/cmd/pack/write_files.c
pkg_SOURCES += main/cmd/pack/pack.h main/cmd/pack/pack.c

# dump command
pkg_SOURCES += main/cmd/dump/dump.c main/cmd/dump/dump.h main/cmd/dump/dump_toc.c

# unpack command
pkg_SOURCES += main/cmd/unpack/unpack.c main/cmd/unpack/unpack.h
pkg_SOURCES += main/cmd/unpack/mkdir_p.c main/cmd/unpack/create_hierarchy.c

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
