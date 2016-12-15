
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XIElib.h>
#include "errors.h"

#include <stdio.h>

static ClientErrorPtr resourceErrors[ xieNumErrors - 1 ]; 
static ClientErrorPtr floErrors[ xieNumFloErrors + 1 ]; 
static int (*oldErrorHandler)() = NULL;

static XieExtensionInfo *xieExtInfo;
static int errorsInited = 0;

static void InitClientHandlers();
static int CallClientErrorHandlers();
static void AddList();

static int
CallClientErrorHandlers( d, ev )
Display	*d;
XErrorEvent *ev;
{
	int	which;
	ClientErrorPtr p = (ClientErrorPtr) NULL;
    	XieFloAccessError *flo_error = (XieFloAccessError *) ev;

    	if (ev->error_code == xieExtInfo->first_error + xieErrNoFlo)
	{
        	which = flo_error->flo_error_code;
		if ( which >= 1 && which <= xieNumFloErrors )
			p = floErrors[ which ];
	}
	else if (ev->error_code >= xieExtInfo->first_error)
        {
        	which = ev->error_code - xieExtInfo->first_error;
        	if ( which >= 0 && which < xieNumErrors )
			p = resourceErrors[ which ];
	}
	if (p) 
	{
		while ( p )
		{
			(p->func)( d, which, ev, p->private ); 
			p = p->next;
		}
	}
	else if ( oldErrorHandler )
	{
		(*oldErrorHandler)(d, ev);
	}
	
	return( 0 );
}

static void
AddList(type, which, p)
int	type;
int	which;
ClientErrorPtr p;
{
	ClientErrorPtr q, r;

	if ( type == RESOURCEERROR )
		q = resourceErrors[ which ];
	else
		q = floErrors[ which ];

	if ( q == (ClientErrorPtr) NULL )
	{
		if ( type == RESOURCEERROR )
			resourceErrors[ which ] = p;
		else
			floErrors[ which ] = p;
	}
	else
	{
		while( q ) 
		{
			r = q; q = q->next;
		}
		r->next = p;
	}
}
	
/* void 
   myHandler( Display *display, int which, XPointer error, XPointer private ) */

#define VALIDATE_TYPE if ( type != RESOURCEERROR && type != FLOERROR ) \
				return( (ClientErrorPtr) NULL );
#define VALIDATE_WHICH if ( ( which == RESOURCEERROR && \
				( which < xieErrNoColorList || \
				which > xieErrNoROI ) ) || \
			    ( which == FLOERROR && \
				( which < xieErrNoFloAccess || \
				which > xieErrNoFloImplementation ) ) ) \
				return( (ClientErrorPtr) NULL ); 
#define VALIDATE_FUNC if ( !func ) \
				return( (ClientErrorPtr) NULL );

/* returns a handle */

ClientErrorPtr
SetClientErrorHandler( d, type, which, func, private )
Display *d;
int	type;			/* RESOURCEERROR or FLOERROR */
int	which;
int	(*func)();
XPointer private; 
{
	ClientErrorPtr p = (ClientErrorPtr) NULL;

	if ( !errorsInited )
	{
		InitClientHandlers();
	} 
	
	VALIDATE_TYPE
	VALIDATE_WHICH
	VALIDATE_FUNC

	p = (ClientErrorPtr) malloc( sizeof( ClientErrorRec ) );
	if ( p == (ClientErrorPtr) NULL )
		return( p ); 
	p->func = func;
	p->private = private;
	p->which = which;
	p->type = type;
	p->next = (ClientErrorPtr) NULL;
	AddList(type, which, p);
	return( p );
}

void
RemoveErrorHandler(p)
ClientErrorPtr p;
{
	ClientErrorPtr q, r;
	int i;

	if ( p->type == RESOURCEERROR )
		q = resourceErrors[ p->which ];
	else
		q = floErrors[ p->which ];

	if ( q == (ClientErrorPtr) NULL )
		return;

	if ( p == q )
	{
		if ( p->type == RESOURCEERROR )
			resourceErrors[ p->which ] = q->next;
		else
			floErrors[ p->which ] = q->next;
		free( p );
	}
	else
	{	
		r = (ClientErrorPtr) NULL;
		while ( q && q != p )
		{
			r = q;
			q = q->next;
		}
		if ( r )
		{
			r->next = q->next;
			free( p );
		}
	}
}

static void
InitClientHandlers()
{
	int	i;

	for ( i = 0; i < xieNumErrors; i++ )
		resourceErrors[ i ] = (ClientErrorPtr) NULL;
	for ( i = 0; i < xieNumFloErrors + 1; i++ )
		floErrors[ i ] = (ClientErrorPtr) NULL;
	errorsInited = 1;
}

void
EnableClientHandlers( xieInfo )
XieExtensionInfo *xieInfo;
{
	xieExtInfo = xieInfo;
	oldErrorHandler = XSetErrorHandler( CallClientErrorHandlers );
}

void
DisableClientHandlers()
{
	if ( oldErrorHandler )
		XSetErrorHandler( oldErrorHandler );
	else
		XSetErrorHandler( None );
}

void
RestoreDefaultErrorHandler()
{
	XSetErrorHandler( None );
}


