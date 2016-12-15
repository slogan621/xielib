
/* this is based on xgc's testfrac.c */

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Scrollbar.h>


#define SCROLLBAR_LENGTH 125 
#define SLIDER_LENGTH 0.2	/* proportion of scrollbar taken up
				   by the slider */

static Widget label;		/* the label */
static Widget slider;		/* the scrollbar */
static Widget percent;		/* label with chosen percentage */

static float fraction = 1.0;	/* what percent has been chosen */
static int   oldpercent = -1;	/* so we only update when the slider has
				   been moved */

double
GetConstValue()
{
	return( ( double ) fraction * 255.0 );
}

/* slider_jump(w,data,position)
** ----------------------------
** This function is called if the user moves the scrollbar to a new
** position (generally, by using the middle button).  It updates
** information about where the scrollbar is.
*/

/*ARGSUSED*/
static void
slider_jump(w, data, position)
     Widget w;
     caddr_t data;
     caddr_t position;
{
  static Arg percentargs[] = {
    {XtNlabel,   (XtArgVal) NULL}
  };

  float oldpercent;		/* where the scrollbar is */
  float newpercent;		/* normalized scrollbar */
  char snewpercent[3];		/* string representation of scrollbar */

  oldpercent = *(float *) position;

  /* We want the scrollbar to be at 100% when the right edge of the slider
  ** hits the end of the scrollbar, not the left edge.  When the right edge
  ** is at 1.0, the left edge is at 1.0 - SLIDER_LENGTH.  Normalize
  ** accordingly.  */
   
  newpercent = oldpercent / (1.0 - SLIDER_LENGTH);

  /* If the slider's partially out of the scrollbar, move it back in. */

  if (newpercent > 1.0) {
    newpercent = 1.0;
    XawScrollbarSetThumb( slider, 1.0 - SLIDER_LENGTH, SLIDER_LENGTH);
  }

  /* Store the position of the silder where it can be found */

  *(float *)data = newpercent;

  /* Update the label widget */

  sprintf(snewpercent,"%d",(int)(newpercent*255));
  percentargs[0].value = (XtArgVal) snewpercent;
  XtSetValues(percent, percentargs, XtNumber(percentargs));
  ExecuteBlend( XtDisplay( w ) );
}

/* slider_scroll(w,data,position)
** ------------------------------
** This function is called when the user does incremental scrolling, 
** generally with the left or right button.  Right now it just ignores it.
*/

/*ARGSUSED*/
static void
slider_scroll(w, data, position)
     Widget w;
     caddr_t data;
     caddr_t position;
{}

/*ARGSUSED*/
static void
update(w,event,params,num_params)
     Widget w;
     XEvent *event;
     String *params;
     int *num_params;
{
  char buf[80];
  int newpercent;

  newpercent = (int)(fraction * 255.0);
  if (newpercent != oldpercent) {
    sprintf(buf, "percent %d\n", (int)(fraction * 255.0));
/*    interpret(buf); */
    oldpercent = newpercent;
  }
}

/* CreateSrcConstScroller(w, appcontext)
** -------------------------
** Inside w (a form widget), creates:
**   1. A label 
**   2. A scrollbar for the user to choose the value (from 0 to 255)
**   3. A label with the current percentage displayed on it.
** The percentage starts at 128.
**
** When the pointer leaves the scrollbar, a string is sent to interpret()
** so that it knows the position of the scrollbar.
*/

Widget
CreateSrcConstScroller(w, appcontext, vert, horiz)
Widget w;
XtAppContext appcontext;
Widget vert;
Widget horiz;
{
  static XtCallbackRec jumpcallbacks[] = {
    {(XtCallbackProc) slider_jump, NULL},
    {NULL,                         NULL}
  };

  static XtCallbackRec scrollcallbacks[] = {
    {(XtCallbackProc) slider_scroll, NULL},
    {NULL,                           NULL}
  };

  static Arg labelargs[] = {
    {XtNborderWidth,  (XtArgVal) 0},
    {XtNjustify,      (XtArgVal) XtJustifyRight},
    {XtNvertDistance, (XtArgVal) 20},
    {XtNfromHoriz,    (XtArgVal) NULL},
    {XtNfromVert,     (XtArgVal) NULL}
  };

  static Arg percentargs[] = {
    {XtNborderWidth,    (XtArgVal) 1},
    {XtNhorizDistance,  (XtArgVal) 10},
    {XtNfromHoriz,      (XtArgVal) NULL},
    {XtNfromVert,       (XtArgVal) NULL},
    {XtNvertDistance,   (XtArgVal) 20}
  };

  static Arg scrollargs[] = {
    {XtNorientation,     (XtArgVal) XtorientHorizontal},
    {XtNlength,          (XtArgVal) SCROLLBAR_LENGTH},
    {XtNthickness,       (XtArgVal) 10},
    {XtNshown,           (XtArgVal) 10},
    {XtNhorizDistance,   (XtArgVal) 10},
    {XtNfromHoriz,       (XtArgVal) NULL},
    {XtNjumpProc,        (XtArgVal) NULL},
    {XtNscrollProc,      (XtArgVal) NULL},
    {XtNfromVert,        (XtArgVal) NULL},
    {XtNvertDistance,   (XtArgVal) 20}
  };
    
  static char *translationtable = "<Leave>: Update()";

  static XtActionsRec actiontable[] = {
    {"Update",  (XtActionProc) update},
    {NULL,      NULL}
  };

  /* Let the scrollbar know where to store information where we
  ** can see it */

  jumpcallbacks[0].closure = (caddr_t) &fraction;

  labelargs[ 3 ].value = (XtArgVal) horiz;
  labelargs[ 4 ].value = (XtArgVal) vert;
 
  label = XtCreateManagedWidget("Const",labelWidgetClass,w,
				labelargs,XtNumber(labelargs));

  percentargs[2].value = (XtArgVal) label;
  percentargs[3].value = (XtArgVal) vert;

  percent = XtCreateManagedWidget("255",labelWidgetClass,w,
	  percentargs,XtNumber(percentargs));

  scrollargs[5].value = (XtArgVal) percent;
  scrollargs[6].value = (XtArgVal) jumpcallbacks;
  scrollargs[7].value = (XtArgVal) scrollcallbacks;

  scrollargs[8].value = (XtArgVal) vert;

  slider = XtCreateManagedWidget("Slider",scrollbarWidgetClass,w,
	 scrollargs,XtNumber(scrollargs));

  XtAppAddActions(appcontext,actiontable,XtNumber(actiontable));
  XtOverrideTranslations(slider,XtParseTranslationTable(translationtable));

  /* Start the thumb out at 100% */

  XawScrollbarSetThumb(slider, 1.0 - SLIDER_LENGTH, SLIDER_LENGTH);
  return slider;
}

