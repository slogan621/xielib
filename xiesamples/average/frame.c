
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

#define	SIZE	50	

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
	int		d, l, w, h;
	Window		wins[ SIZE + 2 ];
	unsigned char	stride[ 3 ], leftPad[ 3 ], scanlinePad[ 3 ];
	GC 		gc;
	Bool            notify;
	int		pixel;
	XieConstant     constant;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
	XieColorList	clists[ SIZE + 2 ];
        XieColorAllocAllParam *colorParm;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XiePhotomap	photomap;
	XieProcessDomain domain;
	XSetWindowAttributes attribs;
	int             i, done, idx, edSrc, src, frames = SIZE,
			floSize, floId, bandMask = 0x7, flag, size;
	XieLTriplet     width, height, levels;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*bytes, *shotbuf;
	char		*img_file = NULL;
	Bool		isTriple = False;
	float		rate = 0.01;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:f:r:t"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;

		case 'r':	rate = atof( optarg );
				rate /= 2.0;
				break;
	
		case 'f':	frames = atoi(optarg);	
				if ( frames < 1 || frames > SIZE )
					frames = SIZE;
				break;

		case 't':	isTriple = True;
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

	if ( !ReadPPM( img_file, width, height, levels, &bytes, &size ) ) {
		if ( !ReadPGM( img_file, &w, &h, &d, &bytes, &size ) ) {
			printf( "Was unable to read PPM or PGM data from '%s'\n", 
				img_file );
			exit( 1 ); 
		}
		width[0] = width[1] = width[2] = w;
		height[0] = height[1] = height[2] = h;
		levels[0] = levels[1] = levels[2] = d;
	}
	else 
		isTriple = True;

	/* allocate a buffer to hold shot noise versions of image */

	shotbuf = malloc( size );
	if ( !shotbuf ) {
		printf( "Unable to allocate shot noise buffer\n" );
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

	for ( i = 0; i < frames + 2; i++ ) {
		wins[ i ] = XCreateSimpleWindow(display, 
			DefaultRootWindow(display),
	 		0, 0, width[ 0 ], height[ 0 ], 1, 0, 255 );
		XSelectInput( display, wins[ i ], ExposureMask );
		XMapRaised(display, wins[ i ]);
		WaitForWindow( display, wins[ i ] );
	}

	gc = XCreateGC(display, wins[ 0 ], 0L, (XGCValues *) NULL);

        XSetForeground( display, gc, XBlackPixel( display, 0 ) );
        XSetBackground( display, gc, XWhitePixel( display, 0 ) );

	/* Now for the XIE stuff */

	/* display the first shot noise image */

	memcpy( shotbuf, bytes, size );

	pixel = 0x00;
	AddShotNoise( shotbuf, size, pixel, rate );
	pixel = 0xff;
	AddShotNoise( shotbuf, size, pixel, rate );

	floSize = 4;
	if ( isTriple )
		floSize++;

	flograph = XieAllocatePhotofloGraph(floSize);

	/* allocate this once and share with all photoflos */

	colorParm = XieTecColorAllocAll( 128 );	/* mid range fill */

	if ( isTriple ) {
		decodeTech = xieValDecodeUncompressedTriple;
		stride[ 0 ] = stride[ 1 ] = stride[ 2 ] = 24;
		leftPad[ 0 ] = leftPad[ 1 ] = leftPad[ 2 ] = 0;
		scanlinePad[ 0 ] = scanlinePad[ 1 ] = scanlinePad[ 2 ] = 0;

		decodeParms = (XiePointer) XieTecDecodeUncompressedTriple( 
			xieValLSFirst,
			xieValLSFirst,
			xieValLSFirst,
			xieValBandByPixel,
			stride, 	
			leftPad,       
			scanlinePad   
		);
	}
	else {
		decodeTech = xieValDecodeUncompressedSingle;

		decodeParms = (XiePointer) XieTecDecodeUncompressedSingle( 
			xieValLSFirst,
			xieValLSFirst,
			8, 		/* stride */
			0,       	/* left pad */
			0        	/* scanline pad */
		);
	}

	photomap = XieCreatePhotomap( display );

	idx = 0;
	notify = False;
	XieFloImportClientPhoto(&flograph[idx], 
		( isTriple == True  ? xieValTripleBand : xieValSingleBand ),
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify events */ 
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;
	src = idx;

	XieFloExportPhotomap( &flograph[idx],
		idx, 
		photomap,
		xieValEncodeServerChoice,
		(XiePointer) NULL
	);
	idx++;

        if ( isTriple ) {
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

        PumpTheClientData( display, floId, photospace, 1, shotbuf, size, 
                sizeof( char ), 0, True );

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

	XieFreePhotofloGraph(flograph, floSize);

	floSize = 10;
	if ( isTriple )
		floSize++;

	domain.phototag = 0;
	domain.offset_x = 0;
	domain.offset_y = 0;

	for ( i = 0; i < frames; i++ ) {
       		flograph = XieAllocatePhotofloGraph(floSize);

		idx = 0;
		XieFloImportPhotomap(&flograph[idx],
			photomap,
			False
		);
		idx++;

		XieFloUnconstrain(&flograph[idx],
			idx
		);
		idx++;
		src = idx;

		levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 256;
		XieFloImportClientPhoto(&flograph[idx], 
			( isTriple == True ? xieValTripleBand : xieValSingleBand ),
			width,                  /* width of each band */
			height,                 /* height of each band */
			levels,                 /* levels of each band */
			notify,                 /* no DecodeNotify events */ 
			decodeTech,             /* decode technique */
			(char *) decodeParms    /* decode parameters */
		);
		idx++;
		edSrc = idx;

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

		constant[ 0 ] = constant[ 1 ] = constant[ 2 ] = 2.0;
		XieFloArithmetic(&flograph[idx],
			idx,
			0,
			&domain,
			constant,
			xieValDiv,
			bandMask
		);	
		idx++;

		XieFloConstrain( &flograph[idx],
			idx,
			levels,
			xieValConstrainHardClip,
			(XiePointer) NULL
		);
		idx++;
	
		XieFloExportPhotomap( &flograph[idx],
			idx,
			photomap,
			xieValEncodeServerChoice,
			(XiePointer) NULL
		);
		idx++;

		if ( isTriple ) {
			levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 6;
			XieFloDither( &flograph[idx],
				edSrc,
				bandMask,
				levels,
				xieValDitherErrorDiffusion,
				( char * ) NULL
			);
			idx++;
			edSrc = idx;
		}

		clists[ i + 1 ] = XieCreateColorList( display );
		XieFloConvertToIndex( &flograph[idx],
			edSrc,				/* source element */
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

       		memcpy( shotbuf, bytes, size );

		pixel = 0xff;
		AddShotNoise( shotbuf, size, pixel, rate );
		pixel = 0x00;
		AddShotNoise( shotbuf, size, pixel, rate );

		PumpTheClientData( display, floId, photospace, 3, shotbuf, 
			size, sizeof( char ), 0, True );

		eventData.floId = floId;
		eventData.space = photospace;
		eventData.base = xieInfo->first_event;
		eventData.which = xieEvnNoPhotofloDone;
		WaitForXIEEvent(display, &eventData, 10L, &event );

		XieFreePhotofloGraph(flograph, floSize);
	}

	/* display the averaged image  */

	floSize = 3;
	if ( isTriple )
		floSize++;

        flograph = XieAllocatePhotofloGraph(floSize);

        idx = 0;
        XieFloImportPhotomap(&flograph[idx],
        	photomap,
                False
        );
        idx++;

	if ( isTriple ) {
		levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 6;
		XieFloDither( &flograph[idx],
			idx,
			0x7,
			levels,
			xieValDitherErrorDiffusion,
			( char * ) NULL
		);
		idx++;
	}

	clists[frames+1] = XieCreateColorList( display );
	XieFloConvertToIndex( &flograph[idx],
		idx,
		DefaultColormap(display, 
			DefaultScreen(display)),/* colormap to alloc */
		clists[frames+1],           	/* colorlist */
		True,                           /* notify if problems */
		xieValColorAllocAll,            /* color alloc tech */
		( char * ) colorParm            /* technique parms */
        );
	idx++;

	XieFloExportDrawable(&flograph[idx],
		idx,            /* source */
		wins[frames+1], /* drawable to send data to */
		gc,             /* GC */
		0,              /* x offset in window to place data */
		0               /* y offset in window to place data */
	);
	idx++;

	XieExecuteImmediate(display, photospace, floId, notify, flograph, 
		floSize);

	/* free up what we allocated */

	if ( colorParm )
		XFree( colorParm );
	free( bytes );
	free( shotbuf );
	XieDestroyPhotomap( display, photomap );	
	for ( i = 0; i < frames + 2; i++ ) {
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
	printf("usage: %s [-d display] [-f frames] [-r rate] [-t] -i image\n", pgm);
	exit(1);
}
