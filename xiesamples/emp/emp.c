
/*
Copyright 1996, 1997 Syd Logan 

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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

#include <Xm/MainW.h>
#include <Xm/List.h>
#include <Xm/Form.h>
#include <Xm/TextF.h>
#include <Xm/DrawingA.h>

#include <X11/extensions/XIElib.h>
#include "events.h"
#include "backend.h"

static XmStringCharSet charset = XmSTRING_DEFAULT_CHARSET;

#define MAXLINE	128

/* An entry in the employee "database" */

typedef struct _empdat {
	int 	number;		/* employee serial number */
	int 	code;		/* department number */
	char	*name;		/* employee name */
	char	*street;	/* address information */
	char	*city;		/* "" */	
	char	*state;		/* "" */	
	char	*zip;		/* "" */
	char 	*desc;		/* employee title */
	unsigned long salary;	/* yearly salary */
	char	*image;		/* path to an image file */
	XiePhotomap pmap;	/* photomap resource */
	int	bands;		/* # of bands in image */
	struct _empdat *next;	/* used to maintain list */
} EmpDat;

/* list of employees */

static EmpDat *list = (EmpDat *) NULL;
static Display *display;
static GC gc;
static XieExtensionInfo *xieInfo;
static XiePhotospace photospace;

void ClearEmp( EmpDat *emp );
int LoadEmp( char *where );
int AddEmp( EmpDat *emp );
void AddItem( int number );
static void ListCallback(Widget list_w, XtPointer client_data, XmListCallbackStruct *cbs);
static void QuitCallback(Widget menu_item, int item);
static void RedrawPicture(Widget w, XtPointer client_data, XmDrawingAreaCallbackStruct *cbs);
EmpDat * FindChoice( char *name );
void DisplayPhotomap( EmpDat *p );

static Widget list_w, codeT, nameT, streetT, cityT, stateT, zipT, descT, salaryT, drawingArea; 

static EmpDat *gDrawP = (EmpDat *) NULL;

