#ifndef lint
static char rcsid[] = "$Header: /usr/people/sam/tiff/tools/RCS/sgigt.c,v 1.41 93/08/26 15:10:09 sam Exp $";
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
#include <gl.h>
#include <device.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "tiffio.h"

typedef	unsigned char u_char;
typedef	unsigned long u_long;

/* XXX fudge adjustment for window borders */
#define	YFUDGE	20
#define	XFUDGE	20

Cursor	hourglass = {
    0x1ff0, 0x1ff0, 0x0820, 0x0820,
    0x0820, 0x0c60, 0x06c0, 0x0100,
    0x0100, 0x06c0, 0x0c60, 0x0820,
    0x0820, 0x0820, 0x1ff0, 0x1ff0
};
uint32	width, height;			/* image width & height */
uint16	bitspersample;
uint16	samplesperpixel;
uint16	photometric;
uint16	orientation;
uint16	extrasamples;
uint16	planarconfig;
uint16	*redcmap, *greencmap, *bluecmap;/* colormap for pallete images */
uint16	YCbCrHorizSampling, YCbCrVertSampling;
float	*YCbCrCoeffs;
float	*refBlackWhite;
int	isRGB = -1;
int	verbose = 0;
int	stoponerr = 0;			/* stop on read error */

char	*filename;

static	void checkImage(TIFF*);
static	int gt(TIFF*, uint32, uint32, u_long*);
extern	Colorindex rgb(float, float, float);

static void
usage(void)
{
	fprintf(stderr, "usage: tiffgt [-d dirnum] [-f] [-lm] [-s] filename\n");
	exit(-1);
}

void
main(int argc, char* argv[])
{
	u_long *raster;			/* displayable image */
	char title[1024];
	char *cp;
	long max;
	TIFF *tif;
	int fg = 0, c, dirnum = -1, order = 0;

	while ((c = getopt(argc, argv, "d:cerflmsv")) != -1)
		switch (c) {
		case 'c':
			isRGB = 0;
			break;
		case 'd':
			dirnum = atoi(optarg);
			break;
		case 'f':
			fg = 1;
			break;
		case 'l':
			order = FILLORDER_LSB2MSB;
			break;
		case 'm':
			order = FILLORDER_MSB2LSB;
			break;
		case 'r':
			isRGB = 1;
			break;
		case 's':
			stoponerr = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
			usage();
			/*NOTREACHED*/
		}
	if (argc - optind < 1)
		usage();
	filename = argv[optind];
	tif = TIFFOpen(filename, "r");
	if (tif == NULL)
		exit(-1);
	if (dirnum != -1 && !TIFFSetDirectory(tif, dirnum)) {
		TIFFError(filename, "Error, seeking to directory %d", dirnum);
		exit(-1);
	}
	checkImage(tif);
	if (order)
		TIFFSetField(tif, TIFFTAG_FILLORDER, order);
	/*
	 * Use a full-color window if the image is
	 * full color or a palette image and the
	 * hardware support is present.
	 */
	if (isRGB == -1)
		isRGB = (bitspersample >= 8 &&
		    (photometric == PHOTOMETRIC_RGB ||
		     photometric == PHOTOMETRIC_YCBCR ||
		     photometric == PHOTOMETRIC_SEPARATED ||
		     photometric == PHOTOMETRIC_PALETTE));
	/*
	 * Check to see if the hardware can display 24-bit RGB.
	 */
	if (isRGB && getgdesc(GD_BITS_NORM_SNG_RED) < bitspersample &&
	  !getgdesc(GD_DITHER))
		isRGB = 0;
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
	max = getgdesc(GD_XPMAX) - XFUDGE;
	if (width > max)
		width = max;
	TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
	max = getgdesc(GD_YPMAX) - YFUDGE;
	if (height > max)
		height = max;
	prefsize(width, height);
	cp = strrchr(filename, '/');
	if (cp == NULL)
		cp = filename;
	else
		cp++;
	if (fg)
		foreground();
	strcpy(title, cp);
	if (dirnum > 0) {
		char buf[40];
		sprintf(buf, " [%d]", dirnum == -1 ? 0 : dirnum);
		strcat(title, buf);
	}
	if (verbose)
		strcat(title, isRGB ? " rgb" : " cmap");
	if (winopen(title) < 0) {
		TIFFError(filename, "Can not create window");
		exit(-1);
	}
	raster = (u_long *)malloc(width * height * sizeof (long));
	if (raster == 0) {
		TIFFError(filename, "No space for raster buffer");
		exit(-1);
	}
	singlebuffer();
	if (isRGB) {
		RGBmode();
		gconfig();
	} else {
		cmode();
		gconfig();
	}
	curstype(C16X1);
	defcursor(1, hourglass);
	setcursor(1, 0, 0);
	rgb(0.5,0.5,0.5);
	clear();
	if (!gt(tif, width, height, raster))
		exit(-1);
	setcursor(0, 0, 0);
	TIFFClose(tif);
	qdevice(LEFTMOUSE);
	for (;;) {
		short val;
		switch (qread(&val)) {
		case REDRAW:
			lrectwrite(0, 0, width-1, height-1, raster);
			break;
		case LEFTMOUSE:
			if (val)
				exit(0);
			break;
		}
	}
	/*NOTREACHED*/
}

