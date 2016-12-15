#ifndef lint
static char rcsid[] = "$Header: /usr/people/sam/tiff/tools/RCS/tiff2ps.c,v 1.28 93/08/26 15:11:01 sam Exp $";
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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>			/* for atof */
#include <time.h>

#include "tiffio.h"

/*
 * NB: this code assumes uint32 works with printf's %l[ud].
 */
#define	TRUE	1
#define	FALSE	0

int	level2 = FALSE;			/* generate PostScript level 2 */
int	printAll = FALSE;		/* print all images in file */
int	generateEPSF = TRUE;		/* generate Encapsulated PostScript */
char	*filename;			/* input filename */

void	TIFF2PS(FILE*, TIFF*, float, float);
void	PSpage(FILE*, TIFF*, uint32, uint32);
void	PSColorContigPreamble(FILE*, uint32, uint32, int);
void	PSColorSeparatePreamble(FILE*, uint32, uint32, int);
void	PSDataColorContig(FILE*, TIFF*, uint32, uint32, int);
void	PSDataColorSeparate(FILE*, TIFF*, uint32, uint32, int);
void	PSDataPalette(FILE*, TIFF*, uint32, uint32);
void	PSDataBW(FILE*, TIFF*, uint32, uint32);
void	PSRawDataBW(FILE*, TIFF*, uint32, uint32);

static void
usage(int code)
{
	fprintf(stderr, "Usage: tiff2ps %s %s %s %s file\n"
	    , "[-w inches]"
	    , "[-h inches]"
	    , "[-d dirnum]"
	    , "[-aeps2]"
	);
	exit(code);
}

void
main(int argc, char* argv[])
{
	int dirnum = -1, c;
	TIFF *tif;
	char *cp;
	float pageWidth = 0;
	float pageHeight = 0;
	extern char *optarg;
	extern int optind;

	while ((c = getopt(argc, argv, "h:w:d:aeps2")) != -1)
		switch (c) {
		case 'd':
			dirnum = atoi(optarg);
			break;
		case 'e':
			generateEPSF = TRUE;
			break;
		case 'h':
			pageHeight = atof(optarg);
			break;
		case 'a':
			printAll = TRUE;
			/* fall thru... */
		case 'p':
			generateEPSF = FALSE;
			break;
		case 's':
			printAll = FALSE;
			break;
		case 'w':
			pageWidth = atof(optarg);
			break;
		case '2':
			level2 = TRUE;
			break;
		case '?':
			usage(-1);
		}
	if (argc - optind < 1)
		usage(-2);
	tif = TIFFOpen(filename = argv[optind], "r");
	if (tif != NULL) {
		if (dirnum != -1 && !TIFFSetDirectory(tif, dirnum))
			exit(-1);
		TIFF2PS(stdout, tif, pageWidth, pageHeight);
	}
	exit(0);
}

static	uint16 samplesperpixel;
static	uint16 bitspersample;
static	uint16 planarconfiguration;
static	uint16 photometric;
static	uint16 extrasamples;
static	int alpha;

static int
checkImage(TIFF* tif)
{
	switch (bitspersample) {
	case 1: case 2:
	case 4: case 8:
		break;
	default:
		TIFFError(filename, "Can not handle %d-bit/sample image",
		    bitspersample);
		return (0);
	}
	switch (photometric) {
	case PHOTOMETRIC_RGB:
		if (alpha && bitspersample != 8) {
			TIFFError(filename,
			    "Can not handle %d-bit/sample RGB image with alpha",
			    bitspersample);
			return (0);
		}
		/* fall thru... */
	case PHOTOMETRIC_SEPARATED:
	case PHOTOMETRIC_PALETTE:
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_MINISWHITE:
		break;
	default:
		TIFFError(filename,
		    "Can not handle image with PhotometricInterpretation=%d",
		    photometric);
		return (0);
	}
	if (planarconfiguration == PLANARCONFIG_SEPARATE && extrasamples > 0)
		TIFFWarning(filename, "Ignoring extra samples");
	return (1);
}

