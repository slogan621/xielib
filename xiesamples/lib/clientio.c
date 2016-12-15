
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
#include	<X11/extensions/XIElib.h>
#include	"events.h"
#include	"clientio.h"
        
Bool
PumpTheClientData( display, floID, pSpace, element, data, size, aggSize,
        band, final )
Display 	*display;       /* connection to the server */
unsigned long 	floID;          /* photoflo resource ID */
unsigned long 	pSpace;         /* photospace, 0 for a stored photoflo */
int     	element;        /* phototag of ImportClient element */
char    	*data;          /* a buffer containing the entire band */
unsigned int 	size;           /* number of bytes in the buffer */
int     	aggSize;        /* size of an aggregate, e.g. XieRectangle */
int     	band;           /* identifies which band of data, 0, 1, 2 */ 
Bool    	final;          /* if True, this is last data for band */
{
        unsigned int bytesLeft, nBytes, maxBytes;
	int localFinal;

#if     defined( DEBUG )
        int     bytesSent = 0;
#endif

#if     defined( DEBUG )
        printf( "data %d size %d aggSize %d band %d final %d\n", data,
                size, aggSize, band, final );
#endif
        /* if number of bytes not modulus aggregate size, return */

        if ( size % aggSize )
                return( False );

        /* adjust maximum bytes per XiePutClientData to be divisible by 
		aggSize */

        maxBytes = MAXSEND;
        if ( maxBytes % aggSize )
        {
                maxBytes = MAXSEND - ( MAXSEND % aggSize );
        }

        localFinal = 0;
        bytesLeft = size;
        while ( bytesLeft > 0 ) {
                nBytes = (bytesLeft > maxBytes ? maxBytes : bytesLeft);
                if ( nBytes >= bytesLeft )
                        localFinal = ( final == True ? True : False );
                XiePutClientData(
                        display,        /* connection to the server */
                        pSpace,         /* photospace, or 0 for stored flo */
                        floID,          /* photoflo resource ID */
                        element,        /* phototag of ImportClient element */
                        localFinal,     /* if True, last block of data */
                        band,           /* 0, 1, or 2 */
                        data,           /* buffer of data to send */
                        nBytes          /* number of bytes to send */
                );
#if     defined( DEBUG )
                bytesSent += nBytes;
                fprintf( stderr, "%d bytes sent\n", bytesSent );
#endif
                bytesLeft -= nBytes;
                data += nBytes;
        }
#if     defined( DEBUG )
        fprintf( stderr, "Total %d bytes sent\n", bytesSent );
#endif
        return( True );
}

int
ReadNotifyExportData( d, xieInfo, namespace, flo_id, element, elementsz, 
	numels, band, data )