void
main( argc, argv )
int	argc; 
char *argv[];
{
	char	*empdir;
	XmString quit, file;
    	Widget	toplevel, main_w, rowcol, menu, menubar; 
    	XtAppContext app;
    	Arg     args[10];
	int	n;

	if ( ( empdir = getenv( "EMPDIR" ) ) == (char *) NULL ) 
		empdir = "."; 

	if ( empdir == (char *) NULL ) {
		fprintf( stderr, "Unable to determine directory\n" );
		exit( 1 );
	}

    	toplevel = XtVaAppInitialize(&app, "Demos", NULL, 0,
        	&argc, argv, NULL, NULL);

	display = XtDisplay( toplevel );

	if ( !XieInitialize(display, &xieInfo) ) {
		fprintf( stderr, "XIE not supported on this display!\n" );
		exit( 1 );
	}

	photospace = XieCreatePhotospace( display );

    	main_w = XtVaCreateManagedWidget("main_w", xmMainWindowWidgetClass, 
		toplevel, NULL);

    	file = XmStringCreateSimple("File");
    	menubar = XmVaCreateSimpleMenuBar(main_w, "menubar",
        	XmVaCASCADEBUTTON, file, 'F', NULL);
    	XmStringFree(file);

    	quit = XmStringCreateSimple("Quit");
    	menu = XmVaCreateSimplePulldownMenu(menubar, 
		"file_menu", 0, QuitCallback,
        	XmVaSEPARATOR,
        	XmVaPUSHBUTTON, quit, 'Q', NULL, NULL,
        	NULL);
    	XmStringFree(quit);

    	XtManageChild(menubar);

    	rowcol = XtVaCreateWidget("rowcol", xmFormWidgetClass, main_w, NULL);

    	XtSetArg(args[0], XmNvisibleItemCount, 5);
    	XtSetArg(args[1], XmNtopWidget, menu);
    	XtSetArg(args[2], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNbottomAttachment, XmATTACH_FORM);
    	XtSetArg(args[4], XmNselectionPolicy, XmSINGLE_SELECT);
    	list_w = XmCreateScrolledList(rowcol, "scrolled_list", args, 5);
	XtAddCallback(list_w, XmNsingleSelectionCallback, ListCallback, NULL);
    	XtManageChild(list_w);

    	XtSetArg(args[0], XmNtopWidget, menu);
    	XtSetArg(args[1], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[2], XmNleftAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNleftWidget, list_w);
    	XtSetArg(args[4], XmNwidth, 256);
    	XtSetArg(args[5], XmNheight, 256);
    	drawingArea = XtCreateManagedWidget("drawing", xmDrawingAreaWidgetClass,
		rowcol, args, 6);
	XtAddCallback(drawingArea, XmNexposeCallback, RedrawPicture, 0 ); 

    	XtSetArg(args[0], XmNtopWidget, drawingArea);
    	XtSetArg(args[1], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[2], XmNleftAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNleftWidget, list_w);
    	XtSetArg(args[4], XmNeditable, False);
    	XtSetArg(args[5], XmNcolumns, 26);
    	nameT = XtCreateManagedWidget("nameT", xmTextFieldWidgetClass, 
		rowcol, args, 6);

    	XtSetArg(args[0], XmNtopWidget, drawingArea);
    	XtSetArg(args[1], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[2], XmNleftAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNleftWidget, nameT);
    	XtSetArg(args[4], XmNeditable, False);
    	XtSetArg(args[5], XmNcolumns, 3);
    	XtSetArg(args[6], XmNrightAttachment, XmATTACH_FORM);
    	codeT = XtCreateManagedWidget("codeT", xmTextFieldWidgetClass, 
		rowcol, args, 7);

    	XtSetArg(args[0], XmNtopWidget, codeT);
    	XtSetArg(args[1], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[2], XmNleftAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNleftWidget, list_w);
    	XtSetArg(args[4], XmNeditable, False);
    	XtSetArg(args[5], XmNcolumns, 30);
    	XtSetArg(args[6], XmNrightAttachment, XmATTACH_FORM);
    	streetT = XtCreateManagedWidget("streetT", xmTextFieldWidgetClass, 
		rowcol, args, 7);

    	XtSetArg(args[0], XmNtopWidget, streetT);
    	XtSetArg(args[1], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[2], XmNleftAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNleftWidget, list_w);
    	XtSetArg(args[4], XmNeditable, False);
    	XtSetArg(args[5], XmNcolumns, 15);
    	cityT = XtCreateManagedWidget("cityT", xmTextFieldWidgetClass, 
		rowcol, args, 6);

    	XtSetArg(args[0], XmNtopWidget, streetT);
    	XtSetArg(args[1], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[2], XmNleftAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNleftWidget, cityT);
    	XtSetArg(args[4], XmNeditable, False);
    	XtSetArg(args[5], XmNcolumns, 2);
    	stateT = XtCreateManagedWidget("stateT", xmTextFieldWidgetClass, 
		rowcol, args, 6);

    	XtSetArg(args[0], XmNtopWidget, streetT);
    	XtSetArg(args[1], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[2], XmNleftAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNleftWidget, stateT);
    	XtSetArg(args[4], XmNeditable, False);
    	XtSetArg(args[5], XmNcolumns, 10);
    	XtSetArg(args[6], XmNrightAttachment, XmATTACH_FORM);
    	zipT = XtCreateManagedWidget("zipT", xmTextFieldWidgetClass, 
		rowcol, args, 7);

    	XtSetArg(args[0], XmNtopWidget, zipT);
    	XtSetArg(args[1], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[2], XmNleftAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNleftWidget, list_w);
    	XtSetArg(args[4], XmNeditable, False);
    	XtSetArg(args[5], XmNcolumns, 25);
    	descT = XtCreateManagedWidget("descT", xmTextFieldWidgetClass, 
		rowcol, args, 6);

    	XtSetArg(args[0], XmNtopWidget, zipT);
    	XtSetArg(args[1], XmNtopAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[2], XmNleftAttachment, XmATTACH_WIDGET);
    	XtSetArg(args[3], XmNleftWidget, descT);
    	XtSetArg(args[4], XmNeditable, False);
    	XtSetArg(args[5], XmNcolumns, 5);
    	XtSetArg(args[6], XmNrightAttachment, XmATTACH_FORM);
    	salaryT = XtCreateManagedWidget("salaryT", xmTextFieldWidgetClass, 
		rowcol, args, 7);

    	XtManageChild(rowcol);

	n = LoadEmp( empdir );	
	if ( n == 0 ) {
		fprintf( stderr, "There are no employee records to load\n" );
		exit( 1 );
	}

    	XmMainWindowSetAreas(main_w, menubar, NULL, NULL, NULL, rowcol);

    	XtRealizeWidget(toplevel);
	gc = XCreateGC( display, XtWindow( drawingArea ), 0L, (XGCValues *) NULL );

    	XtAppMainLoop(app);
}

