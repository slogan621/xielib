#include <stdio.h>

#include <X11/Xos.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xatom.h>
#include <X11/Shell.h>
#include <X11/Xaw/Cardinals.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Toggle.h>
#include <X11/Xaw/MenuButton.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Sme.h>
#include <X11/Xaw/SmeBSB.h>

#include <X11/Xaw/Cardinals.h>

#include <X11/extensions/XIElib.h>
#include "events.h"
#include "backend.h"
#include "transform.h"

#define MONWIDTH	250
#define MONHEIGHT	200

String fallback_resources[] = {
    "*fileButton.label:            File",
    "*optionsButton.label:         Options",
    "*fileMenu.label:              File",
    "*optionsMenu.label:           Options",
    "*menuLabel.vertSpace:         100",
    "*blank.height:                20",
    "*.font:			   10x20",
    NULL,
};

XtActionProc
DoNothing(w, event, params, num_params)
Widget w;
XEvent *event;
String *params;
Cardinal *num_params;
{
 return;
} /* DoNothing */

static String  trans = "#override \n\
       <Key>Linefeed:   DoNothing() \n\
       <Key>Return:     DoNothing()";

static XtActionsRec actionTable[] = {
        {"DoNothing",         (XtActionProc) DoNothing},
        {NULL, NULL} };

static Widget currentpopup;
static Widget do_options_menu();
static void message_popup();
static void BuildAndExecuteFlo();
static void DestroyPopupPrompt();
static void MyAppMainLoop();
static void CreatePhotomapsAndStoredFlos();
static void CreateFileMenu();
static void CreateOptionsMenu();
static void GetImageFile();
static void TearDownPopup();
static void DisplayNewImage();
static void CancelPopup();
static void ShowHistos();
static void FlushOutput(); 
int ExecuteBlend();
double GetConstValue();
double GetAlphaConstValue();
Widget CreateSrcConstScroller( Widget toplevelForm, XtAppContext app_con, 
	Widget above, Widget left );
Widget CreateAlphaConstScroller( Widget toplevelForm, XtAppContext app_con, 
	Widget above, Widget left );
static void AlphaToggleCb();
static void UseConstToggleCb();

#define streq(a, b)        ( strcmp((a), (b)) == 0 )

static Widget toplevel, pane, toplevelForm, fileButton, optionsButton;
static char *ProgramName;
static GC gc;
static XieExtensionInfo *xieInfo;
static Widget drawing, histo;
static XiePhotomap photomapA, photomapB, photomapC, photomapD;
static XiePhotoflo histoAFlo, histoBFlo, histoCFlo, histoDFlo, 
	exposeSrc1Flo, exposeSrc2Flo, exposeSrc3Flo, exposeSrc4Flo, 
	exposeAlphaFlo, exposeResultFlo, currentFlo;
Backend *backend = (Backend *) NULL; 
static XiePhotospace photospace;
int	haveSrc1 = 0, haveSrc2 = 0, haveAlpha = 0;
Boolean	useAlpha = False, useConst = True;

static XtActionsRec blend_actions[] = {
  { NULL, NULL }
};

