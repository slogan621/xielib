#ifndef lint
static char rcsid[] = "$Header: /usr/people/sam/tiff/tools/RCS/tiffcp.c,v 1.24 93/08/26 18:10:05 sam Exp $";
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

typedef	unsigned char u_char;

#if defined(VMS)
#define unlink delete
#endif

#define	streq(a,b)	(strcmp(a,b) == 0)
#define	CopyField(tag, v) \
    if (TIFFGetField(in, tag, &v)) TIFFSetField(out, tag, v)
#define	CopyField2(tag, v1, v2) \
    if (TIFFGetField(in, tag, &v1, &v2)) TIFFSetField(out, tag, v1, v2)
#define	CopyField3(tag, v1, v2, v3) \
    if (TIFFGetField(in, tag, &v1, &v2, &v3)) TIFFSetField(out, tag, v1, v2, v3)
#define	CopyField4(tag, v1, v2, v3, v4) \
    if (TIFFGetField(in, tag, &v1, &v2, &v3, &v4)) TIFFSetField(out, tag, v1, v2, v3, v4)

#define	TRUE	1
#define	FALSE	0

static  int outtiled = -1;
static  uint32 tilewidth;
static  uint32 tilelength;

static	uint16 config;
static	uint16 compression;
static	uint16 predictor;
static	uint16 fillorder;
static	uint32 rowsperstrip;
static	uint32 g3opts;
static	int ignore = FALSE;		/* if true, ignore read errors */
static	char *filename;

static	int tiffcp(TIFF*, TIFF*);
static	void usage(void);

void
main(int argc, char* argv[])
{
	uint16 defconfig = -1;
	uint16 defcompression = -1;
	uint16 defpredictor = -1;
	uint16 deffillorder = 0;
	uint32 deftilewidth = -1;
	uint32 deftilelength = -1;
	uint32 defrowsperstrip = -1;
	uint32 defg3opts = -1;
	TIFF *in, *out;

	argc--, argv++;
	if (argc < 2)
		usage();
	for (; argc > 2 && argv[0][0] == '-'; argc--, argv++) {
		if (streq(argv[0], "-none")) {
			defcompression = COMPRESSION_NONE;
			continue;
		}
		if (streq(argv[0], "-packbits")) {
			defcompression = COMPRESSION_PACKBITS;
			continue;
		}
		if (streq(argv[0], "-lzw")) {
			defcompression = COMPRESSION_LZW;
			continue;
		}
		if (streq(argv[0], "-g3")) {
			defcompression = COMPRESSION_CCITTFAX3;
			continue;
		}
		if (streq(argv[0], "-g4")) {
			defcompression = COMPRESSION_CCITTFAX4;
			continue;
		}
		if (streq(argv[0], "-contig")) {
			defconfig = PLANARCONFIG_CONTIG;
			continue;
		}
		if (streq(argv[0], "-separate")) {
			defconfig = PLANARCONFIG_SEPARATE;
			continue;
		}
		if (streq(argv[0], "-lsb2msb")) {
			deffillorder = FILLORDER_LSB2MSB;
			continue;
		}
		if (streq(argv[0], "-msb2lsb")) {
			deffillorder = FILLORDER_MSB2LSB;
			continue;
		}
		if (streq(argv[0], "-rowsperstrip")) {
			argc--, argv++;
			defrowsperstrip = atoi(argv[0]);
			continue;
		}
		if (streq(argv[0], "-1d")) {
			if (defg3opts == -1)
				defg3opts = 0;
			defg3opts &= ~GROUP3OPT_2DENCODING;
			continue;
		}
		if (streq(argv[0], "-2d")) {
			if (defg3opts == -1)
				defg3opts = 0;
			defg3opts |= GROUP3OPT_2DENCODING;
			continue;
		}
		if (streq(argv[0], "-fill")) {
			if (defg3opts == -1)
				defg3opts = 0;
			defg3opts |= GROUP3OPT_FILLBITS;
			continue;
		}
		if (streq(argv[0], "-predictor")) {
			argc--, argv++;
			defpredictor = atoi(argv[0]);
			continue;
		}
		if (streq(argv[0], "-strips")) {
			outtiled = FALSE;
			continue;
		}
		if (streq(argv[0], "-tiles")) {
			outtiled = TRUE;
			continue;
		}
		if (streq(argv[0], "-tilewidth")) {
			argc--, argv++;
			outtiled = TRUE;
			deftilewidth = atoi(argv[0]);
			continue;
		}
		if (streq(argv[0], "-tilelength")) {
			argc--, argv++;
			outtiled = TRUE;
			deftilelength = atoi(argv[0]);
			continue;
		}
		if (streq(argv[0], "-ignore")) {
			ignore = TRUE;
			continue;
		}
		usage();
	}
	out = TIFFOpen(argv[argc-1], "w");
	if (out == NULL)
		exit(-2);
	for (; argc > 1; argc--, argv++) {
		in = TIFFOpen(filename = argv[0], "r");
		if (in != NULL) {
			do {
				config = defconfig;
				compression = defcompression;
				predictor = defpredictor;
				fillorder = deffillorder;
				rowsperstrip = defrowsperstrip;
				tilewidth = deftilewidth;
				tilelength = deftilelength;
				g3opts = defg3opts;
				if (!tiffcp(in, out) || !TIFFWriteDirectory(out)) {
					(void) TIFFClose(out);
					exit(1);
				}
			} while (TIFFReadDirectory(in));
			(void) TIFFClose(in);
		}
	}
	(void) TIFFClose(out);
	exit(0);
}

