
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

extern int optind;
extern int opterr;
extern char *optarg;

/* XIE includes */

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>

#include "tiffio.h"
#include "transform.h"
#include "events.h"

void   usage();
static XieExtensionInfo *xieInfo;
static int gPhotometric;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	XEvent          event;
	Bool            notify;
	XiePhotoElement *flograph;
       	unsigned int 	bandMask = 0x07;
	int             done, fd, idx, floSize, flag, floId;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	char		*getenv(), 
			*display_name = getenv("DISPLAY");
	XieConstant	constant;
	char		*file = NULL, *data = NULL;
       	float   	sx, sy;
	char		*decodeParms;
	XieDecodeTechnique decodeTech;
	XiePhotospace	photospace;
	XieLevels	levels;
	TIFF		*tiff;
	unsigned int 	size;
	XieLTriplet	width, height;
        int     	xr, yr;
        unsigned int 	wr, hr;
        unsigned int 	bwr, dr;
       	Window          root, window;
        transformHandle table[2];
        XiePointer      sampleParms;
	XieGeometryTechnique sampleTech;
        float   	coeffs[6];
        GC              gc;
	int		angle = 0;
	XGCValues 	gcvals;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?i:d:a:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case 'i':	file = optarg; 
				break;

		case 'a':	angle = atoi(optarg); 
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

        XGetGeometry( display, DefaultRootWindow(display), &root, &xr, &yr, 
		&wr, &hr, &bwr, &dr );

	/* faxes are typically taller than they are wide */

	sx = sy = 1.0;

	if ( height[ 0 ] > hr )
        	sy = (float) height[0] / hr; 

        sx = sy; 

        InitTransforms();
        table[ 0 ] = CreateScale( sx, sy );
	if ( angle != 0 )
	{
		table[ 1 ] = CreateRotate( TORAD((float)angle), 
                	(float)width[ 0 ]/sx, (float)hr ); 

		table[ 0 ] = ConcatenateTransforms( table, 2 );
	}
        SetCoefficients( table[ 0 ], coeffs );
        FreeTransformHandle( table[ 0 ] );
	if ( angle != 0 )
		FreeTransformHandle( table[ 1 ] );

        window = XCreateSimpleWindow(display, DefaultRootWindow(display),
                0, 0, width[0] / sx, height[ 0 ] / sy, 1, 0, 
		XWhitePixel( display, DefaultScreen( display ) ) );

	if ( gPhotometric == PHOTOMETRIC_MINISWHITE )
	{
		gcvals.foreground = XBlackPixel( display, DefaultScreen( display ) );
		gcvals.background = XWhitePixel( display, DefaultScreen( display ) );
	}
	else
	{
		gcvals.foreground = XWhitePixel( display, DefaultScreen( display ) );
		gcvals.background = XBlackPixel( display, DefaultScreen( display ) );
	}
		
       	gc = XCreateGC(display, window, GCForeground | GCBackground, (XGCValues *) &gcvals);

        XSelectInput( display, window, ExposureMask );
       	XMapRaised(display, window);

	WaitForWindow( display, window );

	/* Create and populate a stored photoflo graph */

	floSize = 3;

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

        constant[ 0 ] = 128.0;

	sampleTech = xieValGeomBilinearInterp;

        sampleParms = ( XieGeometryTechnique ) NULL; 

        XieFloGeometry(&flograph[idx],
                idx,                    /* image source */
                wr,                     /* width of resulting image */
                hr,                     /* height of resulting image */
                coeffs,                 /* a, b, c, d, e, f */
                constant,               /* used if src pixel does not exist */ 
                bandMask,               /* ignored for SingleBand images */
                sampleTech,             /* sample technique... */ 
                sampleParms             /* and parameters, if required */
        );
        idx++;

        XieFloExportDrawablePlane(&flograph[idx],
                idx,            /* source */
                window,         /* drawable to send data to */
                gc,             /* GC */        
                0,              /* x offset in window to place data */
                0               /* y offset in window to place data */
        );
        idx++;

	/* read the image data from the TIFF file, again using libtiff */

	if ( GetTIFFImageData( tiff, &data, &size ) == -1 )
	{
		printf( "Unable to read strips from TIFF file\n" );
		exit( 1 );
	}

        floId = 1;
        notify = True;
        photospace = XieCreatePhotospace(display);

        /* run the flo */

        XieExecuteImmediate(display, photospace, floId, notify, flograph,
            	floSize);

	/* send image data */

	PumpTheClientData( display, floId, photospace, 1, data, size, 1, 
		0, True );

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

        /* wait for a button or key press before exiting */

        XSelectInput(display, window, ButtonPressMask );

        done = 0;
        while (done == 0) {
                XNextEvent(display, &event);

                switch (event.type) {
                case ButtonPress:
                        done = 1;
                        break;
                }
        }

	free( data );
	XieFreePhotofloGraph(flograph, floSize);
       	XieDestroyPhotospace(display, photospace);
	XFree(decodeParms); 
        XFreeGC(display, gc);
	XCloseDisplay(display);
}

int
GetTIFFImageData( tiff, data, size )
TIFF	*tiff;
char	**data;
unsigned int	*size;
{
        unsigned long *bc;
        unsigned long bufsize = 0L;
        int cc, s;
        unsigned char *cp;

#if defined( DEBUG )
fprintf( stderr, "GetTIFFImageData enter\n" );
fflush( stderr );
#endif
        TIFFGetField(tiff, TIFFTAG_STRIPBYTECOUNTS, &bc);
        for (s = 0; s < TIFFNumberOfStrips(tiff); s++) 
		bufsize += bc[s];
	*data = calloc( bufsize, sizeof( char ) );
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

char	*
SetDecodeParms( decodeTech, orientation, photoMetric )
XieDecodeTechnique decodeTech;
XieOrientation orientation;
Bool photoMetric;
{
	char	*parms = ( char * ) NULL;
	Bool	normal;

	normal = True; 
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

#define DEBUG

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
			if ( group3opts & GROUP3OPT_UNCOMPRESSED )
			{
#if	defined( DEBUG )
				printf( "Group 3 uncompressed not supported\n" );
#endif
				return( -1 );
				break;
			}
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
                        if ( group3opts & GROUP3OPT_UNCOMPRESSED )
                        {
#if     defined( DEBUG )
                                printf( "Group 3 uncompressed not supported\n" );
#endif
                                return( -1 );
                                break;
                        }


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
                if ( TIFFGetField(*tiff, TIFFTAG_GROUP4OPTIONS, &group3opts) == 1 )
		{
			if ( group3opts & GROUP4OPT_UNCOMPRESSED )
			{
#if     defined( DEBUG )
				printf( "Group 4 uncompressed not supported\n" );
#endif
				return( -1 );
				break;
			}
		}

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
	gPhotometric = photometric;
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
	if ( fillorder != FILLORDER_MSB2LSB && fillorder != FILLORDER_LSB2MSB )
		orientation = xieValMSFirst; 
	else if ( fillorder == FILLORDER_MSB2LSB )
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
	printf( "Width is %d height is %d\n", *width, *height );
#endif

	/* Now, build the decode parameters */

	*decodeParms = SetDecodeParms( *decodeTech, orientation, pMetric ); 
	return( 0 );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i image [-a angle]\n", pgm);
	exit(1);
}
