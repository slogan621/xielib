#ifndef lint
static char rcsid[] = "$Header: /usr/people/sam/tiff/tools/RCS/sgi2tiff.c,v 1.8 93/08/26 15:09:38 sam Exp $";
#endif

/*
 * Copyright (c) 1991, 1992 Sam Leffler
 * Copyright (c) 1991, 1992 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and 
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 * 
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  
 * 
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 
 * OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gl/image.h>

#include "tiffio.h"

#define	streq(a,b)	(strcmp(a,b) == 0)

static	short config = PLANARCONFIG_CONTIG;
static	uint16 compression = COMPRESSION_LZW;
static	uint16 predictor = 0;
static	uint16 fillorder = 0;
static	uint32 rowsperstrip = (uint32) -1;

static	void usage(void);
static	int cpContig(IMAGE*, TIFF*);
static	int cpSeparate(IMAGE*, TIFF*);

/* XXX image library has no prototypes */
extern	IMAGE* iopen(const char*, const char*);
extern	void iclose(IMAGE*);
extern	void getrow(IMAGE*, short*, int, int);

void
main(int argc, char* argv[])
{
	IMAGE *in;
	TIFF *out;

	argc--, argv++;
	if (argc < 2)
		usage();
	for (; argc > 2 && argv[0][0] == '-'; argc--, argv++) {
		if (streq(argv[0], "-none")) {
			compression = COMPRESSION_NONE;
			continue;
		}
		if (streq(argv[0], "-packbits")) {
			compression = COMPRESSION_PACKBITS;
			continue;
		}
		if (streq(argv[0], "-lzw")) {
			compression = COMPRESSION_LZW;
			continue;
		}
		if (streq(argv[0], "-contig")) {
			config = PLANARCONFIG_CONTIG;
			continue;
		}
		if (streq(argv[0], "-separate")) {
			config = PLANARCONFIG_SEPARATE;
			continue;
		}
		if (streq(argv[0], "-lsb2msb")) {
			fillorder = FILLORDER_LSB2MSB;
			continue;
		}
		if (streq(argv[0], "-msb2lsb")) {
			fillorder = FILLORDER_MSB2LSB;
			continue;
		}
		if (streq(argv[0], "-rowsperstrip")) {
			argc--, argv++;
			rowsperstrip = atoi(argv[0]);
			continue;
		}
		if (streq(argv[0], "-predictor")) {
			argc--, argv++;
			predictor = atoi(argv[0]);
			continue;
		}
		usage();
	}
	in = iopen(argv[0], "r");
	if (in == NULL)
		exit(-1);
	out = TIFFOpen(argv[1], "w");
	if (out == NULL)
		exit(-2);
	TIFFSetField(out, TIFFTAG_IMAGEWIDTH, (uint32) in->xsize);
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, (uint32) in->ysize);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 8);
	TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
	if (predictor != 0)
		TIFFSetField(out, TIFFTAG_PREDICTOR, predictor);
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC,
	    in->zsize == 1 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB);
	if (fillorder != 0)
		TIFFSetField(out, TIFFTAG_FILLORDER, fillorder);
	TIFFSetField(out, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, in->zsize);
	TIFFSetField(out, TIFFTAG_MINSAMPLEVALUE, (uint16) in->min);
	TIFFSetField(out, TIFFTAG_MAXSAMPLEVALUE, (uint16) in->max);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, config);
	if (config != PLANARCONFIG_SEPARATE) {
		if (rowsperstrip <= 0)
			rowsperstrip = (8*1024)/TIFFScanlineSize(out);
		if (rowsperstrip == 0)
			rowsperstrip == 1;
		TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rowsperstrip);
	} else			/* force 1 row/strip for library limitation */
		TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, 1L);
	if (in->name[0] != '\0')
		TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, in->name);
	if (config == PLANARCONFIG_CONTIG)
		cpContig(in, out);
	else
		cpSeparate(in, out);
	(void) iclose(in);
	(void) TIFFClose(out);
	exit(0);
}

static int
cpContig(IMAGE* in, TIFF* out)
{
	unsigned char *buf = (unsigned char *)malloc(TIFFScanlineSize(out));
	short *r = NULL;
	int x, y;

	if (in->zsize == 3) {
		short *g, *b;

		r = (short *)malloc(3 * in->xsize * sizeof (short));
		g = r + in->xsize;
		b = g + in->xsize;
		for (y = in->ysize-1; y >= 0; y--) {
			unsigned char* pp = buf;

			getrow(in, r, y, 0);
			getrow(in, g, y, 1);
			getrow(in, b, y, 2);
			for (x = 0; x < in->xsize; x++) {
				*pp++ = r[x];
				*pp++ = g[x];
				*pp++ = b[x];
			}
			if (TIFFWriteScanline(out, buf, in->ysize-y-1, 0) < 0)
				goto bad;
		}
	} else {
		r = (short *)malloc(in->xsize * sizeof (short));
		for (y = in->ysize-1; y >= 0; y--) {
			getrow(in, r, y, 0);
			for (x = 0; x < in->xsize; x++)
				buf[x] = r[x];
			if (TIFFWriteScanline(out, buf, in->ysize-y-1, 0) < 0)
				goto bad;
		}
	}
done:
	if (r)
		free(r);
	free(buf);
	return (1);
bad:
	if (r)
		free(r);
	free(buf);
	return (0);
}

static int
cpSeparate(IMAGE* in, TIFF* out)
{
	unsigned char *buf = (unsigned char *)malloc(TIFFScanlineSize(out));
	short *r = (short *)malloc(in->xsize * sizeof (short));
	int x, y, z;

	for (z = 0; z < in->zsize; z++) {
		for (y = in->ysize-1; y >= 0; y--) {
			getrow(in, r, y, z);
			for (x = 0; x < in->xsize; x++)
				buf[x] = r[x];
			if (TIFFWriteScanline(out, buf, in->ysize-y-1, z) < 0)
				goto bad;
		}
	}
done:
	free(r);
	free(buf);
	return (1);
bad:
	free(r);
	free(buf);
	return (0);
}

static void
usage(void)
{
	fprintf(stderr, "usage: sgi2tiff [options] input output\n");
	fprintf(stderr, "where options are:\n");
	fprintf(stderr,
	    " -contig\tpack samples contiguously (e.g. RGBRGB...)\n");
	fprintf(stderr,
	    " -separate\tstore samples separately (e.g. RRR...GGG...BBB...)\n");
	fprintf(stderr, "\n");
	fprintf(stderr,
	    " -lzw\t\tcompress output with Lempel-Ziv & Welch encoding\n");
	fprintf(stderr,
	    " -packbits\tcompress output with packbits encoding\n");
	fprintf(stderr,
	    " -none\t\tuse no compression algorithm on output\n");
	fprintf(stderr, "\n");
	fprintf(stderr,
	    " -rowsperstrip #\tmake each strip have no more than # rows\n");
	exit(-1);
}