char* stuff[] = {
"usage: tiffcp [options] input... output",
"where options are:",
" -contig	pack samples contiguously (e.g. RGBRGB...)",
" -separate	store samples separately (e.g. RRR...GGG...BBB...)",
" -strips	write output in strips",
" -tiles	write output in tiles",
" -ignore	ignore read errors",
"",
" -rowsperstrip #	make each strip have no more than # rows",
" -tilewidth #	set output tile width (pixels)",
" -tilelength #	set output tile length (pixels)",
"",
" -lsb2msb	force lsb-to-msb FillOrder for output",
" -msb2lsb	force msb-to-lsb FillOrder for output",
"",
" -lzw		compress output with Lempel-Ziv & Welch encoding",
" -packbits	compress output with packbits encoding",
" -g3		compress output with CCITT Group 3 encoding",
" -g4		compress output with CCITT Group 4 encoding",
" -none		use no compression algorithm on output",
"",
" -1d		use CCITT Group 3 1D encoding",
" -2d		use CCITT Group 3 2D encoding",
" -fill		zero-fill scanlines with CCITT Group 3 encoding",
"",
" -predictor	set predictor value for Lempel-Ziv & Welch encoding",
NULL
};

static void
usage(void)
{
	char buf[BUFSIZ];
	int i;

	setbuf(stderr, buf);
	for (i = 0; stuff[i] != NULL; i++)
		fprintf(stderr, "%s\n", stuff[i]);
	exit(-1);
}

static void
CheckAndCorrectColormap(int n, uint16* r, uint16* g, uint16* b)
{
	int i;

	for (i = 0; i < n; i++)
		if (r[i] >= 256 || g[i] >= 256 || b[i] >= 256)
			return;
	TIFFWarning(filename, "Scaling 8-bit colormap");
#define	CVT(x)		(((x) * ((1L<<16)-1)) / 255)
	for (i = 0; i < n; i++) {
		r[i] = CVT(r[i]);
		g[i] = CVT(g[i]);
		b[i] = CVT(b[i]);
	}
#undef CVT
}

static struct cpTag {
	uint16	tag;
	uint16	count;
	TIFFDataType type;
} tags[] = {
	{ TIFFTAG_SUBFILETYPE,		1, TIFF_LONG },
	{ TIFFTAG_PHOTOMETRIC,		1, TIFF_SHORT },
	{ TIFFTAG_THRESHHOLDING,	1, TIFF_SHORT },
	{ TIFFTAG_DOCUMENTNAME,		1, TIFF_ASCII },
	{ TIFFTAG_IMAGEDESCRIPTION,	1, TIFF_ASCII },
	{ TIFFTAG_MAKE,			1, TIFF_ASCII },
	{ TIFFTAG_MODEL,		1, TIFF_ASCII },
	{ TIFFTAG_ORIENTATION,		1, TIFF_SHORT },
	{ TIFFTAG_MINSAMPLEVALUE,	1, TIFF_SHORT },
	{ TIFFTAG_MAXSAMPLEVALUE,	1, TIFF_SHORT },
	{ TIFFTAG_XRESOLUTION,		1, TIFF_RATIONAL },
	{ TIFFTAG_YRESOLUTION,		1, TIFF_RATIONAL },
	{ TIFFTAG_PAGENAME,		1, TIFF_ASCII },
	{ TIFFTAG_XPOSITION,		1, TIFF_RATIONAL },
	{ TIFFTAG_YPOSITION,		1, TIFF_RATIONAL },
	{ TIFFTAG_GROUP4OPTIONS,	1, TIFF_LONG },
	{ TIFFTAG_RESOLUTIONUNIT,	1, TIFF_SHORT },
	{ TIFFTAG_PAGENUMBER,		2, TIFF_SHORT },
	{ TIFFTAG_SOFTWARE,		1, TIFF_ASCII },
	{ TIFFTAG_DATETIME,		1, TIFF_ASCII },
	{ TIFFTAG_ARTIST,		1, TIFF_ASCII },
	{ TIFFTAG_HOSTCOMPUTER,		1, TIFF_ASCII },
	{ TIFFTAG_WHITEPOINT,		1, TIFF_RATIONAL },
	{ TIFFTAG_PRIMARYCHROMATICITIES,-1,TIFF_RATIONAL },
	{ TIFFTAG_HALFTONEHINTS,	2, TIFF_SHORT },
	{ TIFFTAG_BADFAXLINES,		1, TIFF_LONG },
	{ TIFFTAG_CLEANFAXDATA,		1, TIFF_SHORT },
	{ TIFFTAG_CONSECUTIVEBADFAXLINES,1, TIFF_LONG },
	{ TIFFTAG_INKSET,		1, TIFF_SHORT },
	{ TIFFTAG_INKNAMES,		1, TIFF_ASCII },
	{ TIFFTAG_DOTRANGE,		2, TIFF_SHORT },
	{ TIFFTAG_TARGETPRINTER,	1, TIFF_ASCII },
	{ TIFFTAG_SAMPLEFORMAT,		1, TIFF_SHORT },
	{ TIFFTAG_YCBCRCOEFFICIENTS,	-1,TIFF_RATIONAL },
	{ TIFFTAG_YCBCRSUBSAMPLING,	2, TIFF_SHORT },
	{ TIFFTAG_YCBCRPOSITIONING,	1, TIFF_SHORT },
	{ TIFFTAG_REFERENCEBLACKWHITE,	-1,TIFF_RATIONAL },
	{ TIFFTAG_EXTRASAMPLES,		-1, TIFF_SHORT },
};
#define	NTAGS	(sizeof (tags) / sizeof (tags[0]))

