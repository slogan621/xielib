
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

#define	img_file1	"image.001"
#define img_file2	"image.002"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>
#include "events.h"
#include "backend.h"

Bool   SendDiskImage();
void   usage();
static XieExtensionInfo *xieInfo;

/* the following should really never be hardcoded, but should be determined
   on an image-by-image basis. If uncompressed, we will only need the width, 
   height, and levels, and we will assign their (hardcoded) values later */

static int swidth = 512, sheight = 512, slevels = 256;
static int pixel_stride=8, left_pad = 0, scan_pad=4;
static XieOrientation	pixel_order = xieValLSFirst;
static XieOrientation	fill_order = xieValLSFirst;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	Backend		*backend;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
	Window          window, root;
	Pixmap		pixmap;
	Bool            notify;
	XiePhotoElement *flograph;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XiePhotoflo     flo;
	int             i, j, done, idx, floSize, bandMask;
	XieLTriplet     width, height, levels;
	XieProcessDomain domain;
	XieConstant 	constant;
	int		beSize, screen, flag, src1, src2;
	char		*getenv(), *display_name = getenv("DISPLAY");
	int		x, y, depth;
	unsigned int	w, h, bw;
	XPoint		points[ 5 ];
	GC 		gc;
	Visual		*visual;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		default: 	printf(" unrecognized flag (-%c)\n",flag);
				usage(argv[0]);
				break;
		}
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
	 	0, 0, swidth, sheight, 1, 0, 255 );

	gc = XCreateGC(display, window, 0L, (XGCValues *) NULL);

	XMapRaised(display, window);
	XMoveWindow(display, window, 0, 0);

	XSelectInput( display, window, ExposureMask );
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

	/* create a pixmap resource */

	XGetGeometry( display, window, &root, &x, &y, &w, &h, &bw, &depth );
	pixmap = XCreatePixmap(display, window, swidth, sheight, depth );

	/* initialize the pixmap */

	XSetForeground( display, gc, 0 );
	XFillRectangle( display, pixmap, gc, 0, 0, w, h );

	XSetForeground( display, gc, ~0 );	
	XFillRectangle( display, pixmap, gc, 50, 50, 156, 156 );
	XFillArc( display, pixmap, gc, 305, 50, 156, 156, 0, 360 * 64 );
	points[ 0 ].x = 128;
	points[ 0 ].y = 305;
	points[ 1 ].x = 50;
	points[ 1 ].y = 461;
	points[ 2 ].x = 206;
	points[ 2 ].y = 461;
	XFillPolygon( display, pixmap, gc, points, 3, Convex, CoordModeOrigin );	
	points[ 0 ].x = 384;
	points[ 0 ].y = 305;
	points[ 1 ].x = 305;
	points[ 1 ].y = 381;
	points[ 2 ].x = 384;
	points[ 2 ].y = 461;
	points[ 3 ].x = 461;
	points[ 3 ].y = 381;
	XFillPolygon( display, pixmap, gc, points, 4, Convex, CoordModeOrigin );	


	/* Create and populate a stored photoflo graph */

        visual = DefaultVisual( display, screen );

        backend = (Backend *) InitBackend( display, screen, visual->class,
                xieValSingleBand, 1<<DefaultDepth( display, screen ), -1, &beSize );

        if ( backend == (Backend *) NULL )
        {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }

	floSize = 4 + beSize;

	flograph = XieAllocatePhotofloGraph(floSize);

	width[0] = swidth;
	width[1] = width[2] = 0;
	height[0] = sheight;
	height[1] = height[2] = 0;
	levels[0] = slevels;
	levels[1] = levels[2] = 0;

	decodeTech = xieValDecodeUncompressedSingle;
	decodeParms = ( char * ) XieTecDecodeUncompressedSingle( 
		fill_order, 
		pixel_order, 
		pixel_stride, 
		left_pad, 
		scan_pad
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
		notify, 		/* send DecodeNotify events */
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;
	src1 = idx;

	XieFloImportClientPhoto(&flograph[idx], 
		xieValSingleBand, 	/* class of image data */
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* send DecodeNotify events */
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;
	src2 = idx;

	XieFloImportDrawablePlane(&flograph[idx],
		pixmap,		/* the drawable to import image from */
		0, 		/* X local coord of upper lefthand corner */
		0,		/* Y local coord of upper lefthand corner */
		w,		/* entire width of pixmap */
		h,		/* entire height of pixmap */
		0x00,		/* fill constant not used with pixmaps */
		0x01,		/* bit plane */
		False		/* we can't possibly obscure pixmaps */
	);
	idx++;

	domain.phototag = idx;
	domain.offset_x = 0;
	domain.offset_y = 0;
	bandMask = 1;
	constant[ 0 ] = ( float ) 0.0;

	XieFloLogical(&flograph[idx],	
		src1,		/* first image */	
		src2,		/* second image */
		&domain,	/* specifies our control plane */	
		constant,	/* unused since we have dyadic inputs */
		GXcopy,		/* pixels intersecting domain will be set */
		bandMask	/* for us, just work with band 0 */
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

	SendDiskImage(display, img_file1, 0, flo, 1, 0);
	SendDiskImage(display, img_file2, 0, flo, 2, 0);

	/* wait for the flo to finish */

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
	XFreeGC(display, gc);
	XDestroyWindow(display, window);
	XFreePixmap(display, pixmap);
	XieFreePhotofloGraph(flograph, floSize);
	XFree(decodeParms);
	XieDestroyPhotoflo(display, flo);
	XCloseDisplay(display);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display]\n", pgm);
	exit(1);
}
