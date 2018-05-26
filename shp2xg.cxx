/******************************************************************************
 * $Id: shp2xg.cxx,v 1.0 2014-06-22 00:00:00 gmclane Exp $
 *
 * Project:  Shapelib
 * Purpose:  Sample application for dumping contents of a shapefile to 
 *           the terminal in xgraph form.
 * Author:   Geoff R. McLane, reports _AT_ geoffair _DOT_ info
 *
 * 20170810: Add user output file
 * 20170806: Add option to set a bounding box, inclusive or exclusive
 * --bbox minlon,minlat,maxlon,maxlat
 *
 ******************************************************************************
 * Copyright (c) 2014-2017, Geoff R. McLane
 *
 * This software is available under the following "MIT Style" license,
 * or at the option of the licensee under the LGPL (see LICENSE.LGPL).  This
 * option is discussed in more detail in shapelib.html.
 *
 * --
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
 * $Log: shp2xg.cxx,v $
 *
 * Revision 1.0  2014-06-22 00:00:00  gmclane
 * initial cut
 *
 */
#ifdef _MSC_VER
#pragma warning( disable: 4996 )
#endif

#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include "shapefil.h"

SHP_CVSID("$Id: shp2xg.cxx,v 1.0 2014-06-22 00:00:00 gmclane Exp $")

typedef struct tagBBOX {
    double min_lon, min_lat, max_lon, max_lat;
} BBOX, *PBBOX;
typedef struct tagLL {
    double lat, lon;
}LL, *PLL;

typedef std::vector<std::string> vSTG;
typedef std::vector<LL> vLL;

static vSTG vInputs;
static int verbosity = 0;
static PBBOX pBBox = 0;
static FILE *output = stdout;
static char *out_file = 0;
static bool add_bounds = false;
static char *xg_color = 0;

#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)

#ifndef ISDIGIT
#define ISDIGIT(a) ((a >= '0') && (a <= '9'))
#endif

static void show_help()
{
#ifdef BUILD_DIR
    printf("# Build directory " BUILD_DIR "\n");
#endif
    printf("shp2xg [options] shp_file [shp_file2 [... shp_filen]]\n");
    printf("options:\n");
    printf(" --help        (-h or -?) = This help and exit (0)\n");
    printf(" --add               (-a) = Add bounding box to output. (def=%s)\n", (add_bounds ? "Yes" : "No"));
    printf(" --color <color>     (-c) = Add color to the output xg. (def=%s)\n", (xg_color ? xg_color : "none"));
    printf(" --bbox <box>        (-b) = Set bounding box. box=min.lon,min.lat,max.lon,max.lat\n");
    printf(" --out <file>        (-o) = Set output file. Implies min. verbosity. (def=%s)\n", (out_file ? out_file : "stdout"));
    printf(" --verb[nn]          (-v) = Bump (or set nn) verbosity. (def=%d)\n", verbosity);
    printf("\n");
    printf(" If given the base name of a shape file, read and show information about\n");
    printf(" that shapefile. Given a minimum verbosity of 1, show each of the vertices\n");
    printf(" given. If given a valid bbox, which implies a minimum verbosity, only show\n");
    printf(" when the shape bound or middle, lies within that bbox.\n");
}

/////////////////////////////////////////////////////////////////////////////
//// UTILITIES ////