static void
cpOtherTags(TIFF* in, TIFF* out)
{
	struct cpTag *p;
	uint16 shortv, shortv2, *shortav;
	float floatv, *floatav;
	char *stringv;
	uint32 longv;

	for (p = tags; p < &tags[NTAGS]; p++)
		switch (p->type) {
		case TIFF_SHORT:
			if (p->count == 1) {
				CopyField(p->tag, shortv);
			} else if (p->count == 2) {
				CopyField2(p->tag, shortv, shortv2);
			} else if (p->count == (uint16) -1) {
				CopyField2(p->tag, shortv, shortav);
			}
			break;
		case TIFF_LONG:
			CopyField(p->tag, longv);
			break;
		case TIFF_RATIONAL:
			if (p->count == 1) {
				CopyField(p->tag, floatv);
			} else if (p->count == (uint16) -1) {
				CopyField(p->tag, floatav);
			}
			break;
		case TIFF_ASCII:
			CopyField(p->tag, stringv);
			break;
		}
}

typedef int (*copyFunc)
    (TIFF* in, TIFF* out, uint32 l, uint32 w, uint16 samplesperpixel);
static	copyFunc pickCopyFunc(TIFF*, TIFF*, uint16, uint16);

static int
tiffcp(TIFF* in, TIFF* out)
{
	uint16 bitspersample, samplesperpixel, shortv;
	copyFunc cf;
	uint32 w, l;

	CopyField(TIFFTAG_IMAGEWIDTH, w);
	CopyField(TIFFTAG_IMAGELENGTH, l);
	CopyField(TIFFTAG_BITSPERSAMPLE, bitspersample);
	if (compression != (uint16)-1)
		TIFFSetField(out, TIFFTAG_COMPRESSION, compression);
	else
		CopyField(TIFFTAG_COMPRESSION, compression);
	if (fillorder != 0)
		TIFFSetField(out, TIFFTAG_FILLORDER, fillorder);
	else
		CopyField(TIFFTAG_FILLORDER, shortv);
	CopyField(TIFFTAG_SAMPLESPERPIXEL, samplesperpixel);
	/*
	 * Choose tiles/strip for the output image according to
	 * the command line arguments (-tiles, -strips) and the
	 * structure of the input image.
	 */
	if (outtiled == -1)
		outtiled = TIFFIsTiled(in);
	if (outtiled) {
		/*
		 * Setup output file's tile width&height.  If either
		 * is not specified, it defaults to 256.
		 */
		if (tilewidth < 0 &&
		    !TIFFGetField(in, TIFFTAG_TILEWIDTH, &tilewidth))
			tilewidth = 256;
		if (tilelength < 0 &&
		    !TIFFGetField(in, TIFFTAG_TILELENGTH, &tilelength))
			tilelength = 256;
		TIFFSetField(out, TIFFTAG_TILEWIDTH, tilewidth);
		TIFFSetField(out, TIFFTAG_TILELENGTH, tilelength);
	} else {
		/*
		 * RowsPerStrip is left unspecified: use either the
		 * value from the input image or, if nothing is defined,
		 * something that approximates 8Kbyte strips.
		 */
		if (rowsperstrip < 0 &&
		    !TIFFGetField(in, TIFFTAG_ROWSPERSTRIP, &rowsperstrip))
			rowsperstrip = (8*1024)/TIFFScanlineSize(out);
		if (rowsperstrip == 0)
			rowsperstrip = 1L;
		TIFFSetField(out, TIFFTAG_ROWSPERSTRIP, rowsperstrip);
	}
	if (config != (uint16) -1)
		TIFFSetField(out, TIFFTAG_PLANARCONFIG, config);
	else
		CopyField(TIFFTAG_PLANARCONFIG, config);
	if (g3opts != -1)
		TIFFSetField(out, TIFFTAG_GROUP3OPTIONS, g3opts);
	else
		CopyField(TIFFTAG_GROUP3OPTIONS, g3opts);
	if (samplesperpixel <= 4) {
		uint16 *tr, *tg, *tb, *ta;
		CopyField4(TIFFTAG_TRANSFERFUNCTION, tr, tg, tb, ta);
	}
	if (predictor != (uint16)-1)
		TIFFSetField(out, TIFFTAG_PREDICTOR, predictor);
	else
		CopyField(TIFFTAG_PREDICTOR, predictor);
	{ uint16 *red, *green, *blue;
	  if (TIFFGetField(in, TIFFTAG_COLORMAP, &red, &green, &blue)) {
		CheckAndCorrectColormap(1<<bitspersample, red, green, blue);
		TIFFSetField(out, TIFFTAG_COLORMAP, red, green, blue);
	  }
	}
/* SMinSampleValue & SMaxSampleValue */
/* JPEG stuff */
	cpOtherTags(in, out);

	cf = pickCopyFunc(in, out, bitspersample, samplesperpixel);
	return (cf ? (*cf)(in, out, l, w, samplesperpixel) : FALSE);
}

