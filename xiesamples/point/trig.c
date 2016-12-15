
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

void   usage();
static XieExtensionInfo *xieInfo;

#define	MAXTRIG 3

#define	SIN 0
#define COS 1
#define TANH 2

typedef struct _trig
{
	char	name[ 64 ];
	int	value;
} Trig;

static Trig trigFuncs[ MAXTRIG ] = {
	{ "sin", SIN },
	{ "cos", COS },
	{ "tanh", TANH },
};

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
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
	XieProcessDomain domain;
	int             beSize, done, idx, src, floSize, floId, bandMask;
	XieLTriplet     width, height, levels, length;
	int		screen, flag;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL, *function = NULL;
	GC 		gc;
	int		period = 360;
	float		amplitude = 1.0;
	int		size, trigFunc;
	char		*bytes, *lut;
	char		*CreateTrigLUT();
        Visual          *visual;
       	Backend         *backend;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:f:p:a:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;
	
		case 'f':	function = optarg;	
				break;

		case 'a':	amplitude = atof( optarg );
				break;

		case 'p':	period = atoi( optarg );
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

	if ( function == ( char * ) NULL ) {
		printf( "Trig function is not defined\n" );
		usage( argv[ 0 ] );
	}

	if ( ( trigFunc = ConvertTrigFunction( function ) ) == -1 ) {
		printf( "Not a valid trig function\n" );
		usage( argv[ 0 ] );
	}

	lut = CreateTrigLUT( trigFunc, 256, period, amplitude );

	if ( ( size = GetJFIFData( img_file, &bytes, &deep, &w, &h, &bands ) ) == 0 ) {
		printf( "Problem getting JPEG data from %s\n", img_file );
		exit( 1 );
	}

	if ( bands != 1 || deep != 8 ) {
		printf( "Image must be 256 levels and SingleBand\n" );
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

	WaitForWindow( display, win );
	XSync( display, 0 );

	gc = XCreateGC(display, win, 0L, (XGCValues *) NULL);

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

        visual = DefaultVisual( display, screen );

        backend = (Backend *) InitBackend( display, screen, visual->class, 
                xieValSingleBand, levels, -1, &beSize );

        if ( backend == (Backend *) NULL ) {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }

	floSize = 3 + beSize;

	flograph = XieAllocatePhotofloGraph(floSize);

	width[0] = w; width[1] = width[2] = 0;
	height[0] = h; height[1] = height[2] = 0;
	levels[0] = 256; levels[1] = levels[2] = 0;

	decodeTech = xieValDecodeJPEGBaseline;
	decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
		xieValBandByPixel,
		xieValLSFirst,
		True
	);

	idx = 0;
	notify = False;

	length[ 0 ] = levels[ 0 ];
	XieFloImportClientLUT(&flograph[idx],
		xieValSingleBand,
		xieValLSFirst,
		length,
		levels
	);
	idx++;
	src = idx;	

	XieFloImportClientPhoto(&flograph[idx], 
		xieValSingleBand,	/* class */
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify events */
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;

        domain.offset_x = 0;
        domain.offset_y = 0;
        domain.phototag = 0;    
	bandMask = 0x1;		

	XieFloPoint(&flograph[idx],
		idx,
		&domain,
		src,
		bandMask
	);
	idx++;

        if (!InsertBackend(backend, display, win, 0, 0, gc, flograph, idx)) {
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

	PumpTheClientData( display, floId, photospace, 2, bytes, size, 
		sizeof( char ), 0, True );

	/* and then send the LUT */
	
	PumpTheClientData( display, floId, photospace, 1, lut, levels[ 0 ], 
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
	free( bytes );
	free( lut );
	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	XFree(decodeParms);

	XFreeGC(display, gc);
	XCloseDisplay(display);
}

#if	!defined( M_PI )
#define M_PI 3.14159265358979323846
#endif

#define	TORAD( x ) ( x * M_PI / 180.0 )

char *
CreateTrigLUT( function, levels, period, amplitude )
int	function;
int	levels;
int	period;
float	amplitude;
{
	char	*lut;
	int	i;
	float	val, lo, hi;

	lut = malloc( sizeof( char ) * levels );

	lo = -1.0;
	hi = 1.0;

	for ( i = 0; i < levels; i++ ) {
		switch( function )
		{
		case SIN:
			val = sin( TORAD( i * (360.0 / period) ) ) * amplitude;
			break;
		case COS:
			val = cos( TORAD( i * (360.0 / period) ) ) * amplitude;
			break;
		case TANH:
			val = tanh( TORAD( i ) ) * amplitude;
			break;
		}
		
		/* scale the value to [0, levels - 1] */

		if ( val == 0.0 )
			val = levels / 2.0;
		else
			val = ((levels - 1) / (fabs(hi - lo))) * 
				val + levels / 2.0;
		*(lut + i) = ( char ) val;
	}	
	return( char * ) lut;
}

int
ConvertTrigFunction( function )
char	*function;
{
	int	i;
	int	val = -1;

	for ( i = 0; i < MAXTRIG; i++ ) {
		if ( !strcmp( function, trigFuncs[ i ].name ) )
		{
			val = trigFuncs[ i ].value;
			break;
		}
	}
	return( val );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i image -f [sin cos tanh] [-p period] [-a amplitude]\n", pgm);
	exit(1);
}
