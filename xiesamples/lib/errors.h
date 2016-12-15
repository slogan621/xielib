
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

typedef struct _clienterror {
        int     (*func)();              /* user's function */
        XPointer private;               /* private data */
        int     which;			/* which event */
        int     type;			/* RESOURCEERROR or FLOERROR */ 
        struct _clienterror *next;	/* points to next entry in bucket */
} ClientErrorRec, *ClientErrorPtr;

#define RESOURCEERROR   0
#define FLOERROR        1

/* function prototypes */

ClientErrorPtr SetClientErrorHandler( Display *d, int type, int which, 
	int (*func)(), XPointer private );
void RemoveErrorHandler( ClientErrorPtr p );
void EnableClientHandlers( XieExtensionInfo *xieInfo );
void DisableClientHandlers( void );
void RestoreDefaultErrorHandler( void );


