#	$Header: /usr/people/sam/tiff/contrib/fax2ps/RCS/Makefile,v 1.18 92/10/21 16:33:30 sam Exp $
#
# Copyright (c) 1991, 1992 Sam Leffler
# Copyright (c) 1991, 1992 Silicon Graphics, Inc.
# 
# Permission to use, copy, modify, distribute, and sell this software and 
# its documentation for any purpose is hereby granted without fee, provided
# that (i) the above copyright notices and this permission notice appear in
# all copies of the software and related documentation, and (ii) the names of
# Sam Leffler and Silicon Graphics may not be used in any advertising or
# publicity relating to the software without the specific, prior written
# permission of Stanford and Silicon Graphics.
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
TIFF=	../../libtiff
TOOLS=	../../tools

BIN=	/usr/local/bin
NULL=
OBJS=	fax2ps.o faxdecode.o getopt.o
CONF_LIBRARY=\
	${NULL}
COPTS=	-O
CFLAGS=	${COPTS} -ansi ${CONF_LIBRARY} -I${TIFF}
LIBS=	${TIFF}/libtiff.a

all:	fax2ps

fax2ps:	${OBJS} ${LIBS}
	${CC} -o $@ ${OBJS} ${LDFLAGS} ${LIBS} -lm

${OBJS}: defs.h ${TIFF}/tiffio.h

getopt.o: ${TOOLS}/getopt.c
	${CC} -o getopt.o -c ${CFLAGS} ${TOOLS}/getopt.c

clean:
	rm -f ${OBJS} fax2ps a.out core errs

install: all
	/etc/install -f ${BIN} -m 755 fax2ps
