/*
csv2shp2 - converts a character delimited file to a ESRI shapefile
Copyright (C) 2005 Springs Rescue Mission
Copyright (C) 2018 Geoff R McLane
* Licence: GNU GPL version 2

GRATITUDE
Like this program?  Donate at <http://springsrescuemission.org>.

COMPILING INSTRUCTIONS
This program was written and tested using Shapefile C Library 
at https://github.com/geoffmcl/shapelib, a fork, of a clone of
cvs repo at <http://shapelib.maptools.org>...

UNIX
$ cd build
$ cmake .. [-DCMAKE_INSTALL_PREFIX:PATH=/path/to/shapelib/install] [options]
$ make # depends on the generator chosen

WINDOWS: with say MSVC installed
$ cd build
$ cmake .. [-DCMAKE_INSTALL_PREFIX:PATH=/path/to/shapelib/install] [options]

USAGE NOTES
By default, this program operates on single points only (not polygons or lines).

The input file may be a .csv file (comma separated values) or tab-separated
values, or it may be separated by any other character.  The first row must 
contain column names.  There must be each a column named longitude and 
latitude in the input file.

The .csv parser does not understand text delimiters (e.g. quotation mark).
It parses fields only by the given field delimiter (e.g. comma or tab).
The program has not been tested with null values, and in this case, the
behavior is undefined.  The program will not accept lines with a trailing 
delimiter character.

All columns (including longitude and latitude) in the input file are exported 
to the .dbf file.

The program attempts to find the best type (integer, decimal, string) and
smallest size of the fields necessary for the .dbf file.


SUPPORT
Springs Rescue Mission does not offer any support for this program.


 I will attempt to support issues filed here...

*/

#ifdef _MSC_VER
# ifndef NDEBUG
#  ifndef DEBUG
#   define DEBUG
#  endif
# endif
# ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
# endif
#pragma warning( disable: 4267) // conversion from 'size_t' to 'int', possible loss of data
#endif // #ifdef _MSC_VER

#include <sys/types.h> // for 'off_t', ... in WIN32
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <iostream>     // std::cout
#include <sstream>      // std::stringstream
#include "shapefil.h"
#include "regex.h"
#include "utils/sprtf.hxx"

#define MAX_COLUMNS 30
#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

#ifndef SPRTF
#define SPRTF printf
#endif

static const char *module = "csv2shp2";

static const char *usr_input = 0;

static char delimiter = ',';
static const char *shp_out_file = 0;
static int verbosity = 0;
static const char *xg_file = 0;

#define VERB1 (verbosity >= 1)
#define VERB2 (verbosity >= 2)
#define VERB5 (verbosity >= 5)
#define VERB9 (verbosity >= 9)


void pgm_exit(int ret)
{
#ifdef DEBUG
#define MX_BUFF 264
    static char buf[MX_BUFF];
    SPRTF( "Internal Error: *** FIX ME ***\n");
    fgets(buf, MX_BUFF, stdin);
#endif
}

typedef struct column_t {
	DBFFieldType eType;
	int nWidth;
	int nDecimals;
} column;

//typedef struct sPt3D_t {
//    double x, y, z;
//}sPt3D;
#define M_MX_COLOR 16
typedef struct spoint_t {
    double x, y, z;
    char color[M_MX_COLOR];
}spoint;

typedef std::vector<spoint> vPts;

static vPts vPoints;
//static spoint the_pt;   /* current/trasient/static point */
#define CLRPT(a) memset(a,0,sizeof(spoint))

/* counts the number of occurances of the character in the string */
int strnchr(const char *s, char c)
{
	int n = 0;
	int x = 0;

	for (; x < strlen(s); x++)
	{
		if (c == s[x])
		{
			n++;
		}
	}

	return n;
}

/* Returns a field given by column n (0-based) in a character-
   delimited string s */
