
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

#include <stdio.h>
#include <fcntl.h>
#include <X11/extensions/XIElib.h>
#include <sys/stat.h>
#include <Xlib.h>

Bool 
SendDiskImage(display, filename, photospace, floId, src, band)
Display        *display;
char           *filename;
XiePhotospace   photospace;
int             floId, src, band;
{
	char *buf, *ptr;
	int  fd, final, bytes_left, nbytes, size;
	struct stat StatBuf;

	if (stat(filename,&StatBuf)) {
		fprintf(stderr, "Couldn't stat %s\n", filename);
		return (False);
	} else
	    size = StatBuf.st_size;

	if ((fd = open(filename, O_RDONLY)) == -1) {
		fprintf(stderr, "Couldn't open %s\n", filename);
		return (False);
	}

	if ((buf = (char *)malloc(size)) == (char *) NULL) {
		fprintf(stderr, "Couldn't allocate buffer\n");
		close(fd);
		return(False);
	}

	ptr = buf;
	if (read(fd, ptr, size) != size) {
		fprintf(stderr, "Couldn't read data\n");
		free(buf);
		close(fd);
		return(False);
	}

	bytes_left = size;
	final = 0;
	while (bytes_left > 0) {
		nbytes = (bytes_left > 8192) ? 8192 : bytes_left;
		if ( nbytes >= bytes_left )
			final = 1;
		XiePutClientData(display, 
			photospace, 		/* photospace */
			floId, 			/* which flo */
			src, 			/* element requesting data */
			final,			/* final flag */
			band,			/* 0 for all but triple band
					  	   data BandByPlane, which
					           then may be 0, 1, or 2 */
			(unsigned char *) ptr,	/* the data to send */ 
			nbytes			/* size of data sent */
		);
		bytes_left -= nbytes;
		ptr += nbytes;
	}
	close(fd);
	free(buf);
	return(True);
}

Colormap
AllocateGrayMapColors(display, screen, window, deep)
Display *display;
int     screen;
Window  window;
int	deep;
{
	int             i, n_colors = 1 << deep;
	XColor         *gray;
	Colormap        graycmap;

	gray = ( XColor * ) malloc(sizeof(XColor) * n_colors);
	if (gray == (XColor *) NULL) 
	{
		fprintf(stderr, "Couldn't allocate XColor vector\n");
		exit(1);
	}
	graycmap = XCreateColormap(display, DefaultRootWindow(display),
		DefaultVisual(display, screen), AllocAll);

	/* do a grey scale ramp */
	for (i = 0; i < n_colors; i++) {
		gray[i].pixel = i;
		gray[i].red = gray[i].green = gray[i].blue = 
			(i * 65535L) / (long) (n_colors - 1);
		gray[i].flags = DoRed | DoGreen | DoBlue;
	}

	XStoreColors(display, graycmap, gray, n_colors);

	XSetWindowColormap(display, window, graycmap);

	/* The ICCCM police are going to get us! Normally we should
           allow the window manager to do this... */

	XInstallColormap(display, graycmap);

	XSync(display, 0);

	if (gray)
		free(gray);

	return( graycmap );
}

