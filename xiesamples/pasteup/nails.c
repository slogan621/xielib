
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
#include <dirent.h>

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

#include "transform.h"

void   usage();
static XieExtensionInfo *xieInfo;
static Backend *backend = (Backend *) NULL;

#define	MAXNAILS	128

main(argc, argv)
int  argc;
char *argv[];
{
	GC 		gc;
	Window          win;
	XEvent          event;
	XiePhotomap	*photomaps = ( XiePhotomap * ) NULL;
	Display        	*display;
	int		size, flag, screen, done;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*directory = NULL;
	char		*names[ MAXNAILS ];

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	directory = optarg;	
				break;
	
		default: 	printf(" unrecognized flag (-%c)\n",flag);
				usage(argv[0]);
				break;
		}
	}

	if ( directory == ( char * ) NULL ) {
		printf( "Directory not specified\n" );
		usage( argv[ 0 ] );
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

        InitTransforms();

	/* Do some X stuff - create colormaps, windows, etc. */

	screen = DefaultScreen(display);
	win = XCreateSimpleWindow(display, DefaultRootWindow(display),
	 	0, 0, 640, 480, 1, 0, 255 );

	gc = XCreateGC(display, win, 0L, (XGCValues *) NULL);

        XSetForeground( display, gc, XBlackPixel( display, 0 ) );
        XSetBackground( display, gc, XWhitePixel( display, 0 ) );

	size = GetThumbnailPhotomaps( display, xieInfo, directory, 
		64, 64, &photomaps, names );

	XSelectInput(display, win, ButtonPressMask | KeyPressMask |
		     ExposureMask);
	XMapRaised(display, win);
	done = 0;
	while (done == 0) {
		XNextEvent(display, &event);
		switch (event.type) {
		case KeyPress:
			done = 1;
			break;
		case MappingNotify:
			XRefreshKeyboardMapping((XMappingEvent*)&event);
			break;
		case Expose:
			if ( event.xexpose.count == 0 )
printf( "Calling Draw thumbnails\n" );
				DrawThumbnails( display, xieInfo, win, gc, 
					photomaps, names, size, 64, 64 );
			break;
		}
	}

	if ( backend != (Backend *) NULL )
		CloseBackend( backend, display );

	XFreeGC(display, gc);
	XCloseDisplay(display);
}

int
GetThumbnailPhotomaps( display, xieInfo, dir, tw, th, maps, names )
Display *display;
XieExtensionInfo *xieInfo;
char	*dir;
int	tw, th;
XiePhotomap **maps;
char	*names[];
{
    	DIR            	*dirp;
	Bool		notify;
    	struct dirent  	*file;
	char		*bytes;	
	XiePhotomap	dummy;	
        XiePhotoElement *flograph;
	transformHandle	scaleHandle;
        XieGeometryTechnique sampleTech;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
        XiePointer      sampleParms;
	XiePhotoflo	flo;
	unsigned int	bandMask;
	float		coeffs[ 6 ];
	XEvent 		event;
	XieConstant	constant = { 128.0, 0.0, 0.0 };
	XieLTriplet     width, height, levels;
        char            *decodeParms;
        int             size, floSize, idx, decodeTech, count;
	char		deep, bands;
	short		w, h;
	char		path[ 256 ];

    	if ((dirp = opendir (dir)) == NULL) 
		return( 0 );

	/* build a stored photoflo which will be modified and executed
	   to create a photomap for each image in the directory */

        decodeTech = xieValDecodeJPEGBaseline;
        decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
                xieValBandByPixel,
                xieValLSFirst,
                True
        );

       	floSize = 3;
	idx = 0;

        flograph = XieAllocatePhotofloGraph(floSize);

        notify = False;
        width[0] = width[1] = width[2] = 0;
        height[0] = height[1] = height[2] = 0;
        levels[0] = levels[1] = levels[2] = 256;

        XieFloImportClientPhoto(&flograph[idx], 
                xieValSingleBand,       /* class */
                width,                  /* width of each band */
                height,                 /* height of each band */
                levels,                 /* levels of each band */
                notify,                 /* no DecodeNotify events */ 
               	decodeTech,             /* decode technique */
                (char *) decodeParms    /* decode parameters */
        );
        idx++;

        sampleTech = xieValGeomAntialias; 
        sampleParms = ( XieGeometryTechnique ) NULL; 

	bandMask = 0x1;
        XieFloGeometry(&flograph[idx],
                idx,                    /* image source */
                tw,                     /* width of resulting image */
                th,                     /* height of resulting image */
                coeffs,                 /* a, b, c, d, e, f */
                constant,               /* used if src pixel does not exist */ 
                bandMask,               /* ignored for SingleBand images */
                sampleTech,             /* sample technique... */ 
                sampleParms             /* and parameters, if required */
        );
        idx++;

        XieFloExportPhotomap( &flograph[idx],
                idx,                            /* source element */
                dummy,				/* dummy */
                xieValEncodeServerChoice,
                (XiePointer) NULL
        );
        idx++;

	flo = XieCreatePhotoflo( display, flograph, floSize );

	count = 0;
	while ((file = readdir (dirp)) != NULL) {
		sprintf( path, "%s/%s", dir, file->d_name );
	        if ( ( size = GetJFIFData( path, &bytes, &deep, &w, &h, &bands ) ) == 0 )
			continue;
		if ( deep != 8 && bands != 1 )
			continue;

		*maps = realloc( *maps, (count + 1) * sizeof( XiePhotomap * ) );

		if ( *maps == ( XiePhotomap * ) NULL )
			break;	

		names[ count ] = malloc( strlen( file->d_name ) + 1 ); 
		if ( strlen( file->d_name ) > 12 ) 
			strncpy( names[ count ], file->d_name, 12 );
		else
			strcpy( names[ count ], file->d_name );

        	(*maps)[ count ] = XieCreatePhotomap( display );

		width[0] = width[1] = width[2] = w;
		height[0] = height[1] = height[2] = h;

		idx = 0;
		XieFloImportClientPhoto(&flograph[idx], 
			xieValSingleBand,       /* class */
			width,                  /* width of each band */
			height,                 /* height of each band */
			levels,                 /* levels of each band */
			notify,                 /* no DecodeNotify events */ 
			decodeTech,             /* decode technique */
			(char *) decodeParms    /* decode parameters */
		);
		idx++;

		scaleHandle = CreateScale( w / (float) tw, h / (float) th );
		SetCoefficients( scaleHandle, coeffs );
		FreeTransformHandle( scaleHandle );
		bandMask = 0x1;
		XieFloGeometry(&flograph[idx],
			idx,                    /* image source */
			tw,                     /* width of resulting image */
			th,                     /* height of resulting image */
			coeffs,                 /* a, b, c, d, e, f */
			constant,               /* used if src pixel does not 
						   exist */ 
			bandMask,               /* ignored for SingleBand 
						   images */
			sampleTech,             /* sample technique and... */ 
			sampleParms             /* parameters, if required */
		);
		idx++;

        	XieFloExportPhotomap( &flograph[idx],
			idx,                           
			(*maps)[ count ],	
			xieValEncodeServerChoice,
			(XiePointer) NULL
		);

		XieModifyPhotoflo( display, flo, 1, &flograph[ 0 ], 3 );
		XieExecutePhotoflo( display, flo, True );

		PumpTheClientData( display, flo, 0, 1, bytes, size, 
			sizeof( char ), 0, True );

		eventData.floId = flo;
		eventData.space = 0;
		eventData.base = xieInfo->first_event;
		eventData.which = xieEvnNoPhotofloDone;
		WaitForXIEEvent(display, &eventData, 10L, &event );

		free( bytes );
		count++;
		if ( count == MAXNAILS )
			break;
	}
	closedir( dirp );
	return( count );
}	

