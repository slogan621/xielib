
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
#include	<fcntl.h>
#include 	<sys/types.h>
#include 	<sys/stat.h>
#include	<X11/extensions/XIElib.h>

#define	GrayASCII	0x32
#define	GrayBINARY	0x35

#define	PixmapASCII	0x33
#define	PixmapBINARY	0x36

typedef char Boolean;

#ifndef	MAX
#define MAX( a, b ) ( ( a > b ? a : b ) )
#endif 

#define	True	1
#define False	0

static void SeekAndDestroyComments( int fd, char *line, int *len );
static int getline( int fd, char *buf );

int
WriteBinaryPPM( file, width, height, levels, data, size )
char		*file;		/* file name to write */
XieLTriplet	width;		/* per-band width of the image */ 
XieLTriplet	height;		/* per-band height of the image */
XieLTriplet	levels;		/* per-band levels attribute */
char		*data;		/* the image data */
int		size;		/* the number of bytes of image data */
{
	int		fd, fail = 0;
	FILE		*fp;

	/* verify dimensions */

	if ( width[ 0 ] != width[ 1 ] || width[ 0 ] != width[ 2 ] )
	{
		printf( "Per-band widths must be equal\n" );
		fail++;
	}

	if ( height[ 0 ] != height[ 1 ] || height[ 0 ] != height[ 2 ] )
	{
		printf( "Per-band heights must be equal\n" );
		fail++;
	}

	if ( levels[ 0 ] != levels[ 1 ] || levels[ 0 ] != levels[ 2 ] )
	{
		printf( "Per-band levels must be equal\n" );
		fail++;
	}

	if ( fail )
		return( 0 );

	fp = fopen( file, "w+" );

       	if ( fp == (FILE *) NULL )
        {
                perror( "fopen" );
                return( 0 );
        }

	putc( 0x50, fp );
	putc( PixmapBINARY, fp );
	fprintf( fp, "%d\n", width[ 0 ] );
	fprintf( fp, "%d\n", height[ 0 ] );
	fprintf( fp, "%d\n", levels[ 0 ] );
	fclose( fp );

	if ( ( fd = open( file, O_RDWR ) ) < 0 )
		return( 0 );

	lseek( fd, 0L, SEEK_END );
	write( fd, data, size );
	close( fd );
	return( 1 );
}

int
WriteBinaryPGM( file, width, height, maxGray, data, size )
char    *file;          	/* file name to write */
int     width;         		/* the width of the image */ 
int     height;        		/* the height of the image */
int     maxGray;       		/* maximum gray level */
char    *data;         		/* the image data */
int     size;          		/* the number of bytes of image data */
{
	int		fd;
	FILE		*fp;

	fp = fopen( file, "w+" );

	if ( fp == (FILE *) NULL )
	{
		perror( "fopen" );
		return( 0 );
	}

	putc( 0x50, fp );
	putc( GrayBINARY, fp );
	fprintf( fp, "%d\n", width );
	fprintf( fp, "%d\n", height );
	fprintf( fp, "%d\n", maxGray );
	fclose( fp );

	if ( ( fd = open( file, O_RDWR ) ) < 0 )
		return( 0 );

	lseek( fd, 0L, SEEK_END );
	write( fd, data, size );
	close( fd );
	return( 1 );
}

int
ReadPPM( file, width, height, levels, data, size )
char		*file;		/* file name to read */
XieLTriplet	width;		/* on return, per-band width of the image */ 
XieLTriplet	height;		/* on return, per-band height of the image */
XieLTriplet	levels;		/* on return, per-band levels attribute */
char		**data;		/* on return, the image data stripped 
			           of its headers */
