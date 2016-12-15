
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
#include <X11/extensions/XIElib.h>
#include <Xlib.h>
#include <Xatom.h>
#include <Xutil.h>

#include "backend.h"

#define MIN( a, b ) ( a < b ? a : b )

Bool
RootHasStandardColormap( display, screen, property )
Display	*display;
int	screen;
Atom	property;
{
	int	count;
	XStandardColormap stdcmap;

	if ( !XGetStandardColormap(display, RootWindow( display, screen ), 
		&stdcmap, property)) 
		return( False );
	return( True );
}

Bool
RootHasAnyStandardColormaps( display, screen )
Display	*display;
int	screen;
{
	Atom	i;
	Bool	found = False;

	for ( i = XA_RGB_BEST_MAP; i <= XA_RGB_RED_MAP && found == False; i++ )
		found = RootHasStandardColormap( display, screen, i );
	return( found );
}	

void
GetStdCmapLevels( stdCmap, levels )
XStandardColormap *stdCmap;
XieLevels	levels;
{
#if 0
	levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 
		MIN( MIN( stdCmap->red_max, stdCmap->green_max ), 
			stdCmap->blue_max ) + 1;
#else
	levels[ 0 ] = stdCmap->red_max + 1;
	levels[ 1 ] = stdCmap->green_max + 1;
	levels[ 2 ] = stdCmap->blue_max + 1; 
#endif
}

void
GetVisualLevels( backend, levels )
Backend		*backend;
XieLevels	levels;		/* on output, number of levels per-band */
{
	unsigned long red_mask, green_mask, blue_mask;
	int	rbits, gbits, bbits;
	unsigned long mask;
	Visual	*visual;	/* visual of interest */

	visual = backend->visual;	

	if ( visual->class == StaticColor || visual->class == TrueColor || visual->class == DirectColor ) {
		red_mask = visual->red_mask;
		green_mask = visual->green_mask;
		blue_mask = visual->blue_mask;

		mask = 1L << ( ( sizeof( long ) << 3 ) - 1 );
		rbits = gbits = bbits = 0;

		while ( mask ) {
			if ( mask & red_mask ) rbits++;
			else if ( mask & green_mask ) gbits++;
			else if ( mask & blue_mask ) bbits++;
	
			mask = mask >> 1;
		}
#if 0
        	levels[ 0 ] = levels[ 1 ] = levels[ 2 ] =
                	MIN( MIN( 1 << rbits, 1 << gbits ), 1 << bbits );
#else
		levels[ 0 ] = 1 << rbits;
		levels[ 1 ] = 1 << gbits;
		levels[ 2 ] = 1 << bbits;
#endif
	}
	else {
		levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 
			icbrt( backend->colormapSize );
	}
}

