/*
Copyright 1996 by Syd Logan 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Syd Logan not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

SYD LOGAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
SYD LOGAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#ifdef WIN32
#define _WILLWINSOCK_
#endif
#include <ctype.h>
#ifdef WIN32
#define BOOL wBOOL
#undef Status
#define Status wStatus
#include <winsock.h>
#undef Status
#define Status int
#undef BOOL
#endif

#define DEBUG

extern int optind;
extern int opterr;
extern char *optarg;

/* XIE includes */

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>

/* Needed for libtiff data types */

#include "tiffio.h"

void   usage();
static XieExtensionInfo *xieInfo;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	XEvent          event;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotoflo     flo;
	Bool		radio, comp;
	int             fd, idx, floSize, flag, count;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*file, *ofile, rfile[ 128 ], *data, *encoding;
	char		*encodeParms, *decodeParms;
	XieEncodeTechnique encodeTech;
	XieDecodeTechnique decodeTech;
	XieOrientation 	fillorder;
	XieLevels	levels;
	TIFF		*tiff;
	unsigned int 	size;
	XieLTriplet	width, height;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?f:d:e:o:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case 'f':	file = optarg; 
				break;

		case 'e':	encoding = optarg;
				break;

		case 'o':	ofile = optarg;
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		default: 	printf(" unrecognized flag (-%c)\n",flag);
				usage(argv[0]);
				break;
		}
	}

	/* Connect to the display */

	if (display_name == NULL) {
		printf("Display name not defined\n");
		usage(argv[0]);
	}
	
	if (file == NULL) {
		printf("Input file not defined\n");
		usage(argv[0]);
	}

	if (ofile == NULL) {
		printf("Output file not defined\n");
		usage(argv[0]);
	}

	/* Get the encoding parameters for the technique specified by the
	   user */

	if ( encoding == NULL ) {
		printf("Encoding not specified\n");
		usage(argv[0]);
	}

	if ( GetEncodeParams( encoding, &encodeTech, &encodeParms, &radio,
		&comp, &fillorder ) == -1 )
	{
		printf("Encoding '%s' invalid\n", encoding );
		usage(argv[0]);
	}

	if ( encodeParms == ( char * ) NULL )
	{
		printf( "Unable to get encode parameters\n" );
		exit( 1 );
	}

	/* figure out what kind of file we are dealing with, and get the
	   proper decode parameters */

	if ( GetDecodeParams( &tiff, file, &decodeTech, &decodeParms,
		&width[ 0 ], &height[ 0 ] ) == -1 )
	{
		printf( "File is not TIFF, or invalid encoding\n" );
		exit( 1 );
	}

	if ( decodeParms == ( char * ) NULL )
	{
		printf( "Unable to get decode parameters\n" );
		exit( 1 );
	}

	if ((display = XOpenDisplay(display_name)) == (Display *) NULL) {
		fprintf(stderr, "Unable to connect to display\n");
		exit(1);
	}

	/* Connect to XIE extension */

	if (!XieInitialize(display, &xieInfo)) {
		fprintf(stderr, "XIE not supported on this display!\n");
		exit(1);
	}

	printf("XIE server V%d.%d\n", xieInfo->server_major_rev, 
		xieInfo->server_minor_rev);

	/* Create and populate a stored photoflo graph */

	floSize = 2;

	flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;
	levels[ 0 ] = 2;
	XieFloImportClientPhoto(&flograph[idx],
		xieValSingleBand,	/* bitonals are always SingleBand */
		width,			/* width of image in pixels */
		height,			/* height of image in pixels */
		levels,			/* indicates bi-level */
		False,			/* don't send DecodeNotify events */
		decodeTech,		/* tell XIE how the image is encoded */
		decodeParms		/* and the associated decode params */
	); 
	idx++;

	XieFloExportClientPhoto(&flograph[idx], 
		idx, 			/* source */
		xieValNewData,		/* send ExportAvailable events */
		encodeTech,		/* tell XIE how to encode image */
		encodeParms		/* and the encode params */
	);
	idx++;

	/* Send the flograph to the server and get a handle back */

	flo = XieCreatePhotoflo(display, flograph, floSize);

	/* run the flo. we don't care when the photoflo finishes, so 
	   set notify to False */

	notify = False;
	XieExecutePhotoflo(display, flo, notify);

	/* read the image data from the TIFF file, again using libtiff */

	if ( GetTIFFImageData( tiff, &data, &size ) == -1 )
	{
		printf( "Unable to read strips from TIFF file\n" );
		exit( 1 );
	}

	/* stream it to XIE */

	PumpTheClientData( display, flo, 0, 1, data, size, 1, 0, True );

	/* free the buffer for re-use below */

	free( data );
	data = (char *) NULL;

	/* next, read the newly encoded image back from ExportClientPhoto */

	count = ReadNotifyExportData( display, xieInfo, 0, flo, 2, 1, 100, 
		0, &data );

	/* write the image to a raw file */

	printf( "writing raw file: width %d height %d\n", width[ 0 ], 
		height[ 0 ] );

	sprintf( rfile, "%s.raw", ofile );
	fd =  open( rfile, O_CREAT | O_TRUNC | O_RDWR, 0666 );
	write( fd, data, count );
	close( fd );

	/* then write the image to a TIFF file */ 

	WriteTIFFImageData( ofile, data, count, encodeTech, width[0], 
		height[0], radio, comp, fillorder );

	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotoflo(display, flo);
	XFree(encodeParms); 
	XFree(decodeParms); 
	XCloseDisplay(display);
}