char * delimited_column(char *s, char delim, int n)
{
	static char szreturn[4096];
	static char szbuffer[4096]; /* a copy of s */
	char * pchar;
	int x;
	char szdelimiter[2]; /* delim converted to string */

	if (strnchr(s, delim) < n)
	{
		SPRTF( "delimited_column: n is too large\n");
		return NULL;
	}

	strcpy(szbuffer, s);
	szdelimiter[0] = delim;
	szdelimiter[1] = '\0';
	x = 0;
	pchar = strtok(szbuffer, szdelimiter);
	while (x < n)
	{	
		pchar = strtok(NULL, szdelimiter);
		x++;
	}

	if (NULL == pchar)
	{
		return NULL;
	}

	strcpy(szreturn, pchar);
	return szreturn;
}

/* Determines the most specific column type.
   The most specific types from most to least are integer, float, string.  */
DBFFieldType str_to_fieldtype(const char *s)
{
	regex_t regex_i;
	regex_t regex_d;

	if (0 != regcomp(&regex_i, "^[0-9]+$", REG_NOSUB|REG_EXTENDED))
	{
		SPRTF( "integer regex compilation failed\n");
		pgm_exit (EXIT_FAILURE);
	}
	
	if (0 == regexec(&regex_i, s, 0, NULL, 0))
	{
		regfree(&regex_i);
		return FTInteger;
	}

	regfree(&regex_i);

	if (0 != regcomp(&regex_d, "^-?[0-9]+\\.[0-9]+$", REG_NOSUB|REG_EXTENDED))
	{
		SPRTF( "integer regex compilation failed\n");
		pgm_exit (EXIT_FAILURE);
	}

	if (0 == regexec(&regex_d, s, 0, NULL, 0))
	{
		regfree(&regex_d);
		return FTDouble;
	}

	regfree(&regex_d);

	return FTString;
}