static int
MasksEqual( backend )
Backend         *backend;
{
	XieLevels       levels;         
	int		retval = 0;

	if ( backend->depth > 8 && backend->stdCmapClass != -1 ) 
		GetStdCmapLevels( &backend->stdCmap, levels );
	else
		GetVisualLevels( backend, levels );
	if ( levels[ 0 ] &&  
		( levels[ 0 ] == levels[ 1 ] && levels[ 1 ] == levels[ 2 ] ) )
		retval = 1;
	return( retval );
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

typedef struct 
{
	int	class;		/* visual class */
	int	type;		/* SingleBand or TripleBand */
	int	(*getSize)();	/* number of elements */
	int	(*addBE)();	/* add backend elements */
} BackendPrivate;

static int SizeSGSB(), SizeSGTB(), SizeSCSB(), SizeSCTB(), SizeTCSB(), 
	SizeTCTB(), SizeGSSB(), SizeGSTB(), SizePCSB(), SizePCTB(), 
	SizeDCSB(), SizeDCTB(); 

static int AddSGSB(), AddSCSB(), AddTCSB(), AddGSSB(), AddPCSB(), AddDCSB(),
	AddSGTB(), AddSCTB(), AddTCTB(), AddGSTB(), AddPCTB(), AddDCTB();

static BackendPrivate backEndTab[] = {
{ StaticGray, 	xieValSingleBand, SizeSGSB, AddSGSB }, 
{ StaticGray, 	xieValTripleBand, SizeSGTB, AddSGTB }, 
{ StaticColor, 	xieValSingleBand, SizeSCSB, AddSCSB }, 
{ StaticColor, 	xieValTripleBand, SizeSCTB, AddSCTB }, 
{ TrueColor, 	xieValSingleBand, SizeSCSB, AddSCSB }, 
{ TrueColor, 	xieValTripleBand, SizeSCTB, AddSCTB }, 
{ GrayScale, 	xieValSingleBand, SizeGSSB, AddGSSB }, 
{ GrayScale, 	xieValTripleBand, SizeGSTB, AddGSTB }, 
{ PseudoColor, 	xieValSingleBand, SizePCSB, AddPCSB }, 
{ PseudoColor, 	xieValTripleBand, SizePCTB, AddPCTB }, 
{ DirectColor, 	xieValSingleBand, SizeDCSB, AddDCSB }, 
{ DirectColor, 	xieValTripleBand, SizeDCTB, AddDCTB } 
};

static BackendPrivate *
FindBackendPrivate( class, type )
int	class;
int	type;
{
	int	i;
	BackendPrivate	*ptr = (BackendPrivate *) NULL;

	for ( i = 0; i < sizeof( backEndTab ) / sizeof( BackendPrivate ); i++ ) 
		if ( backEndTab[ i ].class == class && 
			backEndTab[ i ].type == type )
		{
			ptr = &backEndTab[ i ];
			break;
		}
	return( ptr );
}
 
static int
FindBackendSize( ptr )
Backend *ptr;
{
	BackendPrivate *priv;

	/* arguments validated by caller */

	priv = FindBackendPrivate( ptr->visual->class, ptr->type );
	if ( priv && priv->getSize )
		return( ( priv->getSize ) ( ptr ) );
	else
		return( 0 );
}

Backend *
InitBackend( display, screen, class, type, levels, stdCmap, size )
Display	*display;		/* connection to an X server */
int	screen;			/* screen on the connection */
int	class;			/* -1, or PseudoColor, etc */
int	type;			/* SingleBand or TripleBand */
int	levels;			/* if SingleBand, number of 
				   levels in image */
int	stdCmap;		/* -1, or RGB_BEST_MAP, etc */
int	*size;			/* on return, how many elements
				   are needed for backend */
{
	int		depth;
	Backend		*ptr;
	Status 		result;
	XVisualInfo 	vinfo;
        BackendPrivate 	*priv;
	Visual 		*defVisual;
	XStandardColormap *pStdCmap;
	Bool		needCustomCmap = False;

	/* check for bogus StandardColormap request */

	if ( stdCmap != -1 && 
		(stdCmap <  XA_RGB_BEST_MAP || stdCmap >  XA_RGB_RED_MAP) )
		return((Backend *) NULL);

/* Following comments snarf'd from lib/Xmu/VisCmap.c: 
 *
 * Not all standard colormaps are meaningful to all visual classes.  This
 * routine will check and define the following properties for the following
 * classes, provided that the size of the colormap is not too small.
 *
 *      DirectColor and PseudoColor
 *          RGB_DEFAULT_MAP
 *          RGB_BEST_MAP
 *          RGB_RED_MAP
 *          RGB_GREEN_MAP
 *          RGB_BLUE_MAP
 *          RGB_GRAY_MAP
 *
 *      TrueColor and StaticColor
 *          RGB_BEST_MAP
 *
 *      GrayScale and StaticGray
 *          RGB_GRAY_MAP
 */

	if ( stdCmap != -1 ) {
		if ( ( class == TrueColor || class == StaticColor ) &&
			 stdCmap != XA_RGB_BEST_MAP ) {
			fprintf( stderr, 
				"TrueColor and StaticColor only support RGB_BEST_MAP\n" );
			return( (Backend *) NULL );
		}

		if ( ( class == GrayScale || class == StaticGray ) && 
			stdCmap != XA_RGB_GRAY_MAP ) {
			fprintf( stderr, 
				"GrayScale and StaticGray only support RGB_GRAY_MAP\n" );
			return( (Backend *) NULL );
		}
	}

	ptr = (Backend *) malloc( sizeof( Backend ) );
	if ( ptr == (Backend *) NULL )
		return( ptr );

	ptr->can = (struct trash *) NULL;

	ptr->stdCmapClass = stdCmap;

	defVisual = DefaultVisual( display, screen );

	result = XMatchVisualInfo( display, screen, 
		DefaultDepth( display, screen ), class, &vinfo );

	if ( !result ) {
		free( ptr );
		return( (Backend *) NULL );
	}

	ptr->visual = vinfo.visual;

	if ( stdCmap == -1 && ptr->visual->class != defVisual->class ) 
		needCustomCmap = True;

	depth = ptr->depth = vinfo.depth;

	/*  XXX force standard colormap usage if depth > 8 */

	if ( depth > 8 && levels != 2 ) {
                if ( class == TrueColor || class == StaticColor ) 
                         stdCmap = XA_RGB_BEST_MAP;
                else if ( class == GrayScale || class == StaticGray )
                        stdCmap = XA_RGB_GRAY_MAP;
		ptr->stdCmapClass = stdCmap;
	}
	if ( class == DirectColor || class == TrueColor )
		ptr->colormapSize = 1 << vinfo.depth;
	else 
		ptr->colormapSize = vinfo.colormap_size;

	/* if caller wants a StandardColormap, then do it */

	if ( stdCmap != -1 ) {
		/* install StandardColormap. XmuLookupStandardColormap will 
		   determine if the request is legal e.g. it will fail if a 
                   StandardColormap is attempted for a monochrome visual for
		   example. 

		   the property will be replaced if it existed before being 
		   re-installed for the new visual class. 

		   note: if XmuLookupStandardColormap fails, it is probably 
                   because the colormap size is too small. */

		result = XmuLookupStandardColormap( display, screen, 
			ptr->visual->visualid, depth, stdCmap, True, True );

		if ( !result ) {
			fprintf( stderr, 
				"XmuLookupStandardColormap failed\n" );
			free( ptr );
			return( (Backend *) NULL );
		}

		/* we have the StandardColormap installed for the default 
		   visual or the visual requested, so we can fill in the 
                   StandardColormap struct */ 

		if ( !XGetStandardColormap( display, RootWindow( display, 
			screen ), &ptr->stdCmap, stdCmap ) ) {
			fprintf( stderr, "XGetStandardColormap failed\n" );
			free( ptr );
			return( (Backend *) NULL );
		}

		if ( needCustomCmap == True )
			ptr->stdCmap.colormap = XCreateColormap( display, 
				RootWindow( display, screen ), ptr->visual, 
				AllocNone ); 

		ptr->cmap = ptr->stdCmap.colormap;
	}
	else if ( needCustomCmap == True )
		ptr->cmap = XCreateColormap( display, RootWindow( display,
			screen ), ptr->visual, AllocNone ); 
	else
		ptr->cmap = DefaultColormap( display, screen );

	ptr->type = type;
	ptr->levels = levels;
	*size = FindBackendSize( ptr );
	return( ptr );
}	

int
InsertBackend( backend, display, window, x, y, gc, flograph, phototag )
Backend		*backend;
Display		*display;
Drawable 	window;
int		x;
int		y;
GC		gc;
XiePhotoElement *flograph;
int		phototag;
{
	int	retval = 0;
        BackendPrivate *priv;

        priv = FindBackendPrivate( backend->visual->class, backend->type );

	if ( priv )
		retval = (priv->addBE)( backend, display, window, x, y,
			gc, flograph, phototag ); 

	return( retval );
}

int
CloseBackend( backend, display )
Backend	*backend;
Display	*display;
{
	EmptyTrash( display, backend );
	free( backend );
	return( 1 );
}

static int
AddSGSB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
	XieLevels	levels;
	int		idx = phototag;

        if ( backend->ditherSB == True ) {
                levels [ 0 ] = backend->levels;
                XieFloDither( &flograph[idx],
                        idx,
                        0x7,
                        levels,
                        xieValDitherErrorDiffusion,
                        ( char * ) NULL
                );
                idx++;
        }

	if ( backend->levels == 2 )
		XieFloExportDrawablePlane(&flograph[idx],
			idx,            /* source */
			window,         /* drawable to send data to */
			gc,             /* GC */        
			offsetX,        /* x offset in window to place data */
			offsetY         /* y offset in window to place data */
		);
	else
		XieFloExportDrawable(&flograph[idx],
			idx,            /* source */
			window,         /* drawable to send data to */
			gc,             /* GC */        
			offsetX,        /* x offset in window to place data */
			offsetY         /* y offset in window to place data */
		);
        idx++;

        return( 1 );
}

