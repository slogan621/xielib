#define DEBUG

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
#include <Xatom.h>

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

extern int optind;
extern int opterr;
extern char *optarg;

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>
#include "events.h"

#include "backend.h"

/* Needed for libtiff data types */

#include "tiffio.h"

void   usage();
static XieExtensionInfo *xieInfo;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	int		screen;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
	char		d, l;
	short		w, h;
	Window          win;
	Bool            isTIFF, notify;
	XieConstant     bias;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XieYCbCrToRGBParam *rgbParm;
	XSetWindowAttributes attribs;
	Backend 	*backend;
	int             size, flag, done, idx, src, 
			beSize, floSize, floId, bandMask;
	XieLTriplet     width, height, levels;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL;
	GC 		gc;
	Visual		*visual;
	char		*bytes;
	int		map = -1, class = -1;
       	TIFF            *tiff;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:m:c:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;

		case 'm':	map = ConvertMap( optarg );
				break;

		case 'c':	class = ConvertClass( optarg );
				break;
	
		default: 	printf(" unrecognized flag (-%c)\n",flag);
				usage(argv[0]);
				break;
		}
	}

	if ( img_file == ( char * ) NULL )
	{
		printf( "Image file not defined\n" );
		usage( argv[ 0 ] );
	}

        /* figure out what kind of file we are dealing with, and get the
           proper decode parameters */

        if ( GetDecodeParams( &tiff, img_file, &decodeTech, &decodeParms,
                &w, &h ) == -1 )
	{
		if ( ( size = GetJFIFData( img_file, &bytes, &d, &w, &h, &l ) ) == 0 )
		{
			printf( "Problem getting JPEG data from %s\n", img_file );
			exit( 1 );
		}

		if ( d != 8 )
		{
			printf( "JPEG image must be 256 levels\n" );
			exit( 1 );
		}
		isTIFF = False;
	}
	else
	{
		d = 1;
		l = 1;
		isTIFF = True;

		/* read the image data from the TIFF file, using libtiff */

		if ( GetTIFFImageData( tiff, &bytes, &size ) == -1 )
		{
			printf( "Unable to read strips from TIFF file\n" );
			exit( 1 );
		}

		w = 640;
		h = 480;
	}

	/* Connect to the display */

	if (display_name == NULL) {
		printf("Display name not defined\n");
		usage(argv[0]);
	}

	if ((display = XOpenDisplay(display_name)) == (Display *) NULL) {
		fprintf(stderr, "Unable to connect to display\n");
		exit(1);
	}

	screen = DefaultScreen( display );

	/* Connect to XIE extension */

	if (!XieInitialize(display, &xieInfo)) {
		fprintf(stderr, "XIE not supported on this display!\n");
		exit(1);
	}

	printf("XIE server V%d.%d\n", xieInfo->server_major_rev, 
		xieInfo->server_minor_rev);

	/* Do some X stuff - create colormaps, windows, etc. */

	visual = DefaultVisual( display, screen );
	if ( class == -1 )
		class = visual->class;

	if ( l == 1 )
		backend = (Backend *) InitBackend( display, 
			DefaultScreen( display ), class, xieValSingleBand, 
			1 << d, map, &beSize );
	else
		backend = (Backend *) InitBackend( display, 
			DefaultScreen( display ), class, xieValTripleBand, 
			0, map, &beSize );

	if ( backend == (Backend *) NULL )
	{
		fprintf( stderr, "Unable to create backend\n" );
		exit( 1 );
	}

	printf( "size is %d\n", beSize );

	if ( backend )	
	{
		attribs.colormap = backend->cmap;
		attribs.background_pixel = 0L;
		win = XCreateWindow( display, DefaultRootWindow( display ),
			0, 0, w, h, 10, 
			CopyFromParent,
			CopyFromParent, backend->visual,
			(long) CWColormap | CWBackPixel, &attribs );
		XSelectInput( display, win, ExposureMask );
		XMapRaised(display, win);
		WaitForWindow( display, win );
	}
	else
	{
		fprintf( stderr, "Couldn't create backend\n" );
		exit( 1 );
	}

	gc = XCreateGC( display, win, 0L, NULL ); 

        XSetForeground( display, gc, XBlackPixel( display, 0 ) );
        XSetBackground( display, gc, XWhitePixel( display, 0 ) );

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

	floSize = 1;
	if ( l == 3 )
		floSize++;

	/* add in the backend size */

	floSize += beSize;

	flograph = XieAllocatePhotofloGraph(floSize);

	width[0] = width[1] = width[2] = w;
	height[0] = height[1] = height[2] = h;

	if ( isTIFF == False )
	{
		levels[0] = levels[1] = levels[2] = 256; 
		decodeTech = xieValDecodeJPEGBaseline;
		decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
			xieValBandByPixel,
			xieValLSFirst,
			True
		);
	}
	else
		levels[ 0 ] = 2;

	idx = 0;
	notify = False;
	XieFloImportClientPhoto(&flograph[idx], 
		( l == 3 ? xieValTripleBand : xieValSingleBand ),
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify events */ 
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;

	if ( l == 3 )
	{
		bias[ 0 ] = 0.0;
		bias[ 1 ] = bias[ 2 ] = 127.0;
		levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 256;
                rgbParm = XieTecYCbCrToRGB( 
                        levels,
			(double) 0.2125, 
			(double) 0.7154, 
			(double) 0.0721,
                        bias,
                       	xieValGamutNone, 
                        NULL 
		);

	       	XieFloConvertToRGB( &flograph[idx],
			idx,
			xieValYCbCrToRGB,
			( char * ) rgbParm
		);
		idx++;
	}

	/* add backend */

	if ( !InsertBackend( backend, display, win, 0, 0, gc, flograph, idx ) )
	{
		fprintf( stderr, "Backend failed\n" );
		exit( 1 );
	}

	floId = 1;
	notify = True;
	photospace = XieCreatePhotospace(display);

	/* run the flo */

	XieExecuteImmediate(display, photospace, floId, notify, flograph,
	    floSize);

	/* now that the flo is running, send image data */

	PumpTheClientData( display, floId, photospace, 1, bytes, size, 
		sizeof( char ), 0, True );

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

	/* free up what we allocated */

	if ( backend )
		free( backend );
	if ( rgbParm )
		XFree( rgbParm );
	free( bytes );
	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	XFree(decodeParms);

	/* wait for a button or key press before exiting */

	XSelectInput(display, win, ButtonPressMask | KeyPressMask |
		     ExposureMask);
	done = 0;
	while (done == 0) {
		XNextEvent(display, &event);

		switch (event.type) {
		case ButtonPress:
		case KeyPress:
			done = 1;
			break;
		case MappingNotify:
			XRefreshKeyboardMapping((XMappingEvent*)&event);
			break;
		}
	}

	CloseBackend( backend, display );
	XFreeGC(display, gc);
	XCloseDisplay(display);
}