int float_width(const char *s)
{
	regex_t regex_d;
	regmatch_t pmatch[2];
	char szbuffer[4096];

	if (0 != regcomp(&regex_d, "^(-?[0-9]+)\\.[0-9]+$", REG_EXTENDED))
	{
		SPRTF( "integer regex compilation failed\n");
		pgm_exit (EXIT_FAILURE);
	}

	if (0 != regexec(&regex_d, s, 2, &pmatch[0], 0))
	{
		return -1;
	}

	strncpy(szbuffer, &s[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
	szbuffer[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';
	regfree(&regex_d);

	return strlen(szbuffer);
}

/* returns the field width */
int str_to_nwidth(const char *s, DBFFieldType eType)
{
	switch (eType)
	{
		case FTString:
		case FTInteger:
		case FTDouble:
			return strlen(s);

		default:
			SPRTF( "str_to_nwidth: unexpected type\n");
			pgm_exit (EXIT_FAILURE);
	}
    return 0;
}

/* returns the number of decimals in a real number given as a string s */
int str_to_ndecimals(const char *s)
{
	regex_t regex_d;
	regmatch_t pmatch[2];
	char szbuffer[4096];

	if (0 != regcomp(&regex_d, "^-?[0-9]+\\.([0-9]+)$", REG_EXTENDED))
	{
		SPRTF( "integer regex compilation failed\n");
		pgm_exit (EXIT_FAILURE);
	}

	if (0 != regexec(&regex_d, s, 2, &pmatch[0], 0))
	{
		return -1;
	}

	strncpy(szbuffer, &s[pmatch[1].rm_so], pmatch[1].rm_eo - pmatch[1].rm_so);
	szbuffer[pmatch[1].rm_eo - pmatch[1].rm_so] = '\0';

	regfree(&regex_d);

	return strlen(szbuffer);
}

/* returns true if f1 is more general than f2, otherwise false */
int more_general_field_type(DBFFieldType t1, DBFFieldType t2)
{
	if (FTInteger == t2 && t1 != FTInteger)
	{
		return 1;
	}

	if (FTDouble == t2 && FTString == t1)
	{
		return 1;
	}
	
	return 0;
}

void strip_crlf (char *line)
{
  /* remove trailing CR/LF */

  if (strchr (line, 0x0D))
    {
      char *pszline;
      pszline = strchr (line, 0x0D);
      pszline[0] = '\0';
    }

  if (strchr (line, 0x0A))
    {
      char *pszline;
      pszline = strchr (line, 0x0A);
      pszline[0] = '\0';
    }
}

void give_version()
{
    SPRTF("csv2shp2 version 1, Copyright (C) 2018 Geoff R. McLane\n");
}


void give_help(char *name)
{
    give_version();
    SPRTF("%s: usage: [options] usr_input\n", module);
    SPRTF("Options:\n");
    SPRTF(" --help  (-h or -?) = This help and exit(0)\n");
    SPRTF(" --out <file>  (-o) = Set the shapfile output. (def=%s)\n",
        shp_out_file ? shp_out_file : "<none>");
    SPRTF(" --xg <file>   (-x) = Output xg string to the file. (def=%s)\n",
        (xg_file) ? "xg_file" : "Off");

    SPRTF("\n");
    SPRTF("  Given a flat set of lon,lat,alt,color in sets of 3's. That is each is a 3D\n");
    SPRTF("  triangle, like maybe extracted from a FlightGear terrain KSFO.btg.gz or\n");
    SPRTF("  test-btg -v2 g:\\S\\Terrain\\e140s40\\e148s32\\5377682.btg.gz -x tempgil.xg -c tempgil.csv,\n");
    SPRTF("  test the data, writing a simple SHPT_POINT out shapefile, and then with all the points\n");
    SPRTF("  collected in a vector, writing a SHPT_POLYGONZ, as an out-3D shapefile.\n");
    SPRTF("\n");
    SPRTF(" The CSV input must have a head of 'longitude,latitude,altitude,color', and then sets of 3 pts,\n");
    SPRTF("     148.631622, -31.700828, 286.892307, #ff0000\n");
    SPRTF("     148.631926, -31.700962, 287.488692, #ff0000\n");
    SPRTF("     148.631625, -31.700821, 286.900121, #ff0000\n");
    SPRTF("     ...\n");
    SPRTF("     for as many triangles that were found in the BTG...\n");
    SPRTF("\n");
}

#ifndef ISDIGIT
#define ISDIGIT(a) ((a >= '0') && (a <= '9'))
#endif

int parse_args(int argc, char **argv)
{
    int i, i2, c;
    char *arg, *sarg;
    for (i = 1; i < argc; i++) {
        arg = argv[i];
        i2 = i + 1;
        if (strcmp(arg, "--version") == 0) {
            give_version();
            return 2;
        }
        if (*arg == '-') {
            sarg = &arg[1];
            while (*sarg == '-')
                sarg++;
            c = *sarg;
            switch (c) {
            case 'h':
            case '?':
                give_help(argv[0]);
                return 2;
                break;
            case 'o':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    shp_out_file = strdup(sarg);
                    SPRTF("%s: Added shp out file '%s'.\n", module, shp_out_file);
                }
                else {
                    SPRTF("%s: Error: Expected file name to follow %s! Aborting...", module,
                        arg);
                    return 1;
                }
                break;
            case 'v':
                verbosity++;
                sarg++;
                while (*sarg) {
                    if (*sarg == 'v') {
                        verbosity++;
                    }
                    else if (ISDIGIT(*sarg)) {
                        verbosity = atoi(sarg);
                        break;
                    }
                }
                if (VERB2) {
                    SPRTF("%s: Set verbosity to %d\n", module, verbosity);
                }
                break;
            case 'x':
                if (i2 < argc) {
                    i++;
                    sarg = argv[i];
                    xg_file = strdup(sarg);
                    if (VERB2) {
                        SPRTF("%s: Added xg text opt to '%s'.\n", module, xg_file);
                    }
                }
                else {
                    SPRTF("%s: Error: Expected file name to follow %s! Aborting...", module,
                        arg);
                    return 1;
                }
                break;
                // TODO: Other arguments
            default:
                SPRTF("%s: Unknown argument '%s'. Try -? for help...\n", module, arg);
                return 1;
            }
        }
        else {
            // bear argument
            if (usr_input) {
                SPRTF("%s: Already have input '%s'! What is this '%s'?\n", module, usr_input, arg);
                return 1;
            }
            usr_input = strdup(arg);
        }
    }
    if (!usr_input) {
        SPRTF("%s: No user input found in command!\n", module);
        return 1;
    }
    if (!shp_out_file) {
        SPRTF("%s: No user output shapefile found in command! See -?\n", module);
        return 1;
    }
    return 0;
}


static char sbuffer[4096];
static char szfield[4096];

char *my_strncpy(char *dst, const char * src, size_t count)
{
    memset(dst, 0, count);
    strncpy(dst, src, count);
    size_t len = strlen(dst);
    while (len) {
        len--;
        if (dst[len] > ' ')
            break;
        dst[len] = 0;
    }
    return dst;
}

void write_xg_file(size_t max)
{
    size_t ii, len, res;
    spoint pt;  /* local */
    char last_color[M_MX_COLOR];
    FILE *fp = fopen(xg_file, "w");
    if (fp) {
        last_color[0] = 0;
        int cnt = 0;
        for (ii = 0; ii < max; ii++) {
            pt = vPoints[ii];
            if (strcmp(last_color, pt.color)) {
                // color changed
                if (cnt) {
                    fprintf(fp, "NEXT\n");
                    cnt = 0;
                }
                strcpy(last_color, pt.color);
                fprintf(fp, "color %s\n", last_color);
            }
            len = sprintf(sbuffer, "%f %f ; %f %s", pt.x, pt.y, pt.z, pt.color);
            res = fprintf(fp, "%s\n", sbuffer);
            cnt++;
        }
        if (cnt) {
            fprintf(fp, "NEXT\n");
            cnt = 0;
        }
        fclose(fp);
        SPRTF("%s: Vector written to %s. count %d!\n", module, xg_file, (int)max);
    }
    else {
        SPRTF("%s: Failed to create %s!\n", module, xg_file);
    }
}

// read hex string as '#112233' hex color....
double get_color_value(char * color)    // pt[cnt].color
{
    double d = 0.0;
    char *hex = color;
    if (hex[0] == '#') {
        hex++;
        unsigned int x;
        std::stringstream ss;
        ss << std::hex << hex;
        ss >> x;
        d = (double)x / (double)(0xffffff);

    }

    return d;
}

int main(int argc, char ** argv)
{
    int iret = 0;
    FILE *csv_f;
    int n_columns; /* 1-based */
    int n_line;
    int n_longitude = -1; /* column with x, 0 based */
    int n_latitude = -1; /* column with y, 0 based */
    int n_altitude = -1; /* column with z, 0 based */
    int n_color = -1; /* column with 'color', 0 based */

    int x;
    DBFHandle dbf_h;
    SHPHandle shp_h;
    column columns[MAX_COLUMNS + 1];
    char *cp;
    delimiter = ',';
    set_log_file("tempshp2.txt", 0);
    iret = parse_args(argc, argv);
    if (iret) {
        if (iret == 2)
            iret = 0;
        return iret;
    }

    csv_f = fopen(usr_input, "r");

    if (NULL == csv_f)
    {
        perror("could not open csv file");
        return (EXIT_FAILURE);
    }

    /* read first header line */
    cp = fgets(sbuffer, 4000, csv_f);
    if (!cp) {
        SPRTF( "%s: Error reading '%s'\n", module, usr_input);
        fclose(csv_f);
        return (EXIT_FAILURE);
    }

    /* check first row */
    strip_crlf(sbuffer);

    if (delimiter == sbuffer[strlen(sbuffer) - 1])
    {
        SPRTF( "lines must not end with the delimiter character\n");
        return EXIT_FAILURE;

    }

    /* count columns and verify consistency*/

    n_columns = strnchr(sbuffer, delimiter);

    if (n_columns > MAX_COLUMNS)
    {
        SPRTF( "too many columns, maximum is %i\n", MAX_COLUMNS);
        return EXIT_FAILURE;
    }

    n_line = 1;

    while (!feof(csv_f))
    {
        n_line++;
        fgets(sbuffer, 4000, csv_f);
        if (n_columns != strnchr(sbuffer, delimiter))
        {
            SPRTF( "Number of columns on row %i does not match number of columns on row 1\n", n_columns);
            return EXIT_FAILURE;
        }
    }

    /* counted columns and verified consistency */
#ifdef DEBUG
    SPRTF("debug: columns = %i, file lines = %i, rows = %i\n", n_columns + 1, n_line, n_line - 1);
#endif

    /* identify longitude latitude and altitude columns */

    fseek(csv_f, 0, SEEK_SET);
    fgets(sbuffer, 4000, csv_f);
    strip_crlf(sbuffer);

    for (x = 0; x <= n_columns; x++)
    {
        char *head = delimited_column(sbuffer, delimiter, x);
        if (!head) {
            continue;
        }
        if (0 == strcasecmp("Longitude", head))
        {
            n_longitude = x;
        }
        else if (0 == strcasecmp("Latitude", head))
        {
            n_latitude = x;
        }
        else if (0 == strcasecmp("altitude", head)) {
            n_altitude = x;
        }
        else if (0 == strcasecmp("color", head)) {
            n_color = x;
        }
    }

#ifdef DEBUG
    SPRTF("debug: column for lat/lon/alt/col = %i/%i/%i/%i\n", n_latitude, n_longitude, n_altitude, n_color);
#endif

    if (-1 == n_longitude || -1 == n_latitude || -1 == n_altitude || -1 == n_color)
	{
		SPRTF( "The header row must define one each a column named longitude,latitude,altitude,color\n");
		return EXIT_FAILURE;
	}

	/* determine best fit for each column */

	SPRTF ("Anaylzing column types...\n");

#ifdef DEBUG
	SPRTF("debug: string type = %i\n", FTString);
	SPRTF("debug: int type = %i\n", FTInteger);
	SPRTF("debug: double type = %i\n", FTDouble);
#endif
    int rows = 0;
    char *rb = sbuffer;
	for (x = 0; x <= n_columns; x++)
	{	
#ifdef DEBUG
		SPRTF("debug: examining column %i of %i\n", x + 1, (n_columns + 1));
#endif
		columns[x].eType = FTInteger;
		columns[x].nWidth = 2;
		columns[x].nDecimals = 0;
	
		fseek(csv_f, 0, SEEK_SET);
		fgets(rb, 4000, csv_f);
        rows = 0;
		while (!feof(csv_f))
		{
			if (NULL == fgets(rb, 4000, csv_f))
			{
				if (!feof(csv_f))
				{
					SPRTF( "error during fgets()\n");
				}
				continue;
			}
            rows++;
#ifdef DEBUG
            if (VERB1) {
                if ((rows % 1000) == 0) {
                    SPRTF("debug:[v1] column %i, type = %i, w = %i, d = %i, r = %i\n", x + 1, columns[x].eType, columns[x].nWidth, columns[x].nDecimals, rows);
                }
            }
            else {
                if (VERB9) {
                    SPRTF("debug:[v9] column %i, type = %i, w = %i, d = %i, r = %i\n", x + 1, columns[x].eType, columns[x].nWidth, columns[x].nDecimals, rows);
                }
            }
#endif
            strcpy(szfield, delimited_column(sbuffer, delimiter, x));
			if (more_general_field_type(str_to_fieldtype(szfield), columns[x].eType))
			{
				columns[x].eType = str_to_fieldtype(szfield);
				columns[x].nWidth = 2;
				columns[x].nDecimals = 0;
				fseek(csv_f, 0, SEEK_SET);
				fgets(rb, 4000, csv_f);
                rows = 0;
				continue;
			}
			if (columns[x].nWidth < str_to_nwidth(szfield, columns[x].eType))
			{
				columns[x].nWidth = str_to_nwidth(szfield, columns[x].eType);
			}
			if (FTDouble == columns[x].eType && columns[x].nDecimals < str_to_ndecimals(szfield))
			{
				columns[x].nDecimals = str_to_ndecimals(szfield);
			}

		}

#ifdef DEBUG
        SPRTF("debug: column %i, type = %i, w = %i, d = %i, r = %i\n", x + 1, columns[x].eType, columns[x].nWidth, columns[x].nDecimals, rows);
#endif

	}


	/* initilize output files */

	SPRTF ("Initializing output files...\n");

	shp_h = SHPCreate(shp_out_file, SHPT_POINT);

    if (NULL == shp_h)
    {
        SPRTF( "SHPCreate failed\n");
        return (EXIT_FAILURE);
    }

    dbf_h = DBFCreate(shp_out_file);

	if (NULL == dbf_h)
	{
		SPRTF( "DBFCreate failed\n");
		return (EXIT_FAILURE);
	}

	fseek(csv_f, 0, SEEK_SET);
	fgets(sbuffer, 4000, csv_f);
	strip_crlf(sbuffer);

	for (x = 0; x <= n_columns; x++)
	{	
        char *col = delimited_column(sbuffer, delimiter, x);
#ifdef DEBUG
		SPRTF ("debug: final: column %i, type = %i, w = %i, d = %i, name=|%s|\n", x + 1, columns[x].eType, columns[x].nWidth, columns[x].nDecimals, col);
#endif
		if (-1 == DBFAddField(dbf_h, col, columns[x].eType, columns[x].nWidth, columns[x].nDecimals))
		{
			SPRTF( "DBFFieldAdd failed column %i\n", x + 1);
			return (EXIT_FAILURE);
		}

	}

	/* write data */

	SPRTF ("Writing data...\n");

	fseek(csv_f, 0, SEEK_SET);
	fgets(sbuffer, 4000, csv_f); /* skip header line */

	n_columns = strnchr(sbuffer, delimiter);
	n_line = 1;
    int ocnt = 0;
	while (!feof(csv_f))
	{
		SHPObject * shp;
		// double x_pt, y_pt, z_pt;
		int shp_i;

        if (NULL == fgets(sbuffer, 4000, csv_f))    /* read next file line */
        {
            if (!feof(csv_f))
            {
                SPRTF("error during fgets()\n");
            }
            continue;
        }
		
        n_line++;
        /* write to shape file */
        spoint pt;
        CLRPT(&pt);
		pt.x = atof(delimited_column(sbuffer, delimiter, n_longitude));
		pt.y = atof(delimited_column(sbuffer, delimiter, n_latitude));
        pt.z = atof(delimited_column(sbuffer, delimiter, n_altitude));  /* add altitude/elevation double */
        my_strncpy(pt.color, delimited_column(sbuffer, delimiter, n_color), M_MX_COLOR);
        vPoints.push_back(pt);
        ocnt++; /* Count another record added */
#ifdef DEBUG
		// assumed newline on buffer
        if (VERB9) {
            SPRTF("debug:[v9] x,y,z = %f,%f,%f - buf %s", pt.x, pt.y, pt.z, sbuffer);
        }
        else {
            if ((ocnt % 1000) == 0) {
                SPRTF("debug:%d: x,y,z = %f,%f,%f - buf %s", ocnt, pt.x, pt.y, pt.z, sbuffer);
            }
        }
#endif

		shp = SHPCreateSimpleObject(SHPT_POINT, 1, &pt.x, &pt.y, &pt.z);
		shp_i = SHPWriteObject(shp_h, -1, shp);
		SHPDestroyObject(shp);

		/* write to dbf */

		for (x = 0; x <= n_columns; x++)
		{	
			int b = 0;

			strcpy(szfield, delimited_column(sbuffer, delimiter, x));

			switch (columns[x].eType)
			{
				case FTInteger:
					b = DBFWriteIntegerAttribute(dbf_h, shp_i, x, atoi(szfield));
					break;
				case FTDouble:
					b = DBFWriteDoubleAttribute(dbf_h, shp_i, x, atof(szfield));
					break;
				case FTString:
					b = DBFWriteStringAttribute(dbf_h, shp_i, x, szfield);
					break;
				default:
					SPRTF( "Error: unexpected column type %i in column %i\n", columns[x].eType, (x + 1));
                    return (EXIT_FAILURE);
            }
			
			if (!b)
			{
				SPRTF( "Error: DBFWrite*Attribute failed\n");
				return (EXIT_FAILURE);
			}
		}
	}

	/* finish up */

	SHPClose(shp_h);
	
    DBFClose(dbf_h);

    size_t max = max = vPoints.size();
    size_t ii;
    if (xg_file && max) {
        write_xg_file(max);
    }

    /* 20180528: Have a go at creating the shapefile, from a set of 3D+c, wgs84 BTG tris 
       extracted from the usr_input CSV file, assumed to be set of 3's, triangles,
       with lon,lat,alt,color[]
       This is -3D shapefile, after the data extraction, into a vPoints array...
       */
    if (max) {
        shp_h = 0;
        dbf_h = 0;
        SHPObject *shp2 = 0;
        int shp_i2;
        spoint pt[4];  /* local */
        size_t cnt;
        strcpy(sbuffer, shp_out_file);  // ge current name
        strcat(sbuffer, "-3D");
        int shpType = SHPT_POLYGONZ;    // SHPT_POLYGON;
        shp_h = SHPCreate(sbuffer, shpType);   // SHPT_POINT);
        if (NULL == shp_h)
        {
            SPRTF("Error: SHPCreate failed for %s\n",sbuffer);
            goto Done_Test;
        }

        dbf_h = DBFCreate(sbuffer);
        if (NULL == dbf_h)
        {
            SPRTF("Error: DBFCreate failed for %s\n", sbuffer);
            goto Done_Test;
        }

        /* write dbf file
            Add field header text for each of the column types
            DBFAddField(dbf_h, col, eType, nWidth, nDecimals))
        */
        for (x = 0; x <= n_columns; x++)
        {
            int nWidth = 20; //  10;   // columns[x].nWidth = 2;
            int nDecimals = 16;
            char *col = "color";
            DBFFieldType eType = FTDouble;
            switch (x)
            {
            case 0:
                col = "longitude";
                break;
            case 1:
                col = "latitude";
                break;
            case 2:
                col = "altitude";
                break;
            case 3:
                col = "color";
                eType = FTString;
                nWidth = 8;
                nDecimals = 0;
                break;
            }
#ifdef DEBUG
            SPRTF("debug: final: column %i, type = %i, w = %i, d = %i, name=|%s|\n", x + 1, eType, nWidth, nDecimals, col);
#endif
            if (-1 == DBFAddField(dbf_h, col, eType, nWidth, nDecimals))
            {
                SPRTF("Error: DBFFieldAdd failed column %i\n", x + 1);
                return (EXIT_FAILURE);
            }

        }

        /* write shp data, and to the dbf */
        /* add polygons, in this case 3 points - x,y,z,
           3D triangles, with a color
        */

        cnt = 0;
        double dx[3 + 1];
        double dy[3 + 1];
        double dz[3 + 1];
        double dc[3 + 1];
        int cnt0 = 0;   /* records consumed */
        int wrap = 0;
        for (ii = 0; ii < max; ii++) {
            cnt = 0;    // get next 3 - it is a tri
            while (cnt < 3) {
                pt[cnt] = vPoints[ii];  /* extract the point */
                /* set up the inputs - array of doubles */
                dx[cnt] = pt[cnt].x;
                dy[cnt] = pt[cnt].y;
                dz[cnt] = pt[cnt].z;
                dc[cnt] = get_color_value(pt[cnt].color);
                ii++;   /* got to next input */
                if (ii > max)
                    break;
                cnt++;
                cnt0++; // records consumed
                if ((cnt0 % 1000) == 0)
                    wrap = 1;
            }
            ii--;   /* let the outer loop do the last increment fo this set of 3, a tri */
            if (cnt != 3) {
                SPRTF("%s: vector count %d 'not' a multiple of 3! %f\n", module, (int)max, (double)max / 3.0);
                break;
            }

            /* got 3 points - a triangle */
            /* Could use - SHPObject SHPCreateObject( int nSHPType, int nShapeId, int nParts,
                       const int * panPartStart, const int * panPartType,
                       int nVertices,
                       const double * padfX, const double * padfY,
                       const double * padfZ, const double * padfM );
               and supply all the additional, but for now use SHPCreateSimpleObject
            */
            //shp2 = SHPCreateSimpleObject(shpType, 3, &pt[0].x, &pt[0].y, &pt[0].z); // NOT array of doubles
            shp2 = SHPCreateSimpleObject(shpType, 3, &dx[0], &dy[0], &dz[0]);   /* use the array of 3 doubles */
            shp_i2 = SHPWriteObject(shp_h, -1, shp2);   /* commit to file */
            SHPDestroyObject(shp2); /* all done */

            cnt = 0;
            /* write to dbf, using 4 components */
            /* int SHPAPI_CALL DBFWriteDoubleAttribute( DBFHandle hDBF, int iShape, int iField, double dFieldValue ); 
                traverse the columns, for each tri, write 3
            */
            for (x = 0; x <= n_columns; x++)
            {
                std::stringstream msg;  /* collect debug only message */
                char *col = "color";
                DBFFieldType eType = FTDouble;
                const char *attr = "Double";
                switch (x)
                {
                case 0:
                    col = "longitude";
                    break;
                case 1:
                    col = "latitude";
                    break;
                case 2:
                    col = "altitude";
                    break;
                case 3:
                    col = "color";
                    eType = FTString;
                    // nWidth = 8;
                    // nDecimals = 0;
                    attr = "String";
                    break;
                }
                cnt = 0;    /* set of three - tri */
                msg << "debug: DBF:" << (ii + 1) << ":";
                msg << " col " << (x + 1);
                while (cnt < 3) {
                    int b = 0;
                    double d;
                    switch (x)
                    {
                    case 0:
                        col = "longitude";
                        d = pt[cnt].x;  /* get lon */
                        msg << " x=" << d;
                        break;
                    case 1:
                        col = "latitude";
                        d = pt[cnt].y; /* get lat */
                        msg << " y=" << d;
                        break;
                    case 2:
                        col = "altitude";
                        d = pt[cnt].z; /* get alt */
                        msg << " z=" << d;
                        break;
                    case 3:
                        col = "color";
                        eType = FTString;
                        msg << " c=" << pt[cnt].color;
                        break;
                    }
                    if (x == n_columns) {
                        b = DBFWriteStringAttribute(dbf_h, shp_i2, x, pt[cnt].color);
                    }
                    else {
                        //b = DBFWriteDoubleAttribute(dbf_h, shp_i2, x, pt[cnt].x);
                        b = DBFWriteDoubleAttribute(dbf_h, shp_i2, x, d);
                    }
                    if (!b)
                    {
                        SPRTF("Error: col %d DBFWrite%sAttribute failed\n", (x+1), attr);
                        return (EXIT_FAILURE);
                    }
                    cnt++;
                }
                cnt0++;
#ifdef DEBUG
                if (VERB9) {
                    SPRTF("%s\n", msg.str().c_str());
                }
                else {
                    if (wrap) { // if ((cnt0 % 1000) == 0) {
                        SPRTF("%s\n", msg.str().c_str());
                        wrap = 0;
                    }
                }
#endif
            }
            cnt = 0;

        }
        SPRTF("%s: Written %d records to %s shapefile, x,y,z,c values...\n", module, (int)(max / 3), sbuffer);

    Done_Test:
        if (shp_h)
            SHPClose(shp_h);
        if (dbf_h)
            DBFClose(dbf_h);

    }




	return EXIT_SUCCESS;
}

/* eof */