static int
SizeSGSB( backend )
Backend		*backend;
{
        int     size;

        backend->ditherSB = False;
        size = 1;

	if ( backend->levels == 2 )
		return( size );

        else if ( backend->levels != backend->colormapSize ) {
                /* need to dither */

                backend->ditherSB = True;
                size++;
		return( size );
        }
	return( size );
}

static int
AddSGTB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
	XieConstant coefficients;
	float	bias;
	int 	idx = phototag;

       	coefficients[ 0 ] = CCIR_601_RED; 
        coefficients[ 1 ] = CCIR_601_GREEN;
	coefficients[ 2 ] = CCIR_601_BLUE;

	bias = 0.0;

	XieFloBandExtract( &flograph[idx],
		idx,
		backend->colormapSize,
		bias, 
		coefficients
	);
	idx++;

        XieFloExportDrawable(&flograph[idx],
                idx,            /* source */
                window,         /* drawable to send data to */
                gc,             /* GC */        
                offsetX,        /* x offset in window to place data */
                offsetY         /* y offset in window to place data */
        );
        idx++;

        return( 1 );
}

static int
SizeSGTB( backend )
Backend		*backend;
{
	return( 2 );
}

static int
AddSCSB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
	float	bias;
	XieConstant inLow, inHigh;
	XieLTriplet outLow, outHigh;	
	XieLevels levels;
	int	blackPixel, whitePixel;
	int 	idx = phototag;
	XieConstant coefficients;
	XiePointer constrainParms;

	if ( backend->levels == 2 ) {
		XieFloExportDrawablePlane(&flograph[idx],
			idx,            /* source */
			window,         /* drawable to send data to */
			gc,             /* GC */
			offsetX,        /* x offset in window to place data */
			offsetY         /* y offset in window to place data */
		);
		return( 1 );
	}
	else if ( backend->stdCmapClass != -1 || MasksEqual( backend ) ) {

		XieFloBandCombine( &flograph[ idx ],
			idx,
			idx,
			idx
		);
		idx++;

		GetVisualLevels( backend, levels );

		if ( backend->stdCmapClass != -1 ) {
			coefficients[ 0 ] = backend->stdCmap.red_mult;
			coefficients[ 1 ] = backend->stdCmap.green_mult;
			coefficients[ 2 ] = backend->stdCmap.blue_mult; 
			bias = backend->stdCmap.base_pixel;
		}
		else {

#if 0
			coefficients[ 0 ] = 1; 
			coefficients[ 1 ] = backend->visual->red_mask + 1; 
			coefficients[ 2 ] = backend->visual->red_mask + 
					    backend->visual->green_mask + 1; 
#else
			coefficients[ 0 ] = 1; 
			coefficients[ 1 ] = levels[0]; 
			coefficients[ 2 ] = levels[0] * levels[1];
#endif
			bias = 0;
		}

                XieFloDither( &flograph[idx],
                        idx,
                        0x7,
                        levels,
                        xieValDitherErrorDiffusion,
                        ( char * ) NULL
                );
                idx++;

		XieFloBandExtract( &flograph[idx],
			idx,
			backend->colormapSize,
			bias, 
			coefficients
		);
		idx++;

		XieFloExportDrawable(&flograph[idx],
			idx,            /* source */
			window,         /* drawable to send data to */
			gc,             /* GC */        
			offsetX,        /* x offset in window to place data */
			offsetY         /* y offset in window to place data */
		);
		idx++;
	}
	else {

	       	levels [ 0 ] = 2; 

                XieFloDither( &flograph[idx],
                        idx,
                        0x7,
                        levels,
                        xieValDitherErrorDiffusion,
                        ( char * ) NULL
                );
                idx++;

		GetVisualLevels( backend, levels );

#if 0
		blackPixel = 0;
		whitePixel = ( levels[ 0 ] - 1) +
			( ( levels[ 1 ] - 1 ) * 
			( backend->visual->red_mask + 1 ) ) +
			( ( levels[ 2 ] - 1 ) *  
			( backend->visual->red_mask +
                          backend->visual->green_mask + 1 ) );
#else
		blackPixel = XBlackPixel(display, XDefaultScreen(display));
		whitePixel = XWhitePixel(display, XDefaultScreen(display));
#endif

		inLow[ 0 ] = 0.0;
		inLow[ 1 ] = inLow[ 2 ] = 0.0;
		inHigh[ 0 ] = 1.0;
		inHigh[ 1 ] = inHigh[ 2 ] = 0.0;
		outLow[ 0 ] = blackPixel;
		outLow[ 1 ] = outLow[ 2 ] = 0;
		outHigh[ 0 ] = whitePixel;
		outHigh[ 1 ] = outHigh[ 2 ] = 0;

                constrainParms = ( XiePointer ) XieTecClipScale(
                        inLow,
                        inHigh,
                        outLow,
                        outHigh
                );

		AddMemoryTrash( backend, constrainParms, XFree );

		levels[ 0 ] = backend->levels;

		XieFloConstrain( &flograph[idx],
			idx,
			levels,
			xieValConstrainClipScale,
			constrainParms
		);
		idx++;

                XieFloExportDrawable(&flograph[idx],
                        idx,            /* source */
                        window,         /* drawable to send data to */
                        gc,             /* GC */        
                        offsetX,        /* x offset in window to place data */
                        offsetY         /* y offset in window to place data */
                );
                idx++;

	}

	return( 1 );
}

