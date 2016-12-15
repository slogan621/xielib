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

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Shell.h>
#include <stdio.h>

#include "xgc.h"
#include "transform.h"
#include "events.h"
#include "backend.h"

extern XStuff X;
extern XIEStuff XIE;

#include <stdlib.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#ifdef WIN32
#define _WILLWINSOCK_
#endif
#include <ctype.h>
#ifdef WIN32
#define BOOL wBOOL
#undef Status
#define Status wStatus
#include <winsock.h>
#undef Status
#define Status int
#undef BOOL
#endif

static XieExtensionInfo *xieInfo;
static Backend	*backend;
static int beSize;
void DoHistos();

int	
InitXIE( display )
Display *display;
{
	Visual	*visual;
	int	screen, retval;

	retval = 1;
	if (!XieInitialize(display, &xieInfo)) 
	{
        	fprintf(stderr, "XIE not supported on this display!\n");
		retval = 0;
  	}
	else
	{
		screen = DefaultScreen( display );
		visual = DefaultVisual( display, screen );

		backend = (Backend *) InitBackend( display, screen, 
			visual->class, xieValSingleBand, 
			1<<DefaultDepth( display, screen ), -1, &beSize );

		if ( backend == (Backend *) NULL )
		{
			fprintf( stderr, "Unable to create backend\n" );
			retval = 0;
		}
	}
	return( retval );
}

int
LoadSingleBandJPEGPhotomap(display, file, photomap)
Display *display;
char	*file;
XiePhotomap *photomap;
{
	char	*bytes;
	int	size;
	char	deep, bands;
	short	w, h;
       	char    *decodeParms;
        int     decodeTech;
	Bool 	notify;
	int	idx;
	float	sx, sy;
	int	floId, floSize;
        unsigned int bandMask;
        XieConstant     constant;
        XieGeometryTechnique sampleTech;
        XiePointer      sampleParms;
	float	coeffs[ 6 ];
	transformHandle handle;
        XiePhotoElement *flograph;
       	XieLTriplet width, height, levels;
        XiePhotospace photospace;

	if ( ( size = GetJFIFData( file, &bytes, &deep, &w, &h, &bands ) ) == 0 )
        {
                printf( "Problem getting JPEG data from %s\n", file );
                return( 0 );
        }

	if ( deep != 8 || bands != 1 )
	{
		printf( "Image must be 256 levels and SingleBand\n" );
		free( bytes );
		return( 0 );
	}

        decodeTech = xieValDecodeJPEGBaseline;
        decodeParms = ( char * ) XieTecDecodeJPEGBaseline(
                xieValBandByPixel,
                xieValLSFirst,
                True
        );

	floSize = 3;

       	flograph = XieAllocatePhotofloGraph(floSize);

	width[ 0 ] = w; width[ 1 ] = width[ 2 ] = 0;
	height[ 0 ] = h; height[ 1 ] = height[ 2 ] = 0;
	levels[ 0 ] = 256; levels[ 1 ] = levels[ 2 ] = 0;

        sx = (float) w / 256;
        sy = (float) h / 256; 
        handle = CreateScale( sx, sy );
        SetCoefficients( handle, coeffs );
        FreeTransformHandle( handle );

        idx = 0;
        notify = False;
        XieFloImportClientPhoto(&flograph[idx], 
                xieValSingleBand,       /* class of image data */
                width,                  /* width of each band */
                height,                 /* height of each band */
                levels,                 /* levels of each band */
                notify,                 /* no DecodeNotify events */ 
                decodeTech,             /* decode technique */
                (char *) decodeParms    /* decode parameters */
        );
        idx++;

       	sampleTech = xieValGeomAntialias; 
        sampleParms = ( XieGeometryTechnique ) NULL; 
	constant[ 0 ] = 128.0;
	bandMask = 1;

        XieFloGeometry(&flograph[idx],
                idx,                    /* image source */
                256,                    /* width of resulting image */
                256,                    /* height of resulting image */
                coeffs,                 /* a, b, c, d, e, f */
                constant,               /* used if src pixel does not exist */ 
                bandMask,               /* ignored for SingleBand images */
                sampleTech,             /* sample technique... */ 
                sampleParms             /* and parameters, if required */
        );
        idx++;

	*photomap = XieCreatePhotomap( display );

	XieFloExportPhotomap( &flograph[idx],
		idx,
		*photomap,
		xieValEncodeServerChoice,
		( XiePointer ) NULL 	
	);
	idx++;

	/* now, execute */

        photospace = XieCreatePhotospace( display );
	floId = 1;

	XieExecuteImmediate( display, photospace, floId, False, flograph, floSize );

        PumpTheClientData( display, floId, photospace, 1, bytes, size,
                sizeof( char ), 0, True );

        XieFreePhotofloGraph( flograph, floSize );
        XieDestroyPhotospace( display, photospace );
        XFree( decodeParms );
	return( 1 );
}