static void
checkImage(TIFF* tif)
{
	int alpha;
	uint16* sampleinfo;

	TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitspersample);
	switch (bitspersample) {
	case 1: case 2: case 4:
	case 8: case 16:
		break;
	default:
		TIFFError(filename, "Sorry, can not handle %d-bit pictures",
		    bitspersample);
		exit(-1);
	}
	TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel);
	TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES,
	    &extrasamples, &sampleinfo);
	alpha = (extrasamples == 1 && sampleinfo[0] == EXTRASAMPLE_ASSOCALPHA);
	TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarconfig);
	switch (samplesperpixel - extrasamples) {
	case 3:
		break;
	case 1: case 4:
/* XXX */
		if (!alpha || planarconfig != PLANARCONFIG_CONTIG)
			break;
		/* fall thru... */
	default:
	        TIFFError(filename,
		    "Sorry, can not handle %d-channel %s images%s",
		    samplesperpixel,
		    planarconfig == PLANARCONFIG_CONTIG ?
			"packed" : "separated",
		    alpha ? " with alpha" : "");
		exit(-1);
	}
	if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric)) {
		switch (samplesperpixel) {
		case 1:
			photometric = PHOTOMETRIC_MINISBLACK;
			break;
		case 3: case 4:
			photometric = PHOTOMETRIC_RGB;
			break;
		default:
			TIFFError(filename, "Missing needed \"%s\" tag",
			    "PhotometricInterpretation");
			exit(-1);
		}
		TIFFError(filename,
		    "No \"PhotometricInterpretation\" tag, assuming %s\n",
		    photometric == PHOTOMETRIC_RGB ? "RGB" : "min-is-black");
	}
	switch (photometric) {
	case PHOTOMETRIC_MINISWHITE:
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_RGB:
	case PHOTOMETRIC_PALETTE:
	case PHOTOMETRIC_YCBCR:
		break;
	case PHOTOMETRIC_SEPARATED: {
		uint16 inkset;
		TIFFGetFieldDefaulted(tif, TIFFTAG_INKSET, &inkset);
		if (inkset != INKSET_CMYK) {
			TIFFError(filename,
			    "Sorry, can not handle separated image with %s=%d",
			    "InkSet", inkset);
			exit(-1);
		}
		break;
	}
	default:
		TIFFError(filename, "Sorry, can not handle image with %s=%d",
		    "PhotometricInterpretation", photometric);
		exit(-1);
	}
}

static int
checkcmap(int n, uint16* r, uint16* g, uint16* b)
{
	while (n-- > 0)
		if (*r++ >= 256 || *g++ >= 256 || *b++ >= 256)
			return (16);
	TIFFWarning(filename, "Assuming 8-bit colormap");
	return (8);
}

/*
 * {red,green,blue}_inverse are tables in libgutil.a that
 * do an inverse map from (r,g,b) to the closest colormap
 * index in the "standard" GL colormap.  grey_inverse is
 * the equivalent map for mapping greyscale values to
 * colormap indices.  We access these maps directly instead
 * of through the rgbi and greyi functions to avoid the
 * additional overhead of the color calls that they make.
 */
extern	u_char red_inverse[256];
extern	u_char green_inverse[256];
extern	u_char blue_inverse[256];
extern	u_char grey_inverse[256];
#define	greyi(g)	grey_inverse[g]

static u_char
rgbi(u_char r, u_char g, u_char b)
{
	return (r == g && g == b ? grey_inverse[r] :
	    red_inverse[r] + green_inverse[g] + blue_inverse[b]);
}

#define	howmany(x, y)	(((x)+((y)-1))/(y))
u_long	**BWmap;
u_long	**PALmap;

static 	int makebwmap(RGBvalue* Map);
static	int makecmap(uint16*, uint16*, uint16*);
static	void initYCbCrConversion(void);
static	int gtTileContig(TIFF*, u_long*, RGBvalue*, u_long, u_long);
static	int gtTileSeparate(TIFF*, u_long*, RGBvalue*, u_long, u_long);
static	int gtStripContig(TIFF*, u_long*, RGBvalue*, u_long, u_long);
static	int gtStripSeparate(TIFF*, u_long*, RGBvalue*, u_long, u_long);

/*
 * Construct a mapping table to convert from the range
 * of the data samples to [0,255] --for display.  This
 * process also handles inverting B&W images when needed.
 */ 
static int
setupMap(uint16 minsamplevalue, uint16 maxsamplevalue, RGBvalue** pMap)
{
	register int x, range;
	RGBvalue *Map;

	range = maxsamplevalue - minsamplevalue;
	Map = (RGBvalue *)malloc((range + 1) * sizeof (RGBvalue));
	if (Map == NULL) {
		TIFFError(filename,
		    "No space for photometric conversion table");
		return (0);
	}
	if (photometric == PHOTOMETRIC_MINISWHITE) {
		for (x = 0; x <= range; x++)
			Map[x] = ((range - x) * 255) / range;
	} else {
		for (x = 0; x <= range; x++)
			Map[x] = (x * 255) / range;
	}
	if (bitspersample <= 8 &&
	    (photometric == PHOTOMETRIC_MINISBLACK ||
	     photometric == PHOTOMETRIC_MINISWHITE)) {
		/*
		 * Use photometric mapping table to construct
		 * unpacking tables for samples <= 8 bits.
		 */
		if (!makebwmap(Map))
			return (0);
		/* no longer need Map, free it */
		free((char *)Map);
		Map = NULL;
	}
	*pMap = Map;
	return (1);
}

