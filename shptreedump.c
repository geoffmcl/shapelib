/******************************************************************************
 * $Id$
 *
 * Project:  Shapelib
 * Purpose:  Mainline for creating and dumping an ASCII representation of
 *           a quadtree.
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999-05-18 17:49:20  warmerda
 * New
 *
 */

static char rcsid[] = 
  "$Id$";

#include "shapefil.h"

#include <assert.h>

static void SHPTreeNodeDump( SHPTree *, SHPTreeNode *, const char *, int );

/************************************************************************/
/*                                main()                                */
/************************************************************************/
int main( int argc, char ** argv )

{
    SHPHandle	hSHP;
    SHPTree	*psTree;
    int		nExpandShapes = 0;
    int		nMaxDepth = 0;

/* -------------------------------------------------------------------- */
/*	Consume flags.							*/
/* -------------------------------------------------------------------- */
    while( argc > 1 )
    {
        if( strcmp(argv[1],"-v") == 0 )
        {
            nExpandShapes = 1;
            argv++;
            argc--;
        }
        else if( strcmp(argv[1],"-maxdepth") == 0 && argc > 2 )
        {
            nMaxDepth = atoi(argv[2]);
            argv += 2;
            argc -= 2;
        }
        else
            break;
    }

/* -------------------------------------------------------------------- */
/*      Display a usage message.                                        */
/* -------------------------------------------------------------------- */
    if( argc < 2 )
    {
	printf( "shptreedump [-v] [-maxdepth n] shp_file\n" );
	exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Open the passed shapefile.                                      */
/* -------------------------------------------------------------------- */
    hSHP = SHPOpen( argv[1], "rb" );

    if( hSHP == NULL )
    {
	printf( "Unable to open:%s\n", argv[1] );
	exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Build a quadtree structure for this file.                       */
/* -------------------------------------------------------------------- */
    psTree = SHPCreateTree( hSHP, 2, nMaxDepth, NULL, NULL );

/* -------------------------------------------------------------------- */
/*      Dump tree by recursive descent.                                 */
/* -------------------------------------------------------------------- */
    SHPTreeNodeDump( psTree, psTree->psRoot, "", nExpandShapes );

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    SHPDestroyTree( psTree );

    SHPClose( hSHP );

#ifdef USE_DBMALLOC
    malloc_dump(2);
#endif

    exit( 0 );
}

/************************************************************************/
/*                           EmitCoordinate()                           */
/************************************************************************/

static void EmitCoordinate( double * padfCoord, int nDimension )

{
    const char	*pszFormat;
    
    if( abs(padfCoord[0]) < 180 && abs(padfCoord[1]) < 180 )
        pszFormat = "%.9f";
    else
        pszFormat = "%.2f";

    printf( pszFormat, padfCoord[0] );
    printf( "," );
    printf( pszFormat, padfCoord[1] );

    if( nDimension > 2 )
    {
        printf( "," );
        printf( pszFormat, padfCoord[2] );
    }
    if( nDimension > 3 )
    {
        printf( "," );
        printf( pszFormat, padfCoord[3] );
    }
}

/************************************************************************/
/*                             EmitShape()                              */
/************************************************************************/

static void EmitShape( SHPObject * psObject, const char * pszPrefix,
                       int nDimension )

{
    int		i;
    
    printf( "%s( Shape\n", pszPrefix );
    printf( "%s  ShapeId = %d\n", pszPrefix, psObject->nShapeId );

    printf( "%s  Min = (", pszPrefix );
    EmitCoordinate( &(psObject->dfXMin), nDimension );
    printf( ")\n" );
    
    printf( "%s  Max = (", pszPrefix );
    EmitCoordinate( &(psObject->dfXMax), nDimension );
    printf( ")\n" );

    for( i = 0; i < psObject->nVertices; i++ )
    {
        double	adfVertex[4];
        
        printf( "%s  Vertex[%d] = (", pszPrefix, i );

        adfVertex[0] = psObject->padfX[i];
        adfVertex[1] = psObject->padfY[i];
        adfVertex[2] = psObject->padfZ[i];
        adfVertex[3] = psObject->padfM[i];
        
        EmitCoordinate( adfVertex, nDimension );
        printf( ")\n" );
    }
    printf( "%s)\n", pszPrefix );
}

/************************************************************************/
/*                          SHPTreeNodeDump()                           */
/*                                                                      */
/*      Dump a tree node in a readable form.                            */
/************************************************************************/

static void SHPTreeNodeDump( SHPTree * psTree,
                             SHPTreeNode * psTreeNode,
                             const char * pszPrefix,
                             int nExpandShapes )

{
    char	szNextPrefix[150];
    int		i;

    strcpy( szNextPrefix, pszPrefix );
    if( strlen(pszPrefix) < sizeof(szNextPrefix) - 3 )
        strcat( szNextPrefix, "  " );

    printf( "%s( SHPTreeNode\n", pszPrefix );

/* -------------------------------------------------------------------- */
/*      Emit the bounds.                                                */
/* -------------------------------------------------------------------- */
    printf( "%s  Min = (", pszPrefix );
    EmitCoordinate( psTreeNode->adfBoundsMin, psTree->nDimension );
    printf( ")\n" );
    
    printf( "%s  Max = (", pszPrefix );
    EmitCoordinate( psTreeNode->adfBoundsMax, psTree->nDimension );
    printf( ")\n" );

/* -------------------------------------------------------------------- */
/*      Emit the list of shapes on this node.                           */
/* -------------------------------------------------------------------- */
    if( nExpandShapes )
    {
        printf( "%s  Shapes(%d):\n", pszPrefix, psTreeNode->nShapeCount );
        for( i = 0; i < psTreeNode->nShapeCount; i++ )
        {
            SHPObject	*psObject;

            psObject = SHPReadObject( psTree->hSHP,
                                      psTreeNode->panShapeIds[i] );
            assert( psObject != NULL );
            if( psObject != NULL )
            {
                EmitShape( psObject, szNextPrefix, psTree->nDimension );
            }
        }
    }
    else
    {
        printf( "%s  Shapes(%d): ", pszPrefix, psTreeNode->nShapeCount );
        for( i = 0; i < psTreeNode->nShapeCount; i++ )
        {
            printf( "%d ", psTreeNode->panShapeIds[i] );
        }
        printf( "\n" );
    }

/* -------------------------------------------------------------------- */
/*      Emit subnodes.                                                  */
/* -------------------------------------------------------------------- */
    if( psTreeNode->psSubNode1 != NULL )
        SHPTreeNodeDump( psTree, psTreeNode->psSubNode1,
                         szNextPrefix, nExpandShapes );

    if( psTreeNode->psSubNode2 != NULL )
        SHPTreeNodeDump( psTree, psTreeNode->psSubNode2,
                         szNextPrefix, nExpandShapes );

    printf( "%s)\n", pszPrefix );

    return;
}
