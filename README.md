# About

This package contains the source code of a small archiving tool with support
for dependency management, aka a package manager, developed for the Pygos
build system.

Building this package produces a program called `pkg` that can:

* take a package description file and a set of input files and pack them to
  a compressed archive.
* dump the contents and meta data of such an archive for inspection.
* install an archive and all its dependencies recursively, in topological
  order, to a specified root directory.
* generate file listings from archives in formats suitable for `mksquashfs`
  and Linux `CONFIG_INITRAMFS_SOURCE`.
* work out a build order for source packages given information on what source
  packages provide what binary packages and what binary packages they need in
  order to build.
* produce pretty dependency graphs.


An additional binary is produced called `pkg2sqfs` that can convert a package
produced by `pkg` into a SquashFS filesystem image.


## License

The source code in this package is provided under the OpenBSD flavored ISC
license. So you can practically do as you wish, as long as you retain the
original copyright notice. The software is provided "as is" (as usual) with
no warranty whatsoever (e.g. it might actually do what it was designed for,
but it could just as well set your carpet on fire).

The sub directory `m4` contains third party macro files used by the build
system which may be subject to their own, respective licenses.

## Package File Format

A writeup on the package file format can be found in
[doc/fileformat.md](doc/fileformat.md).

The format is currrently not finalized and will be frozen with the 1.0 release
of this package.

The `pkg` utility has an extensive built in help that can be accessed through
`pkg help` or `pkg help <command>`.


# Usage Example

An example package could be created from the following description file,
named `foobar.desc`:

	requires coreutils libfrob basefiles
	toc-compressor zlib
	data-compressor lzma

In order to be installed, the package requires the other packages `coreutils`,
`libfrob` and `basefiles` to already be installed beforehand.

For whatever reason, we specify that the table of contents should be compressed
with zlib and the data with lzma, instead of using the built in default
choices.

In addition to the description file, we most likely also need want to include
some files in the package, so we create a file listing `foobar.files`:

	# this is a comment, outlining commentary syntax and explaining
	# that we create a bunch of directories with permission bits 755,
	# owned by the user root
	dir bin 0755 0 0
	dir dev 0755 0 0
	dir home 0755 0 0

	# this directory is owned by a different user and group
	dir home/goliath 0750 1000 1000

	# create a symlink named /root that points to /home/goliath
	slink root 0777 0 0 /home/goliath

	# create a file /home/goliath/README.md from the input file
	# README.md stored relative to the foobar.files
	file home/goliath/README.md 0644 1000 1000 README.md

	# create a file named /bin/foobar. Since we omit the input file path,
	# assume the file ./bin/foobar also exists relative to foobar.files
	file bin/foobar 0755 0 0

	# create a character device with permissions 600, owned by 0:0,
	# device number major 5, minor 1
	nod dev/console 0600 0 0 c 5 1

Running the following command:

	pkg pack -d foobar.desc -l foobar.files -r ./repodir

Creates a file `./repodir/foobar.pkg` from the package description and file
listing. Note that the files in the listing are the only things that actually
have to exist anywhere in the real file system to create the archive.

Lets say, we want to install `foobar` and all its dependencies recursively
into a staging root directory. Running the following command is sufficient:

	pkg install -r ./rootfs -R ./repodir foobar

This installs `foobar.pkg` inside `./repodir`. Since `foobar` depends on
`coreutils`, `libfrob` and `basefiles`, the archive files `coreutils.pkg`,
`libfrob.pkg` and `basefiles.pkg` are also read from `./repodir` and
installed to `./rootfs`. The same way, all transitive dependencies are
installed recursively.

Assuming we are not running as root, the above command will tell us that it
cannot create device nodes while installing and it has trouble changing
permission/ownership on some file.

So instead, we could run the following command, to keep ownership of the user
that ran the command, use default permissions and not create device files:

	pkg install -omD -r ./rootfs -R ./repodir foobar

For more information on possible options, simply run `pkg help install`.

Now lets assume we want to pack the staging root directory into a squashfs
file system.

The `mksquashfs` command is a bit silly in regards to managing user IDs of
files, but has half-assed support for what it calls "pseudo files" that let
us set arbitrary user IDs, permissions and create device files, even as a
regular user.

So we run the following command to generate a squashfs pseudo file for the
installed packages:

	pkg install -l sqfs -r ./rootfs -R ./repodir foobar > pseudo.txt