static int
gt(TIFF* tif, uint32 w, uint32 h, u_long* raster)
{
	uint16 minsamplevalue, maxsamplevalue;
	RGBvalue *Map;
	int e, ncomps;

	TIFFGetFieldDefaulted(tif, TIFFTAG_MINSAMPLEVALUE, &minsamplevalue);
	TIFFGetFieldDefaulted(tif, TIFFTAG_MAXSAMPLEVALUE, &maxsamplevalue);
	Map = NULL;
	switch (photometric) {
	case PHOTOMETRIC_YCBCR:
		TIFFGetFieldDefaulted(tif, TIFFTAG_YCBCRCOEFFICIENTS,
		    &YCbCrCoeffs);
		TIFFGetFieldDefaulted(tif, TIFFTAG_YCBCRSUBSAMPLING,
		    &YCbCrHorizSampling, &YCbCrVertSampling);
		TIFFGetFieldDefaulted(tif, TIFFTAG_REFERENCEBLACKWHITE,
		    &refBlackWhite);
		initYCbCrConversion();
		/* fall thru... */
	case PHOTOMETRIC_RGB:
	case PHOTOMETRIC_SEPARATED:
		if (minsamplevalue == 0 && maxsamplevalue == 255)
			break;
		/* fall thru... */
	case PHOTOMETRIC_MINISBLACK:
	case PHOTOMETRIC_MINISWHITE:
		if (!setupMap(minsamplevalue, maxsamplevalue, &Map))
			return (0);
		break;
	case PHOTOMETRIC_PALETTE:
		if (!TIFFGetField(tif, TIFFTAG_COLORMAP,
		    &redcmap, &greencmap, &bluecmap)) {
			TIFFError(filename,
			    "Missing required \"Colormap\" tag");
			return (0);
		}
		/*
		 * Convert 16-bit colormap to 8-bit (unless it looks
		 * like an old-style 8-bit colormap).
		 */
		if (checkcmap(1<<bitspersample, redcmap, greencmap, bluecmap) == 16) {
			int i;
#define	CVT(x)		(((x) * 255) / ((1L<<16)-1))
			for (i = (1<<bitspersample)-1; i >= 0; i--) {
				redcmap[i] = CVT(redcmap[i]);
				greencmap[i] = CVT(greencmap[i]);
				bluecmap[i] = CVT(bluecmap[i]);
			}
#undef CVT
		}
		if (bitspersample <= 8) {
			/*
			 * Use mapping table and colormap to construct
			 * unpacking tables for samples < 8 bits.
			 */
			if (!makecmap(redcmap, greencmap, bluecmap))
				return (0);
		}
		break;
	}
	ncomps = samplesperpixel - extrasamples;	/* # color components */
	if (planarconfig == PLANARCONFIG_SEPARATE && ncomps > 1) {
		e = TIFFIsTiled(tif) ?
		    gtTileSeparate(tif, raster, Map, h, w) :
		    gtStripSeparate(tif, raster, Map, h, w);
	} else {
		e = TIFFIsTiled(tif) ? 
		    gtTileContig(tif, raster, Map, h, w) :
		    gtStripContig(tif, raster, Map, h, w);
	}
	if (Map)
		free((char *)Map);
	return (e);
}

uint32
setorientation(TIFF* tif, uint32 h)
{
	uint32 y;

	TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &orientation);
	switch (orientation) {
	case ORIENTATION_BOTRIGHT:
	case ORIENTATION_RIGHTBOT:	/* XXX */
	case ORIENTATION_LEFTBOT:	/* XXX */
		TIFFWarning(filename, "using bottom-left orientation");
		orientation = ORIENTATION_BOTLEFT;
		/* fall thru... */
	case ORIENTATION_BOTLEFT:
		y = 0;
		break;
	case ORIENTATION_TOPRIGHT:
	case ORIENTATION_RIGHTTOP:	/* XXX */
	case ORIENTATION_LEFTTOP:	/* XXX */
	default:
		TIFFWarning(filename, "using top-left orientation");
		orientation = ORIENTATION_TOPLEFT;
		/* fall thru... */
	case ORIENTATION_TOPLEFT:
		y = h-1;
		break;
	}
	return (y);
}

typedef void (*tileContigRoutine)
    (u_long*, u_char*, RGBvalue*, uint32, uint32, int, int);
static tileContigRoutine pickTileContigCase(RGBvalue*);

/*
 * Get an tile-organized image that has
 *    PlanarConfiguration contiguous if SamplesPerPixel > 1
 * or
 *    SamplesPerPixel == 1
 */    
static
gtTileContig(TIFF* tif, u_long* raster, RGBvalue* Map, uint32 h, uint32 w)
{
	uint32 col, row, y;
	uint32 tw, th;
	u_char *buf;
	int fromskew, toskew;
	uint32 nrow;
	tileContigRoutine put;

	buf = (u_char *)malloc(TIFFTileSize(tif));
	if (buf == 0) {
		TIFFError(filename, "No space for tile buffer");
		return (0);
	}
	put = pickTileContigCase(Map);
	TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);
	TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);
	y = setorientation(tif, h);
	toskew = (orientation == ORIENTATION_TOPLEFT ? -tw + -w : -tw + w);
	for (row = 0; row < h; row += th) {
		nrow = (row + th > h ? h - row : th);
		for (col = 0; col < w; col += tw) {
			if (TIFFReadTile(tif, buf, col, row, 0, 0) < 0 && stoponerr)
				break;
			if (col + tw > w) {
				/*
				 * Tile is clipped horizontally.  Calculate
				 * visible portion and skewing factors.
				 */
				uint32 npix = w - col;
				fromskew = tw - npix;
				(*put)(raster + y*w + col, buf, Map,
				    npix, nrow, fromskew, toskew + fromskew);
			} else
				(*put)(raster + y*w + col, buf, Map,
				    tw, nrow, 0, toskew);
		}
		if (orientation == ORIENTATION_TOPLEFT) {
			y -= nrow-1;
			lrectwrite(0, y, w-1, y+nrow-1, raster + y*w);
			y--;
		} else {
			lrectwrite(0, y, w-1, y+nrow-1, raster + y*w);
			y += nrow;
		}
	}
	free(buf);
	return (1);
}

typedef void (*tileSeparateRoutine)
    (u_long*, u_char*, u_char*, u_char*, RGBvalue*, uint32, uint32, int, int);
static tileSeparateRoutine pickTileSeparateCase(RGBvalue*);

/*
 * Get an tile-organized image that has
 *     SamplesPerPixel > 1
 *     PlanarConfiguration separated
 * We assume that all such images are RGB.
 */    
