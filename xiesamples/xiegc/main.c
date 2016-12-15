
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

/*

Copyright (c) 1991  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/

/* xgc
**
** main.c
**
** Contains the bare minimum necessary to oversee the whole operation.
*/

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Shell.h>
#include <stdio.h>

#include "xgc.h"

#include "backend.h"
#include "main.h"

static void fill_up_commandform();
extern void run_test();
static void quit();
static void quitAction();
static void clear_test_window();
static void clear_result_window();
static void set_foreground_and_background();
extern ChoiceDesc *create_choice();
extern void choose_defaults();
extern void line_up_labels();
extern Widget create_text_choice();
extern void create_planemask_choice();
extern void create_dashlist_choice();
extern void create_testfrac_choice();
extern void GC_change_foreground();
extern void GC_change_background();
extern void GC_change_font();

XStuff X;			/* GC stuff plus some global variables */
Boolean recording = FALSE;	/* Whether we're recording into a file */
XtAppContext appcontext;	/* To make Xt happy */
static Atom wm_delete_window;
static XtActionsRec actions[] = {
    {"quit",	quitAction}
};

static Widget bigdaddy;		/* the top level widget */
       Widget topform;		/* form surrounding the whole thing */
       Widget GCform;		/* form in which you choose the GC */
static Widget Testform;		/* form in which you choose the test */
       Widget testchoiceform;   /* form inside that */
  ChoiceDesc *testchoicedesc;	/* record of what widgets are in the
				   test choice form */
static Widget commandform;	/* form with run, quit, clear, etc. */
       Widget test1, test2, 
	      test3;		/* where the test is run */
       Widget result;           /* where the results are displayed */
       Widget histo;
static Widget runbutton;	/* command for running */
static Widget quitbutton;	/* command for quitting */
static Widget clearbutton;	/* command for clearing the test window */
       Widget recordbutton;	/* start/stop recording */
static Widget playbackbutton;	/* playback from file */
static Widget keyinputbutton;	/* start reading from keyboard */
static Widget GCchoices[NUMCHOICES]; /* all the forms that contain stuff
				        for changing GC's*/
  ChoiceDesc *GCdescs[NUMCHOICES]; /* record of the widgets inside
				      the choice widgets */
       Widget planemaskchoice;	/* form for choosing the plane mask */
       Widget dashlistchoice;	/* form for choosing the dash list */
static Widget linewidthchoice;	/* form for choosing line width */
       Widget linewidthtext;	/* text widget within that */
static Widget fontchoice;	/* form for choosing the font */
       Widget fonttext;		/* text widget within that */
static Widget foregroundchoice;	/* form for choosing foreground */
       Widget foregroundtext;	/* text widget within that */
static Widget backgroundchoice;	/* form for choosing background */
       Widget backgroundtext;	/* text widget within that */
static Widget percentchoice;	/* form for choosing percentage of test */

/* main(argc.argv)
** ---------------
** Initializes the toolkit, initializes data, puts up the widgets,
** starts the event loop.
*/

XIEStuff XIE;