// important 'split' function
vSTG split_whitespace(const std::string &str, int maxsplit)
{
    vSTG result;
    std::string::size_type len = str.length();
    std::string::size_type i = 0;
    std::string::size_type j;
    int countsplit = 0;
    while (i < len) {
        while (i < len && isspace((unsigned char)str[i])) {
            i++;
        }
        j = i;  // next non-space
        while (i < len && !isspace((unsigned char)str[i])) {
            i++;
        }

        if (j < i) {
            result.push_back(str.substr(j, i - j));
            ++countsplit;
            while (i < len && isspace((unsigned char)str[i])) {
                i++;
            }

            if (maxsplit && (countsplit >= maxsplit) && i < len) {
                result.push_back(str.substr(i, len - i));
                i = len;
            }
        }
    }
    return result;
}
/**
* split a string per a separator - if no sep, use space - split_whitespace
*/
vSTG string_split(const std::string& str, const char* sep, int maxsplit)
{
    if (sep == 0)
        return split_whitespace(str, maxsplit);

    vSTG result;
    int n = (int)strlen(sep);
    if (n == 0) {
        // Error: empty separator string
        return result;
    }
    const char* s = str.c_str();
    std::string::size_type len = str.length();
    std::string::size_type i = 0;
    std::string::size_type j = 0;
    int splitcount = 0;

    while ((i + n) <= len) {
        if ((s[i] == sep[0]) && (n == 1 || memcmp(s + i, sep, n) == 0)) {
            result.push_back(str.substr(j, i - j));
            i = j = i + n;
            ++splitcount;
            if (maxsplit && (splitcount >= maxsplit))
                break;
        }
        else {
            ++i;
        }
    }
    result.push_back(str.substr(j, len - j));
    return result;
}

///////////////////////////////////////////////////////////////
bool in_world_range(double lat, double lon)
{
    if ((lat > 90.0) ||
        (lat < -90.0) ||
        (lon > 180.0) ||
        (lon < -180.0))
        return false;
    return true;
}

bool bbox_valid(PBBOX pb)
{
    if (!pb)
        return false;
    if (!in_world_range(pb->min_lat, pb->min_lon))
        return false;
    if (!in_world_range(pb->max_lat, pb->max_lon))
        return false;
    if (pb->min_lon >= pb->max_lon)
        return false;
    if (pb->min_lat >= pb->max_lat)
        return false;
    return true;
}

bool in_bbox(PBBOX pb, double lat, double lon)
{
    if ((lat >= pb->min_lat) &&
        (lat <= pb->max_lat) &&
        (lon >= pb->min_lon) &&
        (lon <= pb->max_lon))
        return true;
    return false;
}

void set_bbox(PBBOX pb)
{
    pb->max_lat = -400.0;
    pb->min_lat = 400.0;
    pb->max_lon = -400.0;
    pb->min_lon = 400.0;
}

void add_bbox(PBBOX pb, double lat, double lon)
{
    if (lat < pb->min_lat)
        pb->min_lat = lat;
    if (lat > pb->max_lat)
        pb->max_lat = lat;
    if (lon < pb->min_lon)
        pb->min_lon = lon;
    if (lon > pb->max_lon)
        pb->max_lon = lon;
}


