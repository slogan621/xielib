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
** interpret.c
**
** interprets and executes lines in the Xgc syntax.
*/

#include <stdio.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include "xgc.h"

void change_text();
void GC_change_logical();
void GC_change_arithmetic();
void GC_change_constant();
void GC_change_math();
void GC_change_fillstyle();
void GC_change_compare();
void GC_change_compare_combine();
void GC_change_type();
void GC_change_planemask();
void change_test();

extern void update_dashlist();
extern void update_planemask();
extern void update_slider();
extern void select_button();
extern void run_test();

extern XgcStuff TestStuff;
extern XgcStuff LogicalStuff;
extern XgcStuff ArithmeticStuff;
extern XgcStuff MathStuff;
extern XgcStuff CompareCombineStuff;
extern XgcStuff CompareStuff;
extern XgcStuff TypeStuff;

extern XStuff X;
extern XIEStuff XIE;
extern ChoiceDesc *GCdescs[];
extern ChoiceDesc *testchoicedesc;
extern Widget test;
extern Widget GCform;
extern Widget foregroundtext;
extern Widget backgroundtext;
extern Widget linewidthtext;
extern Widget fonttext;
extern Widget dashlistchoice;
extern Widget planemaskchoice;
extern Widget testchoiceform;

extern int fildes[2];
extern FILE *outend;
extern FILE *yyin;

/* interpret(string)
** -----------------
** Takes string, which is a line written in the xgc syntax, figures
** out what it means, and passes the buck to the right procedure.
** That procedure gets called with feedback set to FALSE; interpret()
** is only called if the user is selecting things interactively.
**
** This procedure will go away when I can figure out how to make yacc
** and lex read from strings as well as files.
*/

void
interpret(string)
     String string;
{
  char word1[20], word2[80];
  float atof();
  int i;

  sscanf(string,"%s",word1);
  if (!strcmp(word1,"run")) run_test();

  else {
    sscanf(string,"%s %s",word1,word2);

    /* So word1 is the first word on the line and word2 is the second.
       Now the fun begins... */
    
    if (!strcmp(word1,TestStuff.choice.text))  {
      for (i=0;i<NUM_TESTS;++i) {
	if (!strcmp(word2,(TestStuff.data)[i].text)) {
	  change_test((TestStuff.data)[i].code,FALSE);
	  break;
	}
      }
    }
    else if (!strcmp(word1,LogicalStuff.choice.text)) {
      for (i=0;i<NUM_LOGICAL;++i) {
	if (!strcmp(word2,(LogicalStuff.data)[i].text)) {
	  GC_change_logical((LogicalStuff.data)[i].code,FALSE);
	  break;
	}
      }
    }
    else if (!strcmp(word1,CompareCombineStuff.choice.text)) {
      for (i=0;i<NUM_COMPARE_COMBINE;++i) {
	if (!strcmp(word2,(CompareCombineStuff.data)[i].text)) {
	  GC_change_compare_combine((CompareCombineStuff.data)[i].code,FALSE);
	  break;
	}
      }
    }
    else if (!strcmp(word1,CompareStuff.choice.text)) {
      for (i=0;i<NUM_COMPARE;++i) {
	if (!strcmp(word2,(CompareStuff.data)[i].text)) {
	  GC_change_compare((CompareStuff.data)[i].code,FALSE);
	  break;
	}
      }
    }
    else if (!strcmp(word1,ArithmeticStuff.choice.text)) {
      for (i=0;i<NUM_ARITHMETIC;++i) {
	if (!strcmp(word2,(ArithmeticStuff.data)[i].text)) {
	  GC_change_arithmetic((ArithmeticStuff.data)[i].code,FALSE);
	  break;
	}
      }
    }
    else if (!strcmp(word1,TypeStuff.choice.text)) {
      for (i=0;i<NUM_TYPE;++i) {
	if (!strcmp(word2,(TypeStuff.data)[i].text)) {
	  GC_change_type((TypeStuff.data)[i].code,FALSE);
	  break;
	}
      }
    }
    else if (!strcmp(word1,MathStuff.choice.text)) {
      for (i=0;i<NUM_MATH;++i) {
	if (!strcmp(word2,(MathStuff.data)[i].text)) {
	  GC_change_math((MathStuff.data)[i].code,FALSE);
	  break;
	}
      }
    }
    else if (!strcmp(word1,"planemask")) 
      GC_change_planemask((unsigned int) atoi(word2),FALSE);
    else if (!strcmp(word1,"constant"))
      GC_change_constant((double) atof(word2),FALSE);
    else fprintf(stderr,"Ack... %s %s\n",word1,word2);
  }
}

