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
static void BuildAndExecuteFlo1();
static void DestroyPopupPrompt();
static void MyAppMainLoop();
static void CreatePhotomapsAndStoredFlos();
static void CreateFileMenu();
static void CreateOptionsMenu();
static void DoMedianFilter();
static void DoMatchHistogram();
static void GetImageFile();
static void TearDownPopup();
static void MatchHistoTechCb();
static void ExecuteMatchHistogram();
static void DisplayNewImage();
static void CancelPopup();
static void ShowHistos();

#define streq(a, b)        ( strcmp((a), (b)) == 0 )

static Widget toplevel, pane, toplevelForm, fileButton, optionsButton;
static char *ProgramName;
static GC gc;
static XieExtensionInfo *xieInfo;
static Widget drawing, histo;
static XiePhotomap photomapA, photomapB;
static XiePhotoflo histoFlo, exportFlo, importFlo, exposeFlo, restoreFlo;
static XieColorList exposeClist;
static Bool photomapHasData = False;

static XtActionsRec hybrid_actions[] = {
  { NULL, NULL }
};

main(argc, argv)
    int	    argc;
    char    *argv[];
{

    Widget filemenu, optionsmenu;
    Arg	wargs[ 10 ];
    XtAppContext app_con;
    int arg, i;

    ProgramName = argv[0];

    photomapA = photomapB = (XiePhotomap) NULL;

    toplevel = XtAppInitialize(&app_con, "hybrid", NULL, ZERO,
                          &argc, argv, fallback_resources, NULL, ZERO);

    /* Connect to XIE extension */

    if (!XieInitialize(XtDisplay(toplevel), &xieInfo)) {
    	fprintf(stderr, "XIE not supported on this display!\n");
        exit(1);
    }

    XtAppAddActions (app_con, hybrid_actions, XtNumber (hybrid_actions));
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

    filemenu = XtCreatePopupShell("menu", simpleMenuWidgetClass, 
	fileButton, NULL, ZERO);

    optionsmenu = XtCreatePopupShell("menu", simpleMenuWidgetClass, 
	optionsButton, NULL, ZERO);

    CreateFileMenu( filemenu );
    CreateOptionsMenu( optionsmenu ); 

    XtRealizeWidget (toplevel);

    gc = XCreateGC(XtDisplay( drawing ), XtWindow( drawing ), 
	0L, (XGCValues *) NULL);

    XSetForeground( XtDisplay( drawing ), gc, 
	XBlackPixel( XtDisplay( drawing ), 0 ) );
    XSetBackground( XtDisplay( drawing ), gc, 
	XWhitePixel( XtDisplay( drawing ), 0 ) );

    /* create photomap resources, plus stored photoflos. Note,
       this must be done after widgets are realized so we can
       obtain the resource ID of drawing's window correctly */

    CreatePhotomapsAndStoredFlos( XtDisplay( toplevel ) );

    MyAppMainLoop(app_con);
}

/* We must do our own event processing so we can snarf expose events. The
   preferred way would be to register an expose callback, if the widget
   supported such a thing (like Motif's DrawingArea widget does) */

static void 
MyAppMainLoop(app)
XtAppContext app;
{
    XEvent event;

    do {
        XtAppNextEvent(app, &event);
	if ( event.type == Expose && 
		event.xexpose.window == XtWindow(drawing) &&
		event.xexpose.count == 0 &&
		photomapHasData == True )
	{
		/* refresh the window ourselves */ 

		XieExecutePhotoflo( XtDisplay( drawing ),
			exposeFlo,
			False
		);

		ShowHistos( XtDisplay( drawing ) );
	}
	else
		XtDispatchEvent(&event);
    } while(1);
}

