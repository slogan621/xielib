
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

/* #include <X11/Xlib.h>
#include <X11/Xutil.h>
*/
#include <stdio.h>
#include <X11/extensions/XIElib.h>
/*
#include "events.h"
*/


main(argc, argv)
int  argc;
char *argv[];
{
	int		status = 0;
	Display        	*display;
	XieExtensionInfo *xieInfo;

	if ((display = XOpenDisplay("")) == (Display *) NULL) {
		fprintf(stderr, "Can't open display\n");
		status = 1;
	}

	if (!XieInitialize(display, &xieInfo)) {
		fprintf( stderr, "Couldn't connect to server\n" );
		status = 1;
	}

	exit( status );
}