int
WriteTIFFImageData( ofile, data, size, encode, width, height, radio, comp,
	fillorder )
char	*ofile;
char 	*data;
int	size;
XieEncodeTechnique encode;
unsigned long width, height;
Bool	radio, comp;
XieOrientation fillorder;
{
	TIFF	*tif;
	short	compression;
        unsigned long group3opts;

	/* open the file, and write the header */

    	tif = TIFFOpen(ofile, "w");
    	if (!tif) {
		fprintf( stderr, "Couldn't create '%s'\n", ofile );
		return( -1 );
	}

	/* set the tags in the IFD */

    	TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (uint32) width);
    	TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32) height);

	switch( encode ) {
	case xieValEncodeG31D:
		compression = COMPRESSION_CCITTFAX3;
    		TIFFSetField(tif, TIFFTAG_GROUP3OPTIONS, 0);
		break;
        case xieValEncodeG32D:
		compression = COMPRESSION_CCITTFAX3;
		if ( comp == True )
			TIFFSetField(tif, TIFFTAG_GROUP3OPTIONS, 
				GROUP3OPT_UNCOMPRESSED | GROUP3OPT_2DENCODING);
		else
			TIFFSetField(tif, TIFFTAG_GROUP3OPTIONS, 
				GROUP3OPT_2DENCODING);
		break;
        case xieValEncodeG42D:
		compression = COMPRESSION_CCITTFAX4;
		if ( comp == True )
			TIFFSetField(tif, TIFFTAG_GROUP4OPTIONS, GROUP4OPT_UNCOMPRESSED);
		else
			TIFFSetField(tif, TIFFTAG_GROUP4OPTIONS, 0);
		
		break;
        case xieValEncodeTIFF2:
		compression = COMPRESSION_CCITTRLE;
		break;
        case xieValEncodeTIFFPackBits:
		compression = COMPRESSION_PACKBITS;
		break;
	}
    	TIFFSetField(tif, TIFFTAG_COMPRESSION, compression);

	switch ( radio ) {
	case True:
	    	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
		break;
	case False:
	    	TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);
		break;
	}

	/* resolution is unspecified */

	TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, RESUNIT_NONE);

#if 0
	TIFFSetField(tif, TIFFTAG_XRESOLUTION, 0);
	TIFFSetField(tif, TIFFTAG_YRESOLUTION, 0);
#endif

        if ( fillorder == xieValMSFirst )
        {
#if defined( DEBUG )
	fprintf( stderr, "FillOrder tag is MSFirst\n" );
	fflush( stderr );
#endif
        	TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
	}
	else
	{
#if defined( DEBUG )
	fprintf( stderr, "FillOrder tag is LSFirst\n", size );
	fflush( stderr );
#endif
        	TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_LSB2MSB);
	}

    	TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