Display 	*d;		/* display connection */
XieExtensionInfo *xieInfo;	/* returned by XieInitialize */
unsigned long 	namespace;	/* photospace of the photoflo */
int     	flo_id;		/* photoflo ID */
XiePhototag 	element;	/* element in photoflo being read */ 
unsigned int 	elementsz;	/* canonical size of elements read */
unsigned int 	numels;		/* how many elements to read */
int		band;		/* which band to read */
char    	**data;		/* on return, a pointer to the data */
{
        Bool 		no_errors, reallocFlag;
	XIEEventCheck	eventData;
        unsigned int 	nbytes_ret;
        Bool 		terminate = False;
        unsigned int 	nbytes;
        unsigned int 	cnt;
        char 		*cp, *ptr;
	XEvent 		event;
        int 		bytes;
        XieExportState 	new_state_ret;

        if ( numels == 0 )
        {
		eventData.floId = flo_id;
		eventData.tag = element;
		eventData.space = namespace;
		eventData.base = xieInfo->first_event;
		eventData.which = xieEvnNoExportAvailable;
		eventData.count = 0;
		WaitForXIEEvent(d, &eventData, 10L, &event );
		numels = eventData.count;

                if ( numels == 0 )
                        return( -1 );
        }

        nbytes = numels * elementsz;
        bytes = nbytes;
        no_errors = True;
        reallocFlag = False;
        cnt = 0;
        if ( *data == ( char * ) NULL )
        {
                if ( ( *data = ( char * ) malloc( nbytes ) ) 
			== ( char * ) NULL )
                {
                        fprintf( stderr, 
				"ReadNotifyExportData: couldn't allocate buffer\n" );
                        return( -1 );
                }
        }
        ptr = *data;
        while ( 1 )
        {
                XieGetClientData ( d, namespace, flo_id, element,
                        bytes > MAXREAD ? MAXREAD : bytes, terminate, band,
                        &new_state_ret, (unsigned char **)&cp, &nbytes_ret );

                if ( nbytes_ret && reallocFlag )
                {
                        *data = (char *) realloc( *data, cnt + nbytes_ret );
                        if ( *data == ( char * ) NULL )
                        {
                                /* oh no */

                                fprintf( stderr, 
					"ReadNotifyExportData: realloc failed\n" );
                                no_errors = False;
                                break;
                        }
                        ptr = *data + cnt;
                }

                memcpy( ptr, cp, nbytes_ret * sizeof( char ) );
                ptr += nbytes_ret * sizeof( char );
                XFree( cp );

                cnt += nbytes_ret;
                bytes -= nbytes_ret;
                if ( new_state_ret == xieValExportEmpty )
		{
			eventData.floId = flo_id;
			eventData.tag = element;
			eventData.space = namespace;
			eventData.base = xieInfo->first_event;
			eventData.which = xieEvnNoExportAvailable;
			WaitForXIEEvent(d, &eventData, 10L, &event );
		}
                else if ( new_state_ret == xieValExportMore )
                {
                        if ( bytes <= 0 )
                        {
                                reallocFlag = True;
                                bytes = 2048;
                        }
                }
                else if ( new_state_ret == xieValExportDone )
                {
                        break;
                }
                else if ( new_state_ret == xieValExportError )
                {
                        fprintf( stderr, 
				"ReadNotifyExportData: xieValExportError\n" );
                        no_errors = False;
                        break;
                }

                if ( bytes <= 0 && reallocFlag == False )
                {
                       	/* if we get here, we didn't get an ExportDone,
                           and we have no buffer space left. So turn on
                           the realloc flag. */

                        bytes = MAXREAD;
                        reallocFlag = True;
                }
        }
        if ( no_errors == False )
        {
                return( -1 );
        }
        return( cnt );
}

int
ReadNotifyExportVector( display, xieInfo, readVec, size, timeout )
Display 	*display;	/* connection to the X server */
XieExtensionInfo *xieInfo;	/* returned by XieInitialize */
ReadNotifyData 	readVec[];	/* on input - describes which elements 
				   are to be read, from which photoflos. 
				   on return - contains read status and
				   data */
