
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

#define MONWIDTH 350
#define MONHEIGHT 200

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	Backend 	*backend;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
	char		d, l;
	short		w, h;
	Window          sourceWin, constWin, histoWin1, histoWin2;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
	XieConstrainTechnique constrainTech = xieValConstrainClipScale;
	XiePointer	constrainParms;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XieConstant 	inLow, inHigh;	
	XieProcessDomain domain;
	XSetWindowAttributes attribs;
	void		DoHistos();
	char		*shapeParms = ( char * ) NULL;
	int             beSize, done, idx, src, floSize, floId, bandMask;
	XieLTriplet     width, height, levels, outLow, outHigh;
	int		screen, flag, histoSrc1, histoSrc2;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL;
	GC 		gc;
	int		size;
	char		*bytes;
	Visual		*visual;

	/* handle command line arguments */

	inLow[ 0 ] = 0.0;
	inHigh[ 0 ] = 255.0;
	outLow[ 0 ] = 0;
	outHigh[ 0 ] = 255;

	while ((flag=getopt(argc,argv,"?d:i:hj:k:l:m:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;
	
		case 'h':	constrainTech = xieValConstrainHardClip;
				break;

		case 'j':	inLow[ 0 ] = atof( optarg );
				break;

		case 'k':	inHigh[ 0 ] = atof( optarg );
				break;

		case 'l':	outLow[ 0 ] = atol( optarg );
				break;
		
		case 'm': 	outHigh[ 0 ] = atol( optarg );
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
	sourceWin = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	0, 0, w, h, 1, 0, 255 );
	XSelectInput( display, sourceWin, ExposureMask );
	XMapRaised(display, sourceWin);
	constWin = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	0, h + 10, w, h, 1, 0, 255 );
	XSelectInput( display, constWin, ExposureMask );
	XMapRaised(display, constWin);
	histoWin1 = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	w + 10, 0, MONWIDTH, MONHEIGHT, 1, 0, 255 );
	XSelectInput( display, histoWin1, ExposureMask );
	XMapRaised(display, histoWin1);
	histoWin2 = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	w + 10, h + 10, MONWIDTH, MONHEIGHT, 1, 0, 255 );
	XSelectInput( display, histoWin2, ExposureMask );
	XMapRaised(display, histoWin2);

	WaitForWindow( display, sourceWin );
	WaitForWindow( display, constWin );
	WaitForWindow( display, histoWin1 );
	WaitForWindow( display, histoWin2 );
	XSync( display, 0 );

	gc = XCreateGC(display, sourceWin, 0L, (XGCValues *) NULL);

        XSetForeground( display, gc, XBlackPixel( display, 0 ) );
        XSetBackground( display, gc, XWhitePixel( display, 0 ) );

	attribs.background_pixel = XWhitePixel( display, 0 );
	XChangeWindowAttributes( display, histoWin1, CWBackPixel, 
		&attribs );
	XChangeWindowAttributes( display, histoWin2, CWBackPixel, 
		&attribs );

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

        visual = DefaultVisual( display, screen );

        backend = (Backend *) InitBackend( display, screen, visual->class,
                xieValSingleBand, 1<<DefaultDepth( display, screen ), -1, &beSize );

        if ( backend == (Backend *) NULL )
        {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }

	floSize = 4 + (beSize << 1);
	if ( constrainTech == xieValConstrainHardClip )
		floSize++;

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

	idx = 0;
	notify = False;
	XieFloImportClientPhoto(&flograph[idx], 
		xieValSingleBand, 	/* class of image data */
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify events */
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;
	src = idx;

        if ( !InsertBackend( backend, display, sourceWin, 0, 0, gc, 
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

        domain.offset_x = 0;
        domain.offset_y = 0;
        domain.phototag = 0;    

        XieFloExportClientHistogram( &flograph[ idx ],
                src,
                &domain,        /* get entire image - no ROI */
                xieValNewData   /* send event when new data is ready */ 
        );
        idx++;
	histoSrc1 = idx;

	if ( constrainTech == xieValConstrainHardClip )
	{
		constrainParms = ( XiePointer ) NULL;
		levels[ 0 ] = outHigh[ 0 ];
	}
	else
	{
		constrainParms = ( XiePointer ) XieTecClipScale(
			inLow,
			inHigh,
			outLow,
			outHigh
		);
	}

	XieFloConstrain( &flograph[idx],
		src,
		levels,
		constrainTech,
		constrainParms
	);
	idx++;
	src = idx;

	if ( constrainTech == xieValConstrainHardClip )
	{
		levels[ 0 ] = 256;
		XieFloConstrain( &flograph[idx],
			src,
			levels,
			constrainTech,
			constrainParms
		);
		idx++;
		src = idx;
	}

        if ( !InsertBackend( backend, display, constWin, 0, 0, gc, 
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

        XieFloExportClientHistogram( &flograph[ idx ],
                src,
                &domain,        /* get entire image - no ROI */
                xieValNewData   /* send event when new data is ready */
        );
        idx++;
        histoSrc2 = idx;

	floId = 1;
	notify = True;
	photospace = XieCreatePhotospace(display);

	/* run the flo */

	XieExecuteImmediate(display, photospace, floId, notify, flograph,
	    floSize);

	/* now that the flo is running, send image data */

	PumpTheClientData( display, floId, photospace, 1, bytes, size, 
		sizeof( char ), 0, True );

        DoHistos( display, histoWin1, gc, floId, photospace, histoSrc1 );
        DoHistos( display, histoWin2, gc, floId, photospace, histoSrc2 );

	/* for fun, wait for the flo to finish */

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

	/* wait for a button or key press before exiting */

	XSelectInput(display, sourceWin, ButtonPressMask | KeyPressMask |
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
	if ( constrainParms )
		XFree( constrainParms );
	free( bytes );
	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	XFree(decodeParms);

	XFreeGC(display, gc);
	XCloseDisplay(display);
}

void
DoHistos( display, histoWindow, gc, floId, photospace, histSrc )
Display *display;
Window histoWindow;
GC gc;
int floId;
XiePhotospace photospace;
int histSrc;
{
        XieHistogramData *histos;
        int     numHistos;

        histos = ( XieHistogramData * ) NULL;
        numHistos = ReadNotifyExportData( display, xieInfo, photospace, 
		floId, histSrc, sizeof( XieHistogramData ), 0, 0, 
		(char **) &histos ) / sizeof( XieHistogramData );

        DrawHistogram( display, histoWindow, gc, ( XieHistogramData * ) histos,
                numHistos, 256, MONWIDTH, MONHEIGHT );
        free( histos );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i image [-h] [-j inLow ] [-k inHigh ] [-l outLow] [-m outHigh]\n", pgm);
	exit(1);
}
