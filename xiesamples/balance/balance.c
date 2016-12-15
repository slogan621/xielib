
/*
Copyright 1996 by Syd Logan 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>
#include "events.h"
#include "backend.h"

void   usage();
static XieExtensionInfo *xieInfo;

static float	redValues[ 3 ] = { 1.025, 0.985, 0.985 };
static float	greenValues[ 3 ] = { 0.980, 1.020, 0.980 };
static float	blueValues[ 3 ] = { 0.995, 0.995, 1.035 };

#define	CCIR_RED 	0.2125
#define CCIR_GREEN 	0.7154
#define CCIR_BLUE 	0.0721

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	Backend 	*backend;
	XEvent          event;
	Window          win;
	char		d, l;
	short		w, h;
	Bool            notify;
	char		*decodeParms;
	char		*encodeParms;
	int		decodeTech;
	int		encodeTech;
	XiePhotoElement *flograph;
	XieProcessDomain domain;
	XiePhotospace   photospace;
	GC 		gc;
	int		size;
	char		*bytes;
	XieYCbCrToRGBParam *convertToRGBParm;
	XieConstant 	coefficients, biasVec;	
	XIEEventCheck	eventData;
        unsigned char   samples[3] = { 1, 1, 1 };
	char		*shapeParms = ( char * ) NULL;
	int		redFactor = 0, 
			greenFactor = 0, 
			blueFactor = 0,
			cyanFactor = 0,
			magentaFactor = 0,
			yellowFactor = 0;
	int             beSize, src, i, done, idx, floSize, floId, bandMask;
	XieLTriplet     width, height, levels;
	int		screen, flag, fd, count;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL, *out_file = NULL;
	XieRGBToYCbCrParam *toYCbCrParms = NULL;
        Visual          *visual;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:f:r:g:b:c:m:y:"))!=EOF) {
		switch(flag) {

		case 'r':	redFactor = atoi( optarg );
				break;

		case 'g':	greenFactor = atoi( optarg );
				break;

		case 'b':	blueFactor = atoi( optarg );
				break;

		case 'c':	cyanFactor = atoi( optarg );
				break;

		case 'm':	magentaFactor = atoi( optarg );
				break;

		case 'y':	yellowFactor = atoi( optarg );
				break;

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;

		case 'f':	out_file = optarg;	
				break;

		default: 	printf(" unrecognized flag (-%c)\n",flag);
				usage(argv[0]);
				break;
		}
	}

	if ( img_file == ( char * ) NULL ) {
		printf( "Image file not defined\n" );
		usage( argv[ 0 ] );
	}

	if ( ( size = GetJFIFData( img_file, &bytes, &d, &w, &h, &l ) ) == 0 ) {
		printf( "Problem getting JPEG data from %s\n", img_file );
		exit( 1 );
	}
	if ( l != 3 ) {
		printf( "Image is not TripleBand\n" );
		exit( 1 );
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

	/* Connect to XIE extension */

	if (!XieInitialize(display, &xieInfo)) {
		fprintf(stderr, "XIE not supported on this display!\n");
		exit(1);
	}

	printf("XIE server V%d.%d\n", xieInfo->server_major_rev, 
		xieInfo->server_minor_rev);

	/* Do some X stuff - create colormaps, windows, etc. */

	screen = DefaultScreen(display);
	win = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	0, 0, w, h, 1, 0, 255 );

	XSelectInput( display, win, ExposureMask );

	XMapRaised(display, win);

	gc = XCreateGC(display, win, 0L, (XGCValues *) NULL);

	WaitForWindow( display, win );
	XSync( display, 0 );

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

        visual = DefaultVisual( display, screen );

        backend = (Backend *) InitBackend( display, screen, visual->class,
                xieValTripleBand, 0, -1, &beSize );

        if ( backend == (Backend *) NULL ) {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }

	floSize = 3 + beSize;
	if ( out_file )
		floSize+=2;

	flograph = XieAllocatePhotofloGraph(floSize);

	width[0] = w;
	width[1] = width[2] = w;
	height[0] = h;
	height[1] = height[2] = h;
	levels[0] = pow( 2.0, (double) d);
	levels[1] = levels[2] = levels[0];

	decodeTech = xieValDecodeJPEGBaseline;
	decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
		xieValBandByPixel,
		xieValLSFirst,
		True
	);

	idx = 0;
	notify = False;
	XieFloImportClientPhoto(&flograph[idx], 
		xieValTripleBand, 	/* class of image data */
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify events */
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;

	biasVec[ 0 ] = 0.0;
	biasVec[ 1 ] = biasVec[ 2 ] = 128.0;

	coefficients[ 0 ] = 0.299;
	coefficients[ 1 ] = 0.587;
	coefficients[ 2 ] = 0.114;

	convertToRGBParm = XieTecYCbCrToRGB (
		levels,
		(double) coefficients[ 0 ],
		(double) coefficients[ 1 ],
		(double) coefficients[ 2 ],
		biasVec,
		xieValGamutNone,
		( XiePointer ) NULL
	);
			
	XieFloConvertToRGB( &flograph[idx],
		idx,
		xieValYCbCrToRGB,
		convertToRGBParm
	);
	idx++;

	coefficients[ 0 ] = 1.0;
	coefficients[ 1 ] = 1.0; 
	coefficients[ 2 ] = 1.0; 

	if ( redFactor )
		for ( i = 0; i < redFactor; i++ ) {
			coefficients[ 0 ] *= redValues[ 0 ];	
			coefficients[ 1 ] *= redValues[ 1 ];	
			coefficients[ 2 ] *= redValues[ 2 ];	
		}

	if ( greenFactor ) 
		for ( i = 0; i < greenFactor; i++ ) {
			coefficients[ 0 ] *= greenValues[ 0 ];	
			coefficients[ 1 ] *= greenValues[ 1 ];	
			coefficients[ 2 ] *= greenValues[ 2 ];	
		}

	if ( blueFactor )
		for ( i = 0; i < blueFactor; i++ ) {
			coefficients[ 0 ] *= blueValues[ 0 ];	
			coefficients[ 1 ] *= blueValues[ 1 ];	
			coefficients[ 2 ] *= blueValues[ 2 ];	
		}

	if ( cyanFactor )
		for ( i = 0; i < cyanFactor; i++ ) {
			coefficients[ 0 ] *= 1.0/redValues[ 0 ];	
			coefficients[ 1 ] *= 1.0/redValues[ 1 ];	
			coefficients[ 2 ] *= 1.0/redValues[ 2 ];	
		}

	if ( magentaFactor )
		for ( i = 0; i < magentaFactor; i++ ) {
			coefficients[ 0 ] *= 1.0/greenValues[ 0 ];	
			coefficients[ 1 ] *= 1.0/greenValues[ 1 ];	
			coefficients[ 2 ] *= 1.0/greenValues[ 2 ];	
		}

	if ( yellowFactor )
		for ( i = 0; i < yellowFactor; i++ ) {
			coefficients[ 0 ] *= 1.0/blueValues[ 0 ];	
			coefficients[ 1 ] *= 1.0/blueValues[ 1 ];	
			coefficients[ 2 ] *= 1.0/blueValues[ 2 ];	
		}

	bandMask = 0x7;	
	domain.phototag = 0;
        XieFloArithmetic(&flograph[idx],
		idx,
		0,
		&domain,
		coefficients,
		xieValMul,
		bandMask
        );
        idx++;
	src = idx;

        if ( !InsertBackend( backend, display, win, 0, 0, gc,
                flograph, idx ) ) {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

	if ( out_file ) {
		levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 256;

		toYCbCrParms = XieTecRGBToYCbCr(
			levels,
			CCIR_RED,
			CCIR_GREEN,
			CCIR_BLUE,
			biasVec
		);

		XieFloConvertFromRGB(&flograph[idx],
			src,
			xieValRGBToYCbCr,
			(XiePointer) toYCbCrParms
		);
		idx++;

		encodeTech = xieValEncodeJPEGBaseline;
              	encodeParms = (char *) XieTecEncodeJPEGBaseline (
                        xieValBandByPixel,
                        xieValLSFirst,
                        samples,
                        samples,
                        (char *) NULL,
                        0,
                        (char *) NULL,
                        0,
                        (char *) NULL,
                        0
                );

                XieFloExportClientPhoto(&flograph[idx],
                        idx,            /* source */
                        xieValNewData,  /* send ExportAvailable events */
                        encodeTech,     /* tell XIE how to encode image */
                        encodeParms     /* and the encode params */
                );
                idx++;
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

	free( bytes );
	bytes = (char *) NULL;

        if ( out_file != ( char * ) NULL ) {

                /* read the JPEG image back from ExportClientPhoto */

                count = ReadNotifyExportData( display, xieInfo, photospace, 
			floId, idx, 1, 100, 0, &bytes );
                if ( count <= 0 )
                        printf( "Unable to read JPEG image from XIE\n" );
                else {
                        fd =  open( out_file, O_CREAT|O_TRUNC|O_RDWR, 0666 );
                        if ( fd > 0 ) {
                                write( fd, bytes, count );
                                close( fd );
                        }
                        else
                                printf( "Unable to write file %s\n", 
                                        out_file );
                }
        }

	/* for fun, wait for the flo to finish */

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;

        WaitForXIEEvent(display, &eventData, 10L, &event );

	/* free up what we allocated */

	CloseBackend( backend, display );
	XFree( convertToRGBParm );
	if ( toYCbCrParms )
		XFree( toYCbCrParms );
	if ( bytes )
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

	XFreeGC(display, gc);
	XCloseDisplay(display);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] [-r red] [-g green] [-b blue] [-c cyan] [-m magenta] [-y yellow] -i image\n", pgm);
	exit(1);
}