/*
 * Copy Functions.
 */
#define	DECLAREcpFunc(x) \
static int x(TIFF* in, TIFF* out, \
    uint32 imagelength, uint32 imagewidth, tsample_t spp)

#define	DECLAREreadFunc(x) \
static void x(TIFF* in, \
    u_char* buf, uint32 imagelength, uint32 imagewidth, tsample_t spp)
typedef void (*readFunc)(TIFF*, u_char*, uint32, uint32, tsample_t);

#define	DECLAREwriteFunc(x) \
static int x(TIFF* out, \
    u_char* buf, uint32 imagelength, uint32 imagewidth, tsample_t spp)
typedef int (*writeFunc)(TIFF*, u_char*, uint32, uint32, tsample_t);

/*
 * Contig -> contig by scanline for rows/strip change.
 */
DECLAREcpFunc(cpContig2ContigByRow)
{
	u_char *buf = (u_char *)malloc(TIFFScanlineSize(in));
	uint32 row;

	for (row = 0; row < imagelength; row++) {
		if (TIFFReadScanline(in, buf, row, 0) < 0 && !ignore)
			goto done;
		if (TIFFWriteScanline(out, buf, row, 0) < 0)
			goto bad;
	}
done:
	free(buf);
	return (TRUE);
bad:
	free(buf);
	return (FALSE);
}

/*
 * Strip -> strip for change in encoding.
 */
DECLAREcpFunc(cpDecodedStrips)
{
	tsize_t stripsize  = TIFFStripSize(in);
	u_char *buf = (u_char *)malloc(stripsize);

	if (buf) {
		tstrip_t s, ns = TIFFNumberOfStrips(in);
		uint32 row = 0;
		for (s = 0; s < ns; s++) {
			tsize_t cc = (row + rowsperstrip > imagelength) ?
			    TIFFVStripSize(in, imagelength - row) : stripsize;
			if (TIFFReadEncodedStrip(in, s, buf, cc) < 0 && !ignore)
				break;
			if (TIFFWriteEncodedStrip(out, s, buf, cc) < 0) {
				free(buf);
				return (FALSE);
			}
			row += rowsperstrip;
		}
		free(buf);
		return (TRUE);
	}
	return (FALSE);
}

/*
 * Separate -> separate by row for rows/strip change.
 */
DECLAREcpFunc(cpSeparate2SeparateByRow)
{
	u_char *buf = (u_char *)malloc(TIFFScanlineSize(in));
	uint32 row;
	tsample_t s;

	for (s = 0; s < spp; s++) {
		for (row = 0; row < imagelength; row++) {
			if (TIFFReadScanline(in, buf, row, s) < 0 && !ignore)
				goto done;
			if (TIFFWriteScanline(out, buf, row, s) < 0)
				goto bad;
		}
	}
done:
	free(buf);
	return (TRUE);
bad:
	free(buf);
	return (FALSE);
}

/*
 * Contig -> separate by row.
 */
