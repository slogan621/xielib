
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

typedef struct _XIEEventCheckData {
	int		floId;		/* the desired photoflo ID */
	XiePhototag	tag;		/* the desired element */
	XiePhotospace	space;		/* the desired photospace */
	int		base;		/* first_event field from 
					   XieExtensionInfo struct */
	int		which;		/* the desired XIE event */
	int		count;		/* used for xieEvnNoExportAvailable
					   events */
} XIEEventCheck;		 

Bool WaitForXIEEvent( Display *display, XIEEventCheck *eventData,
	long timout, XEvent *event ); 

