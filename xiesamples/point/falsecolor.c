
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

static int contIncr = 16;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
        Backend         *backend;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
	Window          win;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XieProcessDomain domain;
	char		deep, bands;
	short		w, h;
	int             beSize, done, idx, src, floSize, floId, bandMask;
	XieLTriplet     width, height, levels, length;
	int		screen, flag;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL, *lutStyle = NULL;
	GC 		gc;
	int		size;
	char		*bytes, *lut;
	char		*CreateFalseLUT();
        Visual          *visual;


	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:s:c:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;
	
		case 'c':	contIncr = atoi(optarg);	
				if ( contIncr == 0 )
				{
					contIncr = 1;
				}
				break;

		case 's':	lutStyle = optarg;	
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

	if ( lutStyle == ( char * ) NULL )
	{
		printf( "LUT style is not defined\n" );
		usage( argv[ 0 ] );
	}

	lut = CreateFalseLUT( lutStyle, 256 );

	if ( ( size = GetJFIFData( img_file, &bytes, &deep, &w, &h, &bands ) ) == 0 )
	{
		printf( "Problem getting JPEG data from %s\n", img_file );
		exit( 1 );
	}

	if ( deep != 8 || bands != 1 )
	{
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

        XSetForeground( display, gc, XBlackPixel( display, 0 ) );
        XSetBackground( display, gc, XWhitePixel( display, 0 ) );

        visual = DefaultVisual( display, screen );

        backend = (Backend *) InitBackend( display, screen, visual->class, 
		xieValTripleBand, 0, -1, &beSize );

        if ( backend == (Backend *) NULL )
        {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }


	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

	floSize = 3 + beSize;

	flograph = XieAllocatePhotofloGraph(floSize);

	decodeTech = xieValDecodeJPEGBaseline;
	decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
		xieValBandByPixel,
		xieValLSFirst,
		True
	);

	idx = 0;
	notify = False;

	length[ 0 ] = length[ 1 ] = length[ 2 ] = 256;
	levels[0] = levels[1] = levels[2] = 256;
	XieFloImportClientLUT(&flograph[idx],
		xieValTripleBand,
		xieValLSFirst,
		length,
		levels
	);
	idx++;
	src = idx;	

	width[0] = width[1] = width[2] = w;
	height[0] = height[1] = height[2] = h;

	XieFloImportClientPhoto(&flograph[idx], 
		xieValSingleBand,	/* class */
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* don't send decode notify */ 
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;

        domain.offset_x = 0;
        domain.offset_y = 0;
        domain.phototag = 0;    
	bandMask = 0x7;		

	XieFloPoint(&flograph[idx],
		idx,
		&domain,
		src,
		bandMask
	);
	idx++;

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

	/* send the LUT */

	PumpTheClientData( display, floId, photospace, 1, lut, 
		levels[ 0 ], sizeof( char ), 0, True );

	PumpTheClientData( display, floId, photospace, 1, lut + levels[0], 
		levels[ 0 ], sizeof( char ), 1, True );

	PumpTheClientData( display, floId, photospace, 1, lut + 2 * levels[0], 
		levels[ 0 ], sizeof( char ), 2, True );

	/* send the image */

	PumpTheClientData( display, floId, photospace, 2, bytes, size, 
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

int
HSVToRGB( h, s, v, r, g, b )
float	h;
float	s;
float	v;
int	*r;
int	*g;
int	*b;
{
	float	f, p1, p2, p3;
	int	i;

	if ( h == 360.0 )
		h = 0.0;
	h = h / 60.0;
	i = ( int ) floor( h );
	f = h - (float) i;
	p1 = v * ( 1.0 - s );
	p2 = v * ( 1.0 - ( s * f ) );
	p3 = v * ( 1.0 - ( s * ( 1.0 - f ) ) );
	switch ( i )
	{
	case 0:
		*r = (int)( v * 255.0 ); *g = (int)( p3 * 255.0 ); *b = (int) (p1* 255.0);
		break;
	case 1:
		*r = (int)(p2*255.0); *g = (int)(v*255.0); *b = (int)(p1*255.0);
		break;
	case 2:
		*r = (int)(p1*255.0); *g = (int)(v*255.0); *b = (int)(p3*255.0);
		break;
	case 3:
		*r = (int)(p1*255.0); *g = (int)(p2*255.0); *b = (int)(v*255.0);
		break;
	case 4:
		*r = (int)(p3*255.0); *g = (int)(p1*255.0); *b = (int)(v*255.0);
		break;
	case 5:
		*r = (int)(v*255.0); *g = (int)(p1*255.0); *b = (int)(p2*255.0);
		break;
	default:
		printf( "whoops!!!!\n" );
		break;
	}
}

#define DOCONTOUR 	0
#define DOSIN		1
#define DORAINBOW	2

#if	!defined( M_PI )
#define M_PI 3.14159265354
#endif

#define TORAD( x ) ( M_PI * (x) / 180.0 )

char *
CreateFalseLUT( style, levels ) 
char 	*style;
int	levels;
{
	char	*lut;
	int	i, r, g, b, idx;
	float	h, s, v;
	float 	ratio;
	int	type = DORAINBOW;

	lut = malloc( sizeof( char ) * levels * 3 );

	if ( !strcmp( style, "contour" ) )
		type = DOCONTOUR;
	else if ( !strcmp( style, "sin" ) )
		type = DOSIN;	

	ratio = 360.0 / levels;
	s = 1.0; v = 1.0;
	for ( i = 0; i < levels; i++ )
	{
		h = i * ratio;
		if ( ( type == DOCONTOUR && !( i % contIncr ) ) || type == DORAINBOW )
			HSVToRGB( h, s, v, &r, &g, &b );
		else 
			/* sin */

			HSVToRGB( fabs(sin( TORAD( h ) )) * 360.0, s, v, &r, &g, &b );
		*(lut + i) = ( char ) r;
		*(lut + i + levels) = ( char ) g;
		*(lut + i + 2 * levels) = ( char ) b;
	}	
	return( char * ) lut;
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i image -s [contour rainbow sin] [-c incr]\n", pgm);
	exit(1);
}