DECLAREcpFunc(cpContig2SeparateByRow)
{
	u_char *inbuf = (u_char *)malloc(TIFFScanlineSize(in));
	u_char *outbuf = (u_char *)malloc(TIFFScanlineSize(out));
	register u_char *inp, *outp;
	register uint32 n;
	uint32 row;
	tsample_t s;

	/* unpack channels */
	for (s = 0; s < spp; s++) {
		for (row = 0; row < imagelength; row++) {
			if (TIFFReadScanline(in, inbuf, row, 0) < 0 && !ignore)
				goto done;
			inp = inbuf + s;
			outp = outbuf;
			for (n = imagewidth; n-- > 0;) {
				*outp++ = *inp;
				inp += spp;
			}
			if (TIFFWriteScanline(out, outbuf, row, s) < 0)
				goto bad;
		}
	}
done:
	if (inbuf) free(inbuf);
	if (outbuf) free(outbuf);
	return (TRUE);
bad:
	if (inbuf) free(inbuf);
	if (outbuf) free(outbuf);
	return (FALSE);
}

/*
 * Separate -> contig by row.
 */
DECLAREcpFunc(cpSeparate2ContigByRow)
{
	u_char *inbuf = (u_char *)malloc(TIFFScanlineSize(in));
	u_char *outbuf = (u_char *)malloc(TIFFScanlineSize(out));
	register u_char *inp, *outp;
	register uint32 n;
	uint32 row;
	tsample_t s;

	for (row = 0; row < imagelength; row++) {
		/* merge channels */
		for (s = 0; s < spp; s++) {
			if (TIFFReadScanline(in, inbuf, row, s) < 0 && !ignore)
				goto done;
			inp = inbuf;
			outp = outbuf + s;
			for (n = imagewidth; n-- > 0;) {
				*outp = *inp++;
				outp += spp;
			}
		}
		if (TIFFWriteScanline(out, outbuf, row, 0) < 0)
			goto bad;
	}
done:
	if (inbuf) free(inbuf);
	if (outbuf) free(outbuf);
	return (TRUE);
bad:
	if (inbuf) free(inbuf);
	if (outbuf) free(outbuf);
	return (FALSE);
}

static void
cpStripToTile(u_char* out, u_char* in,
	uint32 rows, uint32 cols, int outskew, int inskew)
{
	while (rows-- > 0) {
		uint32 j = cols;
		while (j-- > 0)
			*out++ = *in++;
		out += outskew;
		in += inskew;
	}
}

static void
cpContigBufToSeparateBuf(u_char* out, u_char* in,
	uint32 rows, uint32 cols, int outskew, int inskew, tsample_t spp)
{
	while (rows-- > 0) {
		uint32 j = cols;
		while (j-- > 0)
			*out++ = *in, in += spp;
		out += outskew;
		in += inskew;
	}
}

static int
cpImage(TIFF* in, TIFF* out, readFunc fin, writeFunc fout,
	uint32 imagelength, uint32 imagewidth, tsample_t spp)
{
	u_char *buf = (u_char *) malloc(TIFFScanlineSize(in) * imagelength);
	int status = FALSE;

	if (buf) {
		(*fin)(in, buf, imagelength, imagewidth, spp);
		status = (fout)(out, buf, imagelength, imagewidth, spp);
		free(buf);
	}
	return (status);
}

DECLAREreadFunc(readContigStripsIntoBuffer)
{
	tsize_t scanlinesize = TIFFScanlineSize(in);
     	u_char *bufp = buf;
	uint32 row;

	for (row = 0; row < imagelength; row++) {
		if (TIFFReadScanline(in, bufp, row, 0) < 0 && !ignore)
			break;
		bufp += scanlinesize;
	}
}

DECLAREreadFunc(readSeparateStripsIntoBuffer)
{
	tsize_t scanlinesize = TIFFScanlineSize(in);
	u_char* scanline = (u_char *) malloc(scanlinesize);

	if (scanline) {
		u_char *bufp = buf;
		uint32 row;
		tsample_t s;

		for (row = 0; row < imagelength; row++) {
			/* merge channels */
			for (s = 0; s < spp; s++) {
				u_char* sp = scanline;
				u_char* bp = bufp + s;
				tsize_t n = scanlinesize;

				if (TIFFReadScanline(in, sp, row, s) < 0 && !ignore)
					goto done;
				while (n-- > 0)
					*bp = *bufp++, bp += spp;
			}
			bufp += scanlinesize;
		}
done:
		free(scanline);
	}
}

DECLAREreadFunc(readContigTilesIntoBuffer)
{
	u_char* tilebuf = (u_char *) malloc(TIFFTileSize(in));
	uint32 imagew = TIFFScanlineSize(in);
	uint32 tilew  = TIFFTileRowSize(in);
	int iskew = imagew - tilew;
	u_char *bufp = buf;
	uint32 tw, tl;
	uint32 row;

	if (tilebuf == 0)
		return;
	(void) TIFFGetField(in, TIFFTAG_TILEWIDTH, &tw);
	(void) TIFFGetField(in, TIFFTAG_TILELENGTH, &tl);
	for (row = 0; row < imagelength; row += tl) {
		uint32 nrow = (row+tl > imagelength) ? imagelength-row : tl;
		uint32 colb = 0;
		uint32 col;

		for (col = 0; col < imagewidth; col += tw) {
			if (TIFFReadTile(in, tilebuf, col, row, 0, 0) < 0 &&
			    !ignore)
				goto done;
			if (colb + tilew > imagew) {
				uint32 width = imagew - colb;
				uint32 oskew = tilew - width;
				cpStripToTile(bufp + colb,
					tilebuf, nrow, width,
					oskew + iskew, oskew);
			} else
				cpStripToTile(bufp + colb,
					tilebuf, nrow, tilew,
					iskew, 0);
			colb += tilew;
		}
		bufp += imagew * nrow;
	}
done:
	free(tilebuf);
}

