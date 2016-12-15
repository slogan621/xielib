#include	<stdio.h>
#include	<X11/extensions/XIElib.h>
#include	<stdlib.h>

#include "events.h"

extern int 
ReadPGM( char *file, int *width, int *height, int *maxGray, char **data, 
	int *size );

extern int 
ReadPPM( char *file, XieLTriplet width, XieLTriplet height, 
	XieLTriplet levels, char **data, int *size );


typedef char Boolean;

#define False 0
#define True 1

extern int optind;
extern int opterr;
extern char *optarg;

void usage( char *name );

main( argc, argv )
int     argc;
char    *argv[];
{
        int     w;         	/* on return, the width of the image */
        int     h;        	/* on return, the height of the image */
        char    *data = NULL;   /* on return, the image data stripped of 
				   its headers */
        int     size;          	/* on return, the number of bytes of data */
        int     maxGray;	/* maximum gray level in image */
	XieLTriplet width, height, levels;		
	char	*display = NULL;
	Window	myWindow;
	XImage	*myImage;
	Display	*myDisplay;
	Visual	*myVisual;
	GC	myGC;
	int	myScreen;
	int	depth, format, bitmapPad, bytesPerLine;
        char    *img_file = (char *) NULL;
	XEvent	event;
	Boolean	isTriple = False;	/* Assume image is SingleBand */
	int	done, flag, idx;
	float	scale = 1.0;

        while ((flag=getopt(argc,argv,"?d:i:s:"))!=EOF) {
                switch(flag) {

                case 'd':       display = optarg; 
                                break;

                case 'i':       img_file = optarg;      
                                break;

		case 's':	scale = atof( optarg );
				break;

                default:        printf("unrecognized flag (-%c)\n",flag);
                                usage(argv[0]);
                                break;
                }
        }

        if ( img_file == ( char * ) NULL )
        {
                printf( "Image file not defined\n" );
                usage( argv[ 0 ] );
        }

	myDisplay = XOpenDisplay( display );
	if ( !myDisplay )
	{
		printf( "Unable to open display\n" );
		exit( 1 );
	}
	myScreen = DefaultScreen( myDisplay );
	myVisual = XDefaultVisual( myDisplay, myScreen );

	/* Try PGM first. If not, then try PPM. If that fails, give up */

        data = ( char * ) NULL;
        if ( !ReadPGM(img_file, &w, &h, &maxGray, &data, &size) )
	{
		if ( !ReadPPM(img_file, width, height, levels, &data, &size) )
		{
			printf( "Unable to read image data from '%s'\n", 
				img_file );
			exit( 1 ); 
		}

		/* levels tells us how many levels per band. For example,
		   if the image is 24-bit with 8-bits of red, green, and
		   blue, then the levels will be 256, 256, 256. This is
		   passed to XIE and follows the image as it passes thru
		   the photoflo. It can be changed for example by elements
	           such as Dither, as we possibly do to this image below. */

		if ( levels[ 0 ] != levels[ 1 ] || levels[ 1 ] != levels[ 2 ] )
		{
			printf( "PPM image must have equal per-band depth\n" );
			exit( 1 );
		}
		if ( width[ 0 ] != width[ 1 ] || width[ 1 ] != width[ 2 ] )
		{
			printf( "PPM image must have equal per-band width\n" );
			exit( 1 );
		}
		if ( height[ 0 ] != height[ 1 ] || height[ 1 ] != height[ 2 ] )
		{
			printf( "PPM image must have equal per-band height\n" );
			exit( 1 );
		}

		/* seems o.k. */

		w = (int) width[ 0 ];
		h = (int) height[ 0 ];

		/* We have a TripleBand (color) image. We use this flag 
	           (isTriple) later so we can tell XIE the type of image
		   it is decoding (e.g. the technique is Uncompressed
		   Single or UncompressedTriple ) */

		isTriple = True;	
	}
	else
	{
		/* For SingleBand (grayscale) images, levels[1] and
		   levels[2] are ignored */

		levels[ 0 ] = maxGray;
		levels[ 1 ] = levels[ 2 ] = 0; 	
	}

	myWindow = XCreateSimpleWindow( myDisplay, 
		DefaultRootWindow( myDisplay ), 0, 0, w, h, 
		0, 0, WhitePixel( myDisplay, myScreen ) );

	myGC = XCreateGC( myDisplay, myWindow, 0L, NULL );

	depth = DefaultDepth( myDisplay, myScreen );

	XSelectInput( myDisplay, myWindow, ExposureMask );
	XMapRaised( myDisplay, myWindow );

	done = 0;
	while ( done == 0 )
	{
		XNextEvent( myDisplay, &event );
		switch( event.type ) {
			case Expose:
				if ( event.xexpose.count == 0 )
					done = 1;
				break;
		}
	}

	if ( depth == 1 )
	{
		format = XYPixmap;
		bitmapPad = 32; 
	}
	else
	{
		format = ZPixmap;
		bitmapPad = ( depth > 8 ? 32 : 8 ); 
	}
	bytesPerLine = (isTriple ? w * 3 : w);	

	myImage = XCreateImage( myDisplay, myVisual, depth, format, 0, 
		data, w, h, bitmapPad, bytesPerLine );

	if ( myImage == (XImage *) NULL )
	{
		printf( "Unable to create XImage\n" );
                free( data );
		exit( 1 );
	}

	XiePutScaledImage( myDisplay, myWindow, myGC, myImage, 0, 0, 0, 0,
		w, h, levels, depth, isTriple, scale );

	done = 0;
	XSelectInput( myDisplay, myWindow, ButtonPressMask );
	while ( done == 0 )
	{
		XNextEvent( myDisplay, &event );
		switch( event.type ) {
			case MappingNotify:
				XRefreshKeyboardMapping( (XMappingEvent*) &event );
				break;
			case ButtonPress:
				done = 1;
				break;
		}
	}

	free( data );
	XCloseDisplay( myDisplay );
	exit( 0 );
}