main(argc, argv)
    int	    argc;
    char    *argv[];
{

    Widget toggle1, toggle2, scroller, filemenu, optionsmenu, label;
    Arg	wargs[ 10 ];
    XtAppContext app_con;
    int arg, i;

    ProgramName = argv[0];

    photomapA = photomapB = photomapC = photomapD = (XiePhotomap) NULL;

    toplevel = XtAppInitialize(&app_con, "blend", NULL, ZERO,
	  &argc, argv, fallback_resources, NULL, ZERO);

    /* Connect to XIE extension */

    if (!XieInitialize(XtDisplay(toplevel), &xieInfo)) {
    	fprintf(stderr, "XIE not supported on this display!\n");
        exit(1);
    }

    photospace = XieCreatePhotospace( XtDisplay( toplevel ) );

    XtAppAddActions (app_con, blend_actions, XtNumber (blend_actions));
    XtAppAddActions(app_con, actionTable, XtNumber(actionTable));

    /* pane wrapping everything */

    pane = XtCreateManagedWidget ("pane", panedWidgetClass, 
	toplevel, NULL, ZERO);

    toplevelForm = XtCreateManagedWidget ("form", formWidgetClass, pane, 
	NULL, ZERO);

    fileButton = XtCreateManagedWidget("fileButton", 
	menuButtonWidgetClass, toplevelForm, NULL, 0);

    i = 0;
    XtSetArg( wargs[ i ], XtNfromHoriz, fileButton ); i++;
    optionsButton = XtCreateManagedWidget ("optionsButton", 
	menuButtonWidgetClass, toplevelForm, wargs, i);

    i = 0;
    XtSetArg( wargs[ i ], XtNfromVert, fileButton ); i++;
    XtSetArg( wargs[ i ], XtNlabel, "" ); i++;
    XtSetArg( wargs[ i ], XtNwidth, 640 ); i++;
    XtSetArg( wargs[ i ], XtNheight, 480 ); i++;
    drawing = XtCreateManagedWidget("drawing", labelWidgetClass, 
	toplevelForm, wargs, i);

    i = 0;
    XtSetArg( wargs[ i ], XtNfromVert, fileButton ); i++;
    XtSetArg( wargs[ i ], XtNfromHoriz, drawing ); i++;
    XtSetArg( wargs[ i ], XtNlabel, "" ); i++;
    XtSetArg( wargs[ i ], XtNwidth, MONWIDTH ); i++;
    XtSetArg( wargs[ i ], XtNheight, MONHEIGHT ); i++;
    histo = XtCreateManagedWidget("histo", labelWidgetClass, 
	toplevelForm, wargs, i);

    /* scrollbar used as src constant, range is 0 - 255 */

    scroller = CreateSrcConstScroller( toplevelForm, app_con, histo, drawing );
 
    scroller = CreateAlphaConstScroller( toplevelForm, app_con, scroller, drawing );
 
    /* Use alpha plane toggle button */

    i = 0;
    XtSetArg( wargs[ i ], XtNfromVert, scroller ); i++;
    XtSetArg( wargs[ i ], XtNvertDistance, 25 ); i++;
    XtSetArg( wargs[ i ], XtNfromHoriz, drawing ); i++;
    XtSetArg( wargs[ i ], XtNlabel, "Use Alpha" ); i++;
    XtSetArg( wargs[ i ], XtNhighlightThickness, 1 ); i++;
    XtSetArg( wargs[ i ], XtNstate, False ); i++;

    toggle1 = XtCreateManagedWidget("alphaToggle", toggleWidgetClass, 
	toplevelForm, wargs, i);

    XtAddCallback( toggle1, XtNcallback, AlphaToggleCb, (XtPointer) NULL );

    i = 0;
    XtSetArg( wargs[ i ], XtNfromVert, scroller ); i++;
    XtSetArg( wargs[ i ], XtNvertDistance, 25 ); i++;
    XtSetArg( wargs[ i ], XtNfromHoriz, toggle1 ); i++;
    XtSetArg( wargs[ i ], XtNhorizDistance, 15 ); i++;
    XtSetArg( wargs[ i ], XtNlabel, "Use Constant" ); i++;
    XtSetArg( wargs[ i ], XtNhighlightThickness, 1 ); i++;
    XtSetArg( wargs[ i ], XtNstate, True ); i++;

    toggle2 = XtCreateManagedWidget("constToggle", toggleWidgetClass, 
	toplevelForm, wargs, i);

    XtAddCallback( toggle2, XtNcallback, UseConstToggleCb, (XtPointer) NULL );

    /* Scrollbar used to define alpha constant. Range is
       variable (depends on setting of use alpha toggle */

    filemenu = XtCreatePopupShell("menu", simpleMenuWidgetClass, 
	fileButton, NULL, ZERO);

    optionsmenu = XtCreatePopupShell("menu", simpleMenuWidgetClass, 
	optionsButton, NULL, ZERO);

    CreateFileMenu( filemenu );
    CreateOptionsMenu( optionsmenu ); 

    XtRealizeWidget (toplevel);

    gc = XCreateGC(XtDisplay( drawing ), XtWindow( drawing ), 0L, 
	(XGCValues *) NULL);

    XSetForeground( XtDisplay( drawing ), gc, 
	XBlackPixel( XtDisplay( drawing ), 0 ) );
    XSetBackground( XtDisplay( drawing ), gc, 
	XWhitePixel( XtDisplay( drawing ), 0 ) );

    /* create photomap resources, plus stored photoflos. Note,
       this must be done after widgets are realized so we can
       obtain the resource ID of drawing's window correctly */

    CreatePhotomapsAndStoredFlos( XtDisplay( toplevel ),
	XtWindow( drawing ), gc );

    MyAppMainLoop(app_con);
}