#define PS_UNIT_SIZE	72.0
#define	PSUNITS(npix,res)	((npix) * (PS_UNIT_SIZE / (res)))

static	char RGBcolorimage[] = "\
/bwproc {\n\
    rgbproc\n\
    dup length 3 idiv string 0 3 0\n\
    5 -1 roll {\n\
	add 2 1 roll 1 sub dup 0 eq {\n\
	    pop 3 idiv\n\
	    3 -1 roll\n\
	    dup 4 -1 roll\n\
	    dup 3 1 roll\n\
	    5 -1 roll put\n\
	    1 add 3 0\n\
	} { 2 1 roll } ifelse\n\
    } forall\n\
    pop pop pop\n\
} def\n\
/colorimage where {pop} {\n\
    /colorimage {pop pop /rgbproc exch def {bwproc} image} bind def\n\
} ifelse\n\
";

/*
 * Adobe Photoshop requires a comment line of the form:
 *
 * %ImageData: <cols> <rows> <depth>  <main channels> <pad channels>
 *	<block size> <1 for binary|2 for hex> "data start"
 *
 * It is claimed to be part of some future revision of the EPS spec.
 */
static void
PhotoshopBanner(FILE* fd, uint32 w, uint32 h, int bs, int nc, char* startline)
{
	fprintf(fd, "%%ImageData: %ld %ld %d %d 0 %d 2 \"",
	    w, h, bitspersample, nc, bs);
	fprintf(fd, startline, nc);
	fprintf(fd, "\"\n");
}

static void
setupPageState(TIFF* tif, uint32* pw, uint32* ph, float* pprw, float* pprh)
{
	uint16 res_unit;
	float xres, yres;

	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, pw);
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, ph);
	TIFFGetFieldDefaulted(tif, TIFFTAG_RESOLUTIONUNIT, &res_unit);
	/*
	 * Calculate printable area.
	 */
	if (!TIFFGetField(tif, TIFFTAG_XRESOLUTION, &xres))
		xres = PS_UNIT_SIZE;
	else if (res_unit == RESUNIT_CENTIMETER)
		xres /= 2.54;
	*pprw = PSUNITS(*pw, xres);
	if (!TIFFGetField(tif, TIFFTAG_YRESOLUTION, &yres))
		yres = PS_UNIT_SIZE;
	else if (res_unit == RESUNIT_CENTIMETER)
		yres /= 2.54;
	*pprh = PSUNITS(*ph, yres);
}

static	tsize_t tf_bytesperrow;
static	tsize_t ps_bytesperrow;
static	char *hex = "0123456789abcdef";

