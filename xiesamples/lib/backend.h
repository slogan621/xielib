
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

#define	CCIR_601_RED	0.299	
#define	CCIR_601_GREEN	0.587
#define	CCIR_601_BLUE	0.114	

#define	RES	0
#define	MEM	1

struct trash
{
	int	type;			/* resource (RES) or memory (MEM) */
	char	*ptr;			/* pointer to memory to free... */
	XID	res;			/* or resource ID to destroy */
        void    (*func)();   		/* routine to do the work */
	struct trash *next;		/* next item in list to free */
};

typedef struct
{
	Visual		*visual;	/* visual ID */ 
	int		type;		/* SingleBand or TripleBand */ 
	int		levels;		/* if SingleBand, the levels 
				           attribute of the image data */
	int		colormapSize;	/* number of entries in colormap */
	unsigned long 	depth;
	Bool		ditherSB;	/* if True, dither SingleBand */
	Colormap 	cmap;		/* colormap resource ID */	
	int		stdCmapClass;	/* -1 if not a StandardColormap */
	XStandardColormap stdCmap;	/* valid if stdCmapClass != -1 */
	struct trash	*can;		/* list of stuff to free */	
} Backend;