static int
gtTileSeparate(TIFF* tif, u_long* raster, RGBvalue* Map, uint32 h, uint32 w)
{
	uint32 col, row, y;
	uint32 tw, th;
	u_char *buf;
	u_char *r, *g, *b;
	tsize_t tilesize;
	int fromskew, toskew;
	uint32 nrow;
	tileSeparateRoutine put;

	tilesize = TIFFTileSize(tif);
	buf = (u_char *)malloc(3*tilesize);
	if (buf == 0) {
		TIFFError(filename, "No space for tile buffer");
		return (0);
	}
	r = buf;
	g = r + tilesize;
	b = g + tilesize;
	put = pickTileSeparateCase(Map);
	TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);
	TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);
	y = setorientation(tif, h);
	toskew = (orientation == ORIENTATION_TOPLEFT ? -tw + -w : -tw + w);
	for (row = 0; row < h; row += th) {
		nrow = (row + th > h ? h - row : th);
		for (col = 0; col < w; col += tw) {
			if (TIFFReadTile(tif, r, col, row, 0, 0) < 0 && stoponerr)
				break;
			if (TIFFReadTile(tif, g, col, row, 0, 1) < 0 && stoponerr)
				break;
			if (TIFFReadTile(tif, b, col, row, 0, 2) < 0 && stoponerr)
				break;
			if (col + tw > w) {
				/*
				 * Tile is clipped horizontally.  Calculate
				 * visible portion and skewing factors.
				 */
				uint32 npix = w - col;
				fromskew = tw - npix;
				(*put)(raster + y*w + col, r, g, b, Map,
				    npix, nrow, fromskew, toskew + fromskew);
			} else
				(*put)(raster + y*w + col, r, g, b, Map,
				    tw, nrow, 0, toskew);
		}
		if (orientation == ORIENTATION_TOPLEFT) {
			y -= nrow-1;
			lrectwrite(0, y, w-1, y+nrow-1, raster + y*w);
			y--;
		} else {
			lrectwrite(0, y, w-1, y+nrow-1, raster + y*w);
			y += nrow;
		}
	}
	free(buf);
	return (1);
}

/*
 * Get a strip-organized image that has
 *    PlanarConfiguration contiguous if SamplesPerPixel > 1
 * or
 *    SamplesPerPixel == 1
 */    
static int
gtStripContig(TIFF* tif, u_long* raster, RGBvalue* Map, uint32 h, uint32 w)
{
	uint32 row, y, nrow;
	u_char *buf;
	tileContigRoutine put;
	uint32 rowsperstrip;
	uint32 imagewidth;
	tsize_t scanline;
	int fromskew, toskew;

	buf = (u_char *)malloc(TIFFStripSize(tif));
	if (buf == 0) {
		TIFFError(filename, "No space for strip buffer");
		return (0);
	}
	put = pickTileContigCase(Map);
	y = setorientation(tif, h);
	toskew = (orientation == ORIENTATION_TOPLEFT ? -w + -w : -w + w);
	TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imagewidth);
	scanline = TIFFScanlineSize(tif);
	fromskew = (w < imagewidth ? imagewidth - w : 0);
	for (row = 0; row < h; row += rowsperstrip) {
		nrow = (row + rowsperstrip > h ? h - row : rowsperstrip);
		if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 0),
		    buf, nrow*scanline) < 0 && stoponerr)
			break;
		(*put)(raster + y*w, buf, Map, w, nrow, fromskew, toskew);
		if (orientation == ORIENTATION_TOPLEFT) {
			y -= nrow-1;
			lrectwrite(0, y, w-1, y+nrow-1, raster + y*w);
			y--;
		} else {
			lrectwrite(0, y, w-1, y+nrow-1, raster + y*w);
			y += nrow;
		}
	}
	free(buf);
	return (1);
}

/*
 * Get a strip-organized image with
 *     SamplesPerPixel > 1
 *     PlanarConfiguration separated
 * We assume that all such images are RGB.
 */
static int
gtStripSeparate(TIFF* tif, u_long* raster, RGBvalue* Map, uint32 h, uint32 w)
{
	u_char *buf;
	u_char *r, *g, *b;
	uint32 row, y, nrow;
	tsize_t scanline;
	tileSeparateRoutine put;
	uint32 rowsperstrip;
	uint32 imagewidth;
	tsize_t stripsize;
	int fromskew, toskew;

	stripsize = TIFFStripSize(tif);
	r = buf = (u_char *)malloc(3*stripsize);
	g = r + stripsize;
	b = g + stripsize;
	put = pickTileSeparateCase(Map);
	y = setorientation(tif, h);
	toskew = (orientation == ORIENTATION_TOPLEFT ? -w + -w : -w + w);
	TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);
	TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imagewidth);
	scanline = TIFFScanlineSize(tif);
	fromskew = (w < imagewidth ? imagewidth - w : 0);
	for (row = 0; row < h; row += rowsperstrip) {
		nrow = (row + rowsperstrip > h ? h - row : rowsperstrip);
		if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 0),
		    r, nrow*scanline) < 0 && stoponerr)
			break;
		if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 1),
		    g, nrow*scanline) < 0 && stoponerr)
			break;
		if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 2),
		    b, nrow*scanline) < 0 && stoponerr)
			break;
		(*put)(raster + y*w, r, g, b, Map, w, nrow, fromskew, toskew);
		if (orientation == ORIENTATION_TOPLEFT) {
			y -= nrow-1;
			lrectwrite(0, y, w-1, y+nrow-1, raster + y*w);
			y--;
		} else {
			lrectwrite(0, y, w-1, y+nrow-1, raster + y*w);
			y += nrow;
		}
	}
	free(buf);
	return (1);
}

#define	PACK(r,g,b)	((u_long)(r)|((u_long)(g)<<8)|((u_long)(b)<<16))

/*
 * Greyscale images with less than 8 bits/sample are handled
 * with a table to avoid lots of shifts and masks.  The table
 * is setup so that put*bwtile (below) can retrieve 8/bitspersample
 * pixel values simply by indexing into the table with one
 * number.
 */