static int
SizeSCSB( backend )
Backend	*backend;
{
	int	size;

	if ( backend->levels == 2 ) 
		size = 1;
        else if ( MasksEqual( backend ) || backend->stdCmapClass != -1 ) 
		size = 4;
	else
		size = 3;
	return( size );
}

static int
AddSCTB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
	XieLevels levels;
	int 	idx = phototag;
	XieConstant coefficients;
	float	bias;

	if ( backend->stdCmapClass != -1 )
		GetStdCmapLevels( &backend->stdCmap, levels );
	else
		GetVisualLevels( backend, levels );

	XieFloDither( &flograph[idx],
		idx,
		0x7,
		levels,
		xieValDitherErrorDiffusion,
		( char * ) NULL
	);
	idx++;

	if ( backend->stdCmapClass != -1 ) {	
		coefficients[ 0 ] = backend->stdCmap.red_mult;
		coefficients[ 1 ] = backend->stdCmap.green_mult; 
		coefficients[ 2 ] = backend->stdCmap.blue_mult; 

		bias = backend->stdCmap.base_pixel;
	}
	else {
#if 0
		coefficients[ 0 ] = 1; 
		coefficients[ 1 ] = backend->visual->red_mask + 1; 
		coefficients[ 2 ] = backend->visual->red_mask + 
				    backend->visual->green_mask + 1; 
#else
		coefficients[ 0 ] = 1; 
		coefficients[ 1 ] = levels[0]; 
		coefficients[ 2 ] = levels[0] * levels[1]; 
#endif

		bias = 0.0;
	}

	XieFloBandExtract( &flograph[idx],
		idx,
		backend->colormapSize,
		bias, 
		coefficients
	);
	idx++;

	XieFloExportDrawable(&flograph[idx],
		idx,            /* source */
		window,         /* drawable to send data to */
		gc,             /* GC */        
		offsetX,        /* x offset in window to place data */
		offsetY         /* y offset in window to place data */
	);
	idx++;

	return( 1 );
}

