
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

/* This software is based in part on the work of the Independent JPEG Group */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "jinclude.h"

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

void   usage();
static XieExtensionInfo *xieInfo;

/* The following tables are taken directly from the IJG sample software, 
   and are identical to tables which are used as defaults in the X11R6 
   XIE-SI. However, a slight (and annoying) modification was required to 
   be made to the *bits tables (e.g. dc_luminance_bits) to be compatible
   with XIE. See below. */

/* Standard Huffman tables (cf. JPEG standard section K.3) */
/* IMPORTANT: these are only valid for 8-bit data precision! */

/* NOTE!!! The sum of the entries in *bits must equal the number of
   entries in *val. For example, by adding the individual entries in 
   dc_luminance_bits[] (12) yields the size of dc_luminance_val[] */

/* The following notes pertain to the X11R6 XIE-SI */

/* NOTE!!! The first table sent (e.g. the *bits tables) in each pair is 
   required to have 16 entries. The tables provided by the IJG software 
   have 17 entries, with the first entry always 0. XIE adds this entry 
   implicitly to the front of each *bits table, so we have to strip it 
   here. The original IJG tables are surrounded by USE_IJG_UNMODIFIED to 
   illustrate what change was required. Do not use these tables directly 
   as they are not compatible with XIE. */

/* NOTE!!! XIE requires the following order for the ac tables - luminance
   bits, luminance val, chrominance bits, chrominance val. Any other order
   will be incorrectly interpreted. The protocol says nothing about this. */

/* NOTE!!! XIE requires the following order for the dc tables - luminance
   bits, luminance val, chrominance bits, chrominance val. Any other order
   will be incorrectly interpreted. The protocol says nothing about this. */ 

