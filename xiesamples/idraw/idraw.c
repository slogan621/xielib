
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

#define	LUMA_RED	0.299
#define	LUMA_GREEN	0.587
#define	LUMA_BLUE	0.114

#define	WIDTH	512
#define	HEIGHT	512

#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>
#include "events.h"

#include <X11/cursorfont.h>

void   usage();
static XieExtensionInfo *xieInfo;

Window SelectWindow(Display *dpy);

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	XEvent          event;
	Window          window1, window2, root;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotoflo     flo;
  	XWindowAttributes wa;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	int             src, done, idx, floSize;
	int		screen, flag;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*output_file = NULL;
	GC		gc;
	char		*data;
	int		depth, count, fd;
	unsigned char	samples[3] = { 1, 1, 1 };
	XieEncodeJPEGBaselineParam *encodeParms;
	XieEncodeTechnique encodeTech;
	XieLevels	levels;
	XieConstant	bias;
	Bool		doColor = False;
	XiePointer	convertFromParms = (XiePointer) NULL;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:f:c"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case 'f':	output_file = optarg;
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'c': 	doColor = True;
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
	root = DefaultRootWindow( display );
	window1 = XCreateSimpleWindow(display, root, 
	 	0, 0, WIDTH, HEIGHT, 1, 0, 255 );
	XSelectInput( display, window1, ExposureMask );

	gc = XCreateGC(display, window1, 0L, (XGCValues *) NULL);

	XMapRaised(display, window1);

	WaitForWindow( display, window1 ); 

	/* now, get the window */

        window2 = SelectWindow(display);

        if (window2 != None && window2 != root ) {
              	window2 = XmuClientWindow (display, window2);
        }
	else if ( window2 != root )
	{
		fprintf( stderr, "Couldn't get window\n" );
		exit( 0 );
	}
	
	/* end of code snarfed from xwd */

        XBell(display, 50);

	/* Create and populate a stored photoflo graph */

	floSize = 2;
	if ( output_file != (char *) NULL )	
	{
		floSize+=2;
	}

	if ( doColor == True )
		floSize++;

	flograph = XieAllocatePhotofloGraph(floSize);

  	if (!XGetWindowAttributes(display, window2, &wa))
	{
		fprintf( stderr, "Couldn't get window attributes\n" );
		exit( 0 );
	}

	fprintf( stderr, "Importing from %x: width %d height %d\n",
		window2, wa.width, wa.height );

	idx = 0;
	XieFloImportDrawable(&flograph[idx],
		window2,	/* the drawable to import image from */
		0, 		/* X local coord of upper lefthand corner */
		0,		/* Y local coord of upper lefthand corner */
		(unsigned int )wa.width,  /* entire width of our window */
		(unsigned int )wa.height, /* entire height of our window */
		0x00,		/* fill constant */
		False		/* we don't care about obscured portions */
	);
	idx++;
	src = idx;

	XieFloExportDrawable(&flograph[idx], 
		idx, 	/* source */
		window1, /* drawable to send data to */
		gc,	/* GC */	
		0, 	/* x offset in window to place data */
		0	/* y offset in window to place data */
	);
	idx++;

	depth = DefaultDepth( display, screen );
	if ( output_file != ( char * ) NULL )
	{
		XieFloConvertFromIndex(&flograph[idx],
			src,
			DefaultColormap( display, screen ),
			(doColor == True ? xieValTripleBand : 
				xieValSingleBand ),
			depth
		);
		idx++;

		if ( doColor == True )
		{
			levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 1 << depth;
			bias[ 0 ] = 0.0; bias[ 1 ] = bias[ 2 ] = 127.0; 
			convertFromParms = XieTecRGBToYCbCr(
				levels,
				LUMA_RED,
				LUMA_GREEN,
				LUMA_BLUE,
				bias
			);
				
			XieFloConvertFromRGB( &flograph[idx],
				idx,
				xieValRGBToYCbCr,
				convertFromParms
			);
			idx++;
		}

		encodeTech = xieValEncodeJPEGBaseline;
		encodeParms = XieTecEncodeJPEGBaseline (
			xieValBandByPixel,
			xieValLSFirst,
			samples,
			samples,
			(char *) NULL,
			0,
			(char *) NULL,
			0,
			(char *) NULL,
			0
		);

	        XieFloExportClientPhoto(&flograph[idx],
			idx,            /* source */
			xieValNewData,  /* send ExportAvailable events */
			encodeTech,     /* tell XIE how to encode image */
			encodeParms     /* and the encode params */
		);
		idx++;
		src = idx;
	}

	/* Send the flograph to the server and get a handle back */

	flo = XieCreatePhotoflo(display, flograph, floSize);

	/* run the flo */

	notify = True;
	XieExecutePhotoflo(display, flo, notify);

	if ( output_file != ( char * ) NULL )
	{
		/* read the JPEG image back from ExportClientPhoto */

		data = (char *) NULL;
		count = ReadNotifyExportData( display, xieInfo, 0, flo, 
			src, 1, 100, 0, &data );
		if ( count <= 0 )
			printf( "Unable to read JPEG image from XIE\n" );
		else
		{
			fd =  open( output_file, O_CREAT|O_TRUNC|O_RDWR, 0666 );
			if ( fd > 0 )
			{
				write( fd, data, count );
				close( fd );
			}
			else
				printf( "Unable to write file %s\n", 
					output_file );
			if ( data )
				free( data );
		}
	}

	/* wait for the flo to finish */

        eventData.floId = flo;
        eventData.space = 0;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

      	XSelectInput(display, window1, ButtonPressMask | KeyPressMask |
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
	XDestroyWindow(display, window1);
	if ( convertFromParms )
		XFree( convertFromParms );
	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotoflo(display, flo);
	XCloseDisplay(display);
}

/*
 * Routine to let user select a window using the mouse
 */

Window 
SelectWindow(dpy)
Display *dpy;
{
  int status;
  Cursor cursor;
  XEvent event;
  Window target_win = None, root;
  int buttons = 0;

  root = DefaultRootWindow(dpy);

  /* Make the target cursor */
  cursor = XCreateFontCursor(dpy, XC_crosshair);

  /* Grab the pointer using target cursor, letting it roam all over */
  status = XGrabPointer(dpy, root, False,
     	ButtonPressMask|ButtonReleaseMask, GrabModeSync,
        GrabModeAsync, root, cursor, CurrentTime);
  if (status != GrabSuccess)
  	return( None );

  /* Let the user select a window... */
  while ((target_win == None) || (buttons != 0)) {
    /* allow one more event */
    XAllowEvents(dpy, SyncPointer, CurrentTime);
    XWindowEvent(dpy, root, ButtonPressMask|ButtonReleaseMask, &event);
    switch (event.type) {
    case ButtonPress:
      if (target_win == None) {
        target_win = event.xbutton.subwindow; /* window selected */
        if (target_win == None) target_win = root;
      }
      buttons++;
      break;
    case ButtonRelease:
      if (buttons > 0) /* there may have been some down before we started */
        buttons--;
       break;
    }
  } 

  XUngrabPointer(dpy, CurrentTime);      /* Done with pointer */

  return(target_win);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] [-f output_file] [-c]\n", pgm);
	printf("-c : write a TripleBand result. Default is SingleBand\n" );
	exit(1);
}
