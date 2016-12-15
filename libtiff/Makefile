#! smake
#	$Header: /usr/people/sam/tiff/RCS/Makefile,v 1.34 93/08/26 17:16:51 sam Exp $
#
# Tag Image File Format Library
#
# Copyright (c) 1988, 1989, 1990, 1991, 1992 Sam Leffler
# Copyright (c) 1991, 1992 Silicon Graphics, Inc.
# 
# Permission to use, copy, modify, distribute, and sell this software and 
# its documentation for any purpose is hereby granted without fee, provided
# that (i) the above copyright notices and this permission notice appear in
# all copies of the software and related documentation, and (ii) the names of
# Sam Leffler and Silicon Graphics may not be used in any advertising or
# publicity relating to the software without the specific, prior written
# permission of Sam Leffler and Silicon Graphics.
# 
# THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
# EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
# WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
# 
# IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
# ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
# OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
# WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
# LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
# OF THIS SOFTWARE.
#
NULL=
MAKE=	make ${MAKEFLAGS}

TIFFFILES=\
	Makefile				\
	HOWTO					\
	README					\
	CHANGES-v3.0				\
	CHANGES-v3.1				\
	CHANGES-v3.2				\
	CHANGES-v3.3				\
	VERSION					\
	Copyright				\
	libtiff/README				\
	libtiff/Makefile.aix			\
	libtiff/Makefile.apollo			\
	libtiff/Makefile.gcc			\
	libtiff/Makefile.hpux			\
	libtiff/Makefile.mpw			\
	libtiff/Makefile.next			\
	libtiff/Makefile.sco			\
	libtiff/Makefile.sgi			\
	libtiff/Makefile.sun			\
	libtiff/t4.h				\
	libtiff/tiff.h				\
	libtiff/tiffcomp.h			\
	libtiff/tiffconf.h			\
	libtiff/tiffio.h			\
	libtiff/tiffiop.h			\
	libtiff/mkg3states.c			\
	libtiff/mkspans.c			\
	libtiff/tif_apple.c			\
	libtiff/tif_aux.c			\
	libtiff/tif_ccittrle.c			\
	libtiff/tif_close.c			\
	libtiff/tif_compress.c			\
	libtiff/tif_dir.c			\
	libtiff/tif_dirinfo.c			\
	libtiff/tif_dirread.c			\
	libtiff/tif_dirwrite.c			\
	libtiff/tif_dumpmode.c			\
	libtiff/tif_error.c			\
	libtiff/tif_fax3.c			\
	libtiff/tif_fax3.h			\
	libtiff/tif_fax4.c			\
	libtiff/tif_flush.c			\
	libtiff/tif_getimage.c			\
	libtiff/tif_jpeg.c			\
	libtiff/tif_lzw.c			\
	libtiff/tif_machdep.c			\
	libtiff/tif_msdos.c			\
	libtiff/tif_next.c			\
	libtiff/tif_open.c			\
	libtiff/tif_packbits.c			\
	libtiff/tif_print.c			\
	libtiff/tif_read.c			\
	libtiff/tif_strip.c			\
	libtiff/tif_swab.c			\
	libtiff/tif_thunder.c			\
	libtiff/tif_tile.c			\
	libtiff/tif_unix.c			\
	libtiff/tif_version.c			\
	libtiff/tif_vms.c			\
	libtiff/tif_warning.c			\
	libtiff/tif_write.c			\
	tools/Makefile.aix			\
	tools/Makefile.apollo			\
	tools/Makefile.mpw			\
	tools/Makefile.next			\
	tools/Makefile.sco			\
	tools/Makefile.sgi			\
	tools/Makefile.sun			\
	tools/fax2tiff.c			\
	tools/getopt.c				\
	tools/gif2tiff.c			\
	tools/pal2rgb.c				\
	tools/ppm2tiff.c			\
	tools/ras2tiff.c			\
	tools/rasterfile.h			\
	tools/rgb2ycbcr.c			\
	tools/sgi2tiff.c			\
	tools/sgigt.c				\
	tools/sgisv.c				\
	tools/tiff2bw.c				\
	tools/tiff2ps.c				\
	tools/tiffcmp.c				\
	tools/tiffcp.c				\
	tools/tiffdither.c			\
	tools/tiffdump.c			\
	tools/tiffinfo.c			\
	tools/tiffmedian.c			\
	tools/tiffsplit.c			\
	tools/ycbcr.c				\
	man/man1/fax2tiff.1			\
	man/man1/gif2tiff.1			\
	man/man1/pal2rgb.1			\
	man/man1/ppm2tiff.1			\
	man/man1/ras2tiff.1			\
	man/man1/rgb2ycbcr.1			\
	man/man1/sgi2tiff.1			\
	man/man1/tiff2bw.1			\
	man/man1/tiff2ps.1			\
	man/man1/tiffcmp.1			\
	man/man1/tiffcp.1			\
	man/man1/tiffdither.1			\
	man/man1/tiffdump.1			\
	man/man1/tiffgt.1			\
	man/man1/tiffinfo.1			\
	man/man1/tiffmedian.1			\
	man/man1/tiffsplit.1			\
	man/man1/tiffsv.1			\
	man/man3/TIFFClose.3t			\
	man/man3/TIFFError.3t			\
	man/man3/TIFFFlush.3t			\
	man/man3/TIFFGetField.3t		\
	man/man3/TIFFOpen.3t			\
	man/man3/TIFFPrintDirectory.3t		\
	man/man3/TIFFReadDirectory.3t		\
	man/man3/TIFFReadEncodedStrip.3t	\
	man/man3/TIFFReadEncodedTile.3t		\
	man/man3/TIFFReadRGBAImage.3t		\
	man/man3/TIFFReadRawStrip.3t		\
	man/man3/TIFFReadRawTile.3t		\
	man/man3/TIFFReadScanline.3t		\
	man/man3/TIFFReadTile.3t		\
	man/man3/TIFFSetDirectory.3t		\
	man/man3/TIFFSetField.3t		\
	man/man3/TIFFWarning.3t			\
	man/man3/TIFFWriteDirectory.3t 		\
	man/man3/TIFFWriteEncodedStrip.3t	\
	man/man3/TIFFWriteEncodedTile.3t	\
	man/man3/TIFFWriteRawStrip.3t		\
	man/man3/TIFFWriteRawTile.3t		\
	man/man3/TIFFWriteScanline.3t		\
	man/man3/intro.3t			\
	man/man3/query.3t			\
	man/man3/size.3t			\
	man/man3/strip.3t			\
	man/man3/swab.3t			\
	man/man3/tile.3t			\
	${NULL}
