#include	<stdio.h>
#include	<Xlib.h>

extern Colormap 
AllocateGrayMapColors(Display *display, int screen, Window window, int depth);

extern int 
ReadPGM( char *file, int *width, int *height, int *maxGray, char **data, 
	int *size );

extern void
RemapIntensitiesToCells( Display *display, Colormap cmap, char *data, 
	int size );

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
        int     width;         	/* on return, the width of the image */
        int     height;        	/* on return, the height of the image */
        char    *data = NULL;   /* on return, the image data stripped of 
				   its headers */
	char	*data2;		/* a copy of the image after speckling */
        int     size;          	/* on return, the number of bytes of data */
        int     maxGray;	/* maximum gray level in image */
	char	*display = NULL;
	Window	myWindow, myWindow2;	
	XImage	*myImage;
	Display	*myDisplay;
	Visual	*myVisual;
	GC	myGC;
	int	myScreen;
	Boolean	shareCmap = False, doMedian = False;
	int	depth, format, bitmapPad, bytesPerLine;
        char    *img_file = (char *) NULL;
	char	*kernel_file = (char *) NULL; 
	XEvent	event;
	int	done, flag, idx, median, repeat = 1;
	Colormap graycmap;

        while ((flag=getopt(argc,argv,"?sm:d:i:r:"))!=EOF) {
                switch(flag) {

                case 'd':       display = optarg; 
                                break;

                case 'i':       img_file = optarg;      
                                break;

                case 'm':       doMedian = True;      
				median = atoi( optarg );
				if ( median < 0 || median > 8 )
					median = 3;
                                break;
		
		case 'r':	repeat = atoi( optarg );
				break;

		case 's':	shareCmap = True;
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

        data = ( char * ) NULL;
        if ( !ReadPGM( img_file, &width, &height, &maxGray, &data, &size ) )
	{
		printf( "Was unable to read PGM data from '%s'", img_file );
		exit( 1 ); 
	}

	if ( doMedian == True )
	{
		AddShotNoise( data, size, 0x00, 0.0125 );
		AddShotNoise( data, size, 0xff, 0.0125 );
	}

	myWindow = XCreateSimpleWindow( myDisplay, 
		DefaultRootWindow( myDisplay ), 0, 0, width, height, 
		0, 0, WhitePixel( myDisplay, myScreen ) );

	myGC = XCreateGC( myDisplay, myWindow, 0L, NULL );

	depth = DefaultDepth( myDisplay, myScreen );

	if ( shareCmap == False )
		graycmap = AllocateGrayMapColors( myDisplay, myScreen, 
			myWindow, depth );
	else
	{
		graycmap = XDefaultColormap( myDisplay, myScreen );
		RemapIntensitiesToCells( myDisplay, graycmap, data, size );
	}

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
	bytesPerLine = width;	

	myImage = XCreateImage( myDisplay, myVisual, depth, format, 0, 
		data, width, height, bitmapPad, bytesPerLine );

	if ( myImage == (XImage *) NULL )
	{
		printf( "Unable to create XImage\n" );
                free( data );
		exit( 1 );
	}

	XPutImage( myDisplay, myWindow, myGC, myImage, 0, 0, 0, 0,
		width, height );

	data2 = (char *) malloc( size );
	memcpy( data2, data, size );

	if ( doMedian == True )
	{
		myWindow2 = XCreateSimpleWindow( myDisplay, 
			DefaultRootWindow( myDisplay ), 0, 0, width, height, 
			0, 0, WhitePixel( myDisplay, myScreen ) );

		if ( shareCmap == False )
			XSetWindowColormap(myDisplay, myWindow, graycmap);

		XMapRaised( myDisplay, myWindow2 );

		done = 0;
		XSelectInput( myDisplay, myWindow2, ExposureMask );
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

		for ( idx = 0; idx < repeat; idx++ )
			MedianFilter( data2, median, width, height );

		myImage->data = data2;
		XPutImage( myDisplay, myWindow2, myGC, myImage, 0, 0, 0, 0,
			width, height );
	}

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
	free( data2 );
	XCloseDisplay( myDisplay );
	exit( 0 );
}

void
usage( pgm )
char *pgm;
{
        printf("usage: %s [-s] [-m] [-d display] [-r repeat] -i image\n", pgm );
        exit(1);
}

