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
** xgc
**
** constants.h
**
** Lots of constants which many files need.
*/

/* Find the max of two numbers */
#define max(x,y) (((x)>(y))?(x):(y))

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define MAXCHOICES 16           /* Max # of choices for any option */

#define Black BlackPixel(X.dpy,0) 
#define White WhitePixel(X.dpy,0)

#define LogicalTest       0		/* different tests */
#define ArithmeticTest    1
#define MathTest          2
#define CompareTest       3

#define CLogical     	0		/* different GC things you can choose */
#define CArithmetic	1
#define CMath    	2
#define CCompare    	3
#define	CCombine	4
#define	CType		5
#define NUMCHOICES    	6
#define	CTest		6

#define TLineWidth     0	/* different editable text widgets */
#define TFont          1
#define TForeground    2
#define TBackground    3
#define NUMTEXTWIDGETS 4

#define StartTimer   0		/* flags for timing tests */
#define EndTimer     1
#define start_timer()   timer(StartTimer)
#define end_timer()     timer(EndTimer)

/* the number of toggle widgets in various groups */
#define NUM_TESTS       4
#define NUM_LOGICAL   	16
#define NUM_ARITHMETIC  9
#define NUM_COMPARE    	6
#define NUM_COMPARE_COMBINE    	2
#define NUM_MATH    	6
#define NUM_TYPE    	2

/* The number of bits in the plane mask description */
#define PLANEMASKLENGTH 8
#define BANDMASKLENGTH 3

#define MONWIDTH 256
#define MONHEIGHT 200