static void
CreatePhotomapsAndStoredFlos( display )
Display	*display;
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

	photomapA = XieCreatePhotomap( display );
	photomapB = XieCreatePhotomap( display );

	/* IP(A) -> EP(B) (used to restore working image to original) */

	idx = 0;
	floSize = 2;

        flograph = XieAllocatePhotofloGraph(floSize);
 
	XieFloImportPhotomap( &flograph[ idx ],
                photomapA,
                False
        );
        idx++;

	XieFloExportPhotomap( &flograph[ idx ],
		idx,
		photomapB,
		xieValEncodeServerChoice,
		(XiePointer) NULL
	);

	restoreFlo = XieCreatePhotoflo( display,
		flograph,
		floSize
	);

       	XieFreePhotofloGraph( flograph, floSize );

	/* IP (B) -> ED (used to refresh display if expose event recv'd) */

	idx = 0;
	floSize = 3;

        flograph = XieAllocatePhotofloGraph(floSize);
 
	XieFloImportPhotomap( &flograph[ idx ],
                photomapB,
                False
        );
        idx++;

	exposeClist = XieCreateColorList( display );
        colorParm = XieTecColorAllocAll( 128 ); /* mid range fill */
        XieFloConvertToIndex( &flograph[idx],
                idx,                            /* source element */
                DefaultColormap(display, 
                        DefaultScreen(display)),/* colormap to alloc */
                exposeClist,                    /* colorlist */
                True,                           /* notify if problems */
                xieValColorAllocAll,            /* color alloc tech */
                ( char * ) colorParm            /* technique parms */
        );
        idx++;

	XieFloExportDrawable( &flograph[ idx ],
                idx,    
                XtWindow( drawing ),
                gc,
                0,
                0
	);

	exposeFlo = XieCreatePhotoflo( display,
		flograph,
		floSize
	);

       	XieFreePhotofloGraph( flograph, floSize );
        XFree( colorParm );

	/* IP(B) -> ECP (used to send image to client for processing) */

	idx = 0;
	floSize = 2;

        flograph = XieAllocatePhotofloGraph(floSize);
 
	XieFloImportPhotomap( &flograph[ idx ],
                photomapB,
                False
        );
        idx++;

        encodeTech = xieValEncodeUncompressedSingle;
        encodeParms = ( char * ) XieTecEncodeUncompressedSingle(
                fillOrder,
                pixelOrder,
                pixelStride,
                scanlinePad
        );

        XieFloExportClientPhoto(&flograph[idx],
                idx,            /* source */
                xieValNewData,  /* send ExportAvailable events */
                encodeTech,
                encodeParms
        );
        idx++;

	exportFlo = XieCreatePhotoflo( display,
		flograph,
		floSize
	);

       	XieFreePhotofloGraph( flograph, floSize );
	XFree( encodeParms );

	/* ICP -> EP(B) (used to retrieve image after processing by client) */

	idx = 0;
	floSize = 2;

        flograph = XieAllocatePhotofloGraph(floSize);

        decodeTech = xieValDecodeUncompressedSingle;
        decodeParms = ( char * ) XieTecDecodeUncompressedSingle(
                fillOrder,
                pixelOrder,
                pixelStride,
		leftPad,
                scanlinePad
        );

	width[ 0 ] = 640;
	width[ 1 ] = width[ 2 ] = 0;
	height[ 0 ] = 480;
	height[ 1 ] = height[ 2 ] = 0;
	levels[ 0 ] = 256; 
	levels[ 1 ] = levels[ 2 ] = 0;

        XieFloImportClientPhoto(&flograph[idx],
                xieValSingleBand,       /* class of image data */
                width,                  /* width of each band */
                height,                 /* height of each band */
                levels,                 /* levels of each band */
                False,                 	/* no DecodeNotify events */
                decodeTech,             /* decode technique */
                (char *) decodeParms    /* decode parameters */
        );
        idx++;

        XieFloExportPhotomap(&flograph[idx],
                idx,            /* source */
		photomapB,
		xieValEncodeServerChoice,
		(XiePointer) NULL
        );
        idx++;

	importFlo = XieCreatePhotoflo( display,
		flograph,
		floSize
	);

       	XieFreePhotofloGraph( flograph, floSize );
	XFree( decodeParms );

        /* IP(B) -> ECH (used to send histogram to client for display) */

        idx = 0;
        floSize = 2;

	domain.phototag = 0;

        flograph = XieAllocatePhotofloGraph(floSize);
 
        XieFloImportPhotomap( &flograph[ idx ],
                photomapB,
                False
        );
        idx++;

        XieFloExportClientHistogram(&flograph[idx],
		idx,            /* source phototag number */
                &domain,        /* get entire image - no ROI */
                xieValNewData   /* send event when new data is ready */ 
        );
        idx++;

        histoFlo = XieCreatePhotoflo( display,
                flograph,
                floSize
        );

        XieFreePhotofloGraph( flograph, floSize );

}

