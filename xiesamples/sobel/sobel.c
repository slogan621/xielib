
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
	char		d, l;
	short		w, h;
	Window          win;
	Bool            notify;
	XieConstant     bias, constant;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
	float		*kernel;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XieYCbCrToRGBParam *rgbParm;
	XieProcessDomain domain;
	XSetWindowAttributes attribs;
	int             beSize, done, src, idx, src1, src2, size,
			floSize, floId, bandMask, kernelSize, flag,
			screen;
	XieLTriplet     width, height, levels;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL, *v_kernel_file = NULL,
			*h_kernel_file = NULL;
	GC 		gc;
	char		*bytes;
	Visual		*visual;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:v:h:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;
	
		case 'v':	v_kernel_file = optarg;	
				break;

		case 'h':	h_kernel_file = optarg;	
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

	if ( h_kernel_file == ( char * ) NULL ||
		v_kernel_file == (char *) NULL )
	{
		printf( "Kernel file not defined\n" );
		usage( argv[ 0 ] );
	}

	if ( ( kernelSize = GetKernel( v_kernel_file, &kernel ) ) == 0 )
	{
		printf( "Problem getting kernel data from %s\n", v_kernel_file );
		exit( 1 );
	}

	if ( ( size = GetJFIFData( img_file, &bytes, &d, &w, &h, &l ) ) == 0 )
	{
		printf( "Problem getting JPEG data from %s\n", img_file );
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

	win = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	0, 0, w, h, 1, 0, 255 );
	XSelectInput( display, win, ExposureMask );

	XMapRaised(display, win);

	WaitForWindow( display, win );
	XSync( display, 0 );

	gc = XCreateGC(display, win, 0L, (XGCValues *) NULL);

        XSetForeground( display, gc, XBlackPixel( display, 0 ) );
        XSetBackground( display, gc, XWhitePixel( display, 0 ) );

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

	screen = DefaultScreen( display );
        visual = DefaultVisual( display, screen );

        if ( l == 1 )
                backend = (Backend *) InitBackend( display, screen, 
                        visual->class, xieValSingleBand, 
                        1 << DefaultDepth( display, screen ), -1, &beSize );
        else
                backend = (Backend *) InitBackend( display, screen, 
                        visual->class, xieValTripleBand, 0, -1, &beSize );

        if ( backend == (Backend *) NULL )
        {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }


	floSize = 4 + beSize;
	if ( l == 3 )
		floSize++;

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
	src = idx;

        domain.offset_x = 0;
        domain.offset_y = 0;
        domain.phototag = 0;    

	bandMask = 0x7;			/* ignored if SingleBand */
	XieFloConvolve(&flograph[idx],
		src,
		&domain,
		kernel,
		kernelSize,
		bandMask,
		xieValConvolveDefault,
		( char * ) NULL
	);
	idx++;
	src1 = idx;
	free( kernel );

	if ( ( kernelSize = GetKernel( h_kernel_file, &kernel ) ) == 0 )
	{
		printf( "Problem getting kernel data from %s\n", h_kernel_file );
		exit( 1 );
	}

	XieFloConvolve(&flograph[idx],
		src,
		&domain,
		kernel,
		kernelSize,
		bandMask,
		xieValConvolveDefault,
		( char * ) NULL
	);
	idx++;
	src2 = idx;

	XieFloArithmetic(&flograph[idx],
		src1,
		src2,
		&domain,
		constant,
		xieValAdd,
		bandMask
	);
	idx++;

        if ( !InsertBackend( backend, display, win, 0, 0, gc,
                flograph, idx ) )
        {
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
			XRefreshKeyboardMapping( (XMappingEvent*) &event);
			break;
		}
	}

	/* free up what we allocated */

	CloseBackend( backend, display );
	if ( rgbParm )
		XFree( rgbParm );
	free( bytes );
	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	XFree(decodeParms);
	XFreeGC(display, gc);
	XCloseDisplay(display);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i image -v vertical_kernel_file -h horizontal_kernel_file\n", pgm);
	exit(1);
}