/* We do our own event processing so we can snarf expose events. The
   preferred way would be to register an expose callback, if the widget
   supported such a thing (like Motif's DrawingArea widget does). */

static void 
MyAppMainLoop(app)
XtAppContext app;
{
    XEvent event;

    do {
        XtAppNextEvent(app, &event);
	if ( event.type == Expose && 
		event.xexpose.window == XtWindow(drawing) &&
		event.xexpose.count == 0 ) {

		/* refresh the window ourselves */ 

		if ( currentFlo == exposeSrc1Flo ) 
			FlushOutput( photomapA );
		else if ( currentFlo == exposeSrc2Flo ) 
			FlushOutput( photomapB );
		else if ( currentFlo == exposeAlphaFlo ) 
			FlushOutput( photomapC );
		else if ( currentFlo == exposeResultFlo ) 
			FlushOutput( photomapD );
	}
	else
		XtDispatchEvent(&event);
    } while(1);
}

static void
CreatePhotomapsAndStoredFlos( display, window, gc )
Display	*display;
Window window;
GC gc;
{
        XiePhotoElement *flograph;
       	XieColorAllocAllParam *colorParm;
	int		idx, floSize;
        XieOrientation  fillOrder = xieValLSFirst;
        XieOrientation  pixelOrder = xieValLSFirst;
        unsigned int    pixelStride = 8;
        unsigned int    scanlinePad = 1;
	unsigned int	leftPad = 0;
        char            *encodeParms, *decodeParms;
        int             encodeTech, decodeTech;
	XieLevels	width, height, levels;
	XieProcessDomain domain;
	Visual		*visual;
	int		screen, beSize;

	/* src1 */

	photomapA = XieCreatePhotomap( display );

	/* src2 */

	photomapB = XieCreatePhotomap( display );

	/* alpha src */

	photomapC = XieCreatePhotomap( display );

	/* operation result */

	photomapD = XieCreatePhotomap( display );

	/* IP (A|B|C|D) -> backend (refresh display if expose recv'd) */

	screen = XDefaultScreen( display );
	visual = DefaultVisual(display, screen); 

	backend = (Backend *) InitBackend( display, screen, visual->class,
		xieValSingleBand, 1<<DefaultDepth( display, screen ), -1,
		&beSize );

	if ( backend == (Backend *) NULL ) {
		fprintf( stderr, "Unable to create photoflo backends\n" );
		exit( 1 );
	}	

	idx = 0;
	floSize = 1 + beSize;

        flograph = XieAllocatePhotofloGraph(floSize);
 
	XieFloImportPhotomap( &flograph[ idx ],
                photomapA,
                False
        );
        idx++;

	if ( !InsertBackend( backend, display, window, 0, 0, gc,
		flograph, idx ) ) {
		fprintf( stderr, "Unable to insert backend\n" );
		exit( 1 );
	}

	exposeSrc1Flo = XieCreatePhotoflo( display,
		flograph,
		floSize
	);

	idx = 0;

	XieFloImportPhotomap( &flograph[ idx ],
                photomapB,
                False
        );
        idx++;

	if ( !InsertBackend( backend, display, window, 0, 0, gc,
		flograph, idx ) )
	{
		fprintf( stderr, "Unable to insert backend\n" );
		exit( 1 );
	}

	exposeSrc2Flo = XieCreatePhotoflo( display,
		flograph,
		floSize
	);

	idx = 0;

	XieFloImportPhotomap( &flograph[ idx ],
                photomapC,
                False
        );
        idx++;

	if ( !InsertBackend( backend, display, window, 0, 0, gc,
		flograph, idx ) )
	{
		fprintf( stderr, "Unable to insert backend\n" );
		exit( 1 );
	}

	exposeAlphaFlo = XieCreatePhotoflo( display,
		flograph,
		floSize
	);

	idx = 0;

	XieFloImportPhotomap( &flograph[ idx ],
                photomapD,
                False
        );
        idx++;

	if ( !InsertBackend( backend, display, window, 0, 0, gc,
		flograph, idx ) )
	{
		fprintf( stderr, "Unable to insert backend\n" );
		exit( 1 );
	}

	exposeResultFlo = XieCreatePhotoflo( display,
		flograph,
		floSize
	);

       	XieFreePhotofloGraph( flograph, floSize );

        /* IP(A|B|C|D) -> ECH (used to send histogram to client for display) */

        idx = 0;
        floSize = 2;

	domain.phototag = 0;

        flograph = XieAllocatePhotofloGraph(floSize);
 
        XieFloImportPhotomap( &flograph[ idx ],
                photomapA,
                False
        );
        idx++;

        XieFloExportClientHistogram(&flograph[idx],
		idx,            /* source phototag number */
                &domain,        /* get entire image - no ROI */
                xieValNewData   /* send event when new data is ready */ 
        );
        idx++;

        histoAFlo = XieCreatePhotoflo( display,
                flograph,
                floSize
        );

        XieFloImportPhotomap( &flograph[ 0 ],
                photomapB,
                False
        );

        histoBFlo = XieCreatePhotoflo( display,
                flograph,
                floSize
        );

        XieFloImportPhotomap( &flograph[ 0 ],
                photomapC,
                False
        );

        histoCFlo = XieCreatePhotoflo( display,
                flograph,
                floSize
        );

        XieFloImportPhotomap( &flograph[ 0 ],
                photomapD,
                False
        );

        histoDFlo = XieCreatePhotoflo( display,
                flograph,
                floSize
        );

        XieFreePhotofloGraph( flograph, floSize );

}

