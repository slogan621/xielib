
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

#define kSize 3

#ifndef MAX
#define MAX( a, b ) ( ( a > b ? a : b ) )
#endif

#define	SIZE	10	

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	char		*decodeParms = NULL;
	int		decodeTech;
	XEvent          event;
	char		d, l;
	short		w, h;
	Window		wins[ SIZE + 1 ];
	GC 		gc;
	Bool            notify;
	XieConstant     bias, constant;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
	XieColorList	clists[ SIZE + 1 ];
        XieColorAllocAllParam *colorParm;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XieYCbCrToRGBParam *rgbParm = NULL;
	XieProcessDomain domain;
	XSetWindowAttributes attribs;
	int             i, done, idx, src, reps = SIZE,
			floSize, floId, bandMask, flag, size;
	XieLTriplet     width, height, levels;
	XiePhotomap	photomap1, photomap2;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*bytes = NULL;
	char		*img_file = NULL;

	static float kernel[ kSize ][ kSize ] = {
		{ 3.0, 1.0, -1.0 },
		{ 2.0, 1.0, -2.0 },
		{ 1.0, -1.0, -3.0 }
	};

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:r:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;
	
		case 'r':	reps = atoi(optarg);	
				if ( reps < 1 || reps > SIZE )
					reps = SIZE;
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

	if ( ( size = GetJFIFData( img_file, &bytes, &d, &w, &h, &l ) ) == 0 )
	{
		printf( "Problem getting JPEG data from %s\n", img_file );
		exit( 1 );
	}

	if ( d != 8 ) {
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

	for ( i = 0; i < reps + 1; i++ ) {
		wins[ i ] = XCreateSimpleWindow(display, 
			DefaultRootWindow(display),
	 		0, 0, w, h, 1, 0, 255 );
		XSelectInput( display, wins[ i ], ExposureMask );
		XMapRaised(display, wins[ i ]);
		WaitForWindow( display, wins[ i ] );
	}

	gc = XCreateGC(display, wins[ 0 ], 0L, (XGCValues *) NULL);

        XSetForeground( display, gc, XBlackPixel( display, 0 ) );
        XSetBackground( display, gc, XWhitePixel( display, 0 ) );

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

	floSize = 6;
	if ( l == 3 )
		floSize+=2;

	flograph = XieAllocatePhotofloGraph(floSize);

	width[0] = width[1] = width[2] = w;
	height[0] = height[1] = height[2] = h;
	levels[0] = levels[1] = levels[2] = 256;

	/* allocate this once and share with all photoflos */

	colorParm = XieTecColorAllocAll( 128 );	/* mid range fill */

	decodeTech = xieValDecodeJPEGBaseline;
	decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
		xieValBandByPixel,
		xieValLSFirst,
		True
	);

	photomap1 = XieCreatePhotomap( display );
	photomap2 = XieCreatePhotomap( display );

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

	if ( l == 3 ) {
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

	XieFloExportPhotomap( &flograph[idx],
		idx, 
		photomap1,
		xieValEncodeServerChoice,
		(XiePointer) NULL
	);
	idx++;

	constant[ 0 ] = constant[ 1 ] = constant[ 2 ] = 0;

        domain.offset_x = 0;
        domain.offset_y = 0;
        domain.phototag = 0;    

	bandMask = 0x7;

	XieFloConvolve( &flograph[idx],
		src,
		&domain,
		(float *) kernel,
		kSize,
		bandMask,
		xieValConvolveDefault,
		(XiePointer) NULL
	);
	idx++;
	src = idx;
		
	XieFloExportPhotomap( &flograph[idx],
		idx, 
		photomap2,
		xieValEncodeServerChoice,
		(XiePointer) NULL
	);
	idx++;

        if ( l == 3 ) {
          	levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 6;
           	XieFloDither( &flograph[idx],
            		src,
             		0x7,
            		levels,
            		xieValDitherErrorDiffusion,
             		( char * ) NULL
        	);
           	idx++;
		src = idx;
        }

        clists[ 0 ] = XieCreateColorList( display );
        XieFloConvertToIndex( &flograph[idx],
           	src,                            /* source element */
         	DefaultColormap(display, 
            		DefaultScreen(display)),/* colormap to alloc */
         	clists[ 0 ],                    /* colorlist */
           	True,                           /* notify if problems */
         	xieValColorAllocAll,            /* color alloc tech */
          	( char * ) colorParm            /* technique parms */
      	);
       	idx++;

        XieFloExportDrawable(&flograph[idx], 
            	idx,            /* source */
             	wins[ 0 ],      /* drawable to send data to */
             	gc,             /* GC */        
             	0,              /* x offset in window to place data */
             	0               /* y offset in window to place data */
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

	XieFreePhotofloGraph(flograph, floSize);

	floSize = 10;
	if ( l == 3 )
		floSize++;

	for ( i = 0; i < reps; i++ ) {
       		flograph = XieAllocatePhotofloGraph(floSize);

		idx = 0;
		XieFloImportPhotomap(&flograph[idx],
			photomap1,
			False
		);
		idx++;

		XieFloUnconstrain(&flograph[idx],
			idx
		);
		idx++;
		src = idx;

		XieFloImportPhotomap(&flograph[idx],
			photomap2,
			False
		);
		idx++;

		XieFloUnconstrain(&flograph[idx],
			idx
		);
		idx++;

		constant[ 0 ] = constant[ 1 ] = constant[ 2 ] = 0;
		XieFloArithmetic(&flograph[idx],
			src,
			idx,	
			&domain,
			constant,
			xieValAdd,
			bandMask
		);
		idx++;

		constant[ 0 ] = constant[ 1 ] = constant[ 2 ] = 2;
		XieFloArithmetic(&flograph[idx],
			idx,
			0,
			&domain,
			constant,
			xieValDiv,
			bandMask
		);	
		idx++;

		levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 256;
		XieFloConstrain( &flograph[idx],
			idx,
			levels,
			xieValConstrainHardClip,
			(XiePointer) NULL
		);
		idx++;
		src = idx;
	
		XieFloExportPhotomap( &flograph[idx],
			idx,
			photomap2,
			xieValEncodeServerChoice,
			(XiePointer) NULL
		);
		idx++;

		if ( l == 3 ) {
			levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 6;
			XieFloDither( &flograph[idx],
				src,
				0x7,
				levels,
				xieValDitherErrorDiffusion,
				( char * ) NULL
			);
			idx++;
			src = idx;
		}

		clists[ i + 1 ] = XieCreateColorList( display );
		XieFloConvertToIndex( &flograph[idx],
			src,				/* source element */
			DefaultColormap(display, 
				DefaultScreen(display)),/* colormap to alloc */
			clists[ i + 1 ],		/* colorlist */
			True,				/* notify if problems */
			xieValColorAllocAll,		/* color alloc tech */
			( char * ) colorParm		/* technique parms */
		);
		idx++;

		XieFloExportDrawable(&flograph[idx], 
			idx, 		/* source */
			wins[ i + 1 ], 	/* drawable to send data to */
			gc,		/* GC */	
			0, 		/* x offset in window to place data */
			0		/* y offset in window to place data */
		);
		idx++;

		XieExecuteImmediate(display, photospace, floId, notify, 
			flograph, floSize);

		eventData.floId = floId;
		eventData.space = photospace;
		eventData.base = xieInfo->first_event;
		eventData.which = xieEvnNoPhotofloDone;
		WaitForXIEEvent(display, &eventData, 10L, &event );

		XieFreePhotofloGraph(flograph, floSize);
	}

	/* free up what we allocated */

	if ( colorParm )
		XFree( colorParm );
	if ( rgbParm )
		XFree( rgbParm );
	free( bytes );
	XieDestroyPhotomap( display, photomap1 );	
	XieDestroyPhotomap( display, photomap2 );	
	for ( i = 0; i < reps + 1; i++ ) {
		XieDestroyColorList( display, clists[ i ] );
	}
	XieDestroyPhotospace(display, photospace);
	XFree(decodeParms);

	/* wait for a button or key press before exiting */

	XSelectInput(display, wins[ 0 ], ButtonPressMask | KeyPressMask |
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
	printf("usage: %s [-d display] [-r reps] -i image\n", pgm);
	exit(1);
}