void
main(argc,argv)
     int argc;
     char **argv;
{
  extern Backend *backend;
  void	usage();

  static Arg shellargs[] = {
    {XtNinput, 	      (XtArgVal) True}
  };

  static Arg testformargs[] = {	
    {XtNfromVert,     (XtArgVal) NULL} /* put it under GCform */
  };

  static Arg commandformargs[] = {
    {XtNfromVert,    (XtArgVal) NULL}, /* put it under GCform */
    {XtNfromHoriz,   (XtArgVal) NULL}  /* and to the right of Testform */
  };

  static Arg test1args[] = {
    {XtNheight,     (XtArgVal) MONWIDTH},
    {XtNwidth,      (XtArgVal) MONWIDTH},
    {XtNfromHoriz,  (XtArgVal) NULL} /* put it to the right of GCform */
  };

  static Arg test2args[] = {
    {XtNheight,     (XtArgVal) MONWIDTH},
    {XtNwidth,      (XtArgVal) MONWIDTH},
    {XtNfromHoriz,  (XtArgVal) NULL} /* put it to the right of test1 */
  };

  static Arg test3args[] = {
    {XtNheight,     (XtArgVal) MONWIDTH},
    {XtNwidth,      (XtArgVal) MONWIDTH},
    {XtNfromHoriz,  (XtArgVal) NULL},/* put it to the right of Testform */
    {XtNfromVert,   (XtArgVal) NULL} /* and under test2 */
  };

  static Arg histoargs[] = {
    {XtNheight,     (XtArgVal) MONHEIGHT},
    {XtNwidth,      (XtArgVal) MONWIDTH},
    {XtNfromHoriz,  (XtArgVal) NULL}, /* put it to the right of GCform */
    {XtNfromVert,   (XtArgVal) NULL} /* and under test */
  };

  static Arg resultargs[] = {
    {XtNheight,     (XtArgVal) 50},
    {XtNwidth,      (XtArgVal) MONWIDTH},
    {XtNfromHoriz,  (XtArgVal) NULL}, /* put it to the right of GCform */
    {XtNfromVert,   (XtArgVal) NULL} /* and under test */
  };

  static Arg gcchoiceargs[] = {
    {XtNfromVert,    (XtArgVal) NULL}, /* put it under the one above it */
    {XtNfromHoriz,   (XtArgVal) NULL}, /* and next to that one */
    {XtNborderWidth, (XtArgVal) 0}     /* no ugly borders */
  };

  static Arg testchoiceargs[] = {
    {XtNborderWidth, (XtArgVal) 0}
  };

  int i;			/* counter */

  /* Open the pipe */

#ifdef notdef
  pipe(fildes);
  outend = fdopen(fildes[0],"r");
#endif

  /* Initialize toolkit stuff */

  bigdaddy = XtAppInitialize(&appcontext, "Xgc", (XrmOptionDescList) NULL,
			     (Cardinal) 0, &argc, argv, (String *) NULL,
			     shellargs, XtNumber(shellargs));
  X.dpy = XtDisplay(bigdaddy);

  /* Connect to XIE extension */

  if ( !InitXIE( X.dpy ) )
  {
  	fprintf(stderr, "Unable to initialize XIE\n");
	exit( 1 );
  }

  /* get the image file arguments */

  if ( argc != 3 )
  {
	usage( argv[ 0 ] );
	exit( 1 );
  }
 
  if ( !LoadSingleBandJPEGPhotomap(X.dpy, argv[ 1 ], &XIE.src1Photo ) ||
	!LoadSingleBandJPEGPhotomap(X.dpy, argv[ 2 ], &XIE.src2Photo ) )
  {
	usage( argv[ 0 ] );
	exit( 1 );
  }

  XtAppAddActions(appcontext, actions, XtNumber(actions));
  XtOverrideTranslations
      (bigdaddy, XtParseTranslationTable("<Message>WM_PROTOCOLS: quit()"));

  /* Initialize GC stuff */

  X.scr = DefaultScreenOfDisplay(X.dpy);
  X.gc = XCreateGC(X.dpy,RootWindowOfScreen(X.scr),0,(XGCValues *) NULL);
  X.miscgc = XCreateGC(X.dpy,RootWindowOfScreen(X.scr),0,(XGCValues *) NULL);

  /* Find out what the foreground & background are, and update the GC
  ** accordingly */

  topform = XtCreateManagedWidget("topform",formWidgetClass,bigdaddy,
				  NULL,0);

  GCform = XtCreateManagedWidget("GCform",formWidgetClass,topform,
				NULL,0);

  /* create all the GCchoices forms */

  for (i=0;i<NUMCHOICES;++i) {
    if (i==0)			/* on top */
      gcchoiceargs[0].value = (XtArgVal) NULL;
    else			/* under the last one */
      gcchoiceargs[0].value = (XtArgVal) GCchoices[i-1];

    GCchoices[i] = XtCreateManagedWidget(Everything[i]->choice.text,
					 formWidgetClass,GCform,
					 gcchoiceargs,XtNumber(gcchoiceargs));

    /* now fill up that form */
    GCdescs[i] = create_choice(GCchoices[i],Everything[i]);
  }

  gcchoiceargs[0].value = (XtArgVal) GCchoices[NUMCHOICES-1]; 
  foregroundchoice = XtCreateManagedWidget("constant",formWidgetClass,GCform,
				   gcchoiceargs,XtNumber(gcchoiceargs));
  foregroundtext = create_text_choice(foregroundchoice,TForeground,4,50);

  /* make all the labels inside the choices line up nicely */
  line_up_labels(GCdescs,(int) XtNumber(GCdescs));

  /* put the test form under the GC form */
  testformargs[0].value = (XtArgVal) GCform;
  Testform = XtCreateManagedWidget("Testform",formWidgetClass,topform,
				   testformargs,XtNumber(testformargs));
  
  testchoiceform = XtCreateManagedWidget("testchoiceform",formWidgetClass,
			     Testform,testchoiceargs,XtNumber(testchoiceargs));
  testchoicedesc = create_choice(testchoiceform,Everything[CTest]);

  commandformargs[0].value = (XtArgVal) GCform;
  commandformargs[1].value = (XtArgVal) Testform;
  commandform = XtCreateManagedWidget("commandform",formWidgetClass,topform,
			      commandformargs,XtNumber(commandformargs));

  /* Put the appropriate command buttons in the command form */

  fill_up_commandform(commandform);

  test1args[2].value = (XtArgVal) GCform;    /* to the right of */
  test1 = XtCreateManagedWidget("test1",widgetClass,topform,
			       test1args,XtNumber(test1args));

  test2args[2].value = (XtArgVal) test1;    /* to the right of */
  test2 = XtCreateManagedWidget("test2",widgetClass,topform,
			       test2args,XtNumber(test2args));

  test3args[2].value = (XtArgVal) GCform;    /* to the right of */
  test3args[3].value = (XtArgVal) test1; /* under */
  test3 = XtCreateManagedWidget("test3",widgetClass,topform,
			       test3args,XtNumber(test3args));

  histoargs[2].value = (XtArgVal) test1; /* to the right of */
  histoargs[3].value = (XtArgVal) test2; /* under */
  histo = XtCreateManagedWidget("histo",widgetClass,topform,
				 histoargs,XtNumber(histoargs));

  resultargs[2].value = (XtArgVal) test1; /* to the right of */
  resultargs[3].value = (XtArgVal) histo; /* under */
  result = XtCreateManagedWidget("result",asciiTextWidgetClass,topform,
				 resultargs,XtNumber(resultargs));

  /* Now realize all the widgets */

  XtRealizeWidget(bigdaddy);

  /* Now do things we couldn't do until we had a window available */

  X.win1 = XtWindow(test1);
  X.win2 = XtWindow(test2);
  X.win3 = XtWindow(test3);
  X.win4 = XtWindow(histo);

  wm_delete_window = XInternAtom(X.dpy, "WM_DELETE_WINDOW", False);
  (void) XSetWMProtocols(X.dpy, XtWindow(bigdaddy), &wm_delete_window, 1);

  /* Act like the user picked the first choice in each group */

  choose_defaults(GCdescs,(int)XtNumber(GCdescs));
  choose_defaults(&testchoicedesc,1);
  
  BuildRefreshPhotoflos( &X, &XIE );
  RefreshSources( &X, &XIE );

  /* Loop forever, dealing with events */

  XtAppMainLoop(appcontext);
}

