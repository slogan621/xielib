
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
#include <math.h>

int
DrawHistogram( d, w, gc, histos, size, levels, width, height )
Display *d;
Window  w;
GC	gc;
XieHistogramData histos[];
int     size;
unsigned long levels;
int	width;
int	height;
{
        unsigned long maxcount;
        int     i;
        float   sx, sy;
        XRectangle *rects;
        short   yadd, xadd;
        char    buf[ 32 ];

        maxcount = 0;
        for ( i = 0; i < size; i++ ) {
                if ( histos[ i ].count > maxcount )
                        maxcount = histos[ i ].count;
	}

        if ( maxcount == 0 )
                return;

        xadd = ( short ) ( ( float ) width * 0.15 );
        yadd = xadd;

        sx = ( float ) ( width - xadd ) / ( float ) ( levels + 1 );
        sy = ( float ) ( height - yadd ) / ( float ) maxcount;

        XClearWindow( d, w );

        /* label x */

        XDrawImageString( d, w, gc, xadd, height - 3, "0", 1 );
        sprintf( buf, "%d", levels );
        XDrawImageString( d, w, gc, width - strlen( buf ) * 14, 
                height - 3, buf, strlen( buf ) );

        /* label y */

        sprintf( buf, "%d", maxcount );
        XDrawImageString( d, w, gc, 3, 20, buf, strlen( buf ) );
        XDrawImageString( d, w, gc, 3, height - yadd, "0", 1 );

        /* y axis */

        XDrawLine( d, w, gc, xadd, height - yadd, xadd, 0 );

        /* x axis */

        XDrawLine( d, w, gc, xadd, height - yadd,
                width, height - yadd );

        rects = ( XRectangle * ) malloc( sizeof( XRectangle ) * size );
        if ( rects == ( XRectangle * ) NULL )
                return;

        /* create the rectangles */

        for ( i = 0; i < size; i++ ) {
                rects[ i ].width = ( short ) ceil( sx );
                rects[ i ].height = ( short ) ( sy * histos[ i ].count );
                rects[ i ].x = xadd + ( short ) ( sx * histos[ i ].value );
               	rects[ i ].y = ( short ) height - yadd - rects[ i ].height;
        }

        /* draw it */

        XFillRectangles( d, w, gc, rects, size );
        XSync( d, 0 );

        free( rects );
}