int	
BuildRefreshPhotoflos( X, XIE )
XStuff *X;
XIEStuff *XIE;
{
        int     idx;
        int     floSize;
        XiePhotoElement *flograph;

        floSize = 1 + beSize;
        flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;

	XieFloImportPhotomap( &flograph[ idx ],
		XIE->src1Photo,
		False
	);
	idx++;

        if ( !InsertBackend( backend, X->dpy, X->win1, 0, 0, X->gc,
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

	XIE->refresh1= XieCreatePhotoflo( X->dpy,
		flograph,
		floSize
	);

	/* Make the second one */

	idx = 0;

	XieFloImportPhotomap( &flograph[ idx ],
		XIE->src2Photo,
		False
	);
	idx++;

        if ( !InsertBackend( backend, X->dpy, X->win2, 0, 0, X->gc,
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

	XIE->refresh2 = XieCreatePhotoflo( X->dpy,
		flograph,
		floSize
	);

	/* free up the photoflo graph and the color alloc parms */

        XieFreePhotofloGraph( flograph, floSize );
	return( 1 );
}
 
int
RefreshSources( X, XIE )
XStuff *X;
XIEStuff *XIE;
{
	XieExecutePhotoflo( X->dpy, XIE->refresh1, False );
	XieExecutePhotoflo( X->dpy, XIE->refresh2, False );
	return( 1 );
}

int
DoLogicalFlo()
{
        int     idx, src1, src2, histoSrc;
        int     floId, floSize;
	XieConstant constant;
	XiePhotospace photoSpace;
        XiePhotoElement *flograph;
	XieProcessDomain domain;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XEvent event;

	floSize = 3 + beSize;
	if ( XIE.isDyadic == True )
		floSize++;

        flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;
        XieFloImportPhotomap( &flograph[ idx ],
                XIE.src1Photo,
                False
        );
        idx++;
	src1 = idx;

	if ( XIE.isDyadic == True )
	{
		XieFloImportPhotomap( &flograph[ idx ],
			XIE.src2Photo,
			False
		);
		idx++;
		src2 = idx;
	}

	domain.offset_x = 0;
	domain.offset_y = 0;
	domain.phototag = 0;	

	constant[ 0 ] = ( XIE.isDyadic == True ? 0.0 : (double) XIE.constant );
	constant[ 1 ] = constant[ 2 ] = 0.0;
	XieFloLogical( &flograph[ idx ],
		src1,
		( XIE.isDyadic == True ? src2 : 0 ),
		&domain,
		constant,
		XIE.logicalOp,
		0x1
	);
	idx++;
	histoSrc = idx;

        if ( !InsertBackend( backend, X.dpy, X.win3, 0, 0, X.gc,
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;
	
	XieFloExportClientHistogram( &flograph[ idx ],
		histoSrc,
		&domain,        /* get entire image - no ROI */
                xieValNewData   /* send event when new data is ready */ 
        );
	idx++;

	photoSpace = XieCreatePhotospace( X.dpy );
	floId = 1;
	XieExecuteImmediate( X.dpy,
		photoSpace,
		floId,
		True,
		flograph,
		floSize 
	);

	DoHistos( X.dpy, X.win4, X.gc, floId, photoSpace, idx );
        eventData.floId = floId;
        eventData.space = photoSpace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(X.dpy, &eventData, 10L, &event );

	XieFreePhotofloGraph( flograph, floSize );
	XieDestroyPhotospace( X.dpy, photoSpace );
	return( 1 );
}

int
DoArithmeticFlo()
{
        int     idx, src1, src2, histoSrc;
        int     floId, floSize;
	XieConstant constant;
	XiePhotospace photoSpace;
        XiePhotoElement *flograph;
	XieProcessDomain domain;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XEvent event;

	floSize = 3 + beSize;
	if ( XIE.isDyadic == True )
		floSize++;

        flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;
        XieFloImportPhotomap( &flograph[ idx ],
                XIE.src1Photo,
                False
        );
        idx++;
	src1 = idx;

	if ( XIE.isDyadic == True )
	{
		XieFloImportPhotomap( &flograph[ idx ],
			XIE.src2Photo,
			False
		);
		idx++;
		src2 = idx;
	}

	domain.offset_x = 0;
	domain.offset_y = 0;
	domain.phototag = 0;	

	constant[ 0 ] = ( XIE.isDyadic == True ? 0.0 : (double) XIE.constant );
	constant[ 1 ] = constant[ 2 ] = 0.0;
	XieFloArithmetic( &flograph[ idx ],
		src1,
		( XIE.isDyadic == True ? src2 : 0 ),
		&domain,
		constant,
		XIE.arithmeticOp,
		0x1
	);
	idx++;
	histoSrc = idx;

        if ( !InsertBackend( backend, X.dpy, X.win3, 0, 0, X.gc,
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

	XieFloExportClientHistogram( &flograph[ idx ],
		histoSrc,
		&domain,        /* get entire image - no ROI */
                xieValNewData   /* send event when new data is ready */ 
        );
	idx++;

	photoSpace = XieCreatePhotospace( X.dpy );
	floId = 1;
	XieExecuteImmediate( X.dpy,
		photoSpace,
		floId,
		True,
		flograph,
		floSize 
	);

	DoHistos( X.dpy, X.win4, X.gc, floId, photoSpace, idx );
        eventData.floId = floId;
        eventData.space = photoSpace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(X.dpy, &eventData, 10L, &event );

	XieFreePhotofloGraph( flograph, floSize );
	XieDestroyPhotospace( X.dpy, photoSpace );
	return( 1 );
}

int
DoMathFlo()
{
        int     idx, src1, src2, histoSrc;
        int     floId, floSize;
	XiePhotospace photoSpace;
        XiePhotoElement *flograph;
	XieProcessDomain domain;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
        XEvent event;

	floSize = 3 + beSize;
	if ( XIE.isDyadic == True )
		floSize++;

        flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;
        XieFloImportPhotomap( &flograph[ idx ],
                XIE.src1Photo,
                False
        );
        idx++;
	src1 = idx;

	if ( XIE.isDyadic == True )
	{
		XieFloImportPhotomap( &flograph[ idx ],
			XIE.src2Photo,
			False
		);
		idx++;
		src2 = idx;
	}

	domain.offset_x = 0;
	domain.offset_y = 0;
	domain.phototag = 0;	

	XieFloMath( &flograph[ idx ],
		src1,
		&domain,
		XIE.mathOp,
		0x1
	);
	idx++;
	histoSrc = idx;

        if ( !InsertBackend( backend, X.dpy, X.win3, 0, 0, X.gc,
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

	XieFloExportClientHistogram( &flograph[ idx ],
		histoSrc,
		&domain,        /* get entire image - no ROI */
                xieValNewData   /* send event when new data is ready */ 
        );
	idx++;

	photoSpace = XieCreatePhotospace( X.dpy );
	floId = 1;
	XieExecuteImmediate( X.dpy,
		photoSpace,
		floId,
		True,
		flograph,
		floSize 
	);

	DoHistos( X.dpy, X.win4, X.gc, floId, photoSpace, idx );
        eventData.floId = floId;
        eventData.space = photoSpace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(X.dpy, &eventData, 10L, &event );

	XieFreePhotofloGraph( flograph, floSize );
	XieDestroyPhotospace( X.dpy, photoSpace );
	return( 1 );
}

int
DoCompareFlo()
{
        int     idx, src1, src2, histoSrc;
        int     floId, floSize;
	XieConstant constant;
	XiePhotospace photoSpace;
        XiePhotoElement *flograph;
	XieProcessDomain domain;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	XEvent	event;

	floSize = 3 + beSize;
	if ( XIE.isDyadic == True )
		floSize++;

        flograph = XieAllocatePhotofloGraph(floSize);

	idx = 0;
        XieFloImportPhotomap( &flograph[ idx ],
                XIE.src1Photo,
                False
        );
        idx++;
	src1 = idx;

	if ( XIE.isDyadic == True )
	{
		XieFloImportPhotomap( &flograph[ idx ],
			XIE.src2Photo,
			False
		);
		idx++;
		src2 = idx;
	}

	domain.offset_x = 0;
	domain.offset_y = 0;
	domain.phototag = 0;	

	constant[ 0 ] = ( XIE.isDyadic == True ? 0.0 : (double) XIE.constant );
	constant[ 1 ] = constant[ 2 ] = 0.0;
	XieFloCompare( &flograph[ idx ],
		src1,
		( XIE.isDyadic == True ? src2 : 0 ),
		&domain,
		constant,
		XIE.compareOp,
		XIE.combine,
		0x1
	);
	idx++;
	histoSrc = idx;

        if ( !InsertBackend( backend, X.dpy, X.win3, 0, 0, X.gc,
                flograph, idx ) )
        {
                fprintf( stderr, "Backend failed\n" );
                exit( 1 );
        }

        idx += beSize;

	XieFloExportClientHistogram( &flograph[ idx ],
		histoSrc,
		&domain,        /* get entire image - no ROI */
                xieValNewData   /* send event when new data is ready */ 
        );
	idx++;

	photoSpace = XieCreatePhotospace( X.dpy );
	floId = 1;
	XieExecuteImmediate( X.dpy,
		photoSpace,
		floId,
		True,
		flograph,
		floSize 
	);

	DoHistos( X.dpy, X.win4, X.gc, floId, photoSpace, idx );
        eventData.floId = floId;
        eventData.space = photoSpace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(X.dpy, &eventData, 10L, &event );

	XieFreePhotofloGraph( flograph, floSize );
	XieDestroyPhotospace( X.dpy, photoSpace );
	return( 1 );
}

void
DoHistos( display, histoWindow, gc, floId, photospace, histSrc )
Display *display;
Window histoWindow;
GC gc;
int floId;
XiePhotospace photospace;
int histSrc;
{
        XieHistogramData *histos;
	int	numHistos;

        histos = ( XieHistogramData * ) NULL;
        numHistos = ReadNotifyExportData( display, xieInfo, photospace, floId, 
		histSrc, sizeof( XieHistogramData ), 0, 0, (char **) &histos ) 
                / sizeof( XieHistogramData );

	XSetForeground( display, gc, BlackPixelOfScreen( X.scr ) );
	XSetBackground( display, gc, WhitePixelOfScreen( X.scr ) );
        DrawHistogram( display, histoWindow, gc, ( XieHistogramData * ) histos,
                numHistos, 256, 256, 256 );
        free( histos );
}