int		*size;		/* on return, the number of bytes of 
			           image data read */
{
	char	type;
	short	majik;
	Boolean	done = False,
		needWidth = True,
		needHeight = True;	
	char	line[ 128 ];	/* suggested size is 70 actually */
	char 	*ptr;
	FILE	*stream;
	char	*cache;
	char	*cacheTemp = (char *) NULL;
	struct 	stat statBuf;
	int	i, fd, len, base, count, cSize;

	width[0] = width[ 1 ] = width[ 2 ] = 
	height[ 0 ] = height[ 1 ] = height[ 2 ] = 
	levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 0;

	/* open the file */

	if ( file == ( char * ) NULL )
		return( 0 );
	fd = open( file, O_RDONLY );
	if ( fd <= 0 )
		return( 0 );
	stream = fdopen( fd, "r" );

	/* get the header and see what we are working with */

	if ( read( fd, &majik, sizeof( short ) ) != sizeof( short ) )
		goto bad;

	ptr = (char *) &majik;

	if ( *ptr++ != 0x50 )
	{
		printf( "ReadPPM: Bad majik number\n" );
		goto bad;
	}

	type = *ptr;

	if ( type != PixmapASCII && type != PixmapBINARY )
	{
		printf( "ReadPPM: type is not ASCII or BINARY\n" );
		goto bad;
	}

	/* get the dimensions */

	done = False;
	while ( !feof( stream ) && done == False )
	{
		len = getline( fd, line );
		if ( !len )
			continue;
		SeekAndDestroyComments( fd, line, &len );
		if ( len == 0 )
			continue;
		if ( needWidth == True )
		{
			count = len - 1;
			needWidth = False;
			base = 1;
			width[ 0 ] = 0;
			while ( count >= 0 )
			{
				width[ 0 ] += (line[ count ] - '0') * base;
				base *= 10;
				count--;
			}  	
		}
		else if ( needHeight == True )
		{
			count = len - 1;
			needHeight = False;
			base = 1;
			height[ 0 ] = 0;
			while ( count >= 0 )
			{
				height[ 0 ] += (line[ count ] - '0') * base;
				base *= 10;
				count--;
			}  	
		}
		else
		{
			count = len - 1;
			base = 1;
			levels[ 0 ] = 0;
			while ( count >= 0 )
			{
				levels[ 0 ] += (line[ count ] - '0') * base;
				base *= 10;
				count--;
			}  	
			done = True;
		}
	}

	width[ 1 ] = width[ 2 ] = width[ 0 ];
	height[ 1 ] = height[ 2 ] = height[ 0 ];
	levels[ 0 ]++;
	levels[ 1 ] = levels[ 2 ] = levels[ 0 ];

	/* Get the image. Assume the rest of the file has no comments. */

	*size = width[ 0 ] * height[ 0 ] * 3;

	*data = ( char * ) malloc ( *size );
	if ( *data == ( char * ) NULL )
		goto bad;

	if ( type == PixmapBINARY )
	{
		if ( read( fd, *data, *size ) != *size )
		{
			printf( "ReadPPM: failed to read image data\n" );
			goto bad;
		}
	}
	else	/* PixmapASCII */
	{
		ptr = *data;

		if ( stat( file, &statBuf ) == -1 )
		{
			free( data );
			goto bad;
		}

		/* read from disk in one chunk. Things are painfully slow 
		   if we read as we parse the file */

		cacheTemp = cache = ( char * ) malloc( statBuf.st_size );
		if ( cache == ( char * ) NULL )
		{
			free( data );
			goto bad;
		}
		
		if ( ( cSize = read( fd, cache, statBuf.st_size ) ) <= 0 )
		{
			free( data );
			free( cache );
			goto bad;
		} 
	
		/* now, convert ascii to binary, reading from cache and
		   writing to data */	

		done = False;
		while ( done == False )
		{
			/* skip whitespace and find the first digit */

			while ( cSize )
			{
				if ( *cache >= '0' && *cache <= '9' )
					break;
				cache++;
				cSize--;
			}
		
			if ( cSize <= 0 )
			{
				done = True;
				continue;	
			}

			i = 0;
			while ( cSize && *cache >= '0' && *cache <= '9' )
			{
				line[ i ] = *cache;
				i++;	
				cache++;
				cSize--;
			}
			i--;

			base = 1;
			*ptr = 0;
			while ( i >= 0 )
			{
				*ptr += (line[ i ] - '0') * base;
				base *= 10;
				i--;
			}
			ptr++;

			if ( cSize <= 0 )
				done = True;
		}
	}

	if ( cacheTemp )
		free( cacheTemp );
	close( fd );
	return( 1 );
bad:
	close( fd );
	return( 0 );
} 

int
ReadPGM( file, width, height, maxGray, data, size )
char    *file;          /* file name to read */
int     *width;         /* on return, the width of the image */ 
int     *height;        /* on return, the height of the image */
int     *maxGray;       /* on return, maximum gray level */
char    **data;         /* on return, the image data stripped 
                           of its headers */
