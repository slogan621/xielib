
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

#include	<stdio.h>
#include	<memory.h>
#include	"transform.h"
#include	<math.h>

typedef struct {
	float	mat[ 6 ];
	int	allocated;
} transform;

static transform transforms[ MAXTRANSFORMS ];

int 
SetCoefficients( transformHandle handle, float coeffs[] )
{
	if ( handle < 0 || handle >= MAXTRANSFORMS )
		return( 0 );
	if ( transforms[ handle ].allocated == 0 )
		return( 0 );	

	coeffs[ 0 ] = transforms[ handle ].mat[ 0 ];
	coeffs[ 1 ] = transforms[ handle ].mat[ 1 ];
	coeffs[ 2 ] = transforms[ handle ].mat[ 2 ];
	coeffs[ 3 ] = transforms[ handle ].mat[ 3 ];
	coeffs[ 4 ] = transforms[ handle ].mat[ 4 ];
	coeffs[ 5 ] = transforms[ handle ].mat[ 5 ];
	return( 1 );
}

static transformHandle
GetTransformHandle( void )
{
	int		i;
	transformHandle ret;

	i = 0;
        ret = -1;
	while ( i < MAXTRANSFORMS )
        {
		if ( transforms[ i ].allocated == 0 )
		{
			ret = i;
			transforms[ i ].allocated = 1;
			break;
		}
                i++;
	}
	return( ret );
}

transformHandle
CreateIdentity( void )
{
	transformHandle handle;

	handle = GetTransformHandle();
	if ( handle == -1 )
		return( -1 );
	transforms[ handle ].mat[ 0 ] = 1.0;
	transforms[ handle ].mat[ 3 ] = 1.0;
	transforms[ handle ].mat[ 1 ] = 0.0;
	transforms[ handle ].mat[ 2 ] = 0.0;
	transforms[ handle ].mat[ 4 ] = 0.0;
	transforms[ handle ].mat[ 5 ] = 0.0;
        return( handle );
}

transformHandle
CreateShear( float sx, float sy )
{
	transformHandle handle;

	handle = GetTransformHandle();
	if ( handle == -1 )
		return( -1 );
	transforms[ handle ].mat[ 0 ] = 1.0; 
	transforms[ handle ].mat[ 3 ] = 1.0;
	transforms[ handle ].mat[ 1 ] = sx;
	transforms[ handle ].mat[ 2 ] = sy;
	transforms[ handle ].mat[ 4 ] = 0.0; 
	transforms[ handle ].mat[ 5 ] = 0.0; 
        return( handle );
}

transformHandle
CreateMirrorX( float wide )
{
	transformHandle handle;

	handle = GetTransformHandle();
	if ( handle == -1 )
		return( -1 );
	transforms[ handle ].mat[ 0 ] = -1.0;
	transforms[ handle ].mat[ 3 ] = 1.0; 
	transforms[ handle ].mat[ 1 ] = 0.0;
	transforms[ handle ].mat[ 2 ] = 0.0;
	transforms[ handle ].mat[ 4 ] = wide - 1.0; 
	transforms[ handle ].mat[ 5 ] = 0.0; 
        return( handle );
}

transformHandle
CreateMirrorY( float high )
{
	transformHandle handle;

	handle = GetTransformHandle();
	if ( handle == -1 )
		return( -1 );
	transforms[ handle ].mat[ 0 ] = 1.0; 
	transforms[ handle ].mat[ 1 ] = 0.0;
	transforms[ handle ].mat[ 2 ] = 0.0;
	transforms[ handle ].mat[ 3 ] = -1.0;
	transforms[ handle ].mat[ 4 ] = 0.0; 
	transforms[ handle ].mat[ 5 ] = high - 1.0; 
        return( handle );
}

transformHandle
CreateScale( float sx, float sy )
{
	transformHandle handle;

	handle = GetTransformHandle();
	if ( handle == -1 )
		return( -1 );
	transforms[ handle ].mat[ 0 ] = (float) sx;
	transforms[ handle ].mat[ 3 ] = (float) sy;
	transforms[ handle ].mat[ 1 ] = 0.0;
	transforms[ handle ].mat[ 2 ] = 0.0;
	transforms[ handle ].mat[ 4 ] = 0.0; 
	transforms[ handle ].mat[ 5 ] = 0.0; 
        return( handle );
}

transformHandle
CreateRotate( float angle /* in radians */, float w, float h )
{
	transformHandle handle;
	float	cx = cos( angle ), sx = sin( angle );

	handle = GetTransformHandle();
	if ( handle == -1 )
		return( -1 );
	transforms[ handle ].mat[ 0 ] = cx;
	transforms[ handle ].mat[ 1 ] = sx;
	transforms[ handle ].mat[ 2 ] = -sx;
	transforms[ handle ].mat[ 3 ] = cx;
	transforms[ handle ].mat[ 4 ] = w / 2.0 - 0.5 * ( cx * w + sx * h ); 
	transforms[ handle ].mat[ 5 ] = h / 2.0 - 0.5 * ( -sx * w + cx * h );
        return( handle );
}

