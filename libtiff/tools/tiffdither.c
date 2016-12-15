#ifndef lint
static char rcsid[] = "$Header: /usr/people/sam/tiff/tools/RCS/tiffdither.c,v 1.13 93/08/26 15:12:04 sam Exp $";
#endif

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992 Sam Leffler
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

#include "tiffio.h"

#define	CopyField(tag, v) \
	if (TIFFGetField(in, tag, &v)) TIFFSetField(out, tag, v)

uint32	imagewidth;
uint32	imagelength;
int	threshold = 128;

static void
usage(void)
{
	fprintf(stderr,
	    "usage: tiffdither [-f24op] [-t threshold] in.tif out.tif.\n");
	exit(-1);
}

/* 
 * Floyd-Steinberg error propragation with threshold.
 * This code is stolen from tiffmedian.
 */
static void
fsdither(TIFF* in, TIFF* out)
{
	unsigned char *outline, *inputline, *inptr;
	short *thisline, *nextline, *tmpptr;
	register unsigned char	*outptr;
	register short *thisptr, *nextptr;
	register uint32 i, j;
	uint32 imax, jmax;
	int lastline, lastpixel;
	int bit;
	tsize_t outlinesize;

	imax = imagelength - 1;
	jmax = imagewidth - 1;
	inputline = (unsigned char *)malloc(TIFFScanlineSize(in));
	thisline = (short *)malloc(imagewidth * sizeof (short));
	nextline = (short *)malloc(imagewidth * sizeof (short));
	outlinesize = TIFFScanlineSize(out);
	outline = (unsigned char *) malloc(outlinesize);

	/*
	 * Get first line
	 */
	if (TIFFReadScanline(in, inputline, 0, 0) <= 0)
		return;
	inptr = inputline;
	nextptr = nextline;
	for (j = 0; j < imagewidth; ++j)
		*nextptr++ = *inptr++;
	for (i = 1; i < imagelength; ++i) {
		tmpptr = thisline;
		thisline = nextline;
		nextline = tmpptr;
		lastline = (i == imax);
		if (TIFFReadScanline(in, inputline, i, 0) <= 0)
			break;
		inptr = inputline;
		nextptr = nextline;
		for (j = 0; j < imagewidth; ++j)
			*nextptr++ = *inptr++;
		thisptr = thisline;
		nextptr = nextline;
		memset(outptr = outline, 0, outlinesize);
		bit = 0x80;
		for (j = 0; j < imagewidth; ++j) {
			register int v;

			lastpixel = (j == jmax);
			v = *thisptr++;
			if (v < 0)
				v = 0;
			else if (v > 255)
				v = 255;
			if (v > threshold) {
				*outptr |= bit;
				v -= 255;
			}
			bit >>= 1;
			if (bit == 0) {
				outptr++;
				bit = 0x80;
			}
			if (!lastpixel)
				thisptr[0] += v * 7 / 16;
			if (!lastline) {
				if (j != 0)
					nextptr[-1] += v * 3 / 16;
				*nextptr++ += v * 5 / 16;
				if (!lastpixel)
					nextptr[0] += v / 16;
			}
		}
		if (TIFFWriteScanline(out, outline, i-1, 0) < 0)
			break;
	}
	free(inputline);
	free(thisline);
	free(nextline);
	free(outline);
}

void
main(int argc, char* argv[])
{
	TIFF *in, *out;
	uint16 samplesperpixel, bitspersample = 1, shortv, config;
	float floatv;
	char thing[1024];
	uint32 rowsperstrip;
	uint16 compression = COMPRESSION_LZW;
	int onestrip = 0;
	uint16 fillorder = FILLORDER_LSB2MSB;
	uint32 group3options = GROUP3OPT_FILLBITS;
	int c;
	extern int optind;
	extern char *optarg;

	while ((c = getopt(argc, argv, "t:f24ops")) != -1)
		switch (c) {
		case 't':
			threshold = atoi(optarg);
			if (threshold < 0)
				threshold = 0;
			else if (threshold > 255)
				threshold = 255;
			break;
		case 'f':		/* CCITT Group 3 */
			compression = COMPRESSION_CCITTFAX3;
			onestrip = 1;
			break;
		case '4':		/* CCITT Group 4 */
			compression = COMPRESSION_CCITTFAX4;
			onestrip = 1;
			break;
		case 'o':		/* reverse bit ordering in output */
			fillorder = FILLORDER_LSB2MSB;
			break;
		case '2':		/* 2d-encoding */
			group3options |= GROUP3OPT_2DENCODING;
			break;
		case 'p':		/* zero pad scanlines before EOL */
			group3options &= ~GROUP3OPT_FILLBITS;
			break;
		case '?':
			usage();
			/*NOTREACHED*/
		}
	if (argc - optind < 2)
		usage();
	in = TIFFOpen(argv[optind], "r");
	if (in == NULL)
		exit(-1);
	TIFFGetField(in, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
	if (samplesperpixel != 1) {
		fprintf(stderr, "%s: Not a b&w image.\n", argv[0]);
		exit(-1);
	}
	TIFFGetField(in, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	if (bitspersample != 8) {
		fprintf(stderr,
		    " %s: Sorry, only handle 8-bit samples.\n", argv[0]);
		exit(-1);
	}
	out = TIFFOpen(argv[optind+1], "w");
	if (out == NULL)
		exit(-1);
	CopyField(TIFFTAG_IMAGEWIDTH, imagewidth);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &imagelength);
	TIFFSetField(out, TIFFTAG_IMAGELENGTH, imagelength-1);
	TIFFSetField(out, TIFFTAG_BITSPERSAMPLE, 1);
	TIFFSetField(out, TIFFTAG_SAMPLESPERPIXEL, 1);
	TIFFSetField(out, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
	TIFFSetField(out, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
	TIFFSetField(out, TIFFTAG_FILLORDER, fillorder);
	sprintf(thing, "Dithered B&W version of %s", argv[optind]);
	TIFFSetField(out, TIFFTAG_IMAGEDESCRIPTION, thing);
	CopyField(TIFFTAG_ORIENTATION, shortv);
	CopyField(TIFFTAG_XRESOLUTION, floatv);
	CopyField(TIFFTAG_YRESOLUTION, floatv);
	CopyField(TIFFTAG_RESOLUTIONUNIT, shortv);
	if (onestrip)
		rowsperstrip = imagelength-1;
	else
		rowsperstrip = (8*1024)/TIFFScanlineSize(out);
	TIFFSetField(out, TIFFTAG_ROWSPERSTRIP,
	    rowsperstrip == 0 ? 1 : rowsperstrip);
	if (compression == COMPRESSION_CCITTFAX3 && group3options)
		TIFFSetField(out, TIFFTAG_GROUP3OPTIONS, group3options);
	fsdither(in, out);
	TIFFClose(in);
	TIFFClose(out);
}