#if defined( DEBUG )
	fprintf( stderr, "encoded image is %d bytes\n", size );
	fflush( stderr );
#endif
	TIFFWriteRawStrip( tif, ( tstrip_t ) 0, data, size ); 

    	TIFFClose(tif);

	return( 0 );
}

int
GetTIFFImageData( tiff, data, size )
TIFF	*tiff;
char	**data;
unsigned int	*size;
{
        unsigned long *bc;
        unsigned long bufsize = 0;
        int cc, s;
        unsigned char *cp;

#if defined( DEBUG )
fprintf( stderr, "GetTIFFImageData enter\n" );
fflush( stderr );
#endif
        TIFFGetField(tiff, TIFFTAG_STRIPBYTECOUNTS, &bc);
        for (s = 0; s < TIFFNumberOfStrips(tiff); s++) 
		bufsize += bc[s];
	*data = malloc( bufsize );
        if (*data == NULL) {
		printf("No space for strip buffer");
		return( -1 );
	}
	cp = (unsigned char *) *data;
        for (s = 0; s < TIFFNumberOfStrips(tiff); s++) {
#if	defined( DEBUG)
	fprintf( stderr, "cp %d\n", cp );
#endif
		cc = TIFFReadRawStrip(tiff, s, cp, bc[s]);
#if	defined( DEBUG)
	fprintf( stderr, "strip is %d bytes\n", bc[s] );
#endif
                if (cc < 0) {
                        printf("Can't read strip");
                        free(*data);
                        return( -1 );
                }
		cp += bc[s];
	}
	*size = ( unsigned int ) bufsize;
#if defined( DEBUG )
fprintf( stderr, "GetTIFFImageData %d bytes exit\n", bufsize );
fflush( stderr );
#endif
	return( 0 );
}

int
GetResponse( msg, opt1, opt2 )
char	*msg;
char	*opt1;
char	*opt2;
{
	char	buf[ 64 ];

	for ( ;; )
	{
		printf( "\n\n%s:\n\n1) %s\n2) %s\n\nEnter choice (1 or 2): ",
			msg, opt1, opt2 );
		gets( buf );
		if ( buf[ 0 ] == '1' || buf[ 0 ] == '2' )
			break;
	} 
	return( ( buf[ 0 ] - '0' ) - 1 );
}

Bool
GetBool( msg )
char	*msg;
{
	Bool response[ 2 ] = { True, False };
	int	i;

	i = GetResponse( msg, "True", "False" );
	return( response[ i ] );
}

unsigned long
GetUnsignedLong( msg )
char	*msg;
{
	unsigned long val;
	char	buf[ 64 ];

	printf( "%s: ", msg );
	gets( buf );
	return( ( unsigned long ) atol( buf ) );
}

XieOrientation
GetOrientation()
{
	int 	val; 
	char	*ptr; 
	XieOrientation orientation;


	val = GetResponse( "Select Orientation", "LSFirst", "MSFirst" );	
	ptr = ( char*) &val;
	if ( *ptr == 0 )
	{
#if defined( DEBUG )
		fprintf( stderr, "encoding xieValLSFirst\n" );
		fflush( stderr );
#endif
		orientation = xieValLSFirst;	
	}
	else
	{
#if defined( DEBUG )
		fprintf( stderr, "encoding xieValMSFirst\n" );
		fflush( stderr );
#endif
		orientation = xieValMSFirst;
	}
	return( orientation );
}

char *
GetG31DEncodeParms( radio, orientation ) 
Bool	*radio;
XieOrientation *orientation;
{
	/* align-eol is hardcoded to true */


	printf( "\nEncoding G31D" );
	*radio = GetBool( "Image ones represent white?" );
	*orientation = GetOrientation();
	return ((char *) XieTecEncodeG31D( True, *radio, *orientation ));
}

char *
GetG32DEncodeParms( radio, g3comp, orientation )
Bool	*radio;
Bool 	*g3comp;
XieOrientation *orientation;
{
	/* align-eol is hardcoded to true */

	unsigned long kFactor;

	printf( "\nEncoding G32D" );
	*g3comp = GetBool( "Uncompressed mode?" );
	*radio = GetBool( "Image ones represent white?" );
	*orientation = GetOrientation();
	kFactor = GetUnsignedLong( 
		"Enter number of 2D scanlines for each 1D scanline (kFactor)" );
#if	defined( DEBUG )
	printf( "Uncompressed is %s\n", (*g3comp == True ? "True" : "False" ) );
#endif
	return ((char *) XieTecEncodeG32D( *g3comp, 
		True, *radio, *orientation, kFactor ));
}