void
TIFF2PS(FILE* fd, TIFF* tif, float pw, float ph)
{
	uint32 w, h;
	float ox, oy, prw, prh;
	float psunit = PS_UNIT_SIZE;
	uint32 subfiletype;
	uint16* sampleinfo;
	int npages;
	long t;

	if (!TIFFGetField(tif, TIFFTAG_XPOSITION, &ox))
		ox = 0;
	if (!TIFFGetField(tif, TIFFTAG_YPOSITION, &oy))
		oy = 0;
	setupPageState(tif, &w, &h, &prw, &prh);

	t = time(0);
	fprintf(fd, "%%!PS-Adobe-3.0%s\n", generateEPSF ? " EPSF-3.0" : "");
	fprintf(fd, "%%%%Creator: tiff2ps\n");
	fprintf(fd, "%%%%Title: %s\n", filename);
	fprintf(fd, "%%%%CreationDate: %s", ctime(&t));
	fprintf(fd, "%%%%Origin: %ld %ld\n", (long) ox, (long) oy);
	/* NB: should use PageBoundingBox */
	fprintf(fd, "%%%%BoundingBox: 0 0 %ld %ld\n",
	    (long) ceil(prw), (long) ceil(prh));
	fprintf(fd, "%%%%Pages: (atend)\n");
	fprintf(fd, "%%%%EndComments\n");
	npages = 0;
	do {
		setupPageState(tif, &w, &h, &prw, &prh);
		tf_bytesperrow = TIFFScanlineSize(tif);
		TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE,
		    &bitspersample);
		TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL,
		    &samplesperpixel);
		TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarconfiguration);
		TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric);
		TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES,
		    &extrasamples, &sampleinfo);
		alpha = (extrasamples == 1 &&
			 sampleinfo[0] == EXTRASAMPLE_ASSOCALPHA);
		if (checkImage(tif)) {
			npages++;
			fprintf(fd, "%%%%Page: %d %d\n", npages, npages);
			fprintf(fd, "gsave\n");
			fprintf(fd, "100 dict begin\n");
			if (pw != 0 && ph != 0)
				fprintf(fd, "%f %f scale\n",
				    pw*PS_UNIT_SIZE, ph*PS_UNIT_SIZE);
			else
				fprintf(fd, "%f %f scale\n", prw, prh);
			PSpage(fd, tif, w, h);
			fprintf(fd, "end\n");
			fprintf(fd, "grestore\n");
			fprintf(fd, "showpage\n");
		}
		if (generateEPSF)
			break;
		TIFFGetFieldDefaulted(tif, TIFFTAG_SUBFILETYPE, &subfiletype);
	} while (((subfiletype & FILETYPE_PAGE) || printAll) &&
	    TIFFReadDirectory(tif));
	fprintf(fd, "%%%%Trailer\n");
	fprintf(fd, "%%%%Pages: %u\n", npages);
	fprintf(fd, "%%%%EOF\n");
}

static int
emitPSLevel2FilterFunction(FILE* fd, TIFF* tif, uint32 w, uint32 h)
{
	uint16 compression;
	uint32 group3opts;
	int K;

	TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
#define	P(a,b)	(((a)<<4)|((b)&0xf))
	switch (P(compression, photometric)) {
	case P(COMPRESSION_CCITTRLE, PHOTOMETRIC_MINISBLACK):
	case P(COMPRESSION_CCITTRLE, PHOTOMETRIC_MINISWHITE):
		K = 0;
		break;
	case P(COMPRESSION_CCITTFAX3, PHOTOMETRIC_MINISBLACK):
	case P(COMPRESSION_CCITTFAX3, PHOTOMETRIC_MINISWHITE):
		TIFFGetField(tif, TIFFTAG_GROUP3OPTIONS, &group3opts);
		K = group3opts&GROUP3OPT_2DENCODING;
		break;
	case P(COMPRESSION_CCITTFAX4, PHOTOMETRIC_MINISBLACK):
	case P(COMPRESSION_CCITTFAX4, PHOTOMETRIC_MINISWHITE):
		K = -1;
		break;
	case P(COMPRESSION_LZW, PHOTOMETRIC_MINISBLACK):
		fprintf(fd, "/LZWDecode filter dup 6 1 roll\n");
		return (TRUE);
	default:
		return (FALSE);
	}
#undef P
	fprintf(fd, "<<");
	fprintf(fd, "/K %d", K);
	fprintf(fd, " /Columns %d /Rows %d", w, h);
	fprintf(fd, " /EndOfBlock false /BlackIs1 %s",
	    (photometric == PHOTOMETRIC_MINISBLACK) ? "true" : "false");
	fprintf(fd, ">>\n/CCITTFaxDecode filter\n");
	fprintf(fd, "dup 6 1 roll\n");
	return (TRUE);
}

