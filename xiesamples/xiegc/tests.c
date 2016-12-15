
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
#include <X11/Xaw/AsciiText.h>
#include <X11/Xos.h>
#include <stdio.h>
#include <math.h>
#include "xgc.h"
#ifdef SVR4
#define SYSV
#endif
#ifndef SYSV
#include <sys/timeb.h>
#include <sys/resource.h>
#endif

#ifndef PI
#define PI 3.14159265
#endif

#ifdef SYSV
#define random lrand48
#endif

#ifndef sgi
extern long random();
#endif

extern XStuff X;
extern XIEStuff XIE;
extern Widget result;

void show_result();

/* timer(flag)
** -----------
** When called with StartTimer, starts the stopwatch and returns nothing.
** When called with EndTimer, stops the stopwatch and returns the time
** in microseconds since it started.
**
** Uses rusage() so we can subtract the time used by the system and user
** from our timer, and just concentrate on the time used in the X calls.
*/

static long
timer(flag)
     int flag;
{
#ifndef SYSV
  static struct timeval starttime;  /* starting time for gettimeofday() */
  struct timeval endtime;           /* ending time for gettimeofday() */
  static struct rusage startusage;  /* starting time for getrusage() */
  struct rusage endusage;           /* ending time for getrusage() */
  struct timezone tz;               /* to make gettimeofday() happy */

  long elapsedtime;                 /* how long since we started the timer */

  switch (flag) {
    case StartTimer:                       /* store initial values */
      gettimeofday(&starttime,&tz);       
      getrusage(RUSAGE_SELF,&startusage);
      return((long) NULL);
    case EndTimer:
      gettimeofday(&endtime,&tz);          /* store final values */
      getrusage(RUSAGE_SELF,&endusage);

  /* all the following line does is use the formula 
     elapsed time = ending time - starting time, but there are three 
     different timers and two different units of time, ack... */

      elapsedtime = (long) ((long)
	((endtime.tv_sec - endusage.ru_utime.tv_sec - endusage.ru_stime.tv_sec
	 - starttime.tv_sec + startusage.ru_utime.tv_sec
	 + startusage.ru_stime.tv_sec)) * 1000000) + (long)
      ((endtime.tv_usec - endusage.ru_utime.tv_usec - endusage.ru_stime.tv_usec
	 - starttime.tv_usec + startusage.ru_utime.tv_usec
	 + startusage.ru_stime.tv_usec));
      return(elapsedtime);                

    default:                              
      fprintf(stderr,"Invalid flag in timer()\n");
      return((long) NULL);
    }
#else
  static long starttime;
  
  switch (flag) {
    case StartTimer:
      time(&starttime);
      return((long) NULL);
    case EndTimer:
      return( (time(NULL) - starttime) * 1000000);
    default:
      fprintf(stderr,"Invalid flag in timer()\n");
      return((long) NULL);
    }
#endif
}


void
logical_test()
{
  long totaltime;
  char buf[80];

  XSync(X.dpy,0);
  timer(StartTimer);
  DoLogicalFlo();
  XSync(X.dpy,0);
  totaltime = timer(EndTimer);

  sprintf(buf,"Logical: %.2f seconds.",(double)totaltime/1000000.);
  show_result(buf);
}

void
arithmetic_test()
{
  long totaltime;
  char buf[80];

  if ( XIE.isDyadic == True )
  {
	if ( XIE.arithmeticOp == xieValGamma || 
	     XIE.arithmeticOp == xieValMul ||
	     XIE.arithmeticOp == xieValDiv || 
             XIE.arithmeticOp == xieValDivRev )
	{
		sprintf( buf, "Operator invalid for dyadic" );
		goto out;
	}			
  }		
  XSync(X.dpy,0);
  timer(StartTimer);
  DoArithmeticFlo();
  XSync(X.dpy,0);
  totaltime = timer(EndTimer);
  sprintf(buf,"Arithmetic: %.2f seconds.",(double)totaltime/1000000.);
out:  show_result(buf);

}

void
math_test()
{
  long totaltime;
  char buf[80];

  if ( XIE.isDyadic == True )
  {
	sprintf( buf, "Math does not support dyadic inputs" );
	goto out;
  }		
  XSync(X.dpy,0);
  timer(StartTimer);
  DoMathFlo();
  XSync(X.dpy,0);
  totaltime = timer(EndTimer);
  sprintf(buf,"Math: %.2f seconds.",(double)totaltime/1000000.);
out:
  show_result(buf);
}

void
compare_test()
{
  long totaltime;
  char buf[80];

  XSync(X.dpy,0);
  timer(StartTimer);
  DoCompareFlo();
  XSync(X.dpy,0);
  totaltime = timer(EndTimer);
  sprintf(buf,"Compare: %.2f seconds.",(double)totaltime/1000000.);
  show_result(buf);
}

/*****************************/
/*****************************/

void
run_test()
{

  XClearWindow(X.dpy,X.win3);
  RefreshSources( &X, &XIE );
  switch (X.test) {
    case LogicalTest:      	
	logical_test();           
	break;
    case ArithmeticTest:   	
	arithmetic_test();          
	break;
    case CompareTest:     	
	compare_test();          
	break;
    case MathTest:      	
	math_test();           
	break;
    default: 
	fprintf(stderr,"That test doesn't exist yet.\n");
	break;
    }
}

/*****************************/

/* set_text(w,string)
** ------------------
** Sets the text in a read-only text widget to the specified string.
*/

void
set_text(w,string)
     Widget w;
     char *string;
{
  static Arg args[1];

  XtSetArg(args[0], XtNstring, string);
  XtSetValues(w, args, (Cardinal) 1 );
}

void
show_result(string)
     char *string;
{
  char buf[80];

  set_text(result,string);

  strcpy(buf,"# ");
  strcat(buf,string);
  strcat(buf,"\n");
}