/* fill_up_commandform(w)
** ----------------------
** Put the appropriate command buttons in the command form (w).
*/

static void
fill_up_commandform(w)
     Widget w;
{
  static XtCallbackRec runcallbacklist[] = {
    {(XtCallbackProc) run_test,  NULL},
    {NULL,                       NULL}
  };

  static XtCallbackRec quitcallbacklist[] = {
    {(XtCallbackProc) quit,      NULL},
    {NULL,                       NULL}
  };

  static XtCallbackRec clearcallbacklist[] = {
    {(XtCallbackProc) clear_test_window,    NULL},
    {(XtCallbackProc) clear_result_window,  NULL},
    {NULL,                                  NULL}
  };

  static Arg runargs[] = {
    {XtNcallback,    (XtArgVal) NULL}
  };

  static Arg clearargs[] = {
    {XtNcallback,    (XtArgVal) NULL},
    {XtNfromHoriz,    (XtArgVal) NULL} /* put it next to runbutton */
  };

  static Arg quitargs[] = {
    {XtNcallback,    (XtArgVal) NULL},
    {XtNfromVert,    (XtArgVal) NULL}, /* put it under runbutton */
    {XtNvertDistance,(XtArgVal) 7}
  };

  runargs[0].value = (XtArgVal) runcallbacklist;
  runbutton = XtCreateManagedWidget("Run",commandWidgetClass,
			      w,runargs,XtNumber(runargs));

  clearargs[0].value = (XtArgVal) clearcallbacklist;
  clearargs[1].value = (XtArgVal) runbutton; /* under */
  clearbutton = XtCreateManagedWidget("Clear/Refresh",commandWidgetClass,
         		      w,clearargs,XtNumber(clearargs));

  quitargs[0].value = (XtArgVal) quitcallbacklist;
  quitargs[1].value = (XtArgVal) runbutton; /* under */
  quitbutton = XtCreateManagedWidget("Quit",commandWidgetClass,
   			      w,quitargs,XtNumber(quitargs));
    
}    
/* quit()
** ------
** Leave the program nicely.
*/

static void
quit()
{
  exit(0);
}

static void quitAction(w, e, p, n)
    Widget w;
    XEvent *e;
    String *p;
    Cardinal *n;
{
    if (e->type == ClientMessage && e->xclient.data.l[0] != wm_delete_window)
	XBell(XtDisplay(w), 0);
    else 
	quit();
}

/* clear_test_window()
** -------------------
** Clear the test window.
*/

static void
clear_test_window()
{
  XClearWindow(X.dpy,XtWindow(test3));
  RefreshSources( &X, &XIE );
}

/* clear_result_window()
** ---------------------
** Clear the result window.
*/

static void
clear_result_window()
{
  XClearWindow(X.dpy,XtWindow(result));
  RefreshSources( &X, &XIE );
}

void
usage( name )
char	*name;
{
	fprintf( stderr, "usage: %s src1 src2\n", name );
}