int
LoadEmp( char *where )
{
	FILE	*fp;
	DIR	*dirp;
	struct dirent *direntp;
	struct stat sbuf;
	int	count = 0, ret;
	char	buf[ MAXLINE + 1 ];
	EmpDat 	emp;

	memset( &emp, '\0', sizeof( EmpDat ) );

	ret = stat( where, &sbuf ); 
	if ( ret == -1 ) {
		fprintf( stderr, "LoadRecords: stat on %s failed\n", where );
		return( 0 );
	}
	if ( S_ISDIR( sbuf.st_mode ) == 0 ) {
		fprintf( stderr, "LoadRecords: %s is not a directory\n",
			where );
		return( 0 );
	}

	dirp = opendir( where );
	if ( dirp == (DIR *) NULL ) {
		fprintf( stderr, "LoadRecords: %s could not be opened\n",
			where );
		return( 0 );
	}
	while ( ( direntp = readdir( dirp ) ) != (struct dirent *) NULL ) {

		if ( strstr( direntp->d_name, ".emp" ) == (char *) NULL ) 
			continue;

		if ( ( fp = fopen( direntp->d_name, "r" ) ) == (FILE *) NULL ) {
			fprintf( stderr, "LoadRecords: %s could not be opened\n",
				direntp->d_name );
			continue;
		}

		ClearEmp( &emp );

		/* read the fields */

		/* number */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.number = atoi( buf );

		/* code */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.code = atoi( buf );

		/* name */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.name = (char *) malloc( ret );
		strcpy( emp.name, buf );

		/* street */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.street = (char *) malloc( ret );
		strcpy( emp.street, buf );

		/* city */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.city = (char *) malloc( ret );
		strcpy( emp.city, buf );

		/* state */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.state = (char *) malloc( ret );
		strcpy( emp.state, buf );

		/* zip */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.zip = (char *) malloc( ret );
		strcpy( emp.zip, buf );

		/* desc */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.desc = (char *) malloc( ret );
		strcpy( emp.desc, buf );

		/* salary */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.salary = atol( buf );

		/* image */

		fgets( buf, MAXLINE, fp );
		ret = strlen( buf );
		buf[ret - 1] = '\0';
		if ( ret <= 0 ) {
			fprintf( stderr, "LoadRecords: error reading %s\n",
				direntp->d_name );
			fclose( fp );
			continue;
		}
		emp.image = (char *) malloc( ret );
		strcpy( emp.image, buf );
		if ( !AddEmp( &emp ) ) {
			AddItem( emp.number );
			count++;
		}
		fclose( fp );
	}
	closedir( dirp );
	return( count );
}

void
ClearEmp( EmpDat *emp )
{
	if ( emp == (EmpDat *) NULL )
		return;

	if ( emp->name ) {
		free( emp->name );
		emp->name = 0;
	}
	if ( emp->street ) {
		free( emp->street );
		emp->street = 0;
	}
	if ( emp->city ) {
		free( emp->city );
		emp->city = 0;
	}
	if ( emp->state ) {
		free( emp->state );
		emp->state = 0;
	}
	if ( emp->zip ) {
		free( emp->zip );
		emp->zip = 0;
	}
	if ( emp->desc ) {
		free( emp->desc );
		emp->desc = 0;
	}
	if ( emp->image ) {
		free( emp->image );
		emp->image = 0;
	}
}