static void
FileMenuSelect(w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    if (streq(XtName(w), "Import Grayscale JPEG")) {
	BuildAndExecuteFlo1( w );
    }
    else if (streq(XtName(w), "Exit")) {
	XieDestroyColorList( XtDisplay( drawing ), exposeClist );
	XieDestroyPhotoflo( XtDisplay( drawing ), exportFlo );
	XieDestroyPhotoflo( XtDisplay( drawing ), importFlo );
	XieDestroyPhotoflo( XtDisplay( drawing ), exposeFlo );
	XieDestroyPhotoflo( XtDisplay( drawing ), restoreFlo );
	XieDestroyPhotoflo( XtDisplay( drawing ), histoFlo );
        XtDestroyApplicationContext(XtWidgetToApplicationContext(w));
	exit( 0 );
    }
}

static void
BuildAndExecuteFlo1( w )
Widget	w;
{
	GetImageFile( XtParent( w ) );
}

static int
LoadAndShowImage( w, img_file )
Widget	w;
char	*img_file;
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
        XieColorList    myClist;
        XieColorAllocAllParam *colorParm;
        XiePointer      sampleParms;
	XieGeometryTechnique sampleTech;
	XieConstant	constant;
        XiePhotoElement *flograph;
        XiePhotospace   photospace;
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
		&bands ) ) == 0 ) {
		return;
        }

        if ( deep != 8 || bands != 1 ) {
		return;
        }

        floSize = 6;

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
		message_popup( drawing, "Couldn't allocate decode parms",
			DestroyPopupPrompt );
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
	src = idx;

        myClist = XieCreateColorList( display );
        colorParm = XieTecColorAllocAll( 128 ); /* mid range fill */
        XieFloConvertToIndex( &flograph[idx],
                idx,                            /* source element */
                DefaultColormap(display, 
                        DefaultScreen(display)),/* colormap to alloc */
                myClist,                        /* colorlist */
                True,                           /* notify if problems */
                xieValColorAllocAll,            /* color alloc tech */
                ( char * ) colorParm            /* technique parms */
        );
        idx++;

        XieFloExportDrawable(&flograph[idx], 
                idx,            /* source */
                XtWindow( drawing ),  /* drawable to send data to */
		gc,
                0,              /* x offset in window to place data */
                0               /* y offset in window to place data */
        );
        idx++;

	XieFloExportPhotomap(&flograph[idx],
		src,
		photomapA,
		xieValEncodeServerChoice,
		(XiePointer) NULL
	);
	idx++;

	XieFloExportPhotomap(&flograph[idx],
		src,
		photomapB,
		xieValEncodeServerChoice,
		(XiePointer) NULL
	);
	idx++;

        floId = 1;
        notify = True;
        photospace = XieCreatePhotospace(display);

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

        XFree( colorParm );
        XieDestroyColorList( display, myClist );
        XieFreePhotofloGraph(flograph, floSize);
        XieDestroyPhotospace(display, photospace);
        XFree(decodeParms);
        free( bytes );

	photomapHasData = True;

	ShowHistos( display );
}

