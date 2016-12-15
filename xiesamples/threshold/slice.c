
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

#define MONWIDTH      350
#define MONHEIGHT     200

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>
#include "events.h"
#include "backend.h"

void   usage();
static XieExtensionInfo *xieInfo;
static GC tgc;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	Backend		*backend;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
	char		deep, bands;
	short		w, h;
	Window          window, histoWindow, root;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
	XieProcessDomain domain;
        XSetWindowAttributes attribs;
       	XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	int             done, idx, histSrc, src, floSize, floId, bandMask;
	XieLTriplet     width, height, levels, length;
	int		lowerThreshold = 0, upperThreshold = 255;
	int		numHistos, size, screen, flag, beSize;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL;
	char		*bytes, *lut;
	XieHistogramData *histos;
	GC 		gc;
    	XGCValues   	tgcv;
	Visual		*visual;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:l:u:"))!=EOF) {
		switch(flag) {

		case 'l':	lowerThreshold = atoi(optarg); 
				break;

		case 'u':	upperThreshold = atoi(optarg); 
				break;

		case 'd':	display_name = optarg; 
				break;

		case 'i':	img_file = optarg;	
				break;

		case '?':	usage( argv[ 0 ] );
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
	window = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	0, 0, w, h, 1, 0, 255 );

	histoWindow = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	0, 0, MONWIDTH, MONHEIGHT, 1, 0, 255 );

	XSelectInput( display, histoWindow, ExposureMask );
	gc = XCreateGC(display, window, 0L, (XGCValues *) NULL);
    	tgc = XCreateGC(display, histoWindow, GCForeground | GCBackground, 
		&tgcv);

        XSetForeground( display, tgc, XBlackPixel( display, 0 ) );
        XSetBackground( display, tgc, XWhitePixel( display, 0 ) );

        attribs.background_pixel = XWhitePixel( display, 0 );
        XChangeWindowAttributes( display, histoWindow, CWBackPixel, 
                &attribs );

	XMapRaised(display, window);
	XMapRaised(display, histoWindow);

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

	/* build the LUT */

	lut = (char *) malloc( 256 * sizeof( char ) );

	for ( idx = 0; idx < 256; idx++ )
		lut[ idx ] = ( idx < lowerThreshold ? 0 : 
			( idx > upperThreshold ? 255 : idx ) );

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

        visual = DefaultVisual( display, screen );

        backend = (Backend *) InitBackend( display, screen, visual->class,
                xieValSingleBand, 1 << DefaultDepth( display, screen ), 
		-1, &beSize );

        if ( backend == (Backend *) NULL )
        {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }

	floSize = 4 + beSize;

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
	notify = False;
	XieFloImportClientPhoto(&flograph[idx], 
		xieValSingleBand, 	/* class of image data */
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify event */ 
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;
	src = idx;

	length[ 0 ] = 256; length[ 1 ] = length[ 2 ] = 0;
        XieFloImportClientLUT(&flograph[idx],
		xieValSingleBand,	/* class of LUT */
		xieValLSFirst, 		/* band order, ignored */
		length,			/* how many entries */
                levels			/* how wide each entry */
        );
	idx++;

	domain.phototag = 0;
	domain.offset_x = 0;
	domain.offset_y = 0;

	XieFloPoint(&flograph[idx],
		src,			/* the image data */
		&domain,
    		idx,              	/* the lut sent by the client */
		0x01
	);
	idx++;
	src = idx;

        if ( !InsertBackend( backend, display, window, 0, 0, gc,
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

        XieFloExportClientHistogram(&flograph[idx],
		src,            /* source phototag number */
		&domain,	/* get entire image - no ROI */
		xieValNewData	/* send event when new data is ready */	
	);
	idx++;
	histSrc = idx;

	floId = 1;
	notify = True;
	photospace = XieCreatePhotospace(display);

	/* run the flo */

	XieExecuteImmediate(display, photospace, floId, notify, flograph,
	    floSize);

	/* now that the flo is running, send image data */

       	PumpTheClientData( display, floId, photospace, 1, bytes, size, 
        	sizeof( char ), 0, True );

	/* and send the LUT */

       	PumpTheClientData( display, floId, photospace, 2, lut, 256, 
        	sizeof( char ), 0, True );

	/* now, read back the image histogram from XIE and display it */

	histos = ( XieHistogramData * ) NULL;
        numHistos = ReadNotifyExportData( display, xieInfo, photospace, 
		floId, histSrc, sizeof( XieHistogramData ), 0, 0, (char **) 
		&histos ) / sizeof( XieHistogramData );

        DrawHistogram( display, histoWindow, tgc, ( XieHistogramData * ) histos,
                numHistos, 256, MONWIDTH, MONHEIGHT );

	free( histos );

	/* for fun, wait for the flo to finish */

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

	/* wait for a button or key press before exiting */

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

	/* free up what we allocated */

	CloseBackend( backend, display );
	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	XFree(decodeParms);
	free( bytes ); free( lut );
	XFreeGC(display, gc);
	XFreeGC(display, tgc);
	XDestroyWindow(display, window);
	XDestroyWindow(display, histoWindow);
	XCloseDisplay(display);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] [-l lower_threshold] [-u upper_threshold] -i image\n", pgm);
	exit(1);
}