#ifdef USE_IJG_UNMODIFIED	
static unsigned char dc_luminance_bits[17] =
{ /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
#else
static unsigned char dc_luminance_bits[16] =
{ /* 0-base  0, */ 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
#endif

static unsigned char dc_luminance_val[] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  
#ifdef USE_IJG_UNMODIFIED	
static unsigned char dc_chrominance_bits[17] =
{ /* 0-base */ 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
#else
static unsigned char dc_chrominance_bits[16] =
{ /* 0-base 0, */ 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
#endif

static unsigned char dc_chrominance_val[] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
  
#ifdef USE_IJG_UNMODIFIED	
static unsigned char ac_luminance_bits[17] =
{ /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
#else
static unsigned char ac_luminance_bits[16] =
{ /* 0-base 0, */ 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
#endif

static unsigned char ac_luminance_val[] =
{ 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
  0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
  0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
  0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
  0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
  0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
  0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
  0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
  0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
  0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa };
  
#ifdef USE_IJG_UNMODIFIED	
static unsigned char ac_chrominance_bits[17] =
{ /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
#else
static unsigned char ac_chrominance_bits[16] =
{ /* 0-base 0, */ 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
#endif

static unsigned char ac_chrominance_val[] =
{ 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
  0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
  0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
  0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
  0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
  0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
  0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
  0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
  0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
  0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa };

/* The IJG JPEG software uses the above tables as follows:

  add_huff_table(cinfo, &cinfo->dc_huff_tbl_ptrs[0],
                 dc_luminance_bits, dc_luminance_val);
  add_huff_table(cinfo, &cinfo->ac_huff_tbl_ptrs[0],
                 ac_luminance_bits, ac_luminance_val);
  add_huff_table(cinfo, &cinfo->dc_huff_tbl_ptrs[1],
                 dc_chrominance_bits, dc_chrominance_val);
  add_huff_table(cinfo, &cinfo->ac_huff_tbl_ptrs[1],
                 ac_chrominance_bits, ac_chrominance_val);
 
  This will be emulated below using XIElib interfaces if -a and/or -c
  arguments were specified by the user */ 

main(argc, argv)
int  argc;
char *argv[];
{
	Backend 	*backend;
	XieDecodeUncompressedTripleParam *decodeParms;
	XieEncodeJPEGBaselineParam *encodeParms;
	char		*bytes;
	Display        	*display;
	XEvent          event;
	Window          win = (Window) NULL;
	Bool            notify;
	XieConstant     bias;
	XiePhotoElement *flograph;
	XiePhotospace   photospace;
	GC 		gc;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XieRGBToYCbCrParam *convertFromRGBParms;
	int             fd, flag, size, done, idx, src, floSize, floId;
	int		screen, beSize;
	unsigned char 	stride[ 3 ], leftPad[ 3 ], scanlinePad[ 3 ];
	XieLTriplet     width, height, levels;
	char		*getenv(), *display_name = getenv("DISPLAY");
	char		*img_file = NULL, *outfile = NULL;
	char		*qTable, *acTable, *dcTable;
	unsigned int	qSize, acSize, dcSize;
	unsigned char	horizontalSamples[ 3 ] = { 1, 1, 1 }, 
			verticalSamples[ 3 ] = { 1, 1, 1 };
	short		quality = 75;
	Bool		useDefaultQ = False, 
			useDefaultAC = False,
			useDefaultDC = False,
			show = False;	
	double		lumaRed, lumaGreen, lumaBlue;
	Visual		*visual;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:i:o:q:h:v:sfac"))!=EOF) {
		switch(flag) {

		case 'a':	useDefaultAC = True;
				break;

		case 'c':	useDefaultDC = True;
				break;

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		case 'i':	img_file = optarg;	
				break;
	
		case 'o':	outfile = optarg;	
				break;
	
		case 'q':	quality = atoi(optarg);	
				break;

		case 'h':
				horizontalSamples[ 1 ] =
				horizontalSamples[ 2 ]  = atoi(optarg);	
				break;

		case 'v':	
				verticalSamples[ 1 ] =
				verticalSamples[ 2 ] = atoi(optarg);	
				break;

		case 's':	show = True;
				break;

		case 'f':	useDefaultQ = True;
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

	if ( outfile == ( char * ) NULL )
	{
		printf( "Output file not defined\n" );
		usage( argv[ 0 ] );
	}

	if ( !ReadPPM( img_file, &width, &height, &levels, &bytes, &size ) )
	{
		printf( "Problem getting PPM data from %s\n", img_file );
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

	if ( show == True )
	{
		win = XCreateSimpleWindow(display, DefaultRootWindow(display),
			0, 0, width[0], height[ 0 ], 1, 0, 255 );
		XSelectInput( display, win, ExposureMask );
		XMapRaised(display, win);
		WaitForWindow( display, win );
		gc = XCreateGC(display, win, 0L, (XGCValues *) NULL);
		XSetForeground( display, gc, XBlackPixel( display, 0 ) );
		XSetBackground( display, gc, XWhitePixel( display, 0 ) );

		screen = DefaultScreen( display );
        	visual = DefaultVisual( display, screen );

        	backend = (Backend *) InitBackend( display, screen, 
			visual->class, xieValTripleBand, 0, -1, &beSize );

		if ( backend == (Backend *) NULL )
		{
			fprintf( stderr, "Unable to create backend\n" );
			exit( 1 );
		}
	}

	/* Now for the XIE stuff */

	/* Create and populate a photoflo graph */

	floSize = 3;
	if ( show == True )
		floSize += beSize;

	flograph = XieAllocatePhotofloGraph(floSize);

	stride[ 0 ] = stride[ 1 ] = stride[ 2 ] = 24;
	leftPad[ 0 ] = leftPad[ 1 ] = leftPad[ 2 ] = 0;
	scanlinePad[ 0 ] = scanlinePad[ 1 ] = scanlinePad[ 2 ] = 0;

	decodeParms = XieTecDecodeUncompressedTriple( 
		xieValLSFirst,
		xieValLSFirst,
		xieValLSFirst,
		xieValBandByPixel,
		stride,
		leftPad,
		scanlinePad
	);

	idx = 0;
	notify = False;
	XieFloImportClientPhoto(&flograph[idx], 
		xieValTripleBand,
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify event */ 
		xieValDecodeUncompressedTriple,	/* decode technique */
		(XiePointer) decodeParms /* decode parameters */
	);
	idx++;
	src = idx;

	if ( show == True )
	{	
		if ( !InsertBackend( backend, display, win, 0, 0, gc, 
			flograph, idx ) )
		{
			fprintf( stderr, "Backend failed\n" );
			exit( 1 );
		}

		idx += beSize;
	}

        lumaRed = 0.2125; 		/* CCIR coefficients */
        lumaGreen = 0.7154; 
        lumaBlue = 0.0721;
	bias[ 0 ] = 0.0; bias[ 1 ] = 127.0; bias[ 2 ] = 127.0;

	convertFromRGBParms = XieTecRGBToYCbCr(
		levels,
		lumaRed,
		lumaGreen,
		lumaBlue,
		bias
	);

	XieFloConvertFromRGB(&flograph[idx],
		src,
		xieValRGBToYCbCr,
		(XiePointer) convertFromRGBParms
	);
	idx++;

	if ( useDefaultQ == False )
	{
		/* use the IJG software to generate q tables based 
		   upon the specified quality */

		struct Compress_info_struct cinfo;
		struct Compress_methods_struct c_methods;
  		struct External_methods_struct e_methods;
		int	i;

  		/* Initialize the system-dependent method pointers. */

  		cinfo.methods = &c_methods;
  		cinfo.emethods = &e_methods;

		cinfo.emethods->alloc_small = malloc;
		for ( i = 0; i < NUM_QUANT_TBLS; i++ )
			cinfo.quant_tbl_ptrs[i] = NULL;

		/* Specifying True restricts to range [0,255] */

		j_set_quality( &cinfo, quality, True );
		qTable = malloc( DCTSIZE2 * 2 * sizeof( unsigned char ) );

		/* IJG tables contains shorts. XIE wants chars... */

		for ( i = 0; i < DCTSIZE2; i++ )
			qTable[ i ] = (unsigned char)
				*(cinfo.quant_tbl_ptrs[0] + i);
		for ( i = DCTSIZE2; i < DCTSIZE2 * 2; i++ )
			qTable[ i ] = (unsigned char) 
				*(cinfo.quant_tbl_ptrs[1] + (i - DCTSIZE2));
		qSize = 2 * DCTSIZE2;

		/* at this point, you may want to make sure there are no
		   zero valued entries - XIE can't handle them and your 
	           server will crash... */

		for ( i = 0; i < DCTSIZE2 * 2; i++ )
		{
			if ( qTable[ i ] == 0 )
			{
				/* revert to XIE default table */

				printf( "Zero value detected in Q table.\n" );
				printf( "Using XIE default Q tables\n" );
				free( qTable );
				qTable = (char *) NULL;
				qSize = 0;
				break;
			}
		}
	}
	else
	{
		qTable = (char *) NULL;
		qSize = 0;
	}

	if ( useDefaultAC == False )
	{
		char	*ptr;

		acSize = sizeof( ac_luminance_bits ) +
                        sizeof( ac_luminance_val ) +
                        sizeof( ac_chrominance_bits ) +
                        sizeof( ac_chrominance_val );

		acTable = malloc( acSize ); 

		ptr = acTable;
		if ( ptr == (char *) NULL )
		{
			printf( "Unable to allocate acTable\n" );
			acSize = 0;
		} 
		else
		{
		memcpy( ptr, ac_luminance_bits, sizeof( ac_luminance_bits ) );
		ptr+=sizeof( ac_luminance_bits );

		memcpy( ptr, ac_luminance_val, sizeof( ac_luminance_val ) );
		ptr+=sizeof( ac_luminance_val );

		memcpy( ptr, ac_chrominance_bits, sizeof( ac_chrominance_bits ) );
		ptr+=sizeof( ac_chrominance_bits );

		memcpy( ptr, ac_chrominance_val, sizeof( ac_chrominance_val ) );
		}
	}
	else
	{
		acTable = (char *) NULL;
		acSize = 0;
	}

	if ( useDefaultDC == False )
	{
		char	*ptr;

		dcSize = sizeof( dc_luminance_bits ) +
                        sizeof( dc_luminance_val ) +
                        sizeof( dc_chrominance_bits ) +
                        sizeof( dc_chrominance_val );

		dcTable = malloc( dcSize ); 

		ptr = dcTable;
		if ( ptr == (char *) NULL )
		{
			printf( "Unable to allocate dcTable\n" );
			dcSize = 0;
		} 
		else
		{
		memcpy( ptr, dc_luminance_bits, sizeof( dc_luminance_bits ) );
		ptr+=sizeof( dc_luminance_bits );

		memcpy( ptr, dc_luminance_val, sizeof( dc_luminance_val ) );
		ptr+=sizeof( dc_luminance_val );

		memcpy( ptr, dc_chrominance_bits, sizeof( dc_chrominance_bits ) );
		ptr+=sizeof( dc_chrominance_bits );

		memcpy( ptr, dc_chrominance_val, sizeof( dc_chrominance_val ) );
		}
	}
	else
	{
		dcTable = (char *)NULL;
		dcSize = 0;
	}

	encodeParms = XieTecEncodeJPEGBaseline(
		xieValBandByPixel,
		xieValLSFirst,
		horizontalSamples,
		verticalSamples,
		qTable,
		qSize,
		acTable,
		acSize,
		dcTable,
		dcSize
	);

	notify = True;
	XieFloExportClientPhoto(&flograph[idx],
		idx,
		notify,
		xieValEncodeJPEGBaseline,
		(XiePointer) encodeParms	
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

	/* free the buffer for re-use below */

        free( bytes );
        bytes = (char *) NULL;

        /* next, read the newly encoded image back from ExportClientPhoto */

        size = ReadNotifyExportData( display, xieInfo, photospace, floId, idx, 
		1, 100, 0, &bytes );

	fd = open( outfile, O_RDWR | O_TRUNC | O_CREAT );
	if ( fd < 0 )
		fprintf( stderr, "Unable to open '%s'\n", outfile );
	else
	{
		write( fd, bytes, size );
		close( fd );
	}

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

	/* wait for a button or key press before exiting */

	if ( win )
	{
		XSelectInput(display, win, ButtonPressMask | KeyPressMask |
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
	}

	/* free up what we allocated */

	if ( show == True )
		CloseBackend( backend, display );

	if ( decodeParms )
		XFree( decodeParms );
	if ( convertFromRGBParms )
		XFree( convertFromRGBParms );
	if ( encodeParms )
		XieFreeEncodeJPEGBaseline( encodeParms );
	if ( qTable )
		free( qTable );	
	if ( acTable )
		free( acTable );	
	if ( dcTable )
		free( dcTable );	
	free( bytes );

	XieFreePhotofloGraph(flograph, floSize);
	XieDestroyPhotospace(display, photospace);
	XCloseDisplay(display);
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display] -i image -o output [h horizontal sample] [v vertical sample] [-q quality] [-s] [-f] [-a] [-c]\n", pgm);
	printf("-s display image in a window\n");
	printf("-q quality factor in range [0,100]\n" );
	printf("-f use quantization tables provided by XIE (ignoring -q)\n" );
	printf("-a use ac tables provided by XIE\n" );
	printf("-c use dc tables provided by XIE\n" );
	exit(1);
}
