
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

int
GetKernel( file, kernel )
char    *file;
float   **kernel;
{
        FILE    *fp;
        int     i, j, size, err;
        float   tmp;

        if ( ( fp = fopen( file, "r" ) ) == ( FILE * ) NULL )
                return( 0 );

        fscanf( fp, "%d\n", &size );
        printf( "kernel is %dx%d\n", size, size );
        if ( size <= 0 )
        {
                fclose( fp );
                return( 0 );    
        }

        *kernel = ( float * ) malloc( sizeof( float ) * size * size );
        if ( *kernel == ( float * ) NULL )
        {
                fclose( fp );
                return( 0 );
        }

        for ( i = 0; i < size; i++ )
        {
                for ( j = 0; j < size; j++ )
                {
                        err = fscanf( fp, "%f", &tmp ); 
                        if ( err <= 0 )
                        {
                                fclose( fp );
                                free( *kernel );
                                return( 0 );
                        }
                        *(( *kernel + ( i * size ) + j )) = tmp;
                }
        }       
        fclose( fp );
        return( size );
}       