static int
SizeSCTB( backend )
Backend		*backend;
{
	return( 3 );
}

static int
AddTCSB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
	XieLevels levels;
	int 	idx = phototag;
	XieConstant coefficients;
	float	bias;

	if ( backend->levels == 2 ) {
		XieFloExportDrawablePlane(&flograph[idx],
			idx,            /* source */
			window,         /* drawable to send data to */
			gc,             /* GC */
			offsetX,        /* x offset in window to place data */
			offsetY         /* y offset in window to place data */
		);
		return( 1 );
	}

	XieFloBandCombine( &flograph[ idx ],
		idx,
		idx,
		idx
	);
	idx++;

	if ( backend->stdCmapClass != -1 ) {	
		coefficients[ 0 ] = backend->stdCmap.red_mult;
		coefficients[ 1 ] = 0.0;
		coefficients[ 2 ] = 0.0; 

		bias = backend->stdCmap.base_pixel;
	}
	else {
		coefficients[ 0 ] = 1; 
		coefficients[ 1 ] = 0.0;
		coefficients[ 2 ] = 0.0; 

		bias = 0.0;
	}

	XieFloBandExtract( &flograph[idx],
		idx,
		backend->colormapSize,
		bias, 
		coefficients
	);
	idx++;

	XieFloExportDrawable(&flograph[idx],
		idx,            /* source */
		window,         /* drawable to send data to */
		gc,             /* GC */        
		offsetX,        /* x offset in window to place data */
		offsetY         /* y offset in window to place data */
	);
	idx++;

	return( 1 );
}

static int
SizeTCSB( backend )
Backend		*backend;
{
	if ( backend->levels == 2 )
		return( 1 );

	return( 3 );
}

static int
AddTCTB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
	XieLevels levels;
	int 	idx = phototag;
	XieConstant coefficients;
	float	bias;

	if ( backend->stdCmapClass != -1 )
		GetStdCmapLevels( &backend->stdCmap, levels );
	else
		GetVisualLevels( backend, levels );

	XieFloDither( &flograph[idx],
		idx,
		0x7,
		levels,
		xieValDitherErrorDiffusion,
		( char * ) NULL
	);
	idx++;

	if ( backend->stdCmapClass != -1 )
	{	
		coefficients[ 0 ] = backend->stdCmap.red_mult;
		coefficients[ 1 ] = backend->stdCmap.green_mult; 
		coefficients[ 2 ] = backend->stdCmap.blue_mult; 

		bias = backend->stdCmap.base_pixel;
	}
	else
	{
		GetVisualLevels( backend, levels );

		coefficients[ 0 ] = 1; 
		coefficients[ 1 ] = backend->visual->red_mask + 1; 
		coefficients[ 2 ] = backend->visual->red_mask + 
				    backend->visual->green_mask + 1; 

		bias = 0.0;
	}

	XieFloBandExtract( &flograph[idx],
		idx,
		backend->colormapSize,
		bias, 
		coefficients
	);
	idx++;

	XieFloExportDrawable(&flograph[idx],
		idx,            /* source */
		window,         /* drawable to send data to */
		gc,             /* GC */        
		offsetX,        /* x offset in window to place data */
		offsetY         /* y offset in window to place data */
	);
	idx++;

	return( 1 );
}