int 		size;		/* how many elements are in readVec */ 
long 		timeout;	/* how long the caller is willing to wait */
{
	Bool		no_errors, terminate = False;
        long    	endTime, curTime;
	ReadNotifyPriv 	*priv;
	XIEEventCheck	eventData;
	XEvent		event;
        unsigned int 	nbytes_ret;
	char		*cp;
	int		i;
	Bool		notYet;

        endTime = time( ( long * ) NULL ) + timeout;

	/* determine initial counts for those unspecified by caller, and 
	   allocate data buffers */

	if ( size <= 0 )
		return( 0 );

	priv = (ReadNotifyPriv *) malloc( sizeof( ReadNotifyPriv ) * size );
	if ( priv == (ReadNotifyPriv *) NULL )
	{
		fprintf( stderr, "ReadNotifyExportVector: malloc failed\n" );
		return( 0 );
	}

	for ( i = 0; i < size; i++ )
	{
		priv[ i ].notifySeen = False;
		readVec[ i ].status = xieValExportEmpty;
	}

        no_errors = True;

	while ( no_errors == True )
	{
		/* see if we are done yet */

		if( time( ( long * ) NULL ) > endTime )
		{
			no_errors = False;
			fprintf( stderr, "ReadNotifyExportVector: timeout\n" );
			break;
		}

		/* see if we are done yet */

		notYet = False;
		for ( i = 0; i < size; i++ )
		{
			if ( readVec[ i ].status != xieValExportDone ) 
				notYet = True;
		}

		if ( notYet == False )
		{
			break;
		}

		/* Check for any newly available exports */

		for ( i = 0; i < size; i++ )
		{
			if ( priv[ i ].notifySeen == False )
			{
				if ( readVec[ i ].numels == 0 )
				{
					eventData.floId = readVec[i].flo_id;
					eventData.tag = readVec[i].element;
					eventData.space = readVec[i].namespace;
					eventData.base = xieInfo->first_event;
					eventData.which = 
						xieEvnNoExportAvailable;

					if (WaitForXIEEvent(display, 
						&eventData, 1, &event) == False)
						continue;

					readVec[ i ].numels = eventData.count;
				}

				priv[ i ].notifySeen = True;
				
				if ( readVec[ i ].numels == 0 )
				{
					/* this is an interesting case -
					   an ExportAvailable event was
					   sent, but it did not report
					   the size */

					readVec[ i ].numels = MAXREAD; 
				}
				priv[ i ].reallocFlag = False;
				readVec[ i ].bytesRead = 0;
				priv[ i ].bytesLeft = readVec[ i ].numels *
					readVec[ i ].elementsz;
				readVec[ i ].data = 
					(char *) malloc( priv[ i ].bytesLeft );
				if ( readVec[ i ].data == ( char * ) NULL )
				{
					fprintf( stderr, 
						"ReadNotifyExportVector: malloc %ld bytes failed\n",
						priv[ i ].bytesLeft );
					no_errors = False;
					break;
				}
				priv[ i ].ptr = readVec[ i ].data;
			}
		}

		for ( i = 0; i < size; i++ )
		{
			if ( priv[ i ].notifySeen == True && 
				readVec[ i ].status != xieValExportDone )
			{
				XieGetClientData ( 
					display, 
					readVec[ i ].namespace, 
					readVec[ i ].flo_id, 
					readVec[ i ].element,
					(priv[ i ].bytesLeft > MAXREAD ?
					 MAXREAD : priv[ i ].bytesLeft),
					terminate,
					readVec[ i ].band,
					&readVec[ i ].status,
					(unsigned char **) &cp,
					&nbytes_ret
				);

				if ( nbytes_ret && priv[ i ].reallocFlag )
				{
                        		readVec[ i ].data = (char *) realloc( 
						readVec[ i ].data, 
						readVec[ i ].bytesRead + 
							nbytes_ret 
					);
					if ( readVec[ i ].data == (char *) NULL)
					{
						no_errors = False;
						break;
					}	
 	                       		priv[ i ].ptr = readVec[ i ].data + 
						readVec[ i ].bytesRead;
				}

				memcpy( priv[ i ].ptr, cp, 
					nbytes_ret * sizeof( char ) );
				priv[ i ].ptr += nbytes_ret * sizeof( char );
				XFree( cp );

				readVec[ i ].bytesRead += nbytes_ret;
                		priv[ i ].bytesLeft -= nbytes_ret;
				if ( readVec[ i ].status == xieValExportMore ||
					readVec[ i ].status == xieValExportEmpty )
				{
					if ( priv[ i ].bytesLeft <= 0 )
					{
						priv[ i ].reallocFlag = True;
						priv[ i ].bytesLeft = MAXREAD;
					}
				}
				else if ( readVec[ i ].status == xieValExportError )
				{
					fprintf( stderr, 
						"ReadNotifyExportData: xieValExportError\n" );
					no_errors = False;
					break;
				}
				else if ( readVec[ i ].status == xieValExportDone )
				{
					continue;
				}
			}
		}
	}
	free( priv );
        if ( no_errors == False )
        {
                return( 0 );
        }
        return( 1 );
}