transformHandle
CreateTranslate( int tx, int ty )
{
	transformHandle handle;

	handle = GetTransformHandle();
	if ( handle == -1 )
		return( -1 );
	transforms[ handle ].mat[ 0 ] = 1.0;
	transforms[ handle ].mat[ 1 ] = 0.0;
	transforms[ handle ].mat[ 2 ] = 0.0;
	transforms[ handle ].mat[ 3 ] = 1.0;
	transforms[ handle ].mat[ 4 ] = (float) -tx;
	transforms[ handle ].mat[ 5 ] = (float) -ty;
        return( handle );
}

void
InitTransforms()
{
	int	i;

	for ( i = 0; i < MAXTRANSFORMS; i++ )
	{
		transforms[ i ].allocated = 0;
	}
}

void
FreeTransformHandle( transformHandle handle )
{
	if ( handle < 0 || handle >= MAXTRANSFORMS )
		return;
	transforms[ handle ].allocated = 0;
}

transformHandle
ConcatenateTransforms( transformHandle list[], int size )
{
	int	i, j, k;
	transformHandle handle;
	float	temp1[ 3 ][ 3 ];
        float	temp2[ 3 ][ 3 ];
	float 	result[ 3 ][ 3 ];
	float 	Mult();


	handle = list[ 0 ];
	if ( handle < 0 || handle >= MAXTRANSFORMS )
		return ( -1 );

	/* initialize result */

	result[ 0 ][ 0 ] = transforms[ handle ].mat[ 0 ]; /* a */
	result[ 0 ][ 1 ] = transforms[ handle ].mat[ 2 ]; /* c */
	result[ 0 ][ 2 ] = 0.0; 				 
	result[ 1 ][ 0 ] = transforms[ handle ].mat[ 1 ]; /* b */
	result[ 1 ][ 1 ] = transforms[ handle ].mat[ 3 ]; /* d */
	result[ 1 ][ 2 ] = 0.0; 				 
	result[ 2 ][ 0 ] = transforms[ handle ].mat[ 4 ]; /* e */
	result[ 2 ][ 1 ] = transforms[ handle ].mat[ 5 ]; /* f */
	result[ 2 ][ 2 ] = 1.0;

	/* concatenate the remaining matrices */

	for ( i = 1; i < size; i++ )
	{
		handle = list[ i ];
		if ( handle < 0 || handle >= MAXTRANSFORMS )
			return ( -1 );

		/* initialize operand 1 */

 		temp1[ 0 ][ 0 ] = transforms[ handle ].mat[ 0 ]; /* a */
		temp1[ 0 ][ 1 ] = transforms[ handle ].mat[ 2 ]; /* c */
		temp1[ 0 ][ 2 ] = 0.0; 				 
		temp1[ 1 ][ 0 ] = transforms[ handle ].mat[ 1 ]; /* b */
		temp1[ 1 ][ 1 ] = transforms[ handle ].mat[ 3 ]; /* d */
		temp1[ 1 ][ 2 ] = 0.0; 				 
		temp1[ 2 ][ 0 ] = transforms[ handle ].mat[ 4 ]; /* e */
		temp1[ 2 ][ 1 ] = transforms[ handle ].mat[ 5 ]; /* f */
		temp1[ 2 ][ 2 ] = 1.0;

		/* initialize operand 2 */

		memcpy( &temp2, &result, sizeof( float ) * 3 * 3 );

		/* perform the multiply */

		for ( j = 0; j < 3; j++ )
		{
			for ( k = 0; k < 3; k++ )
			{
				result[ j ][ k ] =
					Mult( j, k, temp1, temp2 );
                        }
            	}
	}

	/* get a new transform handle */

	handle = GetTransformHandle();
	if ( handle != -1 )
	{
		/* set the coefficients */

		transforms[ handle ].mat[ 0 ] = result[ 0 ][ 0 ];
		transforms[ handle ].mat[ 1 ] = result[ 1 ][ 0 ];
		transforms[ handle ].mat[ 2 ] = result[ 0 ][ 1 ];
		transforms[ handle ].mat[ 3 ] = result[ 1 ][ 1 ];
		transforms[ handle ].mat[ 4 ] = result[ 2 ][ 0 ];
		transforms[ handle ].mat[ 5 ] = result[ 2 ][ 1 ];
	}
	return( handle );
}              

float
Mult( row, col, A, B )
int	row;
int	col;
float	A[3][3];
float	B[3][3];
{
	float	sum = 0.0;

       	sum = A[ row ][ 0 ] * B[ 0 ][ col ] + A[ row ][ 1 ] * B[ 1 ][ col ] +
		A[ row ][ 2 ] * B[ 2 ][ col ];
	return( sum );
}
	