DECLAREreadFunc(readSeparateTilesIntoBuffer)
{
	uint32 imagew = TIFFScanlineSize(in);
	uint32 tilew = TIFFTileRowSize(in);
	int iskew  = imagew - tilew;
	u_char* tilebuf = (u_char *) malloc(TIFFTileSize(in));
	u_char *bufp = buf;
	uint32 tw, tl;
	uint32 row;

	if (tilebuf == 0)
		return;
	(void) TIFFGetField(in, TIFFTAG_TILEWIDTH, &tw);
	(void) TIFFGetField(in, TIFFTAG_TILELENGTH, &tl);
	for (row = 0; row < imagelength; row += tl) {
		uint32 nrow = (row+tl > imagelength) ? imagelength-row : tl;
		uint32 colb = 0;
		uint32 col;

		for (col = 0; col < imagewidth; col += tw) {
			tsample_t s;

			for (s = 0; s < spp; s++) {
				if (TIFFReadTile(in, tilebuf, col, row, 0, s) < 0 && !ignore)
					goto done;
				/*
				 * Tile is clipped horizontally.  Calculate
				 * visible portion and skewing factors.
				 */
				if (colb + tilew > imagew) {
					uint32 width = imagew - colb;
					int oskew = tilew - width;
					cpContigBufToSeparateBuf(bufp+colb+s,
					    tilebuf, nrow, width,
					    oskew + imagew, oskew/spp, spp);
				} else
					cpContigBufToSeparateBuf(bufp+colb+s,
					    tilebuf, nrow, tilewidth,
					    iskew, 0, spp);
			}
			colb += tilew;
		}
		bufp += imagew * nrow;
	}
done:
	free(tilebuf);
}

DECLAREwriteFunc(writeBufferToContigStrips)
{
	tsize_t scanline = TIFFScanlineSize(out);
	uint32 row;

	for (row = 0; row < imagelength; row++) {
		if (TIFFWriteScanline(out, buf, row, 0) < 0)
			return (FALSE);
		buf += scanline;
	}
	return (TRUE);
}

DECLAREwriteFunc(writeBufferToSeparateStrips)
{
	u_char *obuf = (u_char *) malloc(TIFFScanlineSize(out));
	tsample_t s;

	if (obuf == NULL)
		return (0);
	for (s = 0; s < spp; s++) {
		uint32 row;
		for (row = 0; row < imagelength; row++) {
			u_char* inp = buf + s;
			u_char* outp = obuf;
			uint32 n = imagewidth;

			while (n-- > 0)
				*outp++ = *inp, inp += spp;
			if (TIFFWriteScanline(out, obuf, row, s) < 0) {
				free(obuf);
				return (FALSE);
			}
		}
	}
	free(obuf);
	return (TRUE);

}

DECLAREwriteFunc(writeBufferToContigTiles)
{
	uint32 imagew = TIFFScanlineSize(out);
	uint32 tilew  = TIFFTileRowSize(out);
	int iskew = imagew - tilew;
	u_char* obuf = (u_char *) malloc(TIFFTileSize(out));
	u_char* bufp = buf;
	uint32 tl, tw;
	uint32 row;

	if (obuf == NULL)
		return (FALSE);
	(void) TIFFGetField(out, TIFFTAG_TILELENGTH, &tl);
	(void) TIFFGetField(out, TIFFTAG_TILEWIDTH, &tw);
	for (row = 0; row < imagelength; row += tilelength) {
		uint32 nrow = (row+tl > imagelength) ? imagelength-row : tl;
		uint32 colb = 0;
		uint32 col;

		for (col = 0; col < imagewidth; col += tw) {
			/*
			 * Tile is clipped horizontally.  Calculate
			 * visible portion and skewing factors.
			 */
			if (colb + tilew > imagew) {
				uint32 width = imagew - colb;
				int oskew = tilew - width;
				cpStripToTile(obuf, bufp + colb, nrow, width,
				    oskew, oskew + iskew);
			} else
				cpStripToTile(obuf, bufp + colb, nrow, tilew,
				    0, iskew);
			if (TIFFWriteTile(out, obuf, col, row, 0, 0) < 0) {
				free(obuf);
				return (FALSE);
			}
			colb += tilew;
		}
		bufp += nrow * imagew;
	}
	free(obuf);
	return (TRUE);
}