CONTRIBFILES=\
	contrib/dbs/README			\
	    contrib/dbs/Imakefile		\
	    contrib/dbs/tiff-bi.c		\
	    contrib/dbs/tiff-grayscale.c	\
	    contrib/dbs/tiff-palette.c		\
	    contrib/dbs/tiff-rgb.c		\
	    contrib/dbs/xtiff/README		\
		contrib/dbs/xtiff/Imakefile	\
		contrib/dbs/xtiff/patchlevel.h	\
		contrib/dbs/xtiff/xtiff.c	\
		contrib/dbs/xtiff/xtifficon.h	\
	contrib/fax2ps/README 			\
	    contrib/fax2ps/Makefile 		\
	    contrib/fax2ps/defs.h		\
	    contrib/fax2ps/fax2ps.c		\
	    contrib/fax2ps/faxdecode.c		\
	    contrib/fax2ps/fax2ps.1 		\
	contrib/ras/ras2tif.c			\
	contrib/ras/tif2ras.c			\
	contrib/vms/readme.vms			\
	    contrib/vms/libtiff/makevms.com	\
	    contrib/vms/libtiff/tiff.opt	\
	    contrib/vms/libtiff/tiffshr.opt	\
	    contrib/vms/libtiff/tiffvec.mar	\
	    contrib/vms/tools/makevms.com	\
	${NULL}