char *
GetG42DEncodeParms( radio, g4comp, orientation )
Bool	*radio;
Bool	*g4comp;
XieOrientation *orientation;
{
	printf( "\nEncoding G42D" );
	*g4comp = GetBool( "Store data uncompressed?" );
	*radio = GetBool( "Image ones represent white?" );
	*orientation = GetOrientation();
	return ((char *) XieTecEncodeG42D( *g4comp, *radio, *orientation ));
}

char *
GetTIFF2EncodeParms( radio, orientation )
Bool	*radio;
XieOrientation *orientation;
{
	Bool radiometric;

	printf( "\nEncoding TIFF2" );
	*radio = GetBool( "Image ones represent white?" );
	*orientation = GetOrientation();
	return ((char *) XieTecEncodeTIFF2( *orientation, *radio ));
}

char *
GetPackBitsEncodeParms( orientation )
XieOrientation *orientation;
{

	printf( "\nEncoding TIFFPackBits" );
	*orientation = GetOrientation();
	return ((char *) XieTecEncodeTIFFPackBits( *orientation ));
}

int
GetEncodeParams( encoding, encodeTech, encodeParms, radio, comp, fillorder )
char	*encoding;
XieEncodeTechnique *encodeTech;
char	**encodeParms;
Bool	*radio;
Bool	*comp;
XieOrientation *fillorder;
{
	int	i, len, retval;

	*radio = True;
	retval = 0;
	len = strlen( encoding );
	for ( i = 0; i < len; i++ )
		encoding[ i ] = toupper( encoding[ i ] );

	if ( !strcmp( encoding, "G31D" ) )
	{	
		*encodeTech = xieValEncodeG31D;
		*encodeParms = GetG31DEncodeParms( radio, fillorder );
	}
	else if ( !strcmp( encoding, "G32D" ) )
	{
		*encodeTech = xieValEncodeG32D;
		*encodeParms = GetG32DEncodeParms( radio, comp, fillorder );
	}
	else if ( !strcmp( encoding, "G42D" ) )
	{
		*encodeTech = xieValEncodeG42D;
		*encodeParms = GetG42DEncodeParms( radio, comp, fillorder );
	}
	else if ( !strcmp( encoding, "TIFF2" ) )
	{
		*encodeTech = xieValEncodeTIFF2;
		*encodeParms = GetTIFF2EncodeParms( radio, fillorder );
	}
	else if ( !strcmp( encoding, "TIFFPACKBITS" ) )
	{	
		*encodeTech = xieValEncodeTIFFPackBits;
		*encodeParms = GetPackBitsEncodeParms( fillorder );
	}
	else
		retval = -1;

	return( retval );
}

char	*
SetDecodeParms( decodeTech, orientation, photoMetric )
XieDecodeTechnique decodeTech;
XieOrientation orientation;
Bool photoMetric;
{
	char	*parms = ( char * ) NULL;
	Bool	normal;

	normal = GetBool( "For image being decoded, specify normal" );
	switch( decodeTech )
	{
	case xieValDecodeG31D:
		parms = ( char * ) XieTecDecodeG31D( orientation, normal,
			photoMetric );
		break;
	case xieValDecodeG32D:
		parms = ( char * ) XieTecDecodeG32D( orientation, normal,
			photoMetric );
		break;
	case xieValDecodeG42D:
		parms = ( char * ) XieTecDecodeG42D( orientation, normal,
			photoMetric );
		break;
	case xieValDecodeTIFFPackBits:
		parms = ( char * ) XieTecDecodeTIFFPackBits( orientation, 
			normal );
		break;
	case xieValDecodeTIFF2:
		parms = ( char * ) XieTecDecodeTIFF2( orientation, normal,
			photoMetric );
		break;
	}
	return( parms );
}