static void	
CreateFileMenu( button )
Widget	button;
{
    int	i;
    Widget entry;
    static char * file_menu_item_names[] = {
        "Import Grayscale JPEG", "Exit"
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
        "Match Histogram", "Median Filter", "Revert To Original" 
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

    if ( photomapHasData == False )
	return;

    if (streq(XtName(w), "Match Histogram") ) {
	DoMatchHistogram( toplevel );
    }
    else if (streq(XtName(w), "Median Filter") ) {
	DoMedianFilter( toplevel );
    }
    else if (streq(XtName(w), "Revert To Original") ) {
	XieExecutePhotoflo( display, 
        	restoreFlo,
                False
        );
	XieExecutePhotoflo( display,
        	exposeFlo,
                False
        );
    	ShowHistos( display );
    }
}

static void
ShowHistos( display )
Display *display;
{
    XieHistogramData *histos;
    int         numHistos;

    /* get the exported histogram and display */

    XieExecutePhotoflo( display,
	histoFlo,
	False
    );

    histos = ( XieHistogramData * ) NULL;

    numHistos = ReadNotifyExportData( display, xieInfo, 0, histoFlo, 2, 
        sizeof( XieHistogramData ), 0, 0, (char **) 
        &histos ) / sizeof( XieHistogramData );

    DrawHistogram( display, XtWindow( histo ), gc, 
        ( XieHistogramData * ) histos,
        numHistos, 256, MONWIDTH, MONHEIGHT );

    free( histos );
}

static void
DoMedianFilter( w )
Widget	w;
{
	char	*data = NULL;
	int	i, count;
	Display	*display = XtDisplay( w );

	/* export to the client */

        XieExecutePhotoflo( display,
                exportFlo,
                False
        );

	/* read the image data */

	count = ReadNotifyExportData( display, xieInfo, 0, exportFlo,
		2, 1, 100, 0, &data );

	/* perform the filtering */

	MedianFilter( data, 3, 640, 480 );

	/* send the image data back to the client */
	
	XieExecutePhotoflo( display,
		importFlo,
		False
	);

        PumpTheClientData( display, importFlo, 0, 1, data, count,
                sizeof( char ), 0, True );

	/* refresh the window */

        XieExecutePhotoflo( display,
                exposeFlo,
                False
        );

	free( data );

	ShowHistos( display );
}

static void
DoMatchHistogram( w )
Widget	w;
{
    Arg         args[10];
    static Widget widgets[ 5 ];
    Widget 	button1, button2, toggle1, toggle2, toggle3, toggle4;
    Widget	label1, label2, label3, label4, label5, text1, text2, text3;
    Widget      popup, form1;
    Position    x, y;
    Dimension   width, height;
    Cardinal    n;
    Widget	menu;
    void	CancelPopup(), MatchHistoTechCb(), ExecuteMatchHistogram();

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
        NULL, ZERO );

    XtSetArg(args[0], XtNlabel, "Technique");
    label4 = XtCreateManagedWidget("label4", labelWidgetClass, form1, args, 1);

    XtSetArg(args[0], XtNlabel, "Gaussian");
    XtSetArg(args[1], XtNstate, True);
    XtSetArg(args[2], XtNfromHoriz, label4 ); 
    toggle1 = XtCreateManagedWidget("MatchHistoTechToggle1", toggleWidgetClass,
        form1, args, 3);

    XtSetArg(args[0], XtNlabel, "Hyperbolic");
    XtSetArg(args[1], XtNstate, False);
    XtSetArg(args[2], XtNfromHoriz, toggle1 ); 
    toggle2 = XtCreateManagedWidget("MatchHistoTechToggle2", toggleWidgetClass,
        form1, args, 3);
    XtAddCallback( toggle2, XtNcallback, MatchHistoTechCb, 
	(XtPointer) toggle1 );
    XtAddCallback( toggle1, XtNcallback, MatchHistoTechCb, 
	(XtPointer) toggle2 );

    XtSetArg(args[0], XtNlabel, "Hyperbolic Constant");
    XtSetArg(args[1], XtNfromVert, toggle2);
    XtSetArg(args[2], XtNvertDistance, 30 );
    label1 = XtCreateManagedWidget("label1", labelWidgetClass, form1, args, 3);

    XtSetArg(args[0], XtNfromVert, toggle2);
    XtSetArg(args[1], XtNfromHoriz, label1);
    XtSetArg(args[2], XtNeditType, XawtextEdit);
    XtSetArg(args[3], XtNwidth, 50);
    XtSetArg(args[4], XtNtranslations, XtParseTranslationTable(trans));
    XtSetArg(args[5], XtNvertDistance, 30 );
    text1 = XtCreateManagedWidget("text1", asciiTextWidgetClass, form1,
    	args, 6);

    XtSetArg(args[0], XtNlabel, "Hyperbolic Increasing");
    XtSetArg(args[1], XtNfromVert, label1 );
    label5 = XtCreateManagedWidget("label5", labelWidgetClass, form1, args, 2);

    XtSetArg(args[0], XtNlabel, "True");
    XtSetArg(args[1], XtNstate, True);
    XtSetArg(args[2], XtNfromVert, label1 ); 
    XtSetArg(args[3], XtNfromHoriz, label5 ); 
    toggle3 = XtCreateManagedWidget("MatchHistoTechToggle3", toggleWidgetClass,
        form1, args, 4);

    XtSetArg(args[0], XtNlabel, "False");
    XtSetArg(args[1], XtNstate, False);
    XtSetArg(args[2], XtNfromVert, label1 ); 
    XtSetArg(args[3], XtNfromHoriz, toggle3 ); 
    toggle4 = XtCreateManagedWidget("MatchHistoTechToggle4", toggleWidgetClass,
        form1, args, 4);
    XtAddCallback( toggle3, XtNcallback, MatchHistoTechCb, 
        (XtPointer) toggle4 );
    XtAddCallback( toggle4, XtNcallback, MatchHistoTechCb, 
        (XtPointer) toggle3 );

    XtSetArg(args[0], XtNlabel, "Gaussian Mean");
    XtSetArg(args[1], XtNfromVert, toggle3 );
    XtSetArg(args[2], XtNvertDistance, 30 );
    label2 = XtCreateManagedWidget("label2", labelWidgetClass, form1, args, 3);

    XtSetArg(args[0], XtNfromHoriz, label2);
    XtSetArg(args[1], XtNfromVert, toggle3);
    XtSetArg(args[2], XtNeditType, XawtextEdit);
    XtSetArg(args[3], XtNwidth, 50);
    XtSetArg(args[4], XtNtranslations, XtParseTranslationTable(trans));
    XtSetArg(args[5], XtNvertDistance, 30 );
    text2 = XtCreateManagedWidget("text2", asciiTextWidgetClass, form1,
    	args, 6);

    XtSetArg(args[0], XtNlabel, "Gaussian Sigma");
    XtSetArg(args[1], XtNfromVert, label2 );
    label3 = XtCreateManagedWidget("label3", labelWidgetClass, form1, args, 2);

    XtSetArg(args[0], XtNfromHoriz, label3);
    XtSetArg(args[1], XtNfromVert, label2);
    XtSetArg(args[2], XtNeditType, XawtextEdit);
    XtSetArg(args[3], XtNwidth, 50);
    XtSetArg(args[4], XtNtranslations, XtParseTranslationTable(trans));
    text3 = XtCreateManagedWidget("text3", asciiTextWidgetClass, form1,
    	args, 5);

    XtSetArg(args[0], XtNfromVert, label3);
    XtSetArg(args[1], XtNlabel, "Ok");
    XtSetArg(args[2], XtNvertDistance, 30 );
    button1 = XtCreateManagedWidget("button1", commandWidgetClass, 
        form1, args, 3);
    XtAddCallback( button1, XtNcallback, ExecuteMatchHistogram, 
        (XtPointer) widgets );
    widgets[ 0 ] = toggle1;
    widgets[ 1 ] = toggle3;
    widgets[ 2 ] = text1;
    widgets[ 3 ] = text2;
    widgets[ 4 ] = text3;

    XtSetArg(args[0], XtNfromHoriz, button1);
    XtSetArg(args[1], XtNfromVert, label3);
    XtSetArg(args[2], XtNlabel, "Cancel");
    XtSetArg(args[3], XtNvertDistance, 30 );
    button2 = XtCreateManagedWidget("button2", commandWidgetClass, 
        form1, args, 4);
    XtAddCallback( button2, XtNcallback, CancelPopup, 
        (XtPointer) NULL );

    XtPopup(popup, XtGrabExclusive);
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

static void	
GetImageFile( w )
Widget	w;
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
        NULL, ZERO );

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
    XtAddCallback( button1, XtNcallback, DisplayNewImage, 
        (XtPointer) text1 );

    XtSetArg(args[0], XtNfromHoriz, button1);
    XtSetArg(args[1], XtNfromVert, label1);
    XtSetArg(args[2], XtNlabel, "Cancel");
    button2 = XtCreateManagedWidget("button2", commandWidgetClass, 
        form1, args, 3);
    XtAddCallback( button2, XtNcallback, CancelPopup, 
        (XtPointer) NULL );

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
ExecuteMatchHistogram (w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
	/* toggle1, toggle3, text1, text2, text3 */

	Widget	*widgets = (Widget *) junk;
        Arg     wargs[ 5 ];
        Boolean notify, state;
	Boolean	increasing = True;
	int	idx, floId, floSize, matchTech;
        String  str;
	Display *display = XtDisplay( w );
	char	*shapeParms;
       	XiePhotoElement *flograph;
	XieProcessDomain domain;
	XiePhotospace photospace;
	float	constant, mean, sigma;

        XtDestroyWidget(currentpopup);

        XtSetArg(wargs[0], XtNstate, &state); 
        XtGetValues(widgets[ 0 ], wargs, 1);
	if ( state == True )
		matchTech = xieValHistogramGaussian;
	else
		matchTech = xieValHistogramHyperbolic;	

	if ( matchTech == xieValHistogramHyperbolic )
	{
		XtGetValues(widgets[ 1 ], wargs, 1);
		if ( state == True )
			increasing = True;
		else
			increasing = False;	
		XtSetArg(wargs[0], XtNstring, &str);
        	XtGetValues(widgets[2], wargs, 1);
		constant = atof( str );
		shapeParms = ( char * ) XieTecHistogramHyperbolic(
                	constant, increasing );
	}
	else
	{
		XtSetArg(wargs[0], XtNstring, &str);
                XtGetValues(widgets[3], wargs, 1);
                mean = atof( str );
                XtGetValues(widgets[4], wargs, 1);
                sigma = atof( str );
		if ( sigma <= 0.0 )
		{
			message_popup( drawing, "Sigma was <= 0.0. Setting to 1.0",
				DestroyPopupPrompt );
			sigma = 1.0;
		}
		shapeParms = ( char * ) XieTecHistogramGaussian(
			mean, sigma );
	}

        idx = 0;
        floSize = 3;

        flograph = XieAllocatePhotofloGraph(floSize);
 
        XieFloImportPhotomap( &flograph[ idx ],
                photomapA,
                False
        );
        idx++;

	domain.phototag = 0;
	domain.offset_x = 0;
	domain.offset_y = 0;

	XieFloMatchHistogram(&flograph[idx],
		idx,            /* source */
		&domain,        /* process entire image - no ROI */
		matchTech,      /* histogram shape technique */
		shapeParms      /* technique parameters */
	);
	idx++;

        XieFloExportPhotomap( &flograph[ idx ],
                idx,
                photomapB,
                xieValEncodeServerChoice,
                (XiePointer) NULL
        );

        floId = 1;
        notify = False;
        photospace = XieCreatePhotospace(display);

        /* run the flo */

        XieExecuteImmediate(display, photospace, floId, notify, flograph,
            floSize);

	XFree( shapeParms );
	XieDestroyPhotospace( display, photospace );
        XieFreePhotofloGraph(flograph, floSize);

	/* refresh the display */

        XieExecutePhotoflo( XtDisplay( drawing ),
                exposeFlo,
                False
        );
	
	ShowHistos( XtDisplay( drawing ) );
}

static void 
DisplayNewImage (w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
	Widget	text = (Widget) junk;
	Arg 	args[1];
	String	str;

	XtSetArg( args[0], XtNstring, &str );
	XtGetValues( text, args, 1 );
	LoadAndShowImage( w, str );
        XtDestroyWidget(currentpopup);
}

static void 
CancelPopup (w, junk, garbage)
Widget w;
XtPointer junk, garbage;
{
    XtDestroyWidget(currentpopup);
}