static void
AlphaToggleCb(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
	static Arg args[] = {
		{XtNstate,  (XtArgVal) NULL }
	};
        
        args[0].value = (XtArgVal) &useAlpha;
	XtGetValues( w, args, XtNumber( args ) );
}

static void
UseConstToggleCb(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
	static Arg args[] = {
		{XtNstate,  (XtArgVal) NULL }
	};
        
        args[0].value = (XtArgVal) &useConst;
	XtGetValues( w, args, XtNumber( args ) );
}

static void
FileMenuSelect(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    if (streq(XtName(w), "Import src1")) 
	BuildAndExecuteFlo( w, photomapA );
    else if (streq(XtName(w), "Import src2")) 
	BuildAndExecuteFlo( w, photomapB );
    else if (streq(XtName(w), "Import alpha image")) 
	BuildAndExecuteFlo( w, photomapC );
    else if (streq(XtName(w), "Exit")) {
	XieDestroyPhotoflo( XtDisplay( drawing ), exposeSrc1Flo );
	XieDestroyPhotoflo( XtDisplay( drawing ), exposeSrc2Flo );
	XieDestroyPhotoflo( XtDisplay( drawing ), exposeAlphaFlo );
	XieDestroyPhotoflo( XtDisplay( drawing ), exposeResultFlo );
	XieDestroyPhotoflo( XtDisplay( drawing ), histoAFlo );
	XieDestroyPhotoflo( XtDisplay( drawing ), histoBFlo );
	XieDestroyPhotoflo( XtDisplay( drawing ), histoCFlo );
	XieDestroyPhotoflo( XtDisplay( drawing ), histoDFlo );
	XieDestroyPhotospace( XtDisplay( drawing ), photospace );
        XtDestroyApplicationContext(XtWidgetToApplicationContext(w));
	exit( 0 );
    }
}

