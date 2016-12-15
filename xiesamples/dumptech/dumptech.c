
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
#include <X11/extensions/XIElib.h>

extern int optind;
extern int opterr;
extern char *optarg;

void usage();

#define UNDEFINED_TECH_NAME ""

static char *techniqueNames[ xieValMaxTechGroup + 1 ] = {
UNDEFINED_TECH_NAME, UNDEFINED_TECH_NAME, "ColorAlloc", UNDEFINED_TECH_NAME,
"Constrain", UNDEFINED_TECH_NAME, "ConvertFromRGB", UNDEFINED_TECH_NAME,
"ConvertToRGB", UNDEFINED_TECH_NAME, "Convolve", UNDEFINED_TECH_NAME,
"Decode", UNDEFINED_TECH_NAME, "Dither", UNDEFINED_TECH_NAME, "Encode",
UNDEFINED_TECH_NAME, "Gamut", UNDEFINED_TECH_NAME, "Geometry", 
UNDEFINED_TECH_NAME, "Histogram", UNDEFINED_TECH_NAME, "WhiteAdjust" };

#define UNDEFINED_TECH -1

static XieTechniqueGroup techniques[ xieValMaxTechGroup + 1 ] = {
UNDEFINED_TECH, UNDEFINED_TECH, xieValColorAlloc, UNDEFINED_TECH,
xieValConstrain, UNDEFINED_TECH, xieValConvertFromRGB, UNDEFINED_TECH,
xieValConvertToRGB, UNDEFINED_TECH, xieValConvolve, UNDEFINED_TECH,
xieValDecode, UNDEFINED_TECH, xieValDither, UNDEFINED_TECH, xieValEncode,
UNDEFINED_TECH, xieValGamut, UNDEFINED_TECH, xieValGeometry, UNDEFINED_TECH,
xieValHistogram, UNDEFINED_TECH, xieValWhiteAdjust };

main(argc, argv)
int  argc;
char *argv[];
{
	Display        	*display;
	char 		*display_name = "";
	XieTechnique	tech, *techs;
	int		size, flag;
	XieTechniqueGroup i;
	XieExtensionInfo *xieInfo;

	/* handle command line arguments */

	while ((flag=getopt(argc,argv,"?d:"))!=EOF) {
		switch(flag) {

		case 'd':	display_name = optarg; 
				break;

		case '?':	usage( argv[ 0 ] );
				break;	

		default: 	printf(" unrecognized flag (-%c)\n",flag);
				usage(argv[0]);
				break;
		}
	}

	/* Connect to the display */

	if (display_name == NULL) {
		printf("Display name not defined\n");
		usage(argv[0]);
	}

	if ((display = XOpenDisplay(display_name)) == (Display *) NULL) {
		fprintf(stderr, "Unable to connect to display\n");
		exit(1);
	}

	/* Connect to XIE extension */

	if (!XieInitialize(display, &xieInfo)) {
		fprintf(stderr, "XIE not supported on this display!\n");
		exit(1);
	}

	printf("XIE server V%d.%d\n", xieInfo->server_major_rev, 
		xieInfo->server_minor_rev);

	for ( i = 0; i < xieValMaxTechGroup + 1; i++ )
	{
		if ( techniques[ i ] != UNDEFINED_TECH )
		{
			QueryTechniqueGroup( display, techniques[ i ], 
				&size, &techs );		
			DisplayGroup( i, techs, size );
			XFree( techs );
			QueryTechniqueBest( display, techniques[ i ],
				&tech );
			printf( "Fastest technique is '%s'\n", tech.name );
			QueryTechniqueGroupDefault( display, techniques[ i ],
				&tech );
			printf( "Default technique is '%s'\n", tech.name );
		}
	}
	XCloseDisplay(display);
}

int
DisplayGroup( group, techs, size )
XieTechniqueGroup group;
XieTechnique *techs;
int	size;
{
	int	i;

	printf( "\nGroup '%s' has %d member(s):\n", techniqueNames[ group ],
		size );
	for ( i = 0; i < size; i++ )
	{
		printf( "Name: %s Number: %d Speed: %d Needs parameters: %s\n", 
			techs[ i ].name, techs[ i ].number, techs[ i ].speed,
			(techs[ i ].needs_param == True ? "True" : "False") );
	}
}

void
usage( pgm )
char *pgm;
{
	printf("usage: %s [-d display]\n", pgm);
	exit(1);
}
