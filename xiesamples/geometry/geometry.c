
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>
#include "events.h"
#include "backend.h"

#include "transform.h"

void   usage();
static XieExtensionInfo *xieInfo;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	Backend 	*backend;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
	Window          win;
	char		deep, bands;
	short		w, h;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	int             done, idx, floSize, floId, bandMask;
	XieLTriplet     width, height, levels, length;
	int		screen, flag;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL;
	char		*trans_file = NULL;
	GC 		gc;
	int		beSize, size;
	char		*bytes;
	float		coeffs[ 6 ];
	XieConstant	constant;
	XieGeometryTechnique sampleTech = xieValGeomNearestNeighbor;
	XiePointer	sampleParms = (XiePointer) NULL;
	Visual		*visual;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:s:t:"))!=EOF) {
		switch(flag) {

		case 's':	if ( optarg[0] == 'a' )
					sampleTech = xieValGeomAntialias;
				else if ( optarg[ 0 ] == 'b' )
					sampleTech = xieValGeomBilinearInterp;
				break;

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;
	
		case 't':	trans_file = optarg;	
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

	if ( trans_file == ( char * ) NULL ) {
		printf( "Transform file not defined\n" );
		usage( argv[ 0 ] );
	}

	if ( ( size = GetJFIFData( img_file, &bytes, &deep, &w, &h, &bands ) ) == 0 )
	{
		printf( "Problem getting JPEG data from %s\n", img_file );
		exit( 1 );
	}

	if ( bands != 1 ) {
		printf( "Image must be SingleBand\n" );
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

	XSync( display, 0 );
	WaitForWindow( display, win );

	gc = XCreateGC(display, win, 0L, (XGCValues *) NULL);

        XSetForeground( display, gc, XBlackPixel( display, 0 ) );
        XSetBackground( display, gc, XWhitePixel( display, 0 ) );

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

        visual = DefaultVisual( display, screen );

        backend = (Backend *) InitBackend( display, screen, visual->class,
                xieValSingleBand, 1<<DefaultDepth( display, screen ), -1, &beSize );

        if ( backend == (Backend *) NULL ) {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }

	floSize = 2 + beSize;

	flograph = XieAllocatePhotofloGraph(floSize);

	decodeTech = xieValDecodeJPEGBaseline;
	decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
		xieValBandByPixel,
		xieValLSFirst,
		True
	);

	idx = 0;
	notify = False;

	width[0] = width[1] = width[2] = w;
	height[0] = height[1] = height[2] = h;
	levels[0] = levels[1] = levels[2] = 256;

	XieFloImportClientPhoto(&flograph[idx], 
		xieValSingleBand,	/* class */
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify */ 
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;

	bandMask = 0x1;		

	constant[ 0 ] = 128.0;

	GetTransformCoefficients( trans_file, coeffs, w, h );

	XieFloGeometry(&flograph[idx],
		idx,			/* image source */
		w,			/* width of resulting image */
		h,			/* height of resulting image */
		coeffs,			/* a, b, c, d, e, f */
		constant,		/* used if src pixel does not exist */ 
		bandMask,		/* ignored for SingleBand images */
		sampleTech,		/* sample technique... */ 
		sampleParms		/* and parameters, if required */
	);
	idx++;

        if ( !InsertBackend( backend, display, win, 0, 0, gc, 
                flograph, idx ) ) {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

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

	/* free up what we allocated */

	CloseBackend( backend, display );
	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	XFree(decodeParms);
	free( bytes );

	XFreeGC(display, gc);
	XCloseDisplay(display);
}

int
GetTransformCoefficients( trans_file, coeffs, wide, high )
char	*trans_file;
float	coeffs[];
int	wide;
int	high;
{
	transformHandle	handle, *table = (transformHandle *) NULL;
	char	buf[ BUFSIZ ];
	FILE	*fp;
	float	sx, sy;
	int	i, size = 1;
	int	angle, tx, ty;

        InitTransforms();
        handle = CreateIdentity();
	table = (transformHandle *) malloc( sizeof( transformHandle ) );
	table[ 0 ] = handle;
	size = 1;
	fp = fopen( trans_file, "r" );
	if ( fp == ( FILE * ) NULL ) 
	{
		SetCoefficients( handle, coeffs );
		return( 1 );
	}
	while ( !feof( fp ) )
	{
		fscanf( fp, "%s", buf );
		if ( feof( fp ) )
			break;
		if ( buf[ 0 ] == '#' ) {
			if ( strlen( buf ) > 1 )
				fscanf( fp, "%s\n", buf );
			continue;
		}
		if ( !strcmp( buf, "scale" ) ) {
			fscanf( fp, "%f %f", &sx, &sy );
			handle = CreateScale( sx, sy );
		}
		else if ( !strcmp( buf, "shear" ) ) {
			fscanf( fp, "%f %f", &sx, &sy );
			handle = CreateShear( sx, sy );
		}
		else if ( !strcmp( buf, "mirrorX" ) ) {
			handle = CreateMirrorX( (float) wide ); 
		}
		else if ( !strcmp( buf, "mirrorY" ) ) {
			handle = CreateMirrorY( (float) high );  
		}
		else if ( !strcmp( buf, "rotate" ) ) {
			fscanf( fp, "%d", &angle );
			handle = CreateRotate( TORAD((float)angle), 
				(float)wide, (float)high );
		}
		else if ( !strcmp( buf, "translate" ) ) {
			fscanf( fp, "%d %d", &tx, &ty );
			handle = CreateTranslate( tx, ty );
		}
		else {
			printf( "Unknown function '%s'\n", buf );
			continue;
		}

		table = (transformHandle *) realloc( table, (size + 1) 
			* sizeof( transformHandle ) );
		table[ size++ ] = handle;
	}
	if ( size > 1 )
		handle = ConcatenateTransforms( table, size );
	SetCoefficients( handle, coeffs );
	for ( i = 0; i < size; i++ )
		FreeTransformHandle( table[ i ] );
	free( table );
	fclose( fp );
	return( 1 );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] [-s a] [-s b] -i image -t transforms\n", pgm);
	exit(1);
}
