/* $XConsortium: def.h /main/30 1996/12/04 10:11:12 swick $ */
/* $XFree86: xc/config/makedepend/def.h,v 3.3 1997/01/12 10:38:17 dawes Exp $ */
/*

Copyright (c) 1993, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.

*/

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

/* Default object file suffix */
#define OBJSUFFIX	".o"

#define MAXDEFINES	512
#define MAXFILES	1024
#define MAXDIRS		64
#define SYMTABINC	10	/* must be > 1 for define() to work right */

#undef ERROR			/* On some systems this is defined */

/* the following must match the directives table in main.c */
#define	IF		0
#define	IFDEF		1
#define	IFNDEF		2
#define	ELSE		3
#define	ENDIF		4
#define	DEFINE		5
#define	UNDEF		6
#define	INCLUDE		7
#define	LINE		8
#define	PRAGMA		9
#define ERROR           10
#define IDENT           11
#define SCCS            12
#define ELIF            13
#define EJECT           14
#define WARNING         15
#define IFFALSE         16     /* pseudo value --- never matched */
#define ELIFFALSE       17     /* pseudo value --- never matched */
#define INCLUDEDOT      18     /* pseudo value --- never matched */
#define IFGUESSFALSE    19     /* pseudo value --- never matched */
#define ELIFGUESSFALSE  20     /* pseudo value --- never matched */

#ifdef CS_DEBUG
extern int	_debugmask;
/*
 * debug levels are:
 * 
 *     0	show ifn*(def)*,endif
 *     1	trace defined/!defined
 *     2	show #include
 *     3	show #include SYMBOL
 *     4-6	unused
 */
#define debug(level,arg) { if (_debugmask & (1 << level)) warning arg; }
#else
#define	debug(level,arg) /**/
#endif /* CS_DEBUG */

struct symtab
{
  const char *s_name;
  const char *s_value;
};

/* possible i_flag */
#define DEFCHECKED	(1<<0)	/* whether defines have been checked */
#define NOTIFIED	(1<<1)	/* whether we have revealed includes */
#define MARKED		(1<<2)	/* whether it's in the makefile */
#define SEARCHED	(1<<3)	/* whether we have read this */
#define FINISHED	(1<<4)	/* whether we are done reading this */
#define INCLUDED_SYM	(1<<5)	/* whether #include SYMBOL was found
				   Can't use i_list if true */
#define SYSINCLUDE	(1<<6)	/* whether this is an <system> include file */

struct inclist
{
  char *i_incstring;		/* string from #include line */
  char *i_file;			/* path name of the include file */
  struct inclist **i_list;	/* list of files it itself includes */
  int i_listlen;		/* length of i_list */
  struct symtab **i_defs;	/* symbol table for this file and its
				   children when merged */
  int i_ndefs;			/* current # defines */
  bool *i_merged;		/* whether we have merged child defines */
  unsigned char i_flags;
  struct inclist *i_parent;	/* parent inclist */
};

struct filepointer
{
  char *f_p;
  char *f_base;
  char *f_end;
  long f_len;
  long f_line;
};

extern char *copy(const char *str);
extern char *base_name(char *file);
extern char *getline (struct filepointer *filep);
extern struct symtab **slookup (const char *symbol, struct inclist *file);
extern struct symtab **isdefined (const char *symbol, struct inclist *file, struct inclist **srcfile);
extern struct symtab **fdefined();
extern struct filepointer *getfile (char *file);
extern struct inclist *newinclude (char *newfile, char *incstring);
extern struct inclist *inc_path (char *file, char *include, bool dot);
extern int find_includes (struct filepointer *filep, struct inclist *file,
  struct inclist *file_red, int recursion, bool failOK);
extern void recursive_pr_include (struct inclist *head, char *file, char *base);
extern void inc_clean ();
extern void included_by (struct inclist *ip, struct inclist *newfile);
extern int deftype (char *line, struct filepointer *filep, struct inclist *file_red,
  struct inclist *file, int parse_it);
extern int match (const char *str, const char **list);
extern int zero_value (char *exp, struct filepointer *filep, struct inclist *file_red);
extern int merge2defines (struct inclist *file1, struct inclist *file2);
extern int cppsetup (char *line, struct filepointer *filep, struct inclist *inc);
extern void add_include (struct filepointer *filep, struct inclist *file,
  struct inclist *file_red, char *include, bool dot, bool failOK);
extern void freefile (struct filepointer *fp);

extern void define (char *def, struct inclist *file);
extern void define2 (const char *name, const char *val, struct inclist *file);
extern void undefine (const char *symbol, struct inclist *file);

extern void fatalerr (const char *, ...);
extern void warning (const char *, ...);
extern void warning1 (const char *, ...);

#if defined (__EMX__)
#  define INITIALIZE	_wildcard (&argc, &argv);
#endif

#if defined (__DJGPP__)
#  define rename	_rename
#  define INITIALIZE	__crt0_load_environment_file ("cpp");
#  include <crt0.h>
#endif

#if defined (_WIN32) && !defined (WIN32)
#  define WIN32
#endif