int
AddEmp( EmpDat *emp )
{
	EmpDat *newp, *p;

	if ( emp == (EmpDat *) NULL )
		return( 1 );	
	newp = (EmpDat *) malloc( sizeof( EmpDat ) );
	if ( newp == (EmpDat *) NULL )
		return( 1 );
	memcpy( newp, emp, sizeof( EmpDat ) );

	newp->name = (char *) malloc( strlen( emp->name ) + 1 );
	if ( !newp->name ) {
		free( newp );
		return( 1 );
	}
	else
		strcpy( newp->name, emp->name );

	newp->street = (char *) malloc( strlen( emp->street ) + 1 );
	if ( !newp->street ) {
		free( newp->name );
		free( newp );
		return( 1 );
	}
	else
		strcpy( newp->street, emp->street );

	newp->city = (char *) malloc( strlen( emp->city ) + 1 );
	if ( !newp->city ) {
		free( newp->name );
		free( newp->street );
		free( newp );
		return( 1 );
	}
	else
		strcpy( newp->city, emp->city );

	newp->state = (char *) malloc( strlen( emp->state ) + 1 );
	if ( !newp->state ) {
		free( newp->name );
		free( newp->city );
		free( newp->street );
		free( newp );
		return( 1 );
	}
	else
		strcpy( newp->state, emp->state );

	newp->zip = (char *) malloc( strlen( emp->zip ) + 1 );
	if ( !newp->zip ) {
		free( newp->state );
		free( newp->name );
		free( newp->city );
		free( newp->street );
		free( newp );
		return( 1 );
	}
	else
		strcpy( newp->zip, emp->zip );

	newp->desc = (char *) malloc( strlen( emp->desc ) + 1 );
	if ( !newp->desc ) {
		free( newp );
		free( newp->zip );
		free( newp->state );
		free( newp->name );
		free( newp->city );
		free( newp->street );
		return( 1 );
	}
	else
		strcpy( newp->desc, emp->desc );

	newp->image = (char *) malloc( strlen( emp->image ) + 1 );
	if ( !newp->image ) {
		free( newp->desc );
		free( newp->zip );
		free( newp->state );
		free( newp->name );
		free( newp->city );
		free( newp->street );
		free( newp );
		return( 1 );
	}
	else
		strcpy( newp->image, emp->image );

	if ( LoadImage( newp ) ) {
		free( newp->desc );
		free( newp->zip );
		free( newp->state );
		free( newp->name );
		free( newp->city );
		free( newp->street );
		free( newp->image );
		free( newp );
		return( 1 );
	}

	newp->next = (EmpDat *) NULL;
	if ( list == (EmpDat *) NULL ) 
		list = newp;
	else {
		newp->next = list;
		list = newp;
	}	
	return ( 0 );
}

int
LoadImage( EmpDat *newp )
{
	int	floSize, size, decodeTech, floId = 1, idx;
	Bool	notify;
	short 	w, h;
	char	d, l, *bytes;
	XieConstant bias;
	XiePointer decodeParms;
	XiePhotoElement *flograph;
	XieYCbCrToRGBParam *rgbParm = 0;
	XieLTriplet width, height, levels;

	if ( ( newp->pmap = XieCreatePhotomap( display ) ) == (XiePhotomap) NULL )
		return( 1 );

	if ( ( size = GetJFIFData( newp->image, &bytes, &d, &w, &h, &l ) ) == 0 ) {
		XieDestroyPhotomap( display, newp->pmap );
		fprintf( stderr, "Problem getting JPEG data from %s\n", 
			newp->image );
		return( 1 );
	}

	newp->bands = l;

	if ( d != 8 ) {
		XieDestroyPhotomap( display, newp->pmap );
		fprintf( stderr, "Image %s must be 256 levels\n", newp->image );
		return( 1 );
	}

	floSize = (l == 3 ? 3 : 2 );
	flograph = XieAllocatePhotofloGraph( floSize );

	width[0] = width[1] = width[2] = w;
	height[0] = height[1] = height[2] = h;
	levels[0] = levels[1] = levels[2] = 256;

	decodeTech = xieValDecodeJPEGBaseline;
	decodeParms = ( char * ) XieTecDecodeJPEGBaseline( 
		xieValBandByPixel,
		xieValLSFirst,
		True
	);

	idx = 0;
	notify = False;
	XieFloImportClientPhoto(&flograph[idx], 
		(l == 3 ? xieValTripleBand : xieValSingleBand),
		width,			/* width of each band */
		height, 		/* height of each band */
		levels, 		/* levels of each band */
		notify, 		/* no DecodeNotify event */ 
		decodeTech,		/* decode technique */
		decodeParms		/* decode parameters */
	);
	idx++;

	if ( l == 3 ) {
		bias[ 0 ] = 0.0;
		bias[ 1 ] = bias[ 2 ] = 127.0;
		levels[ 0 ] = levels[ 1 ] = levels[ 2 ] = 256;
                rgbParm = XieTecYCbCrToRGB( 
                        levels,
			(double) 0.2125, 
			(double) 0.7154, 
			(double) 0.0721,
                        bias,
                       	xieValGamutNone, 
                        NULL 
		);

	       	XieFloConvertToRGB( &flograph[idx],
			idx,
			xieValYCbCrToRGB,
			(XiePointer) rgbParm
		);
		idx++;
	}

	XieFloExportPhotomap( &flograph[idx],
		idx,
		newp->pmap,
		xieValEncodeServerChoice,
		(XiePointer) NULL
	);
	idx++;

	XieExecuteImmediate( display, photospace, floId, False, flograph,
		floSize );

	PumpTheClientData( display, floId, photospace, 1, bytes, size, 
		sizeof(char), 0, True );

	if ( rgbParm ) 
		XFree( rgbParm );
	free( bytes );
	XieFreePhotofloGraph( flograph, floSize );
	XFree( decodeParms );

	return( 0 );
}

