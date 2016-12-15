
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
#include "clientio.h"

#define	NUM_RECTS	100
#define LUT_SIZE	256

void   usage();

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	XEvent          event;
	Bool            notify;
	XiePhotoElement *flograph;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	int             i, flag, done, idx, floSize;
	char		*getenv(), *display_name = getenv("DISPLAY");
	XieExtensionInfo *xieInfo;
	XieProcessDomain domain;
	char		*imageFile;
        int             decodeTech, encodeTech;
        char            *decodeParms, *encodeParms;
	XiePhototag	src;
	unsigned int 	size;
        char            d, l;
        short           w, h;
	char		*bytes;
        XieLTriplet     width, height;
	XieLTriplet	length, start;
        XieLevels       levels;
	char		lut1[ LUT_SIZE ], lut2[ LUT_SIZE * 3 ];
	XieRectangle	roi[ NUM_RECTS ];
	ReadNotifyData	readVec[6];

	/* initialize the LUTs */

	for ( i = 0; i < LUT_SIZE; i++ )
		lut1[ i ] = lut2[ i ] = i;

	for ( i = LUT_SIZE; i < LUT_SIZE * 2; i++ )
		lut2[ i ] = i - LUT_SIZE;

	for ( i = LUT_SIZE * 2; i < LUT_SIZE * 3; i++ )
		lut2[ i ] = i - LUT_SIZE * 2;

	/* initialize the rectangles */

	for ( i = 0; i < NUM_RECTS; i++ )
	{
		roi[ i ].x = roi[ i ].y = 0;
		roi[ i ].width = roi[ i ].height = 100L;
	}
		
	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	imageFile = optarg;	
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

	if (imageFile == NULL ) {
		printf("Image file was not specified\n" );
		usage(argv[0]);
	}

        if ( ( size = GetJFIFData( imageFile, &bytes, &d, &w, &h, &l ) ) == 0 )
        {
                printf( "Problem getting JPEG data from %s\n", imageFile );
                exit( 1 );
        }

	if ( l != 1 )
	{
		printf( "Client only supports SingleBand image data\n" );
		free( bytes );
		exit( 1 );
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

	floSize = 2;

	flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;
        decodeTech = xieValDecodeJPEGBaseline;
        decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
                xieValBandByPixel,
                xieValMSFirst,
                True
        );

	encodeTech = xieValEncodeUncompressedSingle;
	encodeParms = (char *) XieTecEncodeUncompressedSingle(
		xieValMSFirst,		/* fill order */
		xieValMSFirst,		/* pixel order */
		8,			/* pixel stride */
		8			/* scanline pad */
	);

        width[0] = width[1] = width[2] = w;
        height[0] = height[1] = height[2] = h;
        levels[0] = levels[1] = levels[2] = 1 << (d - 1);

        idx = 0;
        notify = False;
        XieFloImportClientPhoto(&flograph[idx], 
                xieValSingleBand,	/* class of image */
                width,                  /* width of each band */
                height,                 /* height of each band */
                levels,                 /* levels of each band */
                notify,                 /* No DecodeNotify events */ 
                decodeTech,             /* decode technique */
                (char *) decodeParms    /* decode parameters */
        );
	idx++;

	XieFloExportClientPhoto(&flograph[idx],
                idx,                    /* source */
                xieValNewData,          /* send ExportAvailable events */
                encodeTech,             /* tell XIE how to encode image */
                encodeParms             /* and the encode params */
        );
        idx++;

	readVec[ 0 ].namespace = 0;
	readVec[ 0 ].element = idx;
	readVec[ 0 ].elementsz = sizeof( char );
	readVec[ 0 ].band = 0;
	readVec[ 0 ].numels = 0;

	notify = True;

	/* run the flo */

	readVec[ 0 ].flo_id = XieCreatePhotoflo( display, flograph, floSize );

	XieFreePhotofloGraph(flograph, floSize);

	XieExecutePhotoflo( display, readVec[ 0 ].flo_id, notify );

        PumpTheClientData( display, readVec[ 0 ].flo_id, 0, 1, bytes, size, 
                sizeof( char ), 0, True );

	floSize = 3;
	flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;
        XieFloImportClientPhoto(&flograph[idx],
                xieValSingleBand,	/* class of image */
                width,                  /* width of each band */
                height,                 /* height of each band */
                levels,                 /* levels of each band */
                notify,                 /*  No DecodeNotify events */
                decodeTech,             /* decode technique */
                (char *) decodeParms    /* decode parameters */
        );
	idx++;
	src = idx;

	XieFloExportClientPhoto(&flograph[idx],
                idx,                    /* source */
                xieValNewData,          /* send ExportAvailable events */
                encodeTech,             /* tell XIE how to encode image */
                encodeParms             /* and the encode params */
        );
        idx++;

	readVec[ 1 ].namespace = 0;
	readVec[ 1 ].element = idx;
	readVec[ 1 ].elementsz = sizeof( char );
	readVec[ 1 ].band = 0;
	readVec[ 1 ].numels = 0;

	domain.phototag = 0;
	domain.offset_x = 0;
	domain.offset_y = 0;

        XieFloExportClientHistogram( &flograph[ idx ],
                src,
                &domain,        /* get entire image - no ROI */
                xieValNewData   /* send event when new data is ready */ 
        );
        idx++;

	readVec[ 2 ].namespace = 0;
	readVec[ 2 ].element = idx;
	readVec[ 2 ].elementsz = sizeof( XieHistogramData );
	readVec[ 2 ].band = 0;
	readVec[ 2 ].numels = 0;

	notify = True;

	readVec[ 1 ].flo_id = readVec[ 2 ].flo_id = 
		XieCreatePhotoflo( display, flograph, floSize );

	/* run the flo */

        XieExecutePhotoflo( display, readVec[ 1 ].flo_id, notify );

	XieFreePhotofloGraph(flograph, floSize);

        PumpTheClientData( display, readVec[ 1 ].flo_id, 0, 1, bytes, size, 
                sizeof( char ), 0, True );

	floSize = 4;
	flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;
	length[ 0 ] = LUT_SIZE; length[ 1 ] = length[ 2 ] = 0;
	levels[ 0 ] = 256; levels[ 1 ] = levels[ 2 ] = 0;

	XieFloImportClientLUT(&flograph[idx], 
                xieValSingleBand,
                xieValLSFirst,
                length,
                levels
	);
	idx++;

	start[ 0 ] = start[ 1 ] = start[ 2 ] = 0;
	XieFloExportClientLUT(&flograph[idx],
		idx,
		xieValLSFirst,
		xieValNewData,
		start,
		length
	);
	idx++;

	readVec[ 3 ].namespace = 0;
	readVec[ 3 ].element = idx;
	readVec[ 3 ].band = 0;
	readVec[ 3 ].elementsz = sizeof( char );
	readVec[ 3 ].numels = LUT_SIZE;

	length[ 1 ] = length[ 2 ] = LUT_SIZE;
	levels[ 1 ] = levels[ 2 ] = 256;
	XieFloImportClientLUT(&flograph[idx], 
                xieValTripleBand,
                xieValLSFirst,
                length,
                levels

	);
	idx++;

	XieFloExportClientLUT(&flograph[idx],
		idx,
		xieValLSFirst,
		xieValNewData,
		start,
		length
	);
	idx++;

	readVec[ 4 ].namespace = 0;
	readVec[ 4 ].element = idx;
	readVec[ 4 ].elementsz = sizeof( char );
	readVec[ 4 ].band = 0;
	readVec[ 4 ].numels = LUT_SIZE * 3;

	notify = True;

	/* run the flo */

	readVec[ 3 ].flo_id = readVec[ 4 ].flo_id = 
		XieCreatePhotoflo( display, flograph, floSize );
	XieFreePhotofloGraph(flograph, floSize);

	XieExecutePhotoflo( display, readVec[ 3 ].flo_id, notify );

        PumpTheClientData( display, readVec[ 3 ].flo_id, 0, 1, lut1, 
		LUT_SIZE, sizeof( char ), 0, True );
        PumpTheClientData( display, readVec[ 3 ].flo_id, 0, 3, lut2, 
		LUT_SIZE * 3, sizeof( char ), 0, True );

	free( bytes );

	floSize = 2;
	flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;
	XieFloImportClientROI(&flograph[idx], 
                NUM_RECTS          /* number of rectangles in ROI */
	);
	idx++;

	XieFloExportClientROI(&flograph[idx],
		idx,
		xieValNewData
	);
	idx++;

	readVec[ 5 ].namespace = 0;
	readVec[ 5 ].element = idx;
	readVec[ 5 ].elementsz = sizeof( XieRectangle );
	readVec[ 5 ].band = 0;
	readVec[ 5 ].numels = 0;

	notify = True;

	/* run the flo */

	readVec[ 5 ].flo_id = XieCreatePhotoflo( display, flograph, floSize );

        XieExecutePhotoflo( display, readVec[ 5 ].flo_id, notify );

        PumpTheClientData( display, readVec[ 5 ].flo_id, 0, 1, roi, 
		NUM_RECTS * sizeof( XieRectangle ), sizeof( XieRectangle ), 
		0, True );

        XieFreePhotofloGraph(flograph, floSize);

	/* now, read back the data */

	ReadNotifyExportVector( display, xieInfo, readVec, 6, 60L );

	XieDestroyPhotoflo( display, readVec[ 0 ].flo_id );
	XieDestroyPhotoflo( display, readVec[ 1 ].flo_id );
	XieDestroyPhotoflo( display, readVec[ 3 ].flo_id );
	XieDestroyPhotoflo( display, readVec[ 5 ].flo_id );

	XCloseDisplay(display);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i image\n", pgm);
	exit(1);
}
