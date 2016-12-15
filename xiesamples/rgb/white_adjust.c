
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

typedef struct _whitePointEnt
{
        XieFloat   	X;
        XieFloat   	Y;
        XieFloat   	Z;
        char    	*name;
        Window  	window;
        XieWhiteAdjustCIELabShiftParam *whiteAdjustParms;
        XieRGBToCIEXYZParam *RGBToXYZParms;
} WhitePointEnt;

static WhitePointEnt whitePointTab[] = {
        { 1.000, 1.000, 1.000, "Full Intensity White" },
        { 1.099, 1.000, 0.356, "Illuminant A" },
        { 0.991, 1.000, 0.853, "Illuminant B" },
        { 0.981, 1.000, 1.182, "Illuminant C" },
        { 0.957, 1.000, 0.921, "Illuminant D55" },
        { 0.950, 1.000, 1.089, "Illuminant D65" },
        { 0.949, 1.000, 1.225, "Illuminant D75" }
};

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
	XieConstant     bias;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XieYCbCrToRGBParam *rgbParm;
	XieProcessDomain domain;
	XSetWindowAttributes attribs;
	int             i, done, idx, src, floSize, floId, bandMask;
	XieLTriplet     width, height, levels;
	int		size, beSize, screen, flag;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL, *kernel_file = NULL;
	GC 		gc;
	char		*bytes;
	XieMatrix	fromXYZMat, toXYZMat;
	XieCIEXYZToRGBParam *XYZToRGBParms;
	XieConstant	whitePoint;
        XieConstant 	in_low,in_high;
        XieLTriplet 	out_low,out_high;
	XieClipScaleParam *constrainParms;
	int		numWhitePoints;
	Visual		*visual;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
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

	if ( l != 3 )
	{
		printf( "Image must be TripleBand\n" ); 
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

	numWhitePoints = sizeof(whitePointTab)/sizeof(WhitePointEnt);

	/* Do some X stuff - create colormaps, windows, etc. */

	screen = DefaultScreen(display);

	visual = DefaultVisual( display, screen );

        backend = (Backend *) InitBackend( display, screen, visual->class,
                xieValTripleBand, 0, -1, &beSize );

        if ( backend == (Backend *) NULL )
        {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }

	floSize = 2 + (numWhitePoints * (3 + beSize));

	for ( i = 0; i < numWhitePoints; i++ )
	{
		win = XCreateSimpleWindow(display, DefaultRootWindow(display),
			0, 0, w, h, 1, 0, 255 );
		XSelectInput( display, win, ExposureMask );

		XMapRaised(display, win);
		WaitForWindow( display, win );
		whitePointTab[ i ].window = win;
	}

	gc = XCreateGC(display, win, 0L, (XGCValues *) NULL);

        XSetForeground( display, gc, XBlackPixel( display, 0 ) );
        XSetBackground( display, gc, XWhitePixel( display, 0 ) );

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

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
		xieValTripleBand, 
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify */ 
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;

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
	src = idx;

	/* allocate technique parameters common to each of the subflos
           which are created in the loop below */

	GetXcmsRGBToXYZ( display, screen, visual, toXYZMat ); 
	GetXcmsXYZToRGB( display, screen, visual, fromXYZMat ); 

	XYZToRGBParms = XieTecCIEXYZToRGB( 
		fromXYZMat, 
		xieValWhiteAdjustNone,
		(XiePointer) NULL,
		xieValGamutNone,
		(XiePointer) NULL
	);

	/* for Constrain ClipScale */

       	levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 256;

        in_low[ 0 ] = in_low[ 1 ] = in_low[ 2 ] = 0.0;
        in_high[ 0 ] = in_high[ 1 ] = in_high[ 2 ] = 1.0;

        out_low[ 0 ] = out_low[ 1 ] = out_low[ 2 ] = 0;
        out_high[ 0 ] = out_high[ 1 ] = out_high[ 2 ] = 255;

        constrainParms = XieTecClipScale(in_low, in_high, out_low, out_high);

	/* build the subflos */

	for ( i = 0; i < numWhitePoints; i++ )
	{
		whitePoint[ 0 ] = whitePointTab[ i ].X;
		whitePoint[ 1 ] = whitePointTab[ i ].Y;
		whitePoint[ 2 ] = whitePointTab[ i ].Z;

		whitePointTab[ i ].whiteAdjustParms = 
			XieTecWhiteAdjustCIELabShift( whitePoint );

		whitePointTab[ i ].RGBToXYZParms = XieTecRGBToCIEXYZ( 
			toXYZMat, 
			xieValWhiteAdjustCIELabShift,
			whitePointTab[ i ].whiteAdjustParms 
		);

		XieFloConvertFromRGB( &flograph[idx],
			src,
			xieValRGBToCIEXYZ,
			whitePointTab[ i ].RGBToXYZParms
		);
		idx++;

		XieFloConvertToRGB( &flograph[idx],
			idx,
			xieValCIEXYZToRGB,
			( char * ) XYZToRGBParms
		);
		idx++;

       		levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 256;
		XieFloConstrain( &flograph[idx],
			idx,
			levels,
			xieValConstrainClipScale,
			(XPointer) constrainParms
		);
		idx++;

		if ( !InsertBackend( backend, display, 
			whitePointTab[ i ].window, 0, 0, gc, flograph, idx ) )
		{
			fprintf( stderr, "Backend failed\n" );
			exit( 1 );
		}

		idx += beSize;
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

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

	/* wait for a button or key press before exiting */

	XSelectInput(display, whitePointTab[ 0 ].window, 
		ButtonPressMask | KeyPressMask | ExposureMask);
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
	XFreeGC(display, gc);
	XFree( decodeParms );
	XFree( rgbParm );
	XFree( constrainParms );
	free( bytes );

        for ( i = 0; i < numWhitePoints; i++ )
	{
		XFree( whitePointTab[ i ].whiteAdjustParms );
		XFree( whitePointTab[ i ].RGBToXYZParms );
	}

	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	XCloseDisplay(display);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i image\n", pgm);
	exit(1);
}
