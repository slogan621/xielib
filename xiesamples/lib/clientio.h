
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

#define MAXSEND 8192            /* PutClientData buffer size */
#define MAXREAD MAXSEND		/* GetCLientData buffer size */
        
typedef struct _readnotifypriv {
	unsigned int bytesLeft;		/* how many bytes left */
	char	*ptr;			/* how far into buffer */
	Bool 	reallocFlag;
	Bool	notifySeen;	
} ReadNotifyPriv;
	
typedef struct _readnotifydata {
	unsigned long namespace;	/* photospace of the photoflo */
	int     flo_id;			/* photoflo ID containing element */
	int	band;			/* band to read */
	XiePhototag element;		/* element in photoflo being read */ 
	unsigned int elementsz;		/* canonical size of elements to
					   read */
	unsigned int numels;		/* how many elements to read; 0 
					   means caller doesn't know */ 
	XieExportState status;		/* last status from GetClientData */
	unsigned int bytesRead;		/* how many bytes were read */
	char	*data;			/* the data which was read */
} ReadNotifyData;
	