DECLAREwriteFunc(writeBufferToSeparateTiles)
{
	uint32 imagew = TIFFScanlineSize(out);
	tsize_t tilew  = TIFFTileRowSize(out);
	int iskew = imagew - tilew;
	u_char *obuf = (u_char*) malloc(TIFFTileSize(out));
	u_char *bufp = buf;
	uint32 tl, tw;
	uint32 row;

	if (obuf == NULL)
		return (FALSE);
	(void) TIFFGetField(out, TIFFTAG_TILELENGTH, &tl);
	(void) TIFFGetField(out, TIFFTAG_TILEWIDTH, &tw);
	for (row = 0; row < imagelength; row += tl) {
		uint32 nrow = (row+tl > imagelength) ? imagelength-row : tl;
		uint32 colb = 0;
		uint32 col;

		for (col = 0; col < imagewidth; col += tw) {
			tsample_t s;
			for (s = 0; s < spp; s++) {
				/*
				 * Tile is clipped horizontally.  Calculate
				 * visible portion and skewing factors.
				 */
				if (colb + tilew > imagew) {
					uint32 width = imagew - colb;
					int oskew = tilew - width;

					cpContigBufToSeparateBuf(obuf,
					    bufp + colb + s,
					    nrow, width,
					    oskew/spp, oskew + imagew, spp);
				} else
					cpContigBufToSeparateBuf(obuf,
					    bufp + colb + s,
					    nrow, tilewidth,
					    0, iskew, spp);
				if (TIFFWriteTile(out, obuf, col, row, 0, s) < 0) {
					free(obuf);
					return (FALSE);
				}
			}
			colb += tilew;
		}
		bufp += nrow * imagew;
	}
	free(obuf);
	return (TRUE);
}

/*
 * Contig strips -> contig tiles.
 */
DECLAREcpFunc(cpContigStrips2ContigTiles)
{
	return cpImage(in, out,
	    readContigStripsIntoBuffer,
	    writeBufferToContigTiles,
	    imagelength, imagewidth, spp);
}

/*
 * Contig strips -> separate tiles.
 */
DECLAREcpFunc(cpContigStrips2SeparateTiles)
{
	return cpImage(in, out,
	    readContigStripsIntoBuffer,
	    writeBufferToSeparateTiles,
	    imagelength, imagewidth, spp);
}

/*
 * Separate strips -> contig tiles.
 */
DECLAREcpFunc(cpSeparateStrips2ContigTiles)
{
	return cpImage(in, out,
	    readSeparateStripsIntoBuffer,
	    writeBufferToContigTiles,
	    imagelength, imagewidth, spp);
}

/*
 * Separate strips -> separate tiles.
 */
DECLAREcpFunc(cpSeparateStrips2SeparateTiles)
{
	return cpImage(in, out,
	    readSeparateStripsIntoBuffer,
	    writeBufferToSeparateTiles,
	    imagelength, imagewidth, spp);
}

/*
 * Contig strips -> contig tiles.
 */
DECLAREcpFunc(cpContigTiles2ContigTiles)
{
	return cpImage(in, out,
	    readContigTilesIntoBuffer,
	    writeBufferToContigTiles,
	    imagelength, imagewidth, spp);
}

/*
 * Contig tiles -> separate tiles.
 */
DECLAREcpFunc(cpContigTiles2SeparateTiles)
{
	return cpImage(in, out,
	    readContigTilesIntoBuffer,
	    writeBufferToSeparateTiles,
	    imagelength, imagewidth, spp);
}

/*
 * Separate tiles -> contig tiles.
 */
DECLAREcpFunc(cpSeparateTiles2ContigTiles)
{
	return cpImage(in, out,
	    readSeparateTilesIntoBuffer,
	    writeBufferToContigTiles,
	    imagelength, imagewidth, spp);
}

/*
 * Separate tiles -> separate tiles (tile dimension change).
 */
DECLAREcpFunc(cpSeparateTiles2SeparateTiles)
{
	return cpImage(in, out,
	    readSeparateTilesIntoBuffer,
	    writeBufferToSeparateTiles,
	    imagelength, imagewidth, spp);
}

/*
 * Contig tiles -> contig tiles (tile dimension change).
 */
DECLAREcpFunc(cpContigTiles2ContigStrips)
{
	return cpImage(in, out,
	    readContigTilesIntoBuffer,
	    writeBufferToContigStrips,
	    imagelength, imagewidth, spp);
}

/*
 * Contig tiles -> separate strips.
 */
DECLAREcpFunc(cpContigTiles2SeparateStrips)
{
	return cpImage(in, out,
	    readContigTilesIntoBuffer,
	    writeBufferToSeparateStrips,
	    imagelength, imagewidth, spp);
}

/*
 * Separate tiles -> contig strips.
 */
DECLAREcpFunc(cpSeparateTiles2ContigStrips)
{
	return cpImage(in, out,
	    readSeparateTilesIntoBuffer,
	    writeBufferToContigStrips,
	    imagelength, imagewidth, spp);
}