static int
SizeTCTB( backend )
Backend		*backend;
{
	return( 3 );
}

static int
AddGSSB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
	XieColorAllocAllParam *colorParm;
	XieColorList clist;
	XieConstant coefficients;
	float	bias;
	XieLevels levels;
	int 	idx = phototag;

	if ( backend->levels == 2 )
	{
		XieFloExportDrawablePlane(&flograph[idx],
			idx,            /* source */
			window,         /* drawable to send data to */
			gc,             /* GC */
			offsetX,        /* x offset in window to place data */
			offsetY         /* y offset in window to place data */
		);
		return( 1 );
	}

	if ( backend->ditherSB == True )
	{
		levels [ 0 ] = backend->levels;
		XieFloDither( &flograph[idx],
			idx,
			0x7,
			levels,
			xieValDitherErrorDiffusion,
			( char * ) NULL
		);
		idx++;
	}

	/* use ConvertToIndex */

	colorParm = XieTecColorAllocAll( 128 ); /* mid range fill */

	AddMemoryTrash( backend, colorParm, XFree );

	clist = XieCreateColorList( display );

	AddResourceTrash( backend, clist, XieDestroyColorList );

	XieFloConvertToIndex( &flograph[idx],
		idx,                            /* source element */
		backend->cmap,
		clist,                          /* colorlist */
		True,                           /* notify if problems */
		xieValColorAllocAll,            /* color alloc tech */
		( char * ) colorParm            /* technique parms */
	);
	idx++;

        XieFloExportDrawable(&flograph[idx],
                idx,            /* source */
                window,         /* drawable to send data to */
                gc,             /* GC */        
                offsetX,        /* x offset in window to place data */
                offsetY         /* y offset in window to place data */
        );
        idx++;

	return( 1 );
}

static int
SizeGSSB( backend )
Backend		*backend;
{
        int     size;

	if ( backend->levels == 2 )
		return( 1 );

        backend->ditherSB = False;
	size = 2;
        if ( backend->levels != backend->colormapSize )
        {
                /* need to dither */

                backend->ditherSB = True;
                size++;
        }
        return( size );
}

static int
AddGSTB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
       	XieColorAllocAllParam *colorParm;
        XieColorList clist;
	XieConstant coefficients;
	float	bias;
	XieLevels levels;
	int 	idx = phototag;

	GetVisualLevels( backend, levels );

       	coefficients[ 0 ] = CCIR_601_RED; 
        coefficients[ 1 ] = CCIR_601_GREEN;
	coefficients[ 2 ] = CCIR_601_BLUE;

	bias = 0.0;

	XieFloBandExtract( &flograph[idx],
		idx,
		backend->colormapSize,
		bias, 
		coefficients
	);
	idx++;

	colorParm = XieTecColorAllocAll( 128 ); /* mid range fill */

	AddMemoryTrash( backend, colorParm, XFree );

	clist = XieCreateColorList( display );

	AddResourceTrash( backend, clist, XieDestroyColorList );

	XieFloConvertToIndex( &flograph[idx],
		idx,                            /* source element */
		backend->cmap,
		clist,                          /* colorlist */
		True,                           /* notify if problems */
		xieValColorAllocAll,            /* color alloc tech */
		( char * ) colorParm            /* technique parms */
	);
	idx++;

        XieFloExportDrawable(&flograph[idx],
                idx,            /* source */
                window,         /* drawable to send data to */
                gc,             /* GC */        
                offsetX,        /* x offset in window to place data */
                offsetY         /* y offset in window to place data */
        );
        idx++;

	return( 1 );
}

static int
SizeGSTB( backend )
Backend		*backend;
{
	return( 3 );
}

