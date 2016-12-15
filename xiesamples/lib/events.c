
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
#include <sys/types.h>
#include <sys/time.h>
#include "events.h"

Bool 
FilterProc( display, event, args )
Display 	*display;	/* display connection to the X server */
XEvent 		*event;		/* an event from the event queue */
XPointer	args;		/* client specified args */
{
	XIEEventCheck 	*eventData = (XIEEventCheck *) args;
	int		xieEvent;
	Bool		retval = False;		/* we assume failure */

	/* The following declarations can be used to simplify access to fields 
	   of the various XIE events which are read from the event queue... */

	XieExportAvailableEvent *ExportAvailable = (XieExportAvailableEvent *) event;
	XiePhotofloDoneEvent *PhotofloDone = (XiePhotofloDoneEvent *) event;
	XieColorAllocEvent *ColorAlloc = (XieColorAllocEvent *) event;
	XieDecodeNotifyEvent *DecodeNotify = (XieDecodeNotifyEvent *) event;
	XieImportObscuredEvent *ImportObscured = (XieImportObscuredEvent *) event;

	/* determine the event code by subtracting from the event code 
	   passed by the server the XIE base event code */

	xieEvent = event->type - eventData->base;
	switch( xieEvent ) {
	case xieEvnNoExportAvailable:

		/* make sure that the event is for the desired photoflo 
		   and element */

		if ( ExportAvailable->flo_id == eventData->floId && 
			ExportAvailable->name_space == eventData->space &&
			ExportAvailable->src == eventData->tag ) {
			/* data is element-specific, perhaps a count of items 
                           ready */

			eventData->count = ExportAvailable->data[ 0 ];
			retval = True;
		}
		break;
	case xieEvnNoPhotofloDone:

		/* make sure it is for our photoflo */

		if ( PhotofloDone->flo_id == eventData->floId &&
			PhotofloDone->name_space == eventData->space ) {
			retval = True;
		}
		break;
	case xieEvnNoColorAlloc:

		/* make sure it is for our photoflo */

		if ( ColorAlloc->flo_id == eventData->floId &&
			ColorAlloc->name_space == eventData->space ) {
			retval = True;
		}
		break;
	case xieEvnNoDecodeNotify:

		/* make sure it is for our photoflo */

		if ( DecodeNotify->flo_id == eventData->floId &&
			DecodeNotify->name_space == eventData->space ) {
			retval = True;
		}
		break;
	case xieEvnNoImportObscured:

		/* make sure it is for our photoflo */

		if ( ImportObscured->flo_id == eventData->floId &&
			ImportObscured->name_space == eventData->space ) {
			retval = True;
		}
		break;
	}
	return( retval );
}

Bool
WaitForXIEEvent( display, eventData, timeout, event )
Display		*display;	/* display connection to the X server */
XIEEventCheck	*eventData;	/* describes the event the caller is 
				   interested in */
long		timeout;	/* how long the client is willing to wait 
				   for the event */
XEvent		*event;		/* on return, contains the event information */
{
	int	Xsocket;	/* socket associated with display, needed 
				   by select */ 

	long 	endTime, curTime, delta;
	struct timeval tv;
	Bool 	done, retval;
	int	ret;
#if defined( WIN32 ) || defined( linux )
	fd_set 	rd;
#else
	unsigned int rd;
#endif

	retval = done = False;

	/* Obtain the socket id of the connection to the server, which is 
	   needed by select */

	Xsocket = ConnectionNumber( display );

	/* wait no longer than current time plus timeout seconds */
	
	endTime = time( ( long * ) NULL ) + timeout;
	while ( done == False ) {
		/* see if there is anything for us in the client event 
		   queue... XCheckIfEvent is non-blocking, and will return 
		   after all events in the queue are checked, or FilterProc 
		   finds a match with the criteria specified in eventData */	

		if ( ( ret = XCheckIfEvent( display, event, FilterProc, 
			(XPointer) eventData ) ) == 0 ) {
			/* the event isn't there yet. Go to sleep until there 
                           is something to read on the connection, or we have 
			   timed out */

			/* delta is how much of timeout remains */

			curTime = time( ( long * ) NULL );
			delta = endTime - curTime;
			if ( delta <= 0 ) {
				/* we have waited long enough. Break out of 
                                   the loop */

				done = True;
				continue;
			}

			/* we haven't waited long enough. call select... */

			tv.tv_sec = delta;  
			tv.tv_usec = 0L;
			XFlush( display );
#if defined( WIN32 ) || defined( linux )
			FD_ZERO(&rd);
			FD_SET(Xsocket, &rd);
#else
			rd = 1 << Xsocket;
#endif
			select( Xsocket + 1, &rd, NULL, NULL, &tv );
			continue;
		}
		else {
			done = retval = True;
		}
	}
	return( retval );
}

int
WaitForWindow( display, window )
Display *display;
Window window;
{
        int     done;
        XEvent  event;

        done = 0;
        while( done == 0 ) {
                XNextEvent( display, &event );
                switch( event.type ) {
                        case Expose:
				if ( event.xexpose.window == window )
					done = 1;
                                break;
                }
        }
}