void
PSpage(FILE* fd, TIFF* tif, uint32 w, uint32 h)
{
	uint16 compression;
	uint32 group3opts;
	uint32 rowsperstrip;

	ps_bytesperrow = tf_bytesperrow;
	switch (photometric) {
	case PHOTOMETRIC_RGB:
		if (planarconfiguration == PLANARCONFIG_CONTIG) {
			fprintf(fd, "%s", RGBcolorimage);
			PSColorContigPreamble(fd, w, h, 3);
			PSDataColorContig(fd, tif, w, h, 3);
		} else {
			PSColorSeparatePreamble(fd, w, h, 3);
			PSDataColorSeparate(fd, tif, w, h, 3);
		}
		break;
	case PHOTOMETRIC_SEPARATED:
		/* XXX should emit CMYKcolorimage */
		if (planarconfiguration == PLANARCONFIG_CONTIG) {
			PSColorContigPreamble(fd, w, h, 4);
			PSDataColorContig(fd, tif, w, h, 4);
		} else {
			PSColorSeparatePreamble(fd, w, h, 4);
			PSDataColorSeparate(fd, tif, w, h, 4);
		}
		break;
	case PHOTOMETRIC_PALETTE:
		fprintf(fd, "%s", RGBcolorimage);
		PhotoshopBanner(fd, w, h, 1, 3, "false 3 colorimage");
		fprintf(fd, "/scanLine %d string def\n", ps_bytesperrow);
		fprintf(fd, "%lu %lu 8\n", w, h);
		fprintf(fd, "[%lu 0 0 -%lu 0 %lu]\n", w, h, h);
		fprintf(fd, "{currentfile scanLine readhexstring pop} bind\n");
		fprintf(fd, "false 3 colorimage\n");
		PSDataPalette(fd, tif, w, h);
		break;
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_MINISWHITE:
		PhotoshopBanner(fd, w, h, 1, 1, "image");
		if (level2) {
			int rawdata;
			fprintf(fd, "%lu %lu %d\n", w, h, bitspersample);
			fprintf(fd, "[%lu 0 0 -%lu 0 %lu]\n", w, h, h);
			fprintf(fd, "currentfile /ASCIIHexDecode filter\n");
			fprintf(fd, "dup 6 1 roll\n");
			rawdata = emitPSLevel2FilterFunction(fd, tif, w, h);
			fprintf(fd, "{image flushfile flushfile} cvx exec\n");
			if (rawdata)
				PSRawDataBW(fd, tif, w, h);
			else
				PSDataBW(fd, tif, w, h);
			putc('>', fd);
		} else {
			fprintf(fd, "/scanLine %d string def\n",ps_bytesperrow);
			fprintf(fd, "%lu %lu %d\n", w, h, bitspersample);
			fprintf(fd, "[%lu 0 0 -%lu 0 %lu]\n", w, h, h);
			fprintf(fd,
			    "{currentfile scanLine readhexstring pop} bind\n");
			fprintf(fd, "image\n");
			PSDataBW(fd, tif, w, h);
		}
		break;
	}
	putc('\n', fd);
}

void
PSColorContigPreamble(FILE* fd, uint32 w, uint32 h, int nc)
{
	ps_bytesperrow = nc * (tf_bytesperrow / samplesperpixel);
	PhotoshopBanner(fd, w, h, 1, nc, "false %d colorimage");
	fprintf(fd, "/line %d string def\n", ps_bytesperrow);
	fprintf(fd, "%lu %lu %d\n", w, h, bitspersample);
	fprintf(fd, "[%lu 0 0 -%lu 0 %lu]\n", w, h, h);
	fprintf(fd, "{currentfile line readhexstring pop} bind\n");
	fprintf(fd, "false %d colorimage\n", nc);
}

void
PSColorSeparatePreamble(FILE* fd, uint32 w, uint32 h, int nc)
{
	int i;

	PhotoshopBanner(fd, w, h, ps_bytesperrow, nc, "true %d colorimage");
	for (i = 0; i < nc; i++)
		fprintf(fd, "/line%d %d string def\n", i, ps_bytesperrow);
	fprintf(fd, "%lu %lu %d\n", w, h, bitspersample);
	fprintf(fd, "[%lu 0 0 -%lu 0 %lu] \n", w, h, h);
	for (i = 0; i < nc; i++)
		fprintf(fd, "{currentfile line%d readhexstring pop}bind\n", i);
	fprintf(fd, "true %d colorimage\n", nc);
}