#ifdef notdef
void
interpret(instring)
     char *instring;
{
  FILE *inend;
  
  yyin = outend;
  inend = fdopen(fildes[1],"w");
  fprintf(inend,"%s",instring);
  fclose(inend);
  yyparse();
}
#endif

#define select_correct_button(which,number) \
  select_button(GCdescs[(which)],(number));

void
GC_change_logical(logicalOp,feedback)
     int logicalOp;
     Boolean feedback;
{
	XIE.logicalOp = logicalOp;
}

void
GC_change_compare_combine(type,feedback)
     Boolean type;
     Boolean feedback;
{
	XIE.combine = type; 
}

void
GC_change_type(type,feedback)
     int type;
     Boolean feedback;
{
	XIE.isDyadic = ( type == 0 ? False : True );
}

void
GC_change_arithmetic(arithmeticOp,feedback)
     int arithmeticOp;
     Boolean feedback;
{
	XIE.arithmeticOp = arithmeticOp;
}

void
GC_change_math(mathOp,feedback)
     int mathOp;
     Boolean feedback;
{
	XIE.mathOp = mathOp;
}

void
GC_change_constant(constant,feedback)
     double constant;
     Boolean feedback;
{
	XIE.constant = constant;
}

void
GC_change_compare(compareOp,feedback)
     int compareOp;
     Boolean feedback;
{
	XIE.compareOp = compareOp;
}

void
GC_change_planemask(dashlist,feedback) 
     int dashlist;
     Boolean feedback;
{
  char dasharray[PLANEMASKLENGTH];	/* what we're gonna pass to XSetDashes */
  int dashnumber = 0;		/* which element of dasharray we're currently
				   modifying */
  int i;			/* which bit of the dashlist we're on */
  int state = 1;		/* whether the list bit we checked was
				   on (1) or off (0) */
				  
  /* Initialize the dasharray */

  for (i = 0; i < PLANEMASKLENGTH; ++i) dasharray[i] = 0;

  if (dashlist == 0) return;	/* having no dashes at all is bogus */

  /* XSetDashes expects the dashlist to start with an on bit, so if it
  ** doesn't, we keep on rotating it until it does */

  while (!(dashlist&1)) dashlist /= 2;

  /* Go through all the bits in dashlist, and update the dasharray
  ** accordingly */

  for (i = 0; i < PLANEMASKLENGTH; ++i) {
    /* the following if statements checks to see if the bit we're looking
    ** at as the same on or offness as the one before it (state).  If
    ** so, we increment the length of the current dash. */

    if (((dashlist&1<<i) && state) || (!(dashlist&1<<i) && !state))
      ++dasharray[dashnumber];
    else {			
      state = state^1;		/* reverse the state */
      ++dasharray[++dashnumber]; /* start a new dash */
    }
  } 

#if 0
  XSetDashes(X.dpy,X.gc,0,dasharray,dashnumber+1);
  X.gcv.dashes = dashlist;
#endif

  if (feedback) update_dashlist(dashlist);
}

void
change_test(test,feedback) 
     int test;
     Boolean feedback;
{
  X.test = test;
  if (feedback) select_button(testchoicedesc,test);
}

