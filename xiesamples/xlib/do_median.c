#include	<stdio.h>

#define SetPixel( data, i, j, value ) *((char *)data + (width * i) + j) = value 
#define GetPixel( data, i, j ) (*((char *)data + (width * i) + j))

int
MedianFilter( data, median, width, height )
char    *data;                  /* image to process */
int     median;                 /* kernel size */
int     width;                  /* width of image */
int     height;                 /* height of image */
{
        int     i, j, value[ 9 ];
        char    *buf;

        /* for this sample, we do a 3x3 square median filter. If there are
           any pixels which fall out of the grid, we will simply skip the 
           grid - this is interesting and should be visible as an artifact
           in the resulting image surrounding the outside border */ 

        buf = (char *) malloc( width * height );
        if ( buf == ( char * ) NULL )
                return( 0 );
        for ( i = 1; i < height - 2; i++ )
                for ( j = 1; j < width - 2; j++ )
                {
                        value[0] = GetPixel( data, i - 1, j - 1 ); 
                        value[1] = GetPixel( data, i, j - 1 );
                        value[2] = GetPixel( data, i + 1, j - 1 );
                        value[3] = GetPixel( data, i - 1, j );
                        value[4] = GetPixel( data, i + 1, j );
                        value[5] = GetPixel( data, i - 1, j + 1 );
                        value[6] = GetPixel( data, i, j + 1 );
                        value[7] = GetPixel( data, i + 1, j + 1 );
                        value[8] = GetPixel( data, i, j );
                        SortValues( value, sizeof( value ) / sizeof( int) );
                        SetPixel( buf, i, j, value[ median ] );
                }
        memcpy( data, buf, width * height );
        free( buf );
        return( 1 );
}

int
SortValues( value, size )
int     value[];                /* table to sort */
int     size;                   /* how many elements to sort */
{
        int     i, j, v;

        for ( i = 1; i < size; i++ )
        {
                v = value[ i ]; j = i;
                while ( value[ j - 1 ] > v && j > 1 )
                {
                        value[ j ] = value[ j - 1 ];
                        j--;
                }
                value[ j ] = v;
        }
}