int main( int argc, char ** argv )
{
    SHPHandle	hSHP;
    int		c, a, a2, nShapeType, nEntities, i, iPart, cnt;
    std::string file;
    double 	adfMinBound[4], adfMaxBound[4];
    int give_help = 0;
    char *currArg, *sarg;
    BBOX b;

    for (a = 1; a < argc; a++) {
        a2 = a + 1;
        currArg = argv[a];
        c = *currArg;
        if (c == '-') {
            sarg = &currArg[1];
            while (*sarg == '-') sarg++;
            c = *sarg;
            switch (c) {
            case 'c':
                if (a2 < argc) {
                    a++;
                    sarg = argv[a];
                    xg_color = strdup(sarg);
                }
                else {
                    fprintf(stderr, "# Error: Expected color to follow arg '%s'! Try '-?'\n", currArg);
                    return 1;
                }
                break;
            case 'h':
            case '?':
                give_help = 1;
                break;
            case 'a':
                add_bounds = true;
                break;
            case 'b':
                if (a2 < argc) {
                    a++;
                    sarg = argv[a];
                    vSTG vs = string_split(sarg, ",", 0);
                    size_t max = vs.size();
                    if (max == 4) {
                        pBBox = new BBOX;
                        pBBox->min_lon = atof(vs[0].c_str());
                        pBBox->min_lat = atof(vs[1].c_str());
                        pBBox->max_lon = atof(vs[2].c_str());
                        pBBox->max_lat = atof(vs[3].c_str());
                        if (!bbox_valid(pBBox)) {
                            fprintf(stderr, "# Error: Bounding box '%s' not valid!\n", sarg);
                            return 1;
                        }
                    }
                    else {
                        fprintf(stderr, "# Error: Expected bounding box '%s' to split into 4 on ','! Got %d'\n", sarg, (int)max);
                        return 1;
                    }
                }
                else {
                    fprintf(stderr, "# Error: Expected bounding box to follow arg '%s'!'\n", currArg);
                    return 1;
                }
                break;
            case 'v':
                verbosity++;
                sarg++;
                while ((c = *sarg) != 0) {
                    if (c == 'v') {
                        verbosity++;
                    }
                    else if (ISDIGIT(c)) {
                        verbosity = atoi(sarg);
                        break;
                    }
                    sarg++;
                }
                break;
            case 'o':
                if (a2 < argc) {
                    a++;
                    sarg = argv[a];
                    out_file = strdup(sarg);
                }
                else {
                    fprintf(stderr, "# Error: Expected file name to follow arg '%s'! Try '-?'\n", currArg);
                    return 1;
                }
                break;
            default:
                show_help();
                fprintf(stderr, "# Error: Unknown arg '%s'! Try '-?'\n", currArg);
                return 1;
            }
        }
        else {
            vInputs.push_back(currArg);
        }
    }
/* -------------------------------------------------------------------- */
/*      Display a usage message.                                        */
/* -------------------------------------------------------------------- */
    if ( give_help )
    {
        show_help();
        //exit(1);
        return 0;
    }
    cnt = (int)vInputs.size();
    if ( ! cnt ) {
        show_help();
        fprintf( stderr, "# Error: No shape file names found in command line!\n");
        return 1;
    }
    if ((pBBox || out_file) && !VERB1)
        verbosity = 1;

    if (out_file) {
        output = fopen(out_file, "w");
        if (!output) {
            fprintf(stderr, "# Error: Unable to open out file '%s'!\n", out_file);
            return 1;
        }

    }

    set_bbox(&b);   // set invlaid values

/* -------------------------------------------------------------------- */
/*      Open the passed shapefiles.                                      */
/* -------------------------------------------------------------------- */
    fprintf(output, "# Processing %d assumed shapefiles...\n", cnt);
    //for (a = 1; a < argc; a++)
    for (a = 0; a < cnt; a++)
    {
        file = vInputs[a];

        hSHP = SHPOpen( file.c_str(), "rb" );

        if( hSHP == NULL )
        {
            fprintf( stderr, "# Unable to open : '%s'\n", file.c_str() );
            continue;
        }

        fprintf(output, "# Processing '%s' shapefile...\n", file.c_str());

        /* -------------------------------------------------------------------- */
        /*      Print out the file bounds.                                      */
        /* -------------------------------------------------------------------- */
        SHPGetInfo( hSHP, &nEntities, &nShapeType, adfMinBound, adfMaxBound );

        fprintf(output, "# Shapefile Type: %s   # of Shapes: %d\n",
                SHPTypeName( nShapeType ), nEntities );
    
        fprintf(output, "# File Bounds: (%.15g,%.15g,%.15g,%.15g) to  (%.15g,%.15g,%.15g,%.15g)\n",
                adfMinBound[0], 
                adfMinBound[1], 
                adfMinBound[2], 
                adfMinBound[3], 
                adfMaxBound[0], 
                adfMaxBound[1], 
                adfMaxBound[2], 
                adfMaxBound[3] );

        if (VERB1) {
            if (xg_color) {
                fprintf(output, "color %s\n", xg_color);
            }
            /* -------------------------------------------------------------------- */
            /*	Process the list of shapes, printing all the vertices.	            */
            /* -------------------------------------------------------------------- */
            for (i = 0; i < nEntities; i++)
            {
                int		j;
                SHPObject	*psShape;

                psShape = SHPReadObject(hSHP, i);

                if (psShape == NULL)
                {
                    fprintf(stderr, "# Unable to read shape %d, terminating object reading.\n", i);
                    break;
                }

                if (psShape->bMeasureIsUsed) {
                    if (VERB5) {
                        fprintf(output, "# Shape:%d (%s)  nVertices=%d, nParts=%d Bounds:(%.15g,%.15g, %.15g, %.15g) to (%.15g,%.15g, %.15g, %.15g)\n",
                            i, SHPTypeName(psShape->nSHPType),
                            psShape->nVertices, psShape->nParts,
                            psShape->dfXMin, psShape->dfYMin,
                            psShape->dfZMin, psShape->dfMMin,
                            psShape->dfXMax, psShape->dfYMax,
                            psShape->dfZMax, psShape->dfMMax);
                    }
                }
                else {
                    if (VERB5) {
                        fprintf(output, "# Shape:%d (%s)  nVertices=%d, nParts=%d Bounds:(%.15g,%.15g, %.15g) to (%.15g,%.15g, %.15g)\n",
                            i, SHPTypeName(psShape->nSHPType),
                            psShape->nVertices, psShape->nParts,
                            psShape->dfXMin, psShape->dfYMin,
                            psShape->dfZMin,
                            psShape->dfXMax, psShape->dfYMax,
                            psShape->dfZMax);
                    }
                }
                if (pBBox) {
                    double clon = (psShape->dfXMin + psShape->dfXMax) / 2.0;
                    double clat = (psShape->dfYMin + psShape->dfYMax) / 2.0;
                    if (!in_bbox(pBBox, psShape->dfYMin, psShape->dfXMin) &&
                        !in_bbox(pBBox, psShape->dfYMax, psShape->dfXMax) &&
                        !in_bbox(pBBox, clat, clon)) {
                        SHPDestroyObject(psShape);
                        continue;
                    }
                }

                if (psShape->nParts > 0 && psShape->panPartStart[0] != 0)
                {
                    fprintf(stderr, "# panPartStart[0] = %d, not zero as expected.\n",
                        psShape->panPartStart[0]);
                }

                for (j = 0, iPart = 1; j < psShape->nVertices; j++)
                {
                    const char	*pszPartType = "";

                    if (j == 0 && psShape->nParts > 0)
                        pszPartType = SHPPartTypeName(psShape->panPartType[0]);

                    if (iPart < psShape->nParts && psShape->panPartStart[iPart] == j)
                    {
                        pszPartType = SHPPartTypeName(psShape->panPartType[iPart]);
                        iPart++;
                    }

                    if (psShape->bMeasureIsUsed) {
                        if (pszPartType && *pszPartType && VERB2)
                            fprintf(output, "# PartType %s\n", pszPartType);

                        fprintf(output, "%.15g %.15g\n",
                            psShape->padfX[j],
                            psShape->padfY[j]);
                    }
                    else {
                        if (pszPartType && *pszPartType && VERB2)
                            fprintf(output, "# PartType %s\n", pszPartType);

                        fprintf(output, "%.15g %.15g\n",
                            psShape->padfX[j],
                            psShape->padfY[j]);
                    }
                    add_bbox(&b, psShape->padfY[j], psShape->padfX[j]);
                }
                if (j)
                    fprintf(output, "NEXT\n");

                SHPDestroyObject(psShape);

            }   // for nEntities
        }   // VERB1

        SHPClose( hSHP );
    }
    if (add_bounds && bbox_valid(&b)) {
        fprintf(output, "color gray\n");
        fprintf(output, "%.15g %.15g\n",
            b.min_lon, b.min_lat);
        fprintf(output, "%.15g %.15g\n",
            b.min_lon, b.max_lat);
        fprintf(output, "%.15g %.15g\n",
            b.max_lon, b.max_lat);
        fprintf(output, "%.15g %.15g\n",
            b.max_lon, b.min_lat);
        fprintf(output, "%.15g %.15g\n",
            b.min_lon, b.min_lat);
        fprintf(output, "NEXT\n");
    }

    if (!VERB1)
        fprintf(output, "# set verbosity to output vertices...(-v[1,2,5,9])\n");

    vInputs.clear();
#ifdef USE_DBMALLOC
    malloc_dump(2);
#endif

    if (out_file) {
        fclose(output);
    }
    return 0;
}

// eof = shp2xg.cxx