int     *size;          /* on return, the number of bytes of 
                           image data read */
{
	char	type;
	short	majik;
	Boolean	done = False,
		needWidth = True,
		needHeight = True;	
	char	line[ 128 ];	/* suggested size is 70 actually */
	char 	*ptr;
	FILE	*stream;
	char	*cache;
	char	*cacheTemp = (char *) NULL;
	struct 	stat statBuf;
	int	i, fd, len, base, count, cSize, band;

	*width = *height = *maxGray = 0;

	/* open the file */

	if ( file == ( char * ) NULL )
		return( 0 );
	fd = open( file, O_RDONLY );
	if ( fd <= 0 )
	{
		printf( "ReadPGM: Unable to open file\n" );
		return( 0 );
	}

	stream = fdopen( fd, "r" );

	/* get the header and see what we are working with */

	if ( read( fd, &majik, sizeof( short ) ) != sizeof( short ) )
	{
		printf( "ReadPGM: Bad header\n" );
		goto bad;
	}

	ptr = (char *) &majik;

	if ( *ptr++ != 0x50 )
	{
		printf( "ReadPGM: Bad majik number\n" );
		goto bad;
	}

	type = *ptr;

	if ( type != GrayASCII && type != GrayBINARY )
	{
		printf( "ReadPGM: type is not ASCII or BINARY\n" );
		goto bad;
	}

	/* get the dimensions */

	done = False;
	while ( !feof( stream ) && done == False )
	{
		len = getline( fd, line );
		if ( !len )
			continue;
		SeekAndDestroyComments( fd, line, &len );
		if ( len == 0 )
			continue;
		if ( needWidth == True )
		{
			count = len - 1;
			needWidth = False;
			base = 1;
			*width = 0;
			while ( count >= 0 )
			{
				*width += (line[ count ] - '0') * base;
				base *= 10;
				count--;
			}  	
		}
		else if ( needHeight == True )
		{
			count = len - 1;
			needHeight = False;
			base = 1;
			*height = 0;
			while ( count >= 0 )
			{
				*height += (line[ count ] - '0') * base;
				base *= 10;
				count--;
			}  	
		}
		else
		{
			count = len - 1;
			base = 1;
			*maxGray = 0;
			while ( count >= 0 )
			{
				*maxGray += (line[ count ] - '0') * base;
				base *= 10;
				count--;
			}  	
			done = True;
		}
	}

	/* Get the image. Assume the rest of the file has no comments. */

	*size = *width * *height;

	*data = ( char * ) malloc ( *size );
	if ( *data == ( char * ) NULL )
	{
		printf( "ReadPGM: malloc failed\n" );
		goto bad;
	}

	if ( type == GrayBINARY )
	{
		if ( read( fd, *data, *size ) != *size )
		{
			printf( "ReadPGM: failed to read image data\n" );
			goto bad;
		}
	}
	else	/* GrayASCII */
	{
		ptr = *data;

		if ( stat( file, &statBuf ) == -1 )
		{
			free( data );
			goto bad;
		}

		/* read from disk in one chunk. Things are painfully slow 
		   if we read as we parse the file */

		cacheTemp = cache = ( char * ) malloc( statBuf.st_size );
		if ( cache == ( char * ) NULL )
		{
			free( data );
			goto bad;
		}
		
		if ( ( cSize = read( fd, cache, statBuf.st_size ) ) <= 0 )
		{
			free( data );
			free( cache );
			goto bad;
		} 
	
		/* now, convert ascii to binary, reading from cache and
		   writing to data */	

		done = False;
		while ( done == False )
		{
			/* skip whitespace and find first digit */

			while ( cSize )
			{
				if ( *cache >= '0' && *cache <= '9' )
					break;
				cache++;
				cSize--;
			}
		
			if ( cSize <= 0 )
			{
				done = True;
				continue;	
			}

			i = 0;
			while ( cSize && *cache >= '0' && *cache <= '9' )
			{
				line[ i ] = *cache;
				i++;	
				cache++;
				cSize--;
			}
			i--;

			base = 1;
			*ptr = 0;
			while ( i >= 0 )
			{
				*ptr += (line[ i ] - '0') * base;
				base *= 10;
				i--;
			}
			ptr++;

			if ( cSize <= 0 )
				done = True;
		}
	}

	if ( cacheTemp )
		free( cacheTemp );
	close( fd );
	return( 1 );
bad:
	close( fd );
	return( 0 );
} 

static int
getline( fd, buf )
int	fd;
char	*buf;
{
	int	len = 0;

	while ( read( fd, buf + len, 1 ) == 1 && !isspace( *(buf + len) ) ) 	
		len++;
	return( len );
}	

static void
SeekAndDestroyComments( fd, line, len )
int	fd;
char	*line;		/* a line to strip comments from */
int	*len;		/* coming in, its length. On return, 
			   new length */	  	
{
	int	count = 0;
	char	trash[ 1 ];

	/* look for a '#', if it exists */

	while ( count < *len && line[ count ] != '#' ) count++; 

	if ( count == *len )
		return;

	/* We have a comment in the line. If the newline terminating the 
           comment hasn't been read yet, then read and discard characters
	   until we see it or hit EOF */

	if ( line[ *len - 1 ] != '\n' )
		while ( read( fd, trash, 1 ) == 1 && trash[ 0 ] != '\n' );

	/* return the number of non-comment characters in the line */

	*len = count;
}

