
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
#include 	<unistd.h>
#include 	<sys/stat.h>
#include	<fcntl.h>

typedef enum {                  /* JPEG marker codes */
  M_SOF0  = 0xc0,
  M_SOF1  = 0xc1,
  M_SOF2  = 0xc2,
  M_SOF3  = 0xc3,
  
  M_SOF5  = 0xc5,
  M_SOF6  = 0xc6,
  M_SOF7  = 0xc7,
  
  M_JPG   = 0xc8,
  M_SOF9  = 0xc9,
  M_SOF10 = 0xca,
  M_SOF11 = 0xcb,
  
  M_SOF13 = 0xcd,
  M_SOF14 = 0xce,
  M_SOF15 = 0xcf,
  
  M_DHT   = 0xc4,
  
  M_DAC   = 0xcc,
  
  M_RST0  = 0xd0,
  M_RST1  = 0xd1,
  M_RST2  = 0xd2,
  M_RST3  = 0xd3,
  M_RST4  = 0xd4,
  M_RST5  = 0xd5,
  M_RST6  = 0xd6,
  M_RST7  = 0xd7,
  
  M_SOI   = 0xd8,
  M_EOI   = 0xd9,
  M_SOS   = 0xda,
  M_DQT   = 0xdb,
  M_DNL   = 0xdc,
  M_DRI   = 0xdd,
  M_DHP   = 0xde,
  M_EXP   = 0xdf,
  
  M_APP0  = 0xe0,
  M_APP15 = 0xef,
  
  M_JPG0  = 0xf0,
  M_JPG13 = 0xfd,
  M_COM   = 0xfe,
  
  M_TEM   = 0x01,
  
  M_ERROR = 0x100
} JPEG_MARKER;

int
GetJFIFData( file, bytes, depth, width, height, bands )
char	*file;
char	**bytes;
char	*depth;
short	*width;
short 	*height;
char	*bands;
{
	int	fd, n, endian;
	unsigned long seekOffset;
	char	buf[ 4 ];
	int	size, count;
	unsigned char *ptr;
	struct stat sb;

	*bytes = ( char * ) NULL;
	fd = open( file, O_RDONLY );
	if ( fd < 0 )
	{
		perror( "open" );
		printf( "Couldn't open %s\n", file );
		return( 0 );
	}

	/* check to see if the file is JFIF */

	n = read( fd, buf, 2 );
	if ( n != 2 )
		goto bad;
	if ( !(buf[ 0 ] == (char) 0xff && buf[ 1 ] == (char) 0xd8 ) )
		goto bad;	
	lseek( fd, 0x6, SEEK_SET );
	n = read( fd, buf, 4 );
	if ( n != 4 )
		goto bad;
	if ( !(buf[ 0 ] == (char) 0x4a && buf[ 1 ] == (char) 0x46 
		&& buf[ 2 ] == (char) 0x49 && buf[ 3 ] == (char) 0x46 ) )
		goto bad;

	lseek( fd, 0L, SEEK_SET );
	fstat( fd, &sb );  
	size = sb.st_size;

	*bytes = ( char * ) malloc( size );
	if ( *bytes == ( char * ) NULL )
	{
		printf( "failed mallocing data buffer\n" );
		goto bad;
	}
	n = read( fd, *bytes, size );
	if ( n != size )
	{
		free( *bytes );
		*bytes = ( char * ) NULL;
		goto bad;
	}
	close( fd );

	/* determine width, height, and depth of image */

	count = 1;
	ptr = *bytes + 0x0b;
	while (  count <= size && count > 0 )
	{
		switch (*ptr) {
		    case M_SOF0:
		    case M_SOF1:
		    case M_SOF2:
		    case M_SOF3:
		    case M_SOF5:
		    case M_SOF6:
		    case M_SOF7:
		    case M_JPG:
		    case M_SOF9:
		    case M_SOF10:
		    case M_SOF11:
		    case M_SOF13:
		    case M_SOF14:
		    case M_SOF15:
		    case M_SOI:
		    case M_EOI:
		    case M_SOS:
			count = -1;
			break;
		    default:
			count++;
			ptr++;
		}
	}
	if ( count == size )
	{
               	free( *bytes );
               	*bytes = ( char * ) NULL;
		goto bad;
	}
	else
	{
		ptr += 3;
		*depth = *ptr; ptr++;
		*height = *ptr << 8 | *(ptr+1); ptr+=2;
		*width = *ptr << 8 | *(ptr+1); ptr+=2;
		*bands = *ptr;
	}
	return( size );
bad:  	close( fd );
	return( 0 );
}

#if STANDALONE
main( argc, argv )
int	argc;
char	*argv[];
{
	char	*bytes;
	int	n;

	if ( argc != 2 )
	{
		printf( "usage: readjif file\n" );
		exit( 1 );
	}
	n = GetJFIFData( argv[ 1 ], &bytes );
	printf( "Read %d bytes of JPEG data from file\n", n );
}
#endif