static int
AddPCSB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int		offsetX;
int		offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
	XieConstant coefficients;
        XieColorList clist;
	float	bias;
	XieLevels levels;
	int 	idx = phototag;
       	XieColorAllocAllParam *colorParm;

       	if ( backend->levels == 2 ) {
                XieFloExportDrawablePlane(&flograph[idx],
                        idx,            /* source */
                        window,         /* drawable to send data to */
                        gc,             /* GC */
                        offsetX,        /* x offset in window to place data */
                        offsetY         /* y offset in window to place data */
                );
                return( 1 );
        }

	if ( backend->ditherSB == True ) {
		if ( backend->stdCmapClass == XA_RGB_BEST_MAP )

			/* SingleBand and BEST_MAP don't really match */

			levels[ 0 ] = 2;
		else
			levels [ 0 ] = backend->levels;

		XieFloDither( &flograph[idx],
			idx,
			0x7,
			levels,
			xieValDitherErrorDiffusion,
			( char * ) NULL
		);
		idx++;
	}

	/* use ConvertToIndex */

	colorParm = XieTecColorAllocAll( 128 ); /* mid range fill */

	AddMemoryTrash( backend, colorParm, XFree );

	clist = XieCreateColorList( display );

	AddResourceTrash( backend, clist, XieDestroyColorList );

	XieFloConvertToIndex( &flograph[idx],
		idx,                            /* source element */
		backend->cmap,
		clist,                          /* colorlist */
		True,                           /* notify if problems */
		xieValColorAllocAll,            /* color alloc tech */
		( char * ) colorParm            /* technique parms */
	);
	idx++;

        XieFloExportDrawable(&flograph[idx],
                idx,            /* source */
                window,         /* drawable to send data to */
                gc,             /* GC */        
                offsetX,        /* x offset in window to place data */
                offsetY         /* y offset in window to place data */
        );
        idx++;

	return( 1 );
}

static int
SizePCSB( backend )
Backend		*backend;
{
	int	size;

	if ( backend->levels == 2 )
		return( 1 );
	
	backend->ditherSB = False;
	size = 2;
	if ( backend->stdCmapClass == XA_RGB_BEST_MAP ||
		backend->levels != backend->colormapSize )
	{
		/* need to dither */

		backend->ditherSB = True;
		size++;
	}
	return( size );
}

static int
AddPCTB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
       	XieColorAllocAllParam *colorParm;
        XieColorList clist;
	XieConstant coefficients;
	float	bias;
	XieLevels levels;
	int 	idx = phototag;


	if ( backend->stdCmapClass != -1 )
	{
		GetStdCmapLevels( &backend->stdCmap, levels );

               	XieFloDither( &flograph[idx],
                        idx,
                        0x7,
                        levels,
                        xieValDitherErrorDiffusion,
                        ( char * ) NULL
                );
                idx++;

		/* use a StandardColormap */

		coefficients[ 0 ] = backend->stdCmap.red_mult;
		coefficients[ 1 ] = backend->stdCmap.green_mult;
		coefficients[ 2 ] = backend->stdCmap.blue_mult;

		bias = backend->stdCmap.base_pixel;

                XieFloBandExtract( &flograph[idx],
                        idx,
                        backend->colormapSize,
                       	bias, 
                        coefficients
                );
                idx++;
	}
	else
	{ 
		GetVisualLevels( backend, levels );

                XieFloDither( &flograph[idx],
                        idx,
                        0x7,
                        levels,
                        xieValDitherErrorDiffusion,
                        ( char * ) NULL
                );
                idx++;

		colorParm = XieTecColorAllocAll( 128 ); /* mid range fill */

		AddMemoryTrash( backend, colorParm, XFree );

		clist = XieCreateColorList( display );

		AddResourceTrash( backend, clist, XieDestroyColorList );

		XieFloConvertToIndex( &flograph[idx],
			idx,                            /* source element */
			backend->cmap,
			clist,                          /* colorlist */
			True,                           /* notify if problems */
			xieValColorAllocAll,            /* color alloc tech */
			( char * ) colorParm            /* technique parms */
		);
		idx++;
	}

        XieFloExportDrawable(&flograph[idx],
                idx,            /* source */
                window,         /* drawable to send data to */
                gc,             /* GC */        
                offsetX,        /* x offset in window to place data */
                offsetY         /* y offset in window to place data */
        );
        idx++;

	return( 1 );
}

static int
SizePCTB( backend )
Backend		*backend;
{
	if ( backend->stdCmapClass != -1 )
		return( 3 );
	else
		return( 3 );
}