int
DrawThumbnails( display, xieInfo, window, gc, photomaps, names, size, wide, 
high )
Display	*display;
XieExtensionInfo *xieInfo;
Window 	window;
GC 	gc;
XiePhotomap *photomaps;
char	*names[];
int	size;
int	wide;
int	high;
{
	static int 	beSize;
	XiePhotospace 	photospace;
	XiePhotoElement *flograph;
        int     	xr, yr;
        unsigned int 	wr, hr;
        unsigned int 	bwr, dr;
	Window		root;
	int		screen, i, row, col;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	unsigned int 	bandMask = 0x01;
	int		floId, idx, floSize;
	XieTile		*tiles;
	XieConstant	constant;
	Bool 		notify;
	XEvent 		event;
	Visual		*visual;

	XGetGeometry( display, window, &root, &xr, &yr, &wr, &hr, &bwr, &dr );

	if ( backend == (Backend *) NULL ) {
		screen = DefaultScreen( display );
		visual = DefaultVisual( display, screen );

		backend = (Backend *) InitBackend( display, screen, 
			visual->class, xieValSingleBand, 
			1<<DefaultDepth( display, screen ), -1, &beSize );

		if ( backend == (Backend *) NULL ) {
			fprintf( stderr, "Unable to create backend\n" );
			exit( 1 );
		}
	}

	floSize = size + beSize + 1;

	flograph = XieAllocatePhotofloGraph(floSize);

	tiles = ( XieTile * ) malloc( sizeof( XieTile ) * size );

	idx = 0;
	row = col = 0;

	for ( i = 0; i < size; i++ ) {
		XieFloImportPhotomap( &flograph[ idx ],
			photomaps[ i ],
			False
		);
		idx++;

		tiles[ i ].src = idx;
		tiles[ i ].dst_x = col;
		tiles[ i ].dst_y = row;
		
		col += wide + 10;
		if ( col + wide > wr ) {
			col = 0;
			row += high + 12;
		}
	}

	constant[ 0 ] = 128.0;

	XieFloPasteUp( &flograph[ idx ],
		wr,
		hr,
		constant,
		tiles,
		size
	);
	idx++;

        if ( !InsertBackend( backend, display, window, 0, 0, gc, 
                flograph, idx ) ) {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }
	
	floId = 1;
	notify = True;
	photospace = XieCreatePhotospace(display);

	/* run the flo */

	XieExecuteImmediate(display, photospace, floId, notify, flograph,
	    floSize);

	/* label the thumbnails */

	row = col = 0;

	for ( i = 0; i < size; i++ ) {
		XDrawString( display, window, gc, col, row + high + 10, 
			names[ i ], strlen( names[ i ] ) );
		
		col += wide + 10;
		if ( col + wide > wr ) {
			col = 0;
			row += high + 12;
		}
	}

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	free( tiles );

	return( 1 );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i directory\n", pgm);
	exit(1);
}
