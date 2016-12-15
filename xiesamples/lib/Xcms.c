
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

#include	<X11/Xlib.h>
#include	<X11/Xcms.h>
#include	<X11/extensions/XIElib.h>

/* following defines taken from Xcmsint.h */

#define XDCCC_MATRIX_ATOM_NAME  "XDCCC_LINEAR_RGB_MATRICES"
#define XDCCC_NUMBER    0x8000000L      /* 2**27 per XDCCC */

typedef struct {
    XcmsFloat XYZtoRGBmatrix[3][3];
    XcmsFloat RGBtoXYZmatrix[3][3];
} XcmsPrivate;

static XcmsPrivate XcmsMat;
static isInited = 0;

/* Some of GetMatrices adapted from LINEAR_RGB_InitSCCData() - see LRGB.c in
   lib/X11 source on the X Consortium CD. */

static int
GetMatrices( dpy, screen, vis )
Display	*dpy;
int	screen;
Visual 	*vis;
{
   	Atom  MatrixAtom = XInternAtom (dpy, XDCCC_MATRIX_ATOM_NAME, True);
	XcmsCCC 	ccc;
    	int   		count, format_return;
	unsigned long 	nitems, nbytes_return;
	char 		*property_return, *pChar;
	XcmsFloat 	*pValue;

	if (MatrixAtom == None ||
        	!_XcmsGetProperty (dpy, RootWindow(dpy, screen), 
		MatrixAtom, &format_return, &nitems, &nbytes_return, 
		&property_return) || nitems != 18 || format_return != 32) 
	{
		/*
		 * As per the XDCCC, there must be 18 data items and each must be 
		 * in 32 bits !  
                 */ 

		/* read defaults from Xcms private structures */

		ccc = XcmsCreateCCC(dpy, screen, vis, NULL, NULL, NULL, 
			NULL, NULL ); 

		if ( ccc )
		{
			memcpy( XcmsMat.RGBtoXYZmatrix, 
				((XcmsPrivate *)(ccc->pPerScrnInfo->screenData))->RGBtoXYZmatrix,
				sizeof( XcmsMat.RGBtoXYZmatrix ) ) ;
			memcpy( XcmsMat.XYZtoRGBmatrix, 
				((XcmsPrivate *)(ccc->pPerScrnInfo->screenData))->XYZtoRGBmatrix,
				sizeof( XcmsMat.XYZtoRGBmatrix ) ) ;
		}
		else
		{
			isInited = -1;
			return( 0 );
		}
	} 
	else 
	{
		/*
		 * RGBtoXYZ and XYZtoRGB matrices
		 */

		pValue = (XcmsFloat *) &XcmsMat;
        	pChar = property_return;
        	for (count = 0; count < 18; count++) {
            		*pValue++ = (long)_XcmsGetElement(format_return, 
				&pChar, &nitems) / (XcmsFloat)XDCCC_NUMBER;
        	}
        	free ((char *)property_return);
	}
	isInited = 1;
	return( 1 );
} 

int 
GetXcmsRGBToXYZ( dpy, screen, vis, mat ) 
Display		*dpy; 
int		screen; 
Visual		*vis; 
XieMatrix 	mat; 
{	 
	int		i, j, idx;

	if ( isInited == 0 )
		GetMatrices( dpy, screen, vis );

	if ( isInited == -1 )
		return( 0 );

	idx = 0;
	for ( i = 0; i < 3; i++ )
	{
		for ( j = 0; j < 3; j++ )
		{
			mat[ idx ] = XcmsMat.RGBtoXYZmatrix[ i ][ j ];
			idx++;
		}
	}
	return( 1 );
}

int
GetXcmsXYZToRGB( dpy, screen, vis, mat )
Display         *dpy;
int             screen;
Visual          *vis;
XieMatrix       mat;
{       
	int		i, j, idx;

	if ( isInited == 0 )
		GetMatrices( dpy, screen, vis );

	if ( isInited == -1 )
		return( 0 );

	idx = 0;
        for ( i = 0; i < 3; i++ )
        {
        	for ( j = 0; j < 3; j++ )
                {
			mat[ idx ] = XcmsMat.XYZtoRGBmatrix[ i ][ j ];
			idx++;
                }
        }
        return( 1 );
}

#if	defined( TEST )
main()
{
	Display *dpy;
	int     screen;
	Visual  *vis;
	XieMatrix rgbToxyz, xyzTorgb;
	int	i;

	dpy = XOpenDisplay( "" );
	screen = DefaultScreen( dpy );
	vis = DefaultVisual( dpy, screen );
	if ( GetXcmsXYZToRGB( dpy, screen, vis, xyzTorgb ) )
	{
		printf( "GetXcmsXYZToRGB: success\n" );
		printf( "%f %f %f %f %f %f %f %f %f\n",
			xyzTorgb[ 0 ],
			xyzTorgb[ 1 ],
			xyzTorgb[ 2 ],
			xyzTorgb[ 3 ],
			xyzTorgb[ 4 ],
			xyzTorgb[ 5 ],
			xyzTorgb[ 6 ],
			xyzTorgb[ 7 ],
			xyzTorgb[ 8 ] );
	}	
	if ( GetXcmsRGBToXYZ( dpy, screen, vis, rgbToxyz ) )
	{
		printf( "GetXcmsRGBToXYZ: success\n" );
		printf( "%f %f %f %f %f %f %f %f %f\n",
			rgbToxyz[ 0 ],
			rgbToxyz[ 1 ],
			rgbToxyz[ 2 ],
			rgbToxyz[ 3 ],
			rgbToxyz[ 4 ],
			rgbToxyz[ 5 ],
			rgbToxyz[ 6 ],
			rgbToxyz[ 7 ],
			rgbToxyz[ 8 ] );
	}
}
#endif