#define MAXLINE		36
#define	DOBREAK(len, howmany, fd) \
	if (((len) -= (howmany)) <= 0) {	\
		putc('\n', fd);			\
		(len) = MAXLINE-(howmany);	\
	}
#define	PUTHEX(c,fd)	putc(hex[((c)>>4)&0xf],fd); putc(hex[(c)&0xf],fd)

void
PSDataColorContig(FILE* fd, TIFF* tif, uint32 w, uint32 h, int nc)
{
	uint32 row;
	int breaklen = MAXLINE, cc, es = samplesperpixel - nc;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	tf_buf = (unsigned char *) malloc(tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for scanline buffer");
		return;
	}
	for (row = 0; row < h; row++) {
		if (TIFFReadScanline(tif, tf_buf, row, 0) < 0)
			break;
		cp = tf_buf;
		if (alpha) {
			int adjust;
			cc = 0;
			for (; cc < tf_bytesperrow; cc += samplesperpixel) {
				DOBREAK(breaklen, nc, fd);
				/*
				 * For images with alpha, matte against
				 * a white background; i.e.
				 *    Cback * (1 - Aimage)
				 * where Cback = 1.
				 */
				adjust = 255 - cp[nc];
				switch (nc) {
				case 4: c = *cp++ + adjust; PUTHEX(c,fd);
				case 3: c = *cp++ + adjust; PUTHEX(c,fd);
				case 2: c = *cp++ + adjust; PUTHEX(c,fd);
				case 1: c = *cp++ + adjust; PUTHEX(c,fd);
				}
				cp += es;
			}
		} else {
			cc = 0;
			for (; cc < tf_bytesperrow; cc += samplesperpixel) {
				DOBREAK(breaklen, nc, fd);
				switch (nc) {
				case 4: c = *cp++; PUTHEX(c,fd);
				case 3: c = *cp++; PUTHEX(c,fd);
				case 2: c = *cp++; PUTHEX(c,fd);
				case 1: c = *cp++; PUTHEX(c,fd);
				}
				cp += es;
			}
		}
	}
	free((char *) tf_buf);
}

void
PSDataColorSeparate(FILE* fd, TIFF* tif, uint32 w, uint32 h, int nc)
{
	uint32 row;
	int breaklen = MAXLINE, cc, s, maxs;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	tf_buf = (unsigned char *) malloc(tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for scanline buffer");
		return;
	}
	maxs = (samplesperpixel > nc ? nc : samplesperpixel);
	for (row = 0; row < h; row++) {
		for (s = 0; s < maxs; s++) {
			if (TIFFReadScanline(tif, tf_buf, row, s) < 0)
				break;
			for (cp = tf_buf, cc = 0; cc < tf_bytesperrow; cc++) {
				DOBREAK(breaklen, 1, fd);
				c = *cp++;
				PUTHEX(c,fd);
			}
		}
	}
	free((char *) tf_buf);
}

#define	PUTRGBHEX(c,fd) \
	PUTHEX(rmap[c],fd); PUTHEX(gmap[c],fd); PUTHEX(bmap[c],fd)

static int
checkcmap(TIFF* tif, int n, uint16* r, uint16* g, uint16* b)
{
	while (n-- > 0)
		if (*r++ >= 256 || *g++ >= 256 || *b++ >= 256)
			return (16);
	TIFFWarning(filename, "Assuming 8-bit colormap");
	return (8);
}