static int
makebwmap(RGBvalue* Map)
{
	register int i;
	int nsamples = 8 / bitspersample;
	register u_long *p;

	BWmap = (u_long **)malloc(
	    256*sizeof (u_long *)+(256*nsamples*sizeof(u_long)));
	if (BWmap == NULL) {
		TIFFError(filename, "No space for B&W mapping table");
		return (0);
	}
	p = (u_long *)(BWmap + 256);
	if (isRGB) {
		for (i = 0; i < 256; i++) {
			BWmap[i] = p;
			switch (bitspersample) {
				register RGBvalue c;
#define	GREY(x)	c = Map[x]; *p++ = PACK(c,c,c);
			case 1:
				GREY(i>>7);
				GREY((i>>6)&1);
				GREY((i>>5)&1);
				GREY((i>>4)&1);
				GREY((i>>3)&1);
				GREY((i>>2)&1);
				GREY((i>>1)&1);
				GREY(i&1);
				break;
			case 2:
				GREY(i>>6);
				GREY((i>>4)&3);
				GREY((i>>2)&3);
				GREY(i&3);
				break;
			case 4:
				GREY(i>>4);
				GREY(i&0xf);
				break;
			case 8:
				GREY(i);
				break;
			}
#undef	GREY
		}
	} else {
		for (i = 0; i < 256; i++) {
			BWmap[i] = p;
			switch (bitspersample) {
#define	GREY(x)	*p++ = greyi(Map[x]);
			case 1:
				GREY(i>>7);
				GREY((i>>6)&1);
				GREY((i>>5)&1);
				GREY((i>>4)&1);
				GREY((i>>3)&1);
				GREY((i>>2)&1);
				GREY((i>>1)&1);
				GREY(i&1);
				break;
			case 2:
				GREY(i>>6);
				GREY((i>>4)&3);
				GREY((i>>2)&3);
				GREY(i&3);
				break;
			case 4:
				GREY(i>>4);
				GREY(i&0xf);
				break;
			case 8:
				GREY(i);
				break;
			}
#undef	GREY
		}
	}
	return (1);
}

/*
 * Palette images with <= 8 bits/sample are handled
 * with a table to avoid lots of shifts and masks.  The table
 * is setup so that put*cmaptile (below) can retrieve 8/bitspersample
 * pixel values simply by indexing into the table with one
 * number.
 */
static int
makecmap(uint16* rmap, uint16* gmap, uint16* bmap)
{
	register int i;
	int nsamples = 8 / bitspersample;
	register u_long *p;

	PALmap = (u_long **)malloc(
	    256*sizeof (u_long *)+(256*nsamples*sizeof(u_long)));
	if (PALmap == NULL) {
		TIFFError(filename, "No space for Palette mapping table");
		return (0);
	}
	p = (u_long *)(PALmap + 256);
	if (isRGB) {
		for (i = 0; i < 256; i++) {
			PALmap[i] = p;
#define	CMAP(x)	\
    c = x; *p++ = PACK(rmap[c]&0xff, gmap[c]&0xff, bmap[c]&0xff);
			switch (bitspersample) {
				register RGBvalue c;
			case 1:
				CMAP(i>>7);
				CMAP((i>>6)&1);
				CMAP((i>>5)&1);
				CMAP((i>>4)&1);
				CMAP((i>>3)&1);
				CMAP((i>>2)&1);
				CMAP((i>>1)&1);
				CMAP(i&1);
				break;
			case 2:
				CMAP(i>>6);
				CMAP((i>>4)&3);
				CMAP((i>>2)&3);
				CMAP(i&3);
				break;
			case 4:
				CMAP(i>>4);
				CMAP(i&0xf);
				break;
			case 8:
				CMAP(i);
				break;
			}
#undef CMAP
		}
	} else {
		for (i = 0; i < 256; i++) {
			PALmap[i] = p;
#define	CMAP(x)	\
    c = x; *p++ = rgbi(rmap[c], gmap[c], bmap[c]);
			switch (bitspersample) {
				register RGBvalue c;
			case 1:
				CMAP(i>>7);
				CMAP((i>>6)&1);
				CMAP((i>>5)&1);
				CMAP((i>>4)&1);
				CMAP((i>>3)&1);
				CMAP((i>>2)&1);
				CMAP((i>>1)&1);
				CMAP(i&1);
				break;
			case 2:
				CMAP(i>>6);
				CMAP((i>>4)&3);
				CMAP((i>>2)&3);
				CMAP(i&3);
				break;
			case 4:
				CMAP(i>>4);
				CMAP(i&0xf);
				break;
			case 8:
				CMAP(i);
				break;
			}
#undef CMAP
		}
	}
	return (1);
}

/*
 * The following routines move decoded data returned
 * from the TIFF library into rasters that are suitable
 * for passing to lrecwrite.  They do the necessary
 * conversions based on whether the drawing mode is RGB
 * colormap and whether or not there is a mapping table.
 *
 * The routines have been created according to the most
 * important cases and optimized.  pickTileContigCase and
 * pickTileSeparateCase analyze the parameters and select
 * the appropriate "put" routine to use.
 */
#define	REPEAT8(op)	REPEAT4(op); REPEAT4(op)
#define	REPEAT4(op)	REPEAT2(op); REPEAT2(op)
#define	REPEAT2(op)	op; op
#define	CASE8(x,op)				\
	switch (x) {				\
	case 7: op; case 6: op; case 5: op;	\
	case 4: op; case 3: op; case 2: op;	\
	case 1: op;				\
	}
#define	CASE4(x,op)	switch (x) { case 3: op; case 2: op; case 1: op; }

#define	UNROLL8(w, op1, op2) {		\
	register uint32 x;		\
	for (x = w; x >= 8; x -= 8) {	\
		op1;			\
		REPEAT8(op2);		\
	}				\
	if (x > 0) {			\
		op1;			\
		CASE8(x,op2);		\
	}				\
}
#define	UNROLL4(w, op1, op2) {		\
	register uint32 x;		\
	for (x = w; x >= 4; x -= 4) {	\
		op1;			\
		REPEAT4(op2);		\
	}				\
	if (x > 0) {			\
		op1;			\
		CASE4(x,op2);		\
	}				\
}
#define	UNROLL2(w, op1, op2) {		\
	register uint32 x;		\
	for (x = w; x >= 2; x -= 2) {	\
		op1;			\
		REPEAT2(op2);		\
	}				\
	if (x) {			\
		op1;			\
		op2;			\
	}				\
}
			

#define	SKEW(r,g,b,skew)	{ r += skew; g += skew; b += skew; }

#define	DECLAREContigPutFunc(name) \
static void name(\
    u_long* cp, \
    u_char* pp, \
    RGBvalue* Map, \
    uint32 w, u_long h, \
    int fromskew, int toskew \
)

#define	DECLARESepPutFunc(name) \
static void name(\
    u_long* cp, \
    u_char* r, u_char* g, u_char* b, \
    RGBvalue* Map, \
    uint32 w, uint32 h, \
    int fromskew, int toskew \
)

/*
 * 8-bit packed samples => colormap
 */