int	
ConvertClass( name )
char	*name;
{
	if ( !strcmp( name, "PseudoColor" ) )
		return( PseudoColor );
	else if ( !strcmp( name, "GrayScale" ) )
		return( GrayScale );
	else if ( !strcmp( name, "DirectColor" ) )
		return( DirectColor );
	else if ( !strcmp( name, "StaticGray" ) )
		return( StaticGray );
	else if ( !strcmp( name, "StaticColor" ) )
		return( StaticColor );
	else if ( !strcmp( name, "TrueColor" ) )
		return( TrueColor );
	return( -1 );
}

int
ConvertMap( name )
char	*name;
{
	if ( !strcmp( name, "BEST" ) )
		return( XA_RGB_BEST_MAP );
	else if ( !strcmp( name, "DEFAULT" ) )
		return( XA_RGB_DEFAULT_MAP );
	else if ( !strcmp( name, "GRAY" ) )
		return( XA_RGB_GRAY_MAP );
	else if ( !strcmp( name, "RED" ) )
		return( XA_RGB_RED_MAP );
	else if ( !strcmp( name, "GREEN" ) )
		return( XA_RGB_GREEN_MAP );
	else if ( !strcmp( name, "BLUE" ) )
		return( XA_RGB_BLUE_MAP );
	return( -1 );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] [-m StandardColormap ] [-c visual class] -i image\n", pgm);
	exit(1);
}

/* XXX should probably go in library */

Bool
GetBool( msg )
char    *msg;
{
        Bool response[ 2 ] = { True, False };
        int     i;

        i = GetResponse( msg, "True", "False" );
        return( response[ i ] );
}

int
GetTIFFImageData( tiff, data, size )
TIFF	*tiff;
char	**data;
unsigned int	*size;
{
        unsigned long *bc;
        unsigned long bufsize;
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
	cp = *data;
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

	TIFFGetField(*tiff, TIFFTAG_FILLORDER, &fillorder);
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

int
GetResponse( msg, opt1, opt2 )
char    *msg;
char    *opt1;
char    *opt2;
{
        char    buf[ 64 ];

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

