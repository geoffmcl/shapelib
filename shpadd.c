/*
 * Copyright (c) 1995 Frank Warmerdam
 *
 * This code is in the public domain.
 *
 * $Log$
 * Revision 1.4  1997-03-06 14:01:16  warmerda
 * added memory allocation checking, and free()s.
 *
 * Revision 1.3  1995/10/21 03:14:37  warmerda
 * Changed to use binary file access
 *
 * Revision 1.2  1995/08/04  03:18:01  warmerda
 * Added header.
 *
 */

static char rcsid[] = 
  "$Id$";

#include "shapefil.h"

int main( int argc, char ** argv )

{
    SHPHandle	hSHP;
    int		nShapeType, nVertices, nParts, *panParts, i;
    double	*padVertices;

/* -------------------------------------------------------------------- */
/*      Display a usage message.                                        */
/* -------------------------------------------------------------------- */
    if( argc < 4 )
    {
	printf( "shpadd shp_file [[x y] [+]]*\n" );
	exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open the passed shapefile.                                      */
/* -------------------------------------------------------------------- */
    hSHP = SHPOpen( argv[1], "rb+" );

    if( hSHP == NULL )
    {
	printf( "Unable to open:%s\n", argv[1] );
	exit( 1 );
    }

    SHPGetInfo( hSHP, NULL, &nShapeType );

/* -------------------------------------------------------------------- */
/*	Build a vertex/part list from the command line arguments.	*/
/* -------------------------------------------------------------------- */
    if( (padVertices = (double *) malloc(sizeof(double) * 1000 * 2)) == NULL )
    {
        printf( "Out of memory\n" );
        exit( 1 );
    }
    
    nVertices = 0;

    if( (panParts = (int *) malloc(sizeof(int) * 1000 )) == NULL )
    {
        printf( "Out of memory\n" );
        exit( 1 );
    }
    
    nParts = 1;
    panParts[0] = 0;

    for( i = 2; i < argc;  )
    {
	if( argv[i][0] == '+' )
	{
	    panParts[nParts++] = nVertices;
	    i++;
	}
	else if( i < argc-1 )
	{
	    sscanf( argv[i], "%lg", padVertices+nVertices*2 );
	    sscanf( argv[i+1], "%lg", padVertices+nVertices*2+1 );
	    nVertices += 1;
	    i += 2;
	}
    }

/* -------------------------------------------------------------------- */
/*      Write the new entity to the shape file.                         */
/* -------------------------------------------------------------------- */
    SHPWriteVertices(hSHP, nVertices, nParts, panParts, padVertices );

    SHPClose( hSHP );

    free( panParts );
    free( panVertices );
}
