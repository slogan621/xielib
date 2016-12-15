
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

Bool   SendDiskImage();
void   usage();
static XieExtensionInfo *xieInfo;

/* the following should really never be hardcoded, but should be determined
   on an image-by-image basis. If uncompressed, we will only need the width, 
   height, and levels, and we will assign their (hardcoded) values later */

/* note, only compatible with images/bitonal/g31d.raw !!! */

static int swidth = 1728, sheight = 2180, slevels = 2;
static XieOrientation orientation = xieValMSFirst;
static Bool photometric = True;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	char		*decodeParms;
	int		decodeTech;
	XEvent          event;
	Window          window, subWindow;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotoflo     flo;
	int             done, status, idx, floSize, bandMask, x = 256, y = 256;
	XieLTriplet     width, height, levels;
	int		screen, flag;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL;
	char		*function = "GXcopy";
	char		*fill_style = "FillSolid"; 
	Bool		applyClipMask = False;
	int		subwindowMode = ClipByChildren;
	int		xOffset = 0, yOffset = 0;
	unsigned int	dummyui, bitwide, bithigh;
	XGCValues	gcVals;
	unsigned long	planeMask = ~0L;
	unsigned long 	gcValMask = GCFunction | GCPlaneMask | GCSubwindowMode 
			| GCClipXOrigin | GCClipYOrigin | GCFillStyle |
			GCForeground | GCBackground; 
	GC 		gc;
	Pixmap		clip_pixmap, fill_pixmap, backg;
	Window		root; 
	int		wx, wy; 
	unsigned int	ww, wh, wb, depth;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:x:y:csf:p:h:m:n:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case 'h':	fill_style = optarg; 
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
	subWindow = XCreateSimpleWindow(display, window, 0, 0, swidth >> 3, 
		sheight >> 3, 1, 0, 255 );
	XSelectInput( display, window, ExposureMask );

	gcVals.subwindow_mode = subwindowMode;
	gcVals.fill_style = GetGCFillStyle( fill_style );
	gcVals.function = GetGCFunction( function );
        gcVals.foreground = XWhitePixel( display, DefaultScreen( display ) );
        gcVals.background = XBlackPixel( display, DefaultScreen( display ) );
                
	XGetGeometry( display, window, &root, &wx, &wy, &ww, &wh, &wb, &depth );
	if ( applyClipMask == True )
	{
		status = XReadBitmapFile(display, root, "clip_bitmap", 
			&dummyui, &dummyui, &clip_pixmap, 0, 0);

		if ( status != BitmapSuccess )
		{
			fprintf( stderr, "Couldn't get clip bitmap\n" );
			applyClipMask = False;
		}
		else 
		{
			gcValMask |= GCClipMask;
			gcVals.clip_mask = clip_pixmap;
		}
	}

	gcVals.plane_mask = planeMask;
	gcVals.clip_x_origin = xOffset;
	gcVals.clip_y_origin = yOffset;

	gc = XCreateGC(display, window, gcValMask, (XGCValues *) &gcVals);

	status = XReadBitmapFile(display, root, 
		"fill_bitmap", &bitwide, &bithigh, &fill_pixmap, 0, 0);

	if ( status != BitmapSuccess )
	{
		fprintf( stderr, "Couldn't get fill bitmap\n" );
		fflush( stderr );		
	}
	else 
	{
		backg = XCreatePixmap( display,  root, bitwide, 
			bithigh, depth );
		if ( backg == None )
		{
			fprintf( stderr, "Couldn't create fill pixmap\n" );
			fflush( stderr );		
		}
		XCopyPlane( display, fill_pixmap, backg, gc, 0, 0, 
			bitwide, bithigh, 0, 0, 1 );
			
		XSetWindowBackgroundPixmap( display, window, backg );
		XSync( display, 0 );
	}

	XMapRaised(display, window);
	XMapRaised(display, subWindow);
	XMoveWindow(display, window, 0, 0);
	XMoveWindow(display, subWindow, x + 100, y + 100);

    	XSync( display, 0 );

	WaitForWindow( display, window ); 

	/* Create and populate a stored photoflo graph */

	floSize = 2;

	flograph = XieAllocatePhotofloGraph(floSize);

	width[0] = swidth;
	width[1] = width[2] = 0;
	height[0] = sheight;
	height[1] = height[2] = 0;
	levels[0] = slevels;
	levels[1] = levels[2] = 0;

	decodeTech = xieValDecodeG31D;
	decodeParms = ( char * ) XieTecDecodeG31D( 
		orientation, 
		True, 
		photometric 
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
		notify, 		/* no DecodeNotify events */ 
		decodeTech,		/* decode technique */
		(char *) decodeParms	/* decode parameters */
	);
	idx++;

	XieFloExportDrawablePlane(&flograph[idx], 
		idx, 		/* source */
		window, 	/* drawable to send data to */
		gc,		/* GC */	
		0, 		/* x offset in window to place data */
		0		/* y offset in window to place data */
	);
	idx++;

	/* Send the flograph to the server and get a handle back */

	flo = XieCreatePhotoflo(display, flograph, floSize);

	/* run the flo */

	notify = True;
	XieExecutePhotoflo(display, flo, notify);

	/* now that the flo is running, send image data */

	SendDiskImage(display, img_file, 0, flo, 1, 0);

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

	if ( clip_pixmap )
		XFreePixmap( display, clip_pixmap );
	if ( fill_pixmap )
		XFreePixmap( display, fill_pixmap );
	XFreeGC(display, gc);
	XDestroyWindow(display, window);
	XieFreePhotofloGraph(flograph, floSize);
	XFree(decodeParms);
	XieDestroyPhotoflo(display, flo);
	XCloseDisplay(display);
}

typedef struct _entry {
	char	*name;
	int	func;
} Entry;

Entry funcs[] = {
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

Entry fills[] = {
	{ "FillSolid", FillSolid },
	{ "FillTiled", FillTiled },
	{ "FillStippled", FillStippled },
	{ "FillOpaqueStippled", FillOpaqueStippled }
};

int
GetGCFunction( function )
char	*function;
{
	int	i;

	for ( i = 0; i < sizeof( funcs ) / sizeof( Entry ); i++ )
	{
		if ( !strcmp( funcs[ i ].name, function ) )
			return( funcs[ i ].func );
	}
	return( funcs[ 0 ].func );
}

int
GetGCFillStyle( style )
char	*style;
{
	int	i;

	for ( i = 0; i < sizeof( fills ) / sizeof( Entry ); i++ )
	{
		if ( !strcmp( fills[ i ].name, style ) )
			return( fills[ i ].func );
	}
	return( fills[ 0 ].func );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] [-c] [-s] [-p planemask] [-x xOffset] [-y yOffset] [-f function] [-h fill-style] -i image\n", pgm);
	exit(1);
}

