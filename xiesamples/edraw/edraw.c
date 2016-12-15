
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

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	Backend		*backend;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
        char            d, l;
        short           w, h;
	Window          window, subWindow;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotoflo     flo;
	int             beSize, size, done, idx, floSize, bandMask, x, y;
	XieLTriplet     width, height, levels;
	int		screen, flag;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL;
	char		*function = "GXcopy";
	Bool		applyClipMask = False;
	int		subwindowMode = ClipByChildren;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	int		xOffset = 0, yOffset = 0;
	unsigned int	dummyui;
	XGCValues	gcVals;
	unsigned long	planeMask = ~0L;
	unsigned long 	gcValMask = GCFunction | GCPlaneMask | 
			GCSubwindowMode | GCClipXOrigin | GCClipYOrigin;
	GC 		gc;
	int		status;
	Pixmap		pixmap;
	char		*bytes;
	Visual		*visual;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:x:y:csf:p:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case 'x':	xOffset = atoi( optarg );
				break;

		case 'y':	yOffset = atoi( optarg );
				break;

		case 's':	subwindowMode = IncludeInferiors;
				break;

		case 'c':	applyClipMask = True;
				break;

		case 'p':	planeMask = atol( optarg );
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'f':	function = optarg;
				break;

		case 'i':	img_file = optarg;	
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

        if ( ( size = GetJFIFData( img_file, &bytes, &d, &w, &h, &l ) ) == 0 )
        {
                printf( "Problem getting JPEG data from %s\n", img_file );
                exit( 1 );
        }

        if ( d != 8 || l != 1 )
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
	window = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	0, 0, w, h, 1, 0, 255 );
	subWindow = XCreateSimpleWindow(display, window,
	 	0, 0, w >> 2, h >> 2, 1, 0, 255 );
	XSelectInput( display, window, ExposureMask );

        status = XReadBitmapFile(display, DefaultRootWindow(display), 
		"bitmap", &dummyui, &dummyui, &pixmap, 0, 0);

	if ( status != BitmapSuccess )
	{
		fprintf( stderr, "Couldn't get the bitmap\n" );
		applyClipMask = False;
	}

	gcVals.plane_mask = planeMask;
	gcVals.clip_x_origin = xOffset;
	gcVals.clip_y_origin = yOffset;
	gcVals.subwindow_mode = subwindowMode;
	gcVals.function = GetGCFunction( function );
	
	if ( applyClipMask == True )
	{
		gcValMask |= GCClipMask;
		gcVals.clip_mask = pixmap;
	}

	gc = XCreateGC(display, window, gcValMask, (XGCValues *) &gcVals);

	XMapRaised(display, window);
	XMapRaised(display, subWindow);
	XMoveWindow(display, window, 0, 0);
	XMoveWindow(display, subWindow, 0, 0);

    	XSync( display, 0 );
 
    	done = 0;
    	while( done == 0 )
    	{
        	XNextEvent( display, &event );
        	switch( event.type )
        	{
                	case Expose:
                        	done = 1;
                        	break;
        	}
    	}

	/* Create and populate a stored photoflo graph */

        visual = DefaultVisual( display, screen );

        backend = (Backend *) InitBackend( display, screen, visual->class,
                xieValSingleBand, 1<<DefaultDepth( display, screen ), -1, &beSize );

        if ( backend == (Backend *) NULL )
        {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }

	floSize = 1 + beSize;

	flograph = XieAllocatePhotofloGraph(floSize);

	width[0] = w;
	width[1] = width[2] = 0;
	height[0] = h;
	height[1] = height[2] = 0;
	levels[0] = 256;
	levels[1] = levels[2] = 0;

        decodeTech = xieValDecodeJPEGBaseline;
        decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
                xieValBandByPixel,
                xieValLSFirst,
                True
        );

	if ( decodeParms == (char *) NULL) {
		fprintf(stderr, "Couldn't allocate decode parms\n");
		exit(1);
	}

	idx = 0;
	notify = True;
	XieFloImportClientPhoto(&flograph[idx], 
		xieValSingleBand, 	/* class of image data */
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify */ 
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;

        if ( !InsertBackend( backend, display, window, 0, 0, gc, 
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

	/* Send the flograph to the server and get a handle back */

	flo = XieCreatePhotoflo(display, flograph, floSize);

	/* run the flo */

	XieExecutePhotoflo(display, flo, notify);

	/* now that the flo is running, send image data */

       	PumpTheClientData( display, flo, 0, 1, bytes, size,
        	sizeof( char ), 0, True );

	/* for fun, wait for the flo to finish */

        eventData.floId = flo;
        eventData.space = 0;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

      	XSelectInput(display, window, ButtonPressMask | KeyPressMask |
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
	if ( applyClipMask == True )
		XFreePixmap( display, pixmap );
	XFreeGC(display, gc);
	free( bytes );
	XDestroyWindow(display, window);
	XieFreePhotofloGraph(flograph, floSize);
	XFree(decodeParms);
	XieDestroyPhotoflo(display, flo);
	XCloseDisplay(display);
}

typedef struct _funcs {
	char	*name;
	int	func;
} Funcs;

Funcs funcs[] = {
	{ "GXcopy", GXcopy },
	{ "GXclear", GXclear },
	{ "GXand", GXand },
	{ "GXandReverse", GXandReverse },
	{ "GXandInverted", GXandInverted },
	{ "GXnoop", GXnoop },
	{ "GXxor", GXxor },
	{ "GXor", GXor },
	{ "GXnor", GXnor },
	{ "GXequiv", GXequiv },
	{ "GXinvert", GXinvert	 },
	{ "GXorReverse", GXorReverse },
	{ "GXcopyInverted", GXcopyInverted },
	{ "GXorInverted", GXorInverted },
	{ "GXnand", GXnand },
	{ "GXset", GXset }
};

#define	NUMFUNCS

int
GetGCFunction( function )
char	*function;
{
	int	i;

	for ( i = 0; i < sizeof( funcs ) / sizeof( Funcs ); i++ )
	{
		if ( !strcmp( funcs[ i ].name, function ) )
			return( funcs[ i ].func );
	}
	return( funcs[ 0 ].func );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] [-c] [-s] [-p planemask] [-x xOffset] [-y yOffset] [-f function] -i image\n", pgm);
	exit(1);
}