void
PSDataPalette(FILE* fd, TIFF* tif, uint32 w, uint32 h)
{
	uint16 *rmap, *gmap, *bmap;
	uint32 row;
	int breaklen = MAXLINE, cc, nc;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	if (!TIFFGetField(tif, TIFFTAG_COLORMAP, &rmap, &gmap, &bmap)) {
		TIFFError(filename, "Palette image w/o \"Colormap\" tag");
		return;
	}
	switch (bitspersample) {
	case 8:	case 4: case 2: case 1:
		break;
	default:
		TIFFError(filename, "Depth %d not supported", bitspersample);
		return;
	}
	nc = 3 * (8 / bitspersample);
	tf_buf = (unsigned char *) malloc(tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for scanline buffer");
		return;
	}
	if (checkcmap(tif, 1<<bitspersample, rmap, gmap, bmap) == 16) {
		int i;
#define	CVT(x)		(((x) * 255) / ((1L<<16)-1))
		for (i = (1<<bitspersample)-1; i > 0; i--) {
			rmap[i] = CVT(rmap[i]);
			gmap[i] = CVT(gmap[i]);
			bmap[i] = CVT(bmap[i]);
		}
#undef CVT
	}
	for (row = 0; row < h; row++) {
		if (TIFFReadScanline(tif, tf_buf, row, 0) < 0)
			break;
		for (cp = tf_buf, cc = 0; cc < tf_bytesperrow; cc++) {
			DOBREAK(breaklen, nc, fd);
			switch (bitspersample) {
			case 8:
				c = *cp++; PUTRGBHEX(c, fd);
				break;
			case 4:
				c = *cp++; PUTRGBHEX(c&0xf, fd);
				c >>= 4;   PUTRGBHEX(c, fd);
				break;
			case 2:
				c = *cp++; PUTRGBHEX(c&0x3, fd);
				c >>= 2;   PUTRGBHEX(c&0x3, fd);
				c >>= 2;   PUTRGBHEX(c&0x3, fd);
				c >>= 2;   PUTRGBHEX(c, fd);
				break;
			case 1:
				c = *cp++; PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c&0x1, fd);
				c >>= 1;   PUTRGBHEX(c, fd);
				break;
			}
		}
	}
	free((char *) tf_buf);
}

void
PSDataBW(FILE* fd, TIFF* tif, uint32 w, uint32 h)
{
	uint32 row;
	int breaklen = MAXLINE, cc;
	unsigned char *tf_buf;
	unsigned char *cp, c;

	tf_buf = (unsigned char *) malloc(tf_bytesperrow);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for scanline buffer");
		return;
	}
	for (row = 0; row < h; row++) {
		if (TIFFReadScanline(tif, tf_buf, row, 0) < 0)
			break;
		for (cp = tf_buf, cc = 0; cc < tf_bytesperrow; cc++) {
			DOBREAK(breaklen, 1, fd);
			c = *cp++;
			if (photometric == PHOTOMETRIC_MINISWHITE)
				c = ~c;
			PUTHEX(c, fd);
		}
	}
	free((char *) tf_buf);
}

void
PSRawDataBW(FILE* fd, TIFF* tif, uint32 w, uint32 h)
{
	uint32 *bc;
	uint32 bufsize;
	int breaklen = MAXLINE, cc;
	uint16 fillorder;
	unsigned char *tf_buf;
	unsigned char *cp, c;
	tstrip_t s;

	TIFFGetField(tif, TIFFTAG_FILLORDER, &fillorder);
	TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &bc);
	bufsize = bc[0];
	tf_buf = (unsigned char*) malloc(bufsize);
	if (tf_buf == NULL) {
		TIFFError(filename, "No space for strip buffer");
		return;
	}
	for (s = 0; s < TIFFNumberOfStrips(tif); s++) {
		if (bc[s] > bufsize) {
			tf_buf = (unsigned char *) realloc(tf_buf, bc[0]);
			if (tf_buf == NULL) {
				TIFFError(filename,
				    "No space for strip buffer");
				return;
			}
			bufsize = bc[0];
		}
		cc = TIFFReadRawStrip(tif, s, tf_buf, bc[s]);
		if (cc < 0) {
			TIFFError(filename, "Can't read strip");
			free(tf_buf);
			return;
		}
		if (fillorder == FILLORDER_LSB2MSB)
			TIFFReverseBits(tf_buf, cc);
		for (cp = tf_buf; cc > 0; cc--) {
			DOBREAK(breaklen, 1, fd);
			c = *cp++;
			PUTHEX(c, fd);
		}
	}
	free((char *) tf_buf);
}
