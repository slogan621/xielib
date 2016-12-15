
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
#include "errors.h"

void   usage();
static int sawError = 0;
static XieExtensionInfo *xieInfo;
int MyHandleError();
static ClientErrorPtr lutHandler = (ClientErrorPtr) NULL; 
static ClientErrorPtr floHandler = (ClientErrorPtr) NULL;

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	XEvent          event;
	Bool            notify, handle, doResource;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
	XieLTriplet	start;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	int             flag, done, idx, floSize, floId;
	char		*getenv(), *display_name = getenv("DISPLAY");

	handle = doResource = False;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:rh"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'h':	handle = True;
				break;
		
		case 'r':	doResource = True;
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

	if ( handle == True )
	{
		floHandler = SetClientErrorHandler( display,
			FLOERROR,
			xieErrNoFloLUT,
			MyHandleError,
			(XPointer) "LUT Flo" 
		);

		lutHandler = SetClientErrorHandler( display, 
			RESOURCEERROR, 
			xieErrNoLUT, 
			MyHandleError, 
			(XPointer) "LUT Resource" 
		);
		EnableClientHandlers( xieInfo );
	}

	if ( doResource == True )
	{
		XieDestroyLUT( display, 0 );
		XSync( display, 0 );
	}
	else
	{
		floSize = 2;

		flograph = XieAllocatePhotofloGraph(floSize);

		idx = 0;
		XieFloImportLUT(&flograph[idx], 
			0		/* bogus LUT */
		);
		idx++;

		start[ 0 ] = start[ 1 ] = start[ 2 ] = 0;

		XieFloExportLUT(&flograph[idx], 
			idx, 		/* source */
			0,		/* bogus LUT */
			False,		/* irrelevant */
			start
		);
		idx++;

		floId = 1;
		notify = True;
		photospace = XieCreatePhotospace(display);

		/* run the flo */

		XieExecuteImmediate(display, photospace, floId, notify, 
			flograph, floSize);

		eventData.floId = floId;
		eventData.space = photospace;
		eventData.base = xieInfo->first_event;
		eventData.which = xieEvnNoPhotofloDone;
		WaitForXIEEvent(display, &eventData, 10L, &event );

		XieFreePhotofloGraph(flograph, floSize);
		XieDestroyPhotospace(display, photospace);
	}
	while ( sawError == 0 );
	if ( lutHandler )
		RemoveErrorHandler( lutHandler );
	if ( floHandler )
		RemoveErrorHandler( floHandler );
	DisableClientHandlers();
	XCloseDisplay(display);
}

int
MyHandleError( Display *display, int which, XPointer error, XPointer private )
{
	fprintf( stderr, "Received error %d which is a %s error\n", 
		which, (char *) private );
	sawError = 1;
	return( 0 );
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] [h][r]\n", pgm);
	printf("h -> use our own error handler. Default is use XIElib's\n" );
	printf("r -> generate a LUT resource error. Default is LUT flo error\n" );
	exit(1);
}
