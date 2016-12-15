
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

#include	<Xlib.h>

typedef struct _map
{
	unsigned char alloc;	/* if 1, this intensity is allocated */
	unsigned char map;	/* the mapped pixel value */
} Map;

void
RemapIntensitiesToCells( display, colormap, image, size )
Display		*display;	/* connection to X server */
Colormap	colormap;	/* colormap resource ID, we
				   assume the server default
				   colormap is passed, but 
				   any colormap from which the 
				   client can allocate cells
				   is appropriate */
char		*image;		/* uncompressed, grayscale image
				   data */
int		size;		/* number of 8-bit pixels in
				   the image */
{
	int	i, sum;
	XColor	color;
	Status	status;
	unsigned char idx;
	Map 	map[ 256 ];

	for ( i = 0; i < 256; i++ )
		map[ i ].alloc = 0;

	for ( i = 0; i < size; i++ )
	{
		idx = image[ i ];
		if ( map[ idx ].alloc == 0 )
		{
			color.red = color.green = color.blue = 
				(long) (65535.0 * (idx / 255.0)); 
			if ( !XAllocColor( display, colormap, &color ) )
				continue;
			else
			{
				map[ idx ].map = (char) color.pixel;
				map[ idx ].alloc = 1;
				image[ i ] = (char) color.pixel;
			}
		}
		else
			image[ i ] = map[ idx ].map;
	}
}
	
