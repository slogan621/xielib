
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

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	XEvent          event;
	char		d, l;
	short		w, h;
	Bool            notify;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	int             done, idx, flag, fd, floSize, floId, bandMask;
	int		size, count;
	XieLTriplet     width, height, levels;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL, *out_file = NULL;
	char		*bytes;
        XieConstant     inLow, inHigh;
	XieLTriplet	outLow, outHigh;
	XiePointer 	constrainParms;
       	XieOrientation  fillOrder = xieValLSFirst;
        XieOrientation  pixelOrder = xieValLSFirst;
        unsigned int	pixelStride = 8;
        unsigned int	scanlinePad = 1;
	unsigned int	outLevels = 256;
	char		rfile[ 128 ];
	char		*decodeParms, *encodeParms;
	int		decodeTech, encodeTech;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:f:p:s:c:o:w:"))!=EOF) {
		switch(flag) {

		case 'f':	if ( !strcmp( optarg, "LSFirst" ) )
					fillOrder = xieValLSFirst;
				else
					fillOrder = xieValMSFirst;
				break;

		case 'p':	if ( !strcmp( optarg, "LSFirst" ) )
					pixelOrder = xieValLSFirst;
				else
					pixelOrder = xieValMSFirst;
				break;

		case 's':	pixelStride = atoi( optarg );
				break;

		case 'c':	scanlinePad = atoi( optarg );
				break;

		case 'w':	outLevels = atoi( optarg );
				break;

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;
	
		case 'o':	out_file = optarg;	
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

	if ( out_file == ( char * ) NULL )
	{
		printf( "Output file not defined\n" );
		usage( argv[ 0 ] );
	}

	if ( ( size = GetJFIFData( img_file, &bytes, &d, &w, &h, &l ) ) == 0 )
	{
		printf( "Problem getting JPEG data from %s\n", img_file );
		exit( 1 );
	}

	if ( d != 8 || l != 1 )
	{
		printf( "Image must be SingleBand and have 256 levels\n" );
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

	/* Create and populate a photoflo graph */

	floSize = 2;

	if ( outLevels != 256 )
		floSize++;

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
		xieValSingleBand,	/* data type - SingleBand */
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* don't send any DecodeNotify */ 
		decodeTech,		/* the decode technique */
		(char *) decodeParms	/* the decode parameters */
	);
	idx++;

	if ( outLevels > 256 )
	{
		constrainParms = ( XiePointer ) NULL;
                levels[ 0 ] = outLevels;
                XieFloConstrain( &flograph[idx],
                        idx,
                        levels,
                        xieValConstrainHardClip,
                       	constrainParms 
                );
                idx++;
	}
	else if ( outLevels < 256 )
	{
		inLow[ 0 ] = 0.0;
		inHigh[ 0 ] = 255.0;
		outLow[ 0 ] = 0.0;
		outHigh[ 0 ] = outLevels - 1;
                constrainParms = ( XiePointer ) XieTecClipScale(
                        inLow,
                        inHigh,
                        outLow,
                        outHigh
                );
   
                levels[ 0 ] = outLevels;
                XieFloConstrain( &flograph[idx],
                        idx,
                        levels,
                        xieValConstrainClipScale,
                        (XiePointer) constrainParms
                );
                idx++;
	}

        encodeTech = xieValEncodeUncompressedSingle;
        encodeParms = ( char * ) XieTecEncodeUncompressedSingle(
                fillOrder,
		pixelOrder,
		pixelStride,
		scanlinePad
        );

	XieFloExportClientPhoto(&flograph[idx], 
		idx, 		/* source */
		xieValNewData,		/* send ExportAvailable events */
		encodeTech,
		encodeParms
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

        /* next, read the newly encoded image back from ExportClientPhoto */

	free( bytes );
	bytes = 0;

        count = ReadNotifyExportData( display, xieInfo, photospace, floId, 
		(outLevels == 256 ? 2 : 3 ), 1, 100, 0, &bytes );

        /* write the image to a raw file */

        printf( "writing raw file: width %d height %d\n", width[ 0 ], 
		height[ 0 ] );

        sprintf( rfile, "%s.raw", out_file );
        fd =  open( rfile, O_CREAT | O_TRUNC | O_RDWR, 0666 );
        write( fd, bytes, count );
        close( fd );

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

	/* free up what we allocated */

	free( bytes );
	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	XFree(decodeParms);
	XFree(encodeParms);
	if ( constrainParms )
		XFree( constrainParms );
	XCloseDisplay(display);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i image -o output\n\
[-f fillOrder] [-p pixelOrder] [-s pixelStride] [-c scanlinePad]\
[-w number of output levels]\n", pgm);
	exit(1);
}