/*
 * Separate tiles -> separate strips.
 */
DECLAREcpFunc(cpSeparateTiles2SeparateStrips)
{
	return cpImage(in, out,
	    readSeparateTilesIntoBuffer,
	    writeBufferToSeparateStrips,
	    imagelength, imagewidth, spp);
}

/*
 * Select the appropriate copy function to use.
 */
static copyFunc
pickCopyFunc(TIFF* in, TIFF* out, uint16 bitspersample, uint16 samplesperpixel)
{
	uint16 shortv;
	uint32 w, l, tw, tl;
	int bychunk;

	(void) TIFFGetField(in, TIFFTAG_PLANARCONFIG, &shortv);
	if (shortv != config && bitspersample != 8 && samplesperpixel > 1) {
		fprintf(stderr,
"%s: Can not handle different planar configuration w/ bits/sample != 8\n",
		    TIFFFileName(in));
		return (NULL);
	}
	TIFFGetField(in, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField(in, TIFFTAG_IMAGELENGTH, &l);
	if (TIFFIsTiled(out)) {
		if (!TIFFGetField(in, TIFFTAG_TILEWIDTH, &tw))
			tw = w;
		if (!TIFFGetField(in, TIFFTAG_TILELENGTH, &tl))
			tl = l;
		bychunk = (tw == tilewidth && tl == tilelength);
	} else if (TIFFIsTiled(in)) {
		TIFFGetField(in, TIFFTAG_TILEWIDTH, &tw);
		TIFFGetField(in, TIFFTAG_TILELENGTH, &tl);
		bychunk = (tw == w && tl == rowsperstrip);
	} else {
		uint32 irps = -1L;
		TIFFGetField(in, TIFFTAG_ROWSPERSTRIP, &irps);
		bychunk = (rowsperstrip == irps);
	}
#define	T 1
#define	F 0
#define pack(a,b,c,d,e)	((long)(((a)<<11)|((b)<<3)|((c)<<2)|((d)<<1)|(e)))
	switch(pack(shortv,config,TIFFIsTiled(in),TIFFIsTiled(out),bychunk)) {
/* Strips -> Tiles */
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_CONTIG,   F,T,F):
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_CONTIG,   F,T,T):
		return cpContigStrips2ContigTiles;
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_SEPARATE, F,T,F):
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_SEPARATE, F,T,T):
		return cpContigStrips2SeparateTiles;
        case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_CONTIG,   F,T,F):
        case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_CONTIG,   F,T,T):
		return cpSeparateStrips2ContigTiles;
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_SEPARATE, F,T,F):
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_SEPARATE, F,T,T):
		return cpSeparateStrips2SeparateTiles;
/* Tiles -> Tiles */
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_CONTIG,   T,T,F):
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_CONTIG,   T,T,T):
		return cpContigTiles2ContigTiles;
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_SEPARATE, T,T,F):
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_SEPARATE, T,T,T):
		return cpContigTiles2SeparateTiles;
        case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_CONTIG,   T,T,F):
        case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_CONTIG,   T,T,T):
		return cpSeparateTiles2ContigTiles;
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_SEPARATE, T,T,F):
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_SEPARATE, T,T,T):
		return cpSeparateTiles2SeparateTiles;
/* Tiles -> Strips */
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_CONTIG,   T,F,F):
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_CONTIG,   T,F,T):
		return cpContigTiles2ContigStrips;
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_SEPARATE, T,F,F):
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_SEPARATE, T,F,T):
		return cpContigTiles2SeparateStrips;
        case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_CONTIG,   T,F,F):
        case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_CONTIG,   T,F,T):
		return cpSeparateTiles2ContigStrips;
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_SEPARATE, T,F,F):
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_SEPARATE, T,F,T):
		return cpSeparateTiles2SeparateStrips;
/* Strips -> Strips */
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_CONTIG,   F,F,F):
		return cpContig2ContigByRow;
	case pack(PLANARCONFIG_CONTIG,   PLANARCONFIG_CONTIG,   F,F,T):
		return cpDecodedStrips;
	case pack(PLANARCONFIG_CONTIG, PLANARCONFIG_SEPARATE,   F,F,F):
	case pack(PLANARCONFIG_CONTIG, PLANARCONFIG_SEPARATE,   F,F,T):
		return cpContig2SeparateByRow;
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_CONTIG,   F,F,F):
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_CONTIG,   F,F,T):
		return cpSeparate2ContigByRow;
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_SEPARATE, F,F,F):
	case pack(PLANARCONFIG_SEPARATE, PLANARCONFIG_SEPARATE, F,F,T):
		return cpSeparate2SeparateByRow;
	}
#undef pack
#undef F
#undef T
	fprintf(stderr, "tiffcp: %s: Don't know how to copy/convert image.\n",
	    TIFFFileName(in));
	return (NULL);
}