static void
BuildAndExecuteFlo( w, photomap )
Widget	w;
XiePhotomap photomap;
{
	GetImageFile( XtParent( w ), photomap );
}

static int
LoadAndShowImage( w, img_file, photomap )
Widget	w;
char	*img_file;
XiePhotomap photomap;
{
        Display         *display = XtDisplay( w );
        char            *bytes = NULL;
        int             size, floId, idx, src, floSize;
        XieLTriplet     width, height, levels;
        short           wi, hi;
        XEvent          event;
        char            *decodeParms;
        int             decodeTech;
        Bool            notify;
        char            deep, bands;
        XiePointer      sampleParms;
	XieGeometryTechnique sampleTech;
	XieConstant	constant;
        XiePhotoElement *flograph;
        XIEEventCheck   eventData;      /* passed to WaitForXIEEvent */
	Arg		args[ 2 ];

	/* following used in scaling the image to fit the window with
	   Geometry */
	
        int     	xr, yr;
        unsigned int 	wr, hr;
        unsigned int 	bwr, dr;
        transformHandle handle;
        float   	coeffs[6];
        float   	sx, sy;
        Window  	root;

        if ( ( size = GetJFIFData( img_file, &bytes, &deep, &wi, &hi, 
		&bands ) ) == 0 )
		return;

        if ( deep != 8 || bands != 1 )
		return;

        floSize = 3;

        flograph = XieAllocatePhotofloGraph(floSize);

        width[0] = wi;
        width[1] = width[2] = 0;
        height[0] = hi;
        height[1] = height[2] = 0;
        levels[0] = 256;
        levels[1] = levels[2] = 0;

        decodeTech = xieValDecodeJPEGBaseline;
        decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
                xieValBandByPixel,
                xieValLSFirst,
                True
        );

        if ( decodeParms == (char *) NULL) {
                fprintf(stderr, "Couldn't allocate decode parms\n");
		return;
        }

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

        XGetGeometry( XtDisplay( drawing ), XtWindow( drawing ), 
		&root, &xr, &yr, &wr, &hr, &bwr, &dr );
        sx = (float) wi / wr;
        sy = (float) hi / hr; 
        handle = CreateScale( sx, sy );
        SetCoefficients( handle, coeffs );
        FreeTransformHandle( handle );

        constant[ 0 ] = 128.0;

	sampleTech = xieValGeomNearestNeighbor;
        sampleParms = ( XieGeometryTechnique ) NULL; 

        XieFloGeometry(&flograph[idx],
                idx,                    /* image source */
                wr,                     /* width of resulting image */
                hr,                     /* height of resulting image */
                coeffs,                 /* a, b, c, d, e, f */
                constant,               /* used if src pixel does not exist */ 
                0x7,               	/* ignored for SingleBand images */
                sampleTech,             /* sample technique... */ 
                sampleParms             /* and parameters, if required */
        );
        idx++;

	XieFloExportPhotomap(&flograph[idx],
		idx,
		photomap,
		xieValEncodeServerChoice,
		(XiePointer) NULL
	);

        floId = 1;
        notify = True;

        /* run the flo */

        XieExecuteImmediate(display, photospace, floId, notify, flograph,
            floSize);

        /* now that the flo is running, send image data */

        PumpTheClientData( display, floId, photospace, 1, bytes, size, 
                sizeof( char ), 0, True );

        /* for fun, wait for the flo to finish */

        eventData.floId = floId;
        eventData.space = photospace;
        eventData.base = xieInfo->first_event;
        eventData.which = xieEvnNoPhotofloDone;
        WaitForXIEEvent(display, &eventData, 10L, &event );

        /* free up what we allocated */

        XieFreePhotofloGraph(flograph, floSize);
        XFree(decodeParms);
        free( bytes );

	if ( photomap == photomapA )
		haveSrc1 = 1;
	else if ( photomap == photomapB )
		haveSrc2 = 1;
	else if ( photomap == photomapC )
		haveAlpha = 1;

	FlushOutput( photomap );
}

