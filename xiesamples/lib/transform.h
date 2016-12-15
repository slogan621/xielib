
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

#define MAXTRANSFORMS 500

typedef int transformHandle;

transformHandle CreateIdentity( void );

transformHandle CreateScale( float sx, float sy );

transformHandle CreateShear( float sx, float sy );

transformHandle CreateMirrorX( float w );

transformHandle CreateMirrorY( float h );

transformHandle CreateRotate( float angle, float w, float h );

transformHandle CreateTranslate( int tx, int ty );

void InitTransforms( void );

void FreeTransformHandle( transformHandle handle );

transformHandle ConcatenateTransforms( transformHandle list[], int size );

int SetCoefficients( transformHandle handle, float coeffs[] );

#if     !defined( M_PI )
#define M_PI 3.14159265358979323846
#endif

#define TORAD( x ) ( x * M_PI / 180.0 )

