
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
#include <string.h>
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

#define DEPTH	8		/* assume an 8-bit drawable */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>
#include "events.h"

void   usage();
static XieExtensionInfo *xieInfo;

static unsigned long sleepyTime = 0L;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	char		*decodeParms;
	int		decodeTech;
	char		deep, bands;
	short		w, h;
	XEvent          event;
	Window          window;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotoflo     flo;
        XieColorList    clist;
        XieColorAllocAllParam *colorParm;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	int             done, size, idx, floSize, bandMask, x, y;
	XieLTriplet     width, height, levels;
	int		screen, flag;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL;
	char		*offset_file = NULL;
	char		*ptr;
	GC 		gc;
	char		*bytes;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:o:s:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case 'o':	offset_file = optarg; 
				break;

		case 's':	sleepyTime = atol( optarg ); 
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

	if ( offset_file == ( char * ) NULL )
	{
		printf( "Offset file not defined\n" );
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
	 	0, 0, w, h, 1, 0, 255 );
	XSelectInput( display, window, ExposureMask );

	gc = XCreateGC(display, window, 0L, (XGCValues *) NULL);

	XMapRaised(display, window);
	XMoveWindow(display, window, 0, 0);

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

	/* Create and populate a stored photoflo graph */

	floSize = 3;

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

        colorParm = XieTecColorAllocAll( 128 ); /* mid range fill */
        clist = XieCreateColorList( display );
        XieFloConvertToIndex( &flograph[idx],
                idx,                            /* source element */
                DefaultColormap(display, 
                        DefaultScreen(display)),/* colormap to alloc */
                clist,                          /* colorlist */
                True,                           /* notify if problems */
                xieValColorAllocAll,            /* color alloc tech */
                ( char * ) colorParm            /* technique parms */
        );
        idx++;

	XieFloExportDrawable(&flograph[idx], 
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

	XieExecutePhotoflo(display, flo, notify);

	/* now that the flo is running, send image data */

	PumpTheClientData( display, flo, 0, 1, bytes, size, sizeof( char ), 
		0, True );

	/* for fun, wait for the flo to finish */

        eventData.floId = flo;
        eventData.space = 0;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

	done = 0;
	while (done == 0) {
		sleep( sleepyTime );
		done = !GetNextOffset( offset_file, &x, &y );
		if ( done == 0 )
		{
			XieFloExportDrawable(&flograph[0], 
				2, 	/* source */
				window, /* drawable to send data to */
				gc,	/* GC */	
				x, 	/* x offset in window to place data */
				y	/* y offset in window to place data */
			);

			XieModifyPhotoflo(display,
				flo,
				3,
				flograph,
				1
			);

			/* run the flo */

			XieExecutePhotoflo(display, flo, notify);

			/* now that the flo is running, send image data */

			PumpTheClientData( display, flo, 0, 1, bytes, size, 
				sizeof( char ), 0, True );

			/* for fun, wait for the flo to finish */

			eventData.floId = flo;
			eventData.space = 0;
			eventData.base = xieInfo->first_event;
			eventData.which = xieEvnNoPhotofloDone;
			WaitForXIEEvent(display, &eventData, 10L, &event );
		}
	}

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

        if ( colorParm )
                XFree( colorParm );
        XieDestroyColorList( display, clist );
	XFreeGC(display, gc);
	XDestroyWindow(display, window);
	XieFreePhotofloGraph(flograph, floSize);
	XFree(decodeParms);
	XieDestroyPhotoflo(display, flo);
	XCloseDisplay(display);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] [-s sleeptime ] -i image -o offsetfile\n", pgm);
	exit(1);
}

int
GetNextOffset( offset_file, x, y )
char	*offset_file;
int	*x, *y;
{
	static FILE *fp = ( FILE * ) NULL;
	char	buf[ 128 ];
	char	*ptr;

	if ( fp == ( FILE * ) NULL )
	{
		fp = fopen( offset_file, "r" );
		if ( fp == ( FILE * ) NULL )
		{
			printf( "Error opening offset file '%s'\n", 
				offset_file );
			return( 0 );
		}
	}

	/* get a line from the file */

	if ( fgets( buf, sizeof( buf ), fp ) == ( char * ) NULL )
	{
		fclose( fp );
		fp = ( FILE * ) NULL;
		return( 0 );
	}
	ptr = strtok( buf, ":" );	
	*x = atoi( ptr );
	ptr = strtok( ( char * ) NULL, ":" );
	*y = atoi( ptr );
	return( 1 );
}