static void
FlushOutput( XiePhotomap photomap )
{
#if 0
	XClearWindow( XtDisplay( drawing ), XtWindow( drawing ) );
#endif

	if ( photomap == photomapA ) 
		XieExecutePhotoflo( XtDisplay( drawing ),
			currentFlo = exposeSrc1Flo,
			False
		);
	else if ( photomap == photomapB )
		XieExecutePhotoflo( XtDisplay( drawing ),
			currentFlo = exposeSrc2Flo,
			False
		);
	else if ( photomap == photomapC )
		XieExecutePhotoflo( XtDisplay( drawing ),
			currentFlo = exposeAlphaFlo,
			False
		);
	else if ( photomap == photomapD )
		XieExecutePhotoflo( XtDisplay( drawing ),
			currentFlo = exposeResultFlo,
			False
		);

	ShowHistos( XtDisplay( drawing ), photomap );
}

static void	
CreateFileMenu( button )
Widget	button;
{
    int	i;
    Widget entry;
    static char * file_menu_item_names[] = {
        "Import src1", "Import src2", "Import alpha image", "Exit"
    };

    for (i = 0; i < (int) XtNumber(file_menu_item_names) ; i++) {
        char * item = file_menu_item_names[i];

        entry = XtCreateManagedWidget(item, smeBSBObjectClass, button,
                                      NULL, ZERO);
        XtAddCallback(entry, XtNcallback, FileMenuSelect, NULL);
    }
}

static void	
CreateOptionsMenu( button )
Widget	button;
{
    int	i;
    Widget entry;
    static void OptionsMenuSelect();
    static char * options_menu_item_names[] = {
        "View src1", "View src2", "View alpha image", "View result"
    };

    for (i = 0; i < (int) XtNumber(options_menu_item_names) ; i++) {
        char * item = options_menu_item_names[i];

        entry = XtCreateManagedWidget(item, smeBSBObjectClass, button,
                                      NULL, ZERO);
        XtAddCallback(entry, XtNcallback, OptionsMenuSelect, NULL);
    }
}

static void
OptionsMenuSelect(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    Display	*display = XtDisplay( drawing );

    if (streq(XtName(w), "View src1") && haveSrc1 ) 
	FlushOutput( photomapA );
    else if (streq(XtName(w), "View src2") && haveSrc2 ) 
	FlushOutput( photomapB );
    else if (streq(XtName(w), "View alpha image") && haveAlpha ) 
	FlushOutput( photomapC );
    else if (streq(XtName(w), "View result") ) 
	ExecuteBlend( display );
}