void
AddItem( int number )
{
    	XmString str, *strlist;
    	int	u_bound, l_bound = 0;
    	char 	*text;
    	char 	newtext[ 128 ];

	sprintf( newtext, "%d", number );

    	XtVaGetValues(list_w,
        	XmNitemCount, &u_bound,
        	XmNitems,     &strlist,
        	NULL);

    	u_bound--;

    	while (u_bound >= l_bound) {
        	int i = l_bound + ( u_bound - l_bound ) / 2;

        	if (!XmStringGetLtoR(strlist[i], charset, &text))
            		break;
        	if (strcmp(text, newtext) > 0)
            		u_bound = i - 1; 
        	else
            		l_bound = i + 1; 
        	XtFree(text); 
    	}
    	str = XmStringCreateSimple(newtext); 
    	XmListAddItemUnselected(list_w, str, l_bound + 1);
    	XmStringFree(str);
}

static void
RedrawPicture(Widget w, XtPointer client_data, XmDrawingAreaCallbackStruct *cbs)
{
	if ( gDrawP != (EmpDat *) NULL )
		DisplayPhotomap( gDrawP );		
}

static void
ListCallback(Widget list_w, XtPointer client_data, XmListCallbackStruct *cbs)
{
	char 	*choice, buf[ 32 ];
	EmpDat	*p;

        XmStringGetLtoR(cbs->item, charset, &choice);
	p = FindChoice( choice );
        XtFree(choice);
	if ( p != (EmpDat *) NULL ) {

		/* first do the text fields */

		sprintf( buf, "%d", p->code );
		XmTextFieldSetString( codeT, buf ); 
		XmTextFieldSetString( nameT, p->name ); 
		XmTextFieldSetString( streetT, p->street ); 
		XmTextFieldSetString( cityT, p->city ); 
		XmTextFieldSetString( stateT, p->state ); 
		XmTextFieldSetString( zipT, p->zip ); 
		XmTextFieldSetString( descT, p->desc ); 
		sprintf( buf, "%ld", p->salary );
		XmTextFieldSetString( salaryT, buf ); 

		/* Next, the images */

		gDrawP = p;
		DisplayPhotomap( p );
	}
}

void
DisplayPhotomap( EmpDat *p )
{
	XiePhotoElement *flograph;
	Visual	*visual;
	Backend *backend;
	int	floId = 1, screen, idx, floSize, beSize;
	Display	*display;

	if ( p == (EmpDat *) NULL )
		return;

	display = XtDisplay( drawingArea );
	screen = DefaultScreen( display );
        visual = DefaultVisual( display, screen );

	if ( p->bands == 1 )
		backend = (Backend *) InitBackend( display, screen, 
			visual->class, xieValSingleBand, 
			1<<DefaultDepth( display, screen ), -1, &beSize );
	else
		backend = (Backend *) InitBackend( display, screen, 
			visual->class, xieValTripleBand, 0, -1, &beSize );

        if ( backend == (Backend *) NULL ) {
                fprintf( stderr, "Unable to create backend\n" );
                exit( 1 );
        }

	floSize = 1 + beSize;
	flograph = XieAllocatePhotofloGraph( floSize );

	idx = 0;

	XieFloImportPhotomap( &flograph[idx], p->pmap, False );
	idx++;

	if ( !InsertBackend( backend, display, XtWindow( drawingArea ),
		0, 0, gc, flograph, idx ) ) {
                fprintf( stderr, "Unable to add backend\n" );
                exit( 1 );
	}

	XieExecuteImmediate( display, photospace, floId, False, flograph,
		floSize );

	XieFreePhotofloGraph( flograph, floSize );
	CloseBackend( backend, display );
}

EmpDat *
FindChoice( char *name )
{
	int	number;
	EmpDat	*p;

	p = list;
	number = atoi( name );

	while ( p != (EmpDat *) NULL ) {
		if ( p->number == number )
			break;
		p = p->next;
	}
	return( p );
}

void
QuitCallback(Widget menu_item, int item_no)
{
	exit( 0 );
}

void
_Xsetlocale( void )
{
}