DECLAREContigPutFunc(putcontig8bittile)
{
	register uint32 x;

	fromskew *= samplesperpixel;
	if (Map) {
		while (h-- > 0) {
			for (x = w; x-- > 0;) {
				*cp++ = rgbi(Map[pp[0]], Map[pp[1]], Map[pp[2]]);
				pp += samplesperpixel;
			}
			cp += toskew;
			pp += fromskew;
		}
	} else {
		while (h-- > 0) {
			for (x = w; x-- > 0;) {
				*cp++ = rgbi(pp[0], pp[1], pp[2]);
				pp += samplesperpixel;
			}
			cp += toskew;
			pp += fromskew;
		}
	}
}

/*
 * 16-bit packed samples => colormap
 */
DECLAREContigPutFunc(putcontig16bittile)
{
	register uint32 x;

	fromskew *= samplesperpixel;
	if (Map) {
		while (h-- > 0) {
			for (x = w; x-- > 0;) {
				*cp++ = rgbi(Map[pp[0]], Map[pp[1]], Map[pp[2]]);
				pp += samplesperpixel;
			}
			cp += toskew;
			pp += fromskew;
		}
	} else {
		while (h-- > 0) {
			for (x = w; x-- > 0;) {
				*cp++ = rgbi(pp[0], pp[1], pp[2]);
				pp += samplesperpixel;
			}
			cp += toskew;
			pp += fromskew;
		}
	}
}

/*
 * 8-bit unpacked samples => colormap
 */
DECLARESepPutFunc(putseparate8bittile)
{
	register uint32 x;

	if (Map) {
		while (h-- > 0) {
			for (x = w; x-- > 0;)
				*cp++ =
				  rgbi(Map[*r++], Map[*g++], Map[*b++]);
			SKEW(r, g, b, fromskew);
			cp += toskew;
		}
	} else {
		while (h-- > 0) {
			for (x = w; x-- > 0;)
				*cp++ = rgbi(*r++, *g++, *b++);
			SKEW(r, g, b, fromskew);
			cp += toskew;
		}
	}
}

/*
 * 16-bit unpacked samples => colormap
 */
DECLARESepPutFunc(putseparate16bittile)
{
	register uint32 x;

	if (Map) {
		while (h-- > 0) {
			for (x = 0; x < w; x++)
				*cp++ =
				  rgbi(Map[*r++], Map[*g++], Map[*b++]);
			SKEW(r, g, b, fromskew);
			cp += toskew;
		}
	} else {
		while (h-- > 0) {
			for (x = 0; x < w; x++)
				*cp++ = rgbi(*r++, *g++, *b++);
			SKEW(r, g, b, fromskew);
			cp += toskew;
		}
	}
}

/*
 * 8-bit palette => colormap/RGB
 */
DECLAREContigPutFunc(put8bitcmaptile)
{
	while (h-- > 0) {
		UNROLL8(w,, *cp++ = PALmap[*pp++][0]);
		cp += toskew;
		pp += fromskew;
	}
}

/*
 * 4-bit palette => colormap/RGB
 */
DECLAREContigPutFunc(put4bitcmaptile)
{
	register u_long *bw;

	fromskew /= 2;
	while (h-- > 0) {
		UNROLL2(w, bw = PALmap[*pp++], *cp++ = *bw++);
		cp += toskew;
		pp += fromskew;
	}
}

/*
 * 2-bit palette => colormap/RGB
 */
DECLAREContigPutFunc(put2bitcmaptile)
{
	register u_long *bw;

	fromskew /= 4;
	while (h-- > 0) {
		UNROLL4(w, bw = PALmap[*pp++], *cp++ = *bw++);
		cp += toskew;
		pp += fromskew;
	}
}

/*
 * 1-bit palette => colormap/RGB
 */
DECLAREContigPutFunc(put1bitcmaptile)
{
	register u_long *bw;

	fromskew /= 8;
	while (h-- > 0) {
		UNROLL8(w, bw = PALmap[*pp++], *cp++ = *bw++);
		cp += toskew;
		pp += fromskew;
	}
}

/*
 * 8-bit greyscale => colormap/RGB
 */
DECLAREContigPutFunc(putgreytile)
{
	while (h-- > 0) {
		register uint32 x;
		for (x = w; x-- > 0;)
			*cp++ = BWmap[*pp++][0];
		cp += toskew;
		pp += fromskew;
	}
}

/*
 * 1-bit bilevel => colormap/RGB
 */
DECLAREContigPutFunc(put1bitbwtile)
{
	register u_long *bw;

	fromskew /= 8;
	while (h-- > 0) {
		UNROLL8(w, bw = BWmap[*pp++], *cp++ = *bw++);
		cp += toskew;
		pp += fromskew;
	}
}

/*
 * 2-bit greyscale => colormap/RGB
 */
DECLAREContigPutFunc(put2bitbwtile)
{
	register u_long *bw;

	fromskew /= 4;
	while (h-- > 0) {
		UNROLL4(w, bw = BWmap[*pp++], *cp++ = *bw++);
		cp += toskew;
		pp += fromskew;
	}
}

/*
 * 4-bit greyscale => colormap/RGB
 */
DECLAREContigPutFunc(put4bitbwtile)
{
	register u_long *bw;

	fromskew /= 2;
	while (h-- > 0) {
		UNROLL2(w, bw = BWmap[*pp++], *cp++ = *bw++);
		cp += toskew;
		pp += fromskew;
	}
}

/*
 * 8-bit packed RGB samples => RGB
 */
DECLAREContigPutFunc(putRGBcontig8bittile)
{
	fromskew *= samplesperpixel;
	if (Map) {
		while (h-- > 0) {
			register uint32 x;
			for (x = w; x-- > 0;) {
				*cp++ = PACK(Map[pp[0]], Map[pp[1]], Map[pp[2]]);
				pp += samplesperpixel;
			}
			pp += fromskew;
			cp += toskew;
		}
	} else {
		while (h-- > 0) {
			UNROLL8(w,,
			    *cp++ = PACK(pp[0], pp[1], pp[2]);
			    pp += samplesperpixel);
			cp += toskew;
			pp += fromskew;
		}
	}
}

