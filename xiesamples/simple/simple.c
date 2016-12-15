
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

void   usage();
static XieExtensionInfo *xieInfo;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
	char		d, l;
	short		w, h;
	Window          win;
	GC 		gc;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XieProcessDomain domain;
	XSetWindowAttributes xswa;
	int             n, flag, size, done, idx, floSize, floId, bandMask;
	XieLTriplet     width, height, levels;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*bytes, *img_file = NULL;
	Bool		useStaticColor = False;
	unsigned long 	valueMask;
	Colormap	cmap;
	XVisualInfo 	vinfo;


	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:s"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;

		case 's':	useStaticColor = True;
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

	if ( l == 3 )
	{
		printf( "Image must by Grayscale or bitonal\n" );
		exit( 1 );
	}

	if ( d != 8 )
	{
		printf( "Image must be 256 levels\n" );
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

	if ( useStaticColor == True )
	{
		if ( XMatchVisualInfo( display, 
			DefaultScreen( display ), 8, StaticColor, &vinfo ) )
		{
			cmap = XCreateColormap(display, 
				DefaultRootWindow(display), 
				vinfo.visual, AllocNone );
			XInstallColormap( display, cmap );
		}
	}

	if ( useStaticColor == False ) {
		valueMask = 0L;
		win = XCreateWindow( display, DefaultRootWindow(display), 0, 0, 
			w, h, 0, 0, InputOutput, DefaultVisual( display,
			DefaultScreen( display ) ), valueMask, &xswa );
	}
	else {
		valueMask = CWColormap;
		xswa.colormap = cmap;
		win = XCreateWindow( display, DefaultRootWindow(display), 0, 0, 
			w, h, 0, 0, InputOutput, vinfo.visual, valueMask, &xswa );
	}

	XSelectInput( display, win, ExposureMask );

	XMapRaised(display, win);
	WaitForWindow( display, win );
	XSync( display, 0 );

	if ( useStaticColor )
	{
		XSetWindowColormap( display, win, cmap );
	}

	gc = XCreateGC(display, win, 0L, (XGCValues *) NULL);

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

	floSize = 2;

	flograph = XieAllocatePhotofloGraph(floSize);

	width[0] = width[1] = width[2] = w;
	height[0] = height[1] = height[2] = h;
	levels[0] = levels[1] = levels[2] = 256;

	decodeTech = xieValDecodeJPEGBaseline;
	decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
		xieValBandByPixel,
		xieValLSFirst,
		True
	);

	idx = 0;
	notify = False;
	XieFloImportClientPhoto(&flograph[idx], 
		xieValSingleBand,
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify events */ 
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;

	XieFloExportDrawable(&flograph[idx], 
		idx, 		/* source */
		win, 		/* drawable to send data to */
		gc,		/* GC */	
		0, 		/* x offset in window to place data */
		0		/* y offset in window to place data */
	);
	idx++;

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
	printf("usage: %s [-d display] -i image [-s]\n", pgm);
	exit(1);
}