void
usage( pgm )
char *pgm;
{
        printf("usage: %s [-d display] [-s scale] -i image\n", pgm );
        exit(1);
}

int
XiePutScaledImage( display, drawable, gc, image, src_x, src_y, dst_x,
	dst_y, w, h, levels, depth, isTriple, scale )
Display 	*display;
Drawable 	drawable;
GC 		gc;
XImage 		*image;
int 		src_x;
int 		src_y;
int 		dst_x;
int 		dst_y;
unsigned int 	w;
unsigned int 	h;
XieLTriplet	levels;
int 		depth;
Boolean 	isTriple;
float 		scale;
{
	XieExtensionInfo 	*xieInfo;
	XiePhotofloGraph 	flograph;
	int			idx, floSize, floId, decodeTech;
	XiePhotospace		photospace;
	XiePointer		decodeParms;	
       	unsigned char   	stride[ 3 ],
                        	leftPad[ 3 ],
                        	scanlinePad[ 3 ];
	XieLTriplet		width, height;
	Visual			*visual;
	XieColorList		clist;
	float			coeffs[ 6 ];
        XieColorAllocAllParam 	*colorParm;
	Boolean			notify;
	unsigned int		size;
	char			*bytes;
	XieConstant		constant;
	XEvent			event;
        XIEEventCheck   	eventData;      /* passed to WaitForXIEEvent */

	if ( scale == 1.0 )
	{
		/* Just send it out. Notice the colors are wrong, 
		   however */

		XPutImage( display, drawable, gc, image, src_x, src_y,
			dst_x, dst_y, w, h ); 
		return( 1 );
	}

    	if (!XieInitialize(display, &xieInfo)) {
        	printf( "XIE not supported on this display!\n" );
		return( 0 );
    	}

	floSize = 4;

	if ( isTriple )
	{
		/* see if we need to dither */	

		if ( levels[ 0 ] + levels[ 1 ] + levels[ 2 ] > (1 << depth) )
			floSize++;

		decodeTech = xieValDecodeUncompressedTriple;

                stride[ 0 ] = stride[ 1 ] = stride[ 2 ] = depth * 3;
                leftPad[ 0 ] = leftPad[ 1 ] = leftPad[ 2 ] = 0;
                scanlinePad[ 0 ] = scanlinePad[ 1 ] = scanlinePad[ 2 ] = 0;

                decodeParms = (XiePointer) XieTecDecodeUncompressedTriple( 
                        ( image->bitmap_bit_order == LSBFirst ?
				xieValLSFirst : xieValMSFirst ),
                        ( image->byte_order == LSBFirst ? xieValLSFirst :
				xieValMSFirst ),
                        xieValLSFirst,
                        xieValBandByPixel,
                        stride,                 /* stride */
                        leftPad,                /* left pad */
                        scanlinePad             /* scanline pad */
                );
        }
        else
        {
		/* see if we need to dither */	

		if ( levels[ 0 ] > (1 << depth) )
			floSize++;

                decodeTech = xieValDecodeUncompressedSingle;

                decodeParms = (XiePointer) XieTecDecodeUncompressedSingle( 
                        ( image->bitmap_bit_order == LSBFirst ?
                                xieValLSFirst : xieValMSFirst ),
                        ( image->byte_order == LSBFirst ? xieValLSFirst :
                                xieValMSFirst ),
                        depth,         	/* stride */
                        0,              /* left pad */
                        0               /* scanline pad */
                );
        }

        flograph = XieAllocatePhotofloGraph(floSize);

	/* The photoflos used here are simple...

	   ICP -> GEO -> Dither -> CToI -> ED	

	   Variations include replacing ED with ExportDrawablePlane if
	   the image is XYBitmap, or replacing CToI with a backend more 
	   appropriate for the visual encountered by the client. If
	   the image is TripleBand, we dither so that the per-band
	   levels are approximately the cubed root of 1 << depth of the
	   display. Thus, if the display is 8 bit, we dither to cbrt(256)
	   = 6 (6*6*6 = 216 actually), giving red, green, and blue equal
           parts of the pixel. */ 

	idx = 0;
        notify = False;
	width[ 0 ] = width[ 1 ] = width[ 2 ] = w;
	height[ 0 ] = height[ 1 ] = height[ 2 ] = h;

        XieFloImportClientPhoto(&flograph[idx], 
                (isTriple == True ? xieValTripleBand : xieValSingleBand),
                width,                  /* width of each band */
                height,                 /* height of each band */
                levels,                 /* levels of each band */
                notify,                 /* no DecodeNotify events */ 
                decodeTech,             /* decode technique */
                (char *) decodeParms    /* decode parameters */
        );
        idx++;

	coeffs[ 0 ] = scale;
	coeffs[ 1 ] = 0.0; 
	coeffs[ 2 ] = 0.0; 
	coeffs[ 3 ] = scale;
	coeffs[ 4 ] = 0.0; 
	coeffs[ 5 ] = 0.0; 

        XieFloGeometry(&flograph[idx],
                idx,                    /* image source */
                w,                      /* width of resulting image */
                h,                      /* height of resulting image */
                coeffs,                 /* a, b, c, d, e, f */
                constant,               /* used if src pixel does not exist */ 
                0x7,               	/* ignored for SingleBand images */
                xieValGeomAntialias,    /* XXX hard code technique */ 
                (XiePointer) NULL       /* and parameters */
        );
        idx++;

	if ( floSize == 5 )
	{
	       	visual = DefaultVisual( display, DefaultScreen( display ) );
		GetLevelsFromVisual( visual, levels );
	       	XieFloDither( &flograph[idx],
			idx,
                        0x7,
                        levels,
                        xieValDitherErrorDiffusion,
                        ( char * ) NULL
                );
                idx++;
	}

	clist = XieCreateColorList( display );
        colorParm = XieTecColorAllocAll( 128 ); /* pick a number, any 
						   number :-) */

	XieFloConvertToIndex( &flograph[idx],
		idx,                            /* source element */
		DefaultColormap(display, 
			DefaultScreen(display)),/* colormap to alloc */
		clist,                    	/* colorlist */
		True,                           /* notify if problems */
		xieValColorAllocAll,            /* color alloc tech */
		( char * ) colorParm            /* technique parms */
	);
	idx++;

	XieFloExportDrawable(&flograph[idx], 
		idx,            /* source */
		drawable,      	/* drawable to send data to */
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

	size = image->bytes_per_line * image->height;
	bytes = image->data;

        PumpTheClientData( display, floId, photospace, 1, bytes, size, 
                sizeof( char ), 0, True );

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

        /* free up what we allocated */

        XFree( colorParm );
        XieDestroyColorList( display, clist );
        XieFreePhotofloGraph(flograph, floSize);
        XieDestroyPhotospace(display, photospace);
        XFree(decodeParms);
}

int
GetLevelsFromVisual( visual, levels )
Visual		*visual;
XieLevels 	levels;
{
        unsigned long red_mask, green_mask, blue_mask;
        int     rbits, gbits, bbits;
        unsigned long mask;

        if ( visual->class == StaticColor || 
		visual->class == TrueColor || visual->class == DirectColor )
        {
                red_mask = visual->red_mask;
                green_mask = visual->green_mask;
                blue_mask = visual->blue_mask;

                mask = 1L << ( ( sizeof( long ) << 3 ) - 1 );
                rbits = gbits = bbits = 0;

                while ( mask )
                {
                        if ( mask & red_mask ) rbits++;
                        else if ( mask & green_mask ) gbits++;
                        else if ( mask & blue_mask ) bbits++;
        
                        mask = mask >> 1;
                }
        }
        else
        {
                levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 
                        icbrt( visual->map_entries );
        }
}

/*
 * integer cube roots by Newton's method
 *
 * Stephen Gildea, MIT X Consortium, July 1991
 */

/* Newton's Method:  x_n+1 = x_n - ( f(x_n) / f'(x_n) ) */

/* for cube roots, x^3 - a = 0,  x_new = x - 1/3 (x - a/x^2) */

/*
 * Quick and dirty cube roots.  Nothing fancy here, just Newton's method.
 * Only works for positive integers (since that's all we need).
 * We actually return floor(cbrt(a)) because that's what we need here, too.
 */

static int icbrt_with_guess(a, guess)
    int a, guess;
{
    register int delta;

    if (a <= 0)
        return 0;
    if (guess < 1)
        guess = 1;

    do {
        delta = (guess - a/(guess*guess))/3;
        guess -= delta;
    } while (delta != 0);

    if (guess*guess*guess > a)
        guess--;

    return guess;
}

static int icbrt_with_bits(a, bits)
    int a;
    int bits;                   /* log 2 of a */
{
    return icbrt_with_guess(a, a>>2*bits/3);
}

int icbrt(a)            /* integer cube root */
    int a;
{
    register int bits = 0;
    register unsigned n = a;

    while (n)
    {
        bits++;
        n >>= 1;
    }
    return icbrt_with_bits(a, bits);
}