int 
ExecuteBlend( display )
Display *display;
{
        static XiePhotoElement *floGraph = (XiePhotoElement *) NULL;
	int		floId, src1, src2, alphaSrc, idx, floSize;
	double		alphaConst;
	Bool		notify;
	XieProcessDomain domain = { 0, 0, 0 };
	XieConstant	srcConst = { 0.0, 0.0, 0.0 };

	if ( !haveSrc1 )
		return( 0 );

	srcConst[ 0 ] = GetConstValue();
	alphaConst = GetAlphaConstValue();
	floSize = 3;
	if ( haveAlpha && useAlpha == True ) {
		alphaConst = alphaConst * 255;
		if ( alphaConst == 0.0 )
			alphaConst = 0.1;
		floSize++;
	}
	if ( haveSrc2 && useConst == False )
		floSize++;
	idx = 0;

	/* allocate only once. Note we allocate largest size it
           could possibly be, not actual size of photoflo. Also,
	   first element is always ImportPhotomap so just set it
           once */

	if ( floGraph == (XiePhotoElement *) NULL ) {

		floGraph = XieAllocatePhotofloGraph( 5 );

		XieFloImportPhotomap(&floGraph[idx],
			photomapA,
			False
		);
	}

	idx++;
	src1 = idx;

	src2 = 0;
	if ( haveSrc2 && useConst == False ) {
		XieFloImportPhotomap(&floGraph[idx],
			photomapB,
			False
		);
		idx++;
		src2 = idx;
	}

	alphaSrc = 0;
	if ( haveAlpha && useAlpha == True ) {
		XieFloImportPhotomap(&floGraph[idx],
			photomapC,
			False
		);
		idx++;
		alphaSrc = idx;
	}

	XieFloBlend(&floGraph[ idx ],
		src1,
		src2,
		srcConst,
		alphaSrc,
		alphaConst,
		&domain,
		0x01
	);
	idx++;
		
	XieFloExportPhotomap(&floGraph[idx],
		idx,
		photomapD,
		xieValEncodeServerChoice,
		(XiePointer) NULL
	);

        floId = 1;
        notify = True;

        /* run the flo */

        XieExecuteImmediate(display, photospace, floId, notify, floGraph,
            floSize);

	FlushOutput( photomapD );

	return( 1 );
}

static void
ShowHistos( display, photomap )
Display *display;
XiePhotomap photomap;
{
    XieHistogramData *histos;
    int         numHistos;
    XiePhotoflo flo;

    /* get the exported histogram and display */

    if ( photomap == photomapA ) 
	flo = histoAFlo;
    else if ( photomap == photomapB )
	flo = histoBFlo;
    else if ( photomap == photomapC )
	flo = histoCFlo;
    else if ( photomap == photomapD )
	flo = histoDFlo;
    else
	    return;

    XieExecutePhotoflo( display,
	flo,
	False
    );

    histos = ( XieHistogramData * ) NULL;

    numHistos = ReadNotifyExportData( display, xieInfo, 0, flo, 2, 
        sizeof( XieHistogramData ), 0, 0, (char **) 
        &histos ) / sizeof( XieHistogramData );

    DrawHistogram( display, XtWindow( histo ), gc, 
        (XieHistogramData *) histos,
        numHistos, 256, MONWIDTH, MONHEIGHT );

    free( histos );
}

static void 
message_popup(w, string, vector)
Widget w;
char *string;
void ( *vector )();
{
    Arg         args[5];
    Widget      popup, dialog;
    Position    x, y;
    Dimension   width, height;
    Cardinal    n;

    /*
     * This will position the upper left hand corner of the popup at the
     * center of the widget which invoked this callback, which will also
     * become the parent of the popup.  I don't deal with the possibility
     * that the popup will be all or partially off the edge of the screen.
     */

    n = 0;
    XtSetArg(args[0], XtNwidth, &width); n++;
    XtSetArg(args[1], XtNheight, &height); n++;
    XtGetValues(w, args, n);
    XtTranslateCoords(w, (Position) (width / 2), (Position) (height / 2),
                      &x, &y);

    n = 0;
    XtSetArg(args[n], XtNx, x);                         n++;
    XtSetArg(args[n], XtNy, y);                         n++;

    currentpopup = popup = XtCreatePopupShell("message", 
	transientShellWidgetClass, w, args, n);

    n = 0;
    XtSetArg( args[ n ], XtNlabel, string ); n++;
    dialog = XtCreateManagedWidget("msg_dialog", dialogWidgetClass, popup,
	args, n);

    /*
     * The prompting message's size is dynamic; allow it to request resize.
     */

    n = 0;
    XtSetArg( args[ n ], XtNlabel, string ); n++;
    XtCreateManagedWidget("msg_label", labelWidgetClass, dialog, args, n);
    XawDialogAddButton(dialog, "Ok", vector, (XtPointer) dialog);

    XtPopup(popup, XtGrabExclusive);
}

static void
DestroyPopupPrompt(widget, client_data, call_data)
Widget  widget;
XtPointer client_data, call_data;
{
    Widget popup = XtParent( (Widget) client_data);
    TearDownPopup( popup );
}

