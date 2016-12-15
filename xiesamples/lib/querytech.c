
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

#include	<X11/extensions/XIElib.h>

int
QueryTechniqueGroupDefault( display, group, technique )
Display	*display;		/* connection to the server */
XieTechniqueGroup group;	/* group to query */
XieTechnique *technique;	/* on return, default technique 
				   belonging to the group specified */
{
	int i, nTechs, retval = 0;
	XieTechnique *techs;

	XieQueryTechniques( display, xieValDefault, &nTechs, &techs );
	for ( i = 0; i < nTechs; i++ )
		if ( techs[ i ].group == group )
		{
			memcpy( technique, &techs[ i ], 
				sizeof( XieTechnique ) );
			retval = 1;
			break;
		}
	if ( techs )
		XFree( techs );
	return( retval );
}

int
QueryTechniqueGroup( display, group, size, techniques )
Display	*display;		/* connection to the server */
XieTechniqueGroup group;	/* group to query */
int *size;			/* number of techniques in group */
XieTechnique **techniques;	/* on return, vector of techniques 
				   belonging to the group specified */
{
	XieQueryTechniques( display, group, size, techniques );
	return( 1 );
}

int
QueryTechniqueBest( display, group, technique )
Display	*display;		/* connection to the server */
XieTechniqueGroup group;	/* group to query */
XieTechnique *technique;	/* on return, technique which
				   is "fastest" in the group */
{
        XieTechnique *techs;
	unsigned int best = 0; 
        int i, nTechs, bestIdx = 0;

        XieQueryTechniques( display, group, &nTechs, &techs );

	if ( !nTechs )
		return( 0 );

        for ( i = 0; i < nTechs; i++ )
                if ( techs[ i ].speed > best )
                {
			best = techs[ i ].speed; 
			bestIdx = i;	
                }

	memcpy( technique, &techs[ bestIdx ], sizeof( XieTechnique ) );

        if ( techs )
                XFree( techs );

        return( 1 );
}

int
QueryTechniqueByNumber( display, group, number, technique )
Display *display;               /* connection to the server */
XieTechniqueGroup group;	/* group to query */
int number;			/* technique number to check */
XieTechnique *technique;	/* on return, contains technique
				   data */
{
        int i, nTechs, retval = 0;
        XieTechnique *techs;

        XieQueryTechniques( display, group, &nTechs, &techs );
        for ( i = 0; i < nTechs; i++ )
                if ( techs[ i ].group == group && techs[ i ].number == number )
                {
                        memcpy( &techs[ i ], technique,
                                sizeof( XieTechnique ) );
                        retval = 1;
                        break;
                }
        if ( techs )
                XFree( techs );
        return( retval );
}

int	
QueryTechniqueByName( display, group, name, technique )
Display *display;               /* connection to the server */
XieTechniqueGroup group;        /* group to query */
char *name;                     /* technique number to check */
XieTechnique *technique;        /* on return, contains technique
                                   data */
{
        int i, nTechs, retval = 0;
        XieTechnique *techs;

        XieQueryTechniques( display, group, &nTechs, &techs );
        for ( i = 0; i < nTechs; i++ )
                if ( techs[ i ].group == group && 
			!strcmp( techs[ i ].name, name ) )
                {
                        memcpy( &techs[ i ], technique,
                                sizeof( XieTechnique ) );
                        retval = 1;
                        break;
                }
        if ( techs )
                XFree( techs );
        return( retval );

}
 