int
GetDecodeParams( tiff, file, decodeTech, decodeParms, width, height )
TIFF	**tiff;
char	*file;
XieDecodeTechnique *decodeTech;
char	**decodeParms;
unsigned long *width, *height;
{
	int	retval;
	unsigned short photometric, compression, fillorder, bitspersample;
	unsigned long group3opts;
	XieOrientation orientation;
	Bool	pMetric;

	/* Use libtiff to open the TIFF file */

	*tiff = TIFFOpen( file, "r" );
	if ( *tiff == ( TIFF * ) NULL )
		return( -1 );

	/* If the image is not bilevel, then forget it... */

        TIFFGetFieldDefaulted(*tiff, TIFFTAG_BITSPERSAMPLE, &bitspersample);

#if	defined( DEBUG )
	printf( "bitspersample is %d\n", bitspersample );
#endif

	if ( bitspersample != 1 )
		return( -1 );

	/* Get the decode parameters by inspecting tag fields. We will only 
	   care about those tags which intersect the set of required fields 
	   for TIFF 5.0 and greater, and the decode parameters which are 
	   recognized by XIE */

	/* compression */

        TIFFGetField(*tiff, TIFFTAG_COMPRESSION, &compression);

printf( "compression is %d\n", compression );
	switch( compression ) 
	{
        case COMPRESSION_CCITTRLE:	

#if	defined( DEBUG )
	printf( "compression is TIFF2\n" );
#endif

		*decodeTech = xieValDecodeTIFF2;
                break;
        case COMPRESSION_CCITTRLEW:	
		/* XXX */

#if	defined( DEBUG )
	printf( "compression is TIFF2\n" );
#endif

		*decodeTech = xieValDecodeTIFF2; 
                break;
        case COMPRESSION_CCITTFAX3:
                if ( TIFFGetField(*tiff, TIFFTAG_GROUP3OPTIONS, &group3opts) == 1 )
		{
			if ( group3opts & GROUP3OPT_2DENCODING )
			{	

#if	defined( DEBUG )
				printf( "compression is G32D\n" );
#endif

				*decodeTech = xieValDecodeG32D;
			}
		}
		else
		{

#if	defined( DEBUG )
			printf( "compression is G31D\n" );
#endif

			*decodeTech = xieValDecodeG31D;
		}
                break;
        case COMPRESSION_CCITTFAX4:	

#if	defined( DEBUG )
	printf( "compression is G42D\n" );
#endif

		*decodeTech = xieValDecodeG42D;
                break;
        case COMPRESSION_PACKBITS:

#if	defined( DEBUG )
	printf( "compression is TIFFPackBits\n" );
#endif

		*decodeTech = xieValDecodeTIFFPackBits;
		break;
	default:
		return( -1 );
		break;
	}

	/* photometric */

        TIFFGetField(*tiff, TIFFTAG_PHOTOMETRIC, &photometric);
	if ( photometric == PHOTOMETRIC_MINISWHITE )
	{

#if	defined( DEBUG )
	printf( "photometric is False\n" );
#endif

		pMetric = False;
	}
	else
	{

#if	defined( DEBUG )
	printf( "photometric is True\n" );
#endif

		pMetric = True;
	}

	/* fill order */

	TIFFGetFieldDefaulted(*tiff, TIFFTAG_FILLORDER, &fillorder);
	if ( fillorder == FILLORDER_MSB2LSB )
	{

#if	defined( DEBUG )
	printf( "orientation is MSFirst\n" );
#endif

		orientation = xieValMSFirst;
	}
	else
	{

#if	defined( DEBUG )
	printf( "orientation is LSFirst\n" );
#endif

		orientation = xieValLSFirst;
	}

	/* width and height */

        TIFFGetField(*tiff, TIFFTAG_IMAGEWIDTH, width);
        TIFFGetField(*tiff, TIFFTAG_IMAGELENGTH, height);


#if	defined( DEBUG )
	printf( "width is %d height is %d\n", *width, *height );
#endif

	/* Now, build the decode parameters */

	*decodeParms = SetDecodeParms( *decodeTech, orientation, pMetric ); 
	return( 0 );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -e [G31D | G32D | G42D | TIFF2 | TIFFPackBits] -f file -o outfile\n", pgm);
	exit(1);
}