static void	
TearDownPopup( w )
Widget	w;
{
    XtDestroyWidget(w);
}

typedef struct _getImageFileCb {
	Widget 		w;
	XiePhotomap 	pmap;
} GetImageFileCbType;

static void	
GetImageFile( w, photomap )
Widget	w;
XiePhotomap photomap;
{
    Arg         args[10];
    Widget 	button1, button2;
    Widget	label1, text1;
    Widget      popup, form1;
    Position    x, y;
    Dimension   width, height;
    Cardinal    n;
    Widget	menu;
    void	DisplayNewImage(), CancelPopup();
    static GetImageFileCbType callData;

    String  trans = "#override \n\
       <Key>Linefeed:   DoNothing() \n\
       <Key>Return:     DoNothing()";

    n = 0;
    XtSetArg(args[0], XtNwidth, &width); n++;
    XtSetArg(args[1], XtNheight, &height); n++;
    XtGetValues(w, args, n);
    XtTranslateCoords(w, (Position) (width / 2), (Position) (height / 2),
    	&x, &y);

    n = 0;
    XtSetArg(args[n], XtNx, x); n++;
    XtSetArg(args[n], XtNy, y); n++;

    currentpopup = popup = XtCreatePopupShell("popup", 
	transientShellWidgetClass, w, args, n);

    form1 = XtCreateManagedWidget( "form1", formWidgetClass, popup,
        (Arg *) NULL, 0 );

    XtSetArg(args[0], XtNlabel, "Pathname");
    label1 = XtCreateManagedWidget("label1", labelWidgetClass, form1, args, 1);

    XtSetArg(args[0], XtNfromHoriz, label1);
    XtSetArg(args[1], XtNeditType, XawtextEdit);
    XtSetArg(args[2], XtNwidth, 500);
    XtSetArg(args[3], XtNtranslations, XtParseTranslationTable(trans));
    text1 = XtCreateManagedWidget("text1", asciiTextWidgetClass, form1,
    	args, 4);

    XtSetArg(args[0], XtNfromVert, label1);
    XtSetArg(args[1], XtNlabel, "Ok");
    button1 = XtCreateManagedWidget("button1", commandWidgetClass, 
        form1, args, 2);
    callData.w = text1;
    callData.pmap = photomap;
    XtAddCallback( button1, XtNcallback, DisplayNewImage, 
        (XtPointer) &callData );

    XtSetArg(args[0], XtNfromHoriz, button1);
    XtSetArg(args[1], XtNfromVert, label1);
    XtSetArg(args[2], XtNlabel, "Cancel");
    button2 = XtCreateManagedWidget("button2", commandWidgetClass, 
        form1, args, 3);
    XtAddCallback( button2, XtNcallback, CancelPopup, (XtPointer) NULL );

    XtPopup(popup, XtGrabExclusive);
}

static void
MatchHistoTechCb(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
	Widget	toggle = (Widget) junk;
       	Arg     wargs[ 5 ];
        Boolean state;

        XtSetArg(wargs[0], XtNstate, &state); 
        XtGetValues(w, wargs, 1);
        if ( state == False )
        {
        	XtSetArg( wargs[ 0 ], XtNstate, True );
                XtSetValues( toggle, wargs, 1 );
        }
	else
	{
        	XtSetArg( wargs[ 0 ], XtNstate, False );
                XtSetValues( toggle, wargs, 1 );
	}
}

static void 
DisplayNewImage (w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
	GetImageFileCbType *myjunk = (GetImageFileCbType *) junk;
	Widget	text = (Widget) myjunk->w;
	Arg 	args[1];
	String	str;

	XtSetArg( args[0], XtNstring, &str );
	XtGetValues( text, args, 1 );
	LoadAndShowImage( w, str, (XiePhotomap) myjunk->pmap );
        XtDestroyWidget(currentpopup);
}

static void 
CancelPopup (w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    XtDestroyWidget(currentpopup);
}

