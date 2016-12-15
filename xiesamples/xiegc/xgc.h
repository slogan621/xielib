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
** xgc.h
*/

#include "constants.h"
#include "X11/extensions/XIElib.h"

typedef struct {
  char *string;
  int    code;
} StringTable;

typedef struct {
  Display  *dpy;		/* the display! */
  Screen   *scr;		/* the screen! */
  Window    win1;		/* src1 */ 
  Window    win2;		/* src2 */
  Window    win3;		/* result */
  Window    win4;		/* histogram */
  GC        gc;			/* the GC! */
  GC        miscgc;		/* used for doing stuff when we don't want
				   to change the normal GC */
  XGCValues gcv;		/* a separate copy of what's in the GC,
				   since we're not allowed to look in it */
  Pixmap    tile;		/* what we tile with */
  Pixmap    stipple;		/* what we stipple with */
  XImage   *image;		/* image for GetImage & PutImage */
  int       test;		/* which test is being run */
  float     percent;		/* percentage of test to run */
  Pixel     foreground;
  Pixel     background;
  char      *fontname;
} XStuff;                       /* All the stuff that only X needs to
                                   know about */
typedef struct {
  XiePhotomap	src1Photo;
  XiePhotomap   src2Photo;
  XiePhotoflo	refresh1;
  XiePhotoflo 	refresh2;
  XieColorList	clist1;
  XieColorList  clist2;
  int		logicalOp;
  int		compareOp;
  int		arithmeticOp;
  int		mathOp;
  double	constant;
  Boolean	isDyadic;
  Boolean	combine;
} XIEStuff;

typedef struct {
  char name[40];  		/* name as it will appear on the screen */
  char text[40];	       	/* Xgc command it translates to */
  int num_commands;		/* number of command buttons inside it */
  int columns;			/* how many columns of command buttons; if
				   0, then there's only one row */
  struct {
    char name[40];		/* name as it will appear on the screen */
    char text[40];   		/* Xgc command it translates to */
  } command_data[MAXCHOICES];
} ChoiceStuff;			/* All the info needed to deal with a 
				   choice widget */
typedef struct {
  char *name;
  char *text;
  int code;
} XgcData;

typedef struct {
  struct {
    char *name;
    char *text;
    int   num_toggles;
    int   columns;
  } choice;
  XgcData *data;
} XgcStuff;

typedef struct {
  Widget label;
  int size;
  WidgetList widgets;
} ChoiceDesc;

/************/