static int
AddDCSB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
       	XieColorAllocAllParam *colorParm;
        XieColorList clist;
	XieConstant coefficients;
	float	bias;
	XieLevels levels;
	int 	idx = phototag;

       	if ( backend->levels == 2 ) {
                XieFloExportDrawablePlane(&flograph[idx],
                        idx,            /* source */
                        window,         /* drawable to send data to */
                        gc,             /* GC */
                        offsetX,        /* x offset in window to place data */
                        offsetY         /* y offset in window to place data */
                );
                return( 1 );
        }

        GetVisualLevels( backend, levels );

	XieFloBandCombine( &flograph[idx],
		idx,
		idx,
		idx
	);
	idx++;

	XieFloDither( &flograph[idx],
		idx,
		0x7,
		levels,
		xieValDitherErrorDiffusion,
		( char * ) NULL
	);
	idx++;

        colorParm = XieTecColorAllocAll( 128 ); /* mid range fill */

	AddMemoryTrash( backend, colorParm, XieDestroyColorList );

        clist = XieCreateColorList( display );

	AddResourceTrash( backend, clist, XieDestroyColorList );

        XieFloConvertToIndex( &flograph[idx],
                idx,                            /* source element */
                backend->cmap,
                clist,                          /* colorlist */
                True,                           /* notify if problems */
                xieValColorAllocAll,            /* color alloc tech */
                ( char * ) colorParm            /* technique parms */
        );
        idx++;

        XieFloExportDrawable(&flograph[idx],
                idx,            /* source */
                window,         /* drawable to send data to */
                gc,             /* GC */        
                offsetX,        /* x offset in window to place data */
                offsetY         /* y offset in window to place data */
        );
        idx++;

	return( 1 );
}

static int
SizeDCSB( backend )
Backend		*backend;
{
	int     size;

	if ( backend->levels == 2 )
		return( 1 );

	size = 4;
	return( size );
}

static int
AddDCTB( backend, display, window, offsetX, offsetY, gc, flograph, phototag )
Backend         *backend;
Display         *display;
Drawable        window;
int             offsetX;
int             offsetY;
GC              gc;
XiePhotoElement *flograph;
int             phototag;
{
	XieColorAllocAllParam *colorParm;
	XieColorList clist;
	XieLevels levels;
	int 	idx = phototag;

        if ( backend->stdCmapClass != -1 )
                GetStdCmapLevels( &backend->stdCmap, levels );
        else
                GetVisualLevels( backend, levels );

	XieFloDither( &flograph[idx],
		idx,
		0x7,
		levels,
		xieValDitherErrorDiffusion,
		( char * ) NULL
	);
	idx++;

	/* use ConvertToIndex */

	colorParm = XieTecColorAllocAll( 128 ); /* mid range fill */

	AddMemoryTrash( backend, colorParm, XFree );

	clist = XieCreateColorList( display );

	AddResourceTrash( backend, clist, XieDestroyColorList );

	XieFloConvertToIndex( &flograph[idx],
		idx,                            /* source element */
		backend->cmap,
		clist,                          /* colorlist */
		True,                           /* notify if problems */
		xieValColorAllocAll,            /* color alloc tech */
		( char * ) colorParm            /* technique parms */
	);
	idx++;

        XieFloExportDrawable(&flograph[idx],
                idx,            /* source */
                window,         /* drawable to send data to */
                gc,             /* GC */        
                offsetX,        /* x offset in window to place data */
                offsetY         /* y offset in window to place data */
        );
        idx++;

	return( 1 );
}

static int
SizeDCTB( backend )
Backend		*backend;
{
	return( 3 );
}

/* routines to handle freeing of resources and memory needed by backend */

#define	VALIDATE_AND_FIND \
	struct trash *p, *q, *new; \
	if (!backend) return( 0 ); \
	new = (struct trash *) calloc( 1,  sizeof( struct trash ) ); \
	if ( !new ) return( 0 ); \
	new->next = (struct trash *) NULL;\
	p = backend->can;\
	if ( !p ) backend->can = new; \
	else { \
	q = p; \
	while ( q ) { p = q; q = q->next; }\
	p->next = new; \
	}

int	
AddMemoryTrash( backend, ptr, func )
Backend	*backend;
void	*ptr;
void 	(*func)();
{
	VALIDATE_AND_FIND

	new->func = func;
	new->ptr = ptr;
	new->type = MEM;
	return( 1 );
}

int
AddResourceTrash( backend, res, func )
Backend	*backend;
XID	res;
void 	(*func)();
{
	VALIDATE_AND_FIND

	new->func = func;
	new->res = res;
	new->type = RES;
	return( 1 );
}

int
EmptyTrash( display, backend )
Display *display;
Backend	*backend;
{
	struct trash *p, *q;

	p = backend->can;

	while ( p ) 
	{
		if ( p->type == MEM )
		{
			(p->func)( p->ptr ); 
		}
		else
		{
			(p->func)(display, p->res );
		}
		q = p->next;
		free( p );
		p = q;
	}
	backend->can = 0;
	return( 1 );
}