DOCFILES=\
	doc/ClassF.txt				\
	doc/TIFF6.ps				\
	${NULL}
DISTFILES=\
	${TIFFFILES}				\
	tiff.alpha				\
	${CONTRIBFILES}				\
	${NULL}

default:
	-cd libtiff; ${MAKE}
	-cd tools; ${MAKE}
	-cd contrib/fax2ps; ${MAKE}
	-cd contrib/dbs; ${MAKE}

clean:
	-cd libtiff; ${MAKE} clean; rm -f Makefile
	-cd tools; ${MAKE} clean; rm -f Makefile
	-cd contrib/fax2ps; ${MAKE} clean
	-cd contrib/dbs; ${MAKE} clean
	-cd contrib/dbs/xtiff; ${MAKE} clean
	-cd man/man3; ${MAKE} clean

#
# These rules are used to create the source distribution images
#
rcsclean:
	rcsclean ${TIFFFILES} && co ${TIFFFILES}

alpha:
	${MAKE} alpha.stamp
	${MAKE} alpha.tar

# stamp relevant files according to current alpha
alpha.stamp:
	VERSION=`awk '{print "Alpha" $$3}' tiff.alpha`;			\
	NOW=`date`;							\
	for i in ${TIFFFILES}; do					\
	    REV=`rlog -h -d"$$NOW" $$i|fgrep 'head:'|awk '{print $$2}'`;\
	    rcs "-N$$VERSION:$$REV" "-sExp:$$REV" $$i && co -sExp $$i;	\
	done

purge.alpha.stamp:
	VERSION=`awk '{print "Alpha" $$3}' tiff.alpha`;			\
	NOW=`date`;							\
	for i in ${TIFFFILES}; do					\
	    REV=`rlog -h -d"$$NOW" $$i|fgrep 'head:'|awk '{print $$2}'`;\
	    rcs "-n$$VERSION" "-sExp:$$REV" $$i && co $$i;		\
	done

# create alpha distribution archive
alpha.tar:
	VERSION=`awk '{print "alpha"$$3}' tiff.alpha`;			\
	rm -f $$VERSION.tar.Z;						\
	tar cvf - ${DISTFILES} | compress > $$VERSION.tar.Z

release:
	${MAKE} clobber
	${MAKE} release.stamp
	${MAKE} release.tar

release.stamp:
	VERSION="Release`sed 's/\./_/g' VERSION`";			\
	NOW=`date`;							\
	for i in ${TIFFFILES}; do					\
	    REV=`rlog -h -d"$$NOW" $$i|fgrep 'head:'|awk '{print $$2}'`;\
	    rcs "-N$$VERSION:$$REV" "-sRel:$$REV" $$i && co -sRel $$i;	\
	done

# create release distribution archive
release.tar:
	VERSION=v`cat VERSION`;						\
	rm -f $$VERSION.src.tar.Z;					\
	tar cvf - ${DISTFILES} | compress > $$VERSION.src.tar.Z

PICS=\
	pics/README		 \
	pics/cramps.tif		 \
	pics/cramps-tile.tif	\
	pics/fax2d.tif		\
	pics/g3test.tif		\
	pics/jello.tif		\
	pics/jim___cg.tif	\
	pics/jim___dg.tif	\
	pics/jim___gg.tif	\
	pics/jim___ah.tif	\
	pics/strike.tif		\
	pics/oxford.tif		\
	pics/quad-lzw.tif	\
	pics/quad-tile.tif	\
	pics/text.tif		\
	pics/ycbcr-cat.tif	\
	pics/smallliz.tif	\
	pics/zackthecat.tif	\
	pics/fax2d.g3		\
	pics/g3test.g3		\
	${NULL}

pics.tar:
	tar cvf - ${PICS} | compress > tiffpics.tar.Z