/*
 * 16-bit packed RGB samples => RGB
 */
DECLAREContigPutFunc(putRGBcontig16bittile)
{
	register uint32 x;

	fromskew *= samplesperpixel;
	if (Map) {
		while (h-- > 0) {
			for (x = w; x-- > 0;) {
				*cp++ = PACK(Map[pp[0]], Map[pp[1]], Map[pp[2]]);
				pp += samplesperpixel;
			}
			cp += toskew;
			pp += fromskew;
		}
	} else {
		while (h-- > 0) {
			for (x = w; x-- > 0;) {
				*cp++ = PACK(pp[0], pp[1], pp[2]);
				pp += samplesperpixel;
			}
			cp += toskew;
			pp += fromskew;
		}
	}
}

/*
 * 8-bit unpacked RGB samples => RGB
 */
DECLARESepPutFunc(putRGBseparate8bittile)
{
	if (Map) {
		while (h-- > 0) {
			register uint32 x;
			for (x = w; x > 0; x--)
				*cp++ = PACK(Map[*r++], Map[*g++], Map[*b++]);
			SKEW(r, g, b, fromskew);
			cp += toskew;
		}
	} else {
		while (h-- > 0) {
			UNROLL8(w,, *cp++ = PACK(*r++, *g++, *b++));
			SKEW(r, g, b, fromskew);
			cp += toskew;
		}
	}
}

/*
 * 16-bit unpacked RGB samples => RGB
 */
DECLARESepPutFunc(putRGBseparate16bittile)
{
	register uint32 x;

	if (Map) {
		while (h-- > 0) {
			for (x = w; x > 0; x--)
				*cp++ = PACK(Map[*r++], Map[*g++], Map[*b++]);
			SKEW(r, g, b, fromskew);
			cp += toskew;
		}
	} else {
		while (h-- > 0) {
			for (x = 0; x < w; x++)
				*cp++ = PACK(*r++, *g++, *b++);
			SKEW(r, g, b, fromskew);
			cp += toskew;
		}
	}
}

/*
 * 8-bit packed CMYK samples => RGB
 *
 * NB: The conversion of CMYK->RGB is *very* crude.
 */
DECLAREContigPutFunc(putRGBcontig8bitCMYKtile)
{
	uint16 r, g, b, k;

	fromskew *= samplesperpixel;
	if (Map) {
		while (h-- > 0) {
			register uint32 x;
			for (x = w; x-- > 0;) {
				k = 255 - pp[3];
				r = (k*(255-pp[0]))/255;
				g = (k*(255-pp[1]))/255;
				b = (k*(255-pp[2]))/255;
				*cp++ = PACK(Map[r], Map[g], Map[b]);
				pp += samplesperpixel;
			}
			pp += fromskew;
			cp += toskew;
		}
	} else {
		while (h-- > 0) {
			UNROLL8(w,,
			    k = 255 - pp[3];
			    r = (k*(255-pp[0]))/255;
			    g = (k*(255-pp[1]))/255;
			    b = (k*(255-pp[2]))/255;
			    *cp++ = PACK(r, g, b);
			    pp += samplesperpixel);
			cp += toskew;
			pp += fromskew;
		}
	}
}

/*
 * 8-bit packed CMYK samples => cmap
 *
 * NB: The conversion of CMYK->RGB is *very* crude.
 */
DECLAREContigPutFunc(putcontig8bitCMYKtile)
{
	uint16 r, g, b, k;

	fromskew *= samplesperpixel;
	if (Map) {
		while (h-- > 0) {
			register uint32 x;
			for (x = w; x-- > 0;) {
				k = 255 - pp[3];
				r = (k*(255-pp[0]))/255;
				g = (k*(255-pp[1]))/255;
				b = (k*(255-pp[2]))/255;
				*cp++ = rgbi(Map[r], Map[g], Map[b]);
				pp += samplesperpixel;
			}
			pp += fromskew;
			cp += toskew;
		}
	} else {
		while (h-- > 0) {
			UNROLL8(w,,
			    k = 255 - pp[3];
			    r = (k*(255-pp[0]))/255;
			    g = (k*(255-pp[1]))/255;
			    b = (k*(255-pp[2]))/255;
			    *cp++ = rgbi(r, g, b);
			    pp += samplesperpixel);
			cp += toskew;
			pp += fromskew;
		}
	}
}

#define	Code2V(c, RB, RW, CR)	((((c)-RB)*(float)CR)/(float)(RW-RB))
#define	CLAMP(f,min,max) \
    (int)((f)+.5 < (min) ? (min) : (f)+.5 > (max) ? (max) : (f)+.5)

#define	LumaRed		YCbCrCoeffs[0]
#define	LumaGreen	YCbCrCoeffs[1]
#define	LumaBlue	YCbCrCoeffs[2]

static	float D1, D2;
static	float D3, D4;

static void
initYCbCrConversion(void)
{
	D1 = 2 - 2*LumaRed;
	D2 = D1*LumaRed / LumaGreen;
	D3 = 2 - 2*LumaBlue;
	D4 = D3*LumaBlue / LumaGreen;
}

static void
putRGBContigYCbCrClump(cp, pp, cw, ch, w, n, fromskew, toskew)
	register u_long *cp;
	register u_char *pp;
	int cw, ch;
	uint32 w;
	int n, fromskew, toskew;
{
	float Cb, Cr;
	int j, k;

	Cb = Code2V(pp[n],   refBlackWhite[2], refBlackWhite[3], 127);
	Cr = Code2V(pp[n+1], refBlackWhite[4], refBlackWhite[5], 127);
	for (j = 0; j < ch; j++) {
		for (k = 0; k < cw; k++) {
			float Y, R, G, B;
			Y = Code2V(*pp++,
			    refBlackWhite[0], refBlackWhite[1], 255);
			R = Y + Cr*D1;
			B = Y + Cb*D3;
			G = Y - Cb*D4 - Cr*D2;
			cp[k] = PACK(CLAMP(R,0,255),
				     CLAMP(G,0,255),
				     CLAMP(B,0,255));
		}
		cp += w+toskew;
		pp += fromskew;
	}
}

static void
putCmapContigYCbCrClump(cp, pp, cw, ch, w, n, fromskew, toskew)
	register u_long *cp;
	register u_char *pp;
	int cw, ch;
	uint32 w;
	int n, fromskew, toskew;
{
	float Cb, Cr;
	int j, k;

	Cb = Code2V(pp[n],   refBlackWhite[2], refBlackWhite[3], 127);
	Cr = Code2V(pp[n+1], refBlackWhite[4], refBlackWhite[5], 127);
	for (j = 0; j < ch; j++) {
		for (k = 0; k < cw; k++) {
			float Y, R, G, B;
			Y = Code2V(*pp++,
			    refBlackWhite[0], refBlackWhite[1], 255);
			R = Y + Cr*D1;
			B = Y + Cb*D3;
			G = Y - Cb*D4 - Cr*D2;
			cp[k] = rgbi(CLAMP(R,0,255),
				     CLAMP(G,0,255),
				     CLAMP(B,0,255));
		}
		cp += w+toskew;
		pp += fromskew;
	}
}
#undef LumaBlue
#undef LumaGreen
#undef LumaRed
#undef CLAMP
#undef Code2V

typedef void (*YCbCrPut)(u_long*, u_char*, int, int, uint32, int, int, int);

/*
 * 8-bit packed YCbCr samples => RGB
 */
DECLAREContigPutFunc(putcontig8bitYCbCrtile)
{
	YCbCrPut put;
	int Coff = YCbCrVertSampling * YCbCrHorizSampling;
	u_long *tp;
	uint32 x;

	/* XXX adjust fromskew */
	put = (isRGB ? putRGBContigYCbCrClump : putCmapContigYCbCrClump);
	while (h >= YCbCrVertSampling) {
		tp = cp;
		for (x = w; x >= YCbCrHorizSampling; x -= YCbCrHorizSampling) {
			(*put)(tp, pp, YCbCrHorizSampling, YCbCrVertSampling,
			    w, Coff, 0, toskew);
			tp += YCbCrHorizSampling;
			pp += Coff+2;
		}
		if (x > 0) {
			(*put)(tp, pp, x, YCbCrVertSampling,
			    w, Coff, YCbCrHorizSampling - x, toskew);
			pp += Coff+2;
		}
		cp += YCbCrVertSampling*(w + toskew);
		pp += fromskew;
		h -= YCbCrVertSampling;
	}
	if (h > 0) {
		tp = cp;
		for (x = w; x >= YCbCrHorizSampling; x -= YCbCrHorizSampling) {
			(*put)(tp, pp, YCbCrHorizSampling, h,
			    w, Coff, 0, toskew);
			tp += YCbCrHorizSampling;
			pp += Coff+2;
		}
		if (x > 0)
			(*put)(tp, pp, x, h,
			    w, Coff, YCbCrHorizSampling - x, toskew);
	}
}

/*
 * Select the appropriate conversion routine for packed data.
 */
static tileContigRoutine
pickTileContigCase(RGBvalue* Map)
{
	tileContigRoutine put = 0;

	switch (photometric) {
	case PHOTOMETRIC_RGB:
		if (isRGB) {
			switch (bitspersample) {
			case 8:  put = putRGBcontig8bittile; break;
			case 16: put = putRGBcontig16bittile; break;
			}
		} else {
			switch (bitspersample) {
			case 8:  put = putcontig8bittile; break;
			case 16: put = putcontig16bittile; break;
			}
		}
		break;
	case PHOTOMETRIC_SEPARATED:
		if (isRGB) {
			switch (bitspersample) {
			case 8:  put = putRGBcontig8bitCMYKtile; break;
			}
		} else {
			switch (bitspersample) {
			case 8:  put = putcontig8bitCMYKtile; break;
			}
		}
		break;
	case PHOTOMETRIC_PALETTE:
		switch (bitspersample) {
		case 8:	put = put8bitcmaptile; break;
		case 4: put = put4bitcmaptile; break;
		case 2: put = put2bitcmaptile; break;
		case 1: put = put1bitcmaptile; break;
		}
		break;
	case PHOTOMETRIC_MINISWHITE:
	case PHOTOMETRIC_MINISBLACK:
		switch (bitspersample) {
		case 8:	put = putgreytile; break;
		case 4: put = put4bitbwtile; break;
		case 2: put = put2bitbwtile; break;
		case 1: put = put1bitbwtile; break;
		}
		break;
	case PHOTOMETRIC_YCBCR:
		switch (bitspersample) {
		case 8: put = putcontig8bitYCbCrtile; break;
		}
		break;
	}
	if (put == 0) {
		TIFFError(filename,
		    "Can not handle %lu-bit file with photometric %u",
		    bitspersample, photometric);
		exit(-1);
	}
	return (put);
}

/*
 * Select the appropriate conversion routine for unpacked data.
 *
 * NB: we assume that unpacked single channel data is directed
 *     to the "packed routines.
 */
static tileSeparateRoutine
pickTileSeparateCase(RGBvalue* Map)
{
	tileSeparateRoutine put = 0;

	switch (photometric) {
	case PHOTOMETRIC_RGB:
		if (isRGB) {
			switch (bitspersample) {
#ifdef notdef
			case 1:  put = putRGBseparate1bittile; break;
			case 2:  put = putRGBseparate2bittile; break;
			case 4:  put = putRGBseparate4bittile; break;
#endif
			case 8:  put = putRGBseparate8bittile; break;
			case 16: put = putRGBseparate16bittile; break;
			}
		} else {
			switch (bitspersample) {
#ifdef notdef
			case 1:  put = putseparate1bittile; break;
			case 2:  put = putseparate2bittile; break;
			case 4:  put = putseparate4bittile; break;
#endif
			case 8:  put = putseparate8bittile; break;
			case 16: put = putseparate16bittile; break;
			}
		}
		break;
	}
	if (put == 0) {
		TIFFError(filename,
		    "Can not handle %lu-bit file with photometric %u",
		    bitspersample, photometric);
		exit(-1);
	}
	return (put);
}
