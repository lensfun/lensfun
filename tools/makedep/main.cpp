/* $XConsortium: main.c /main/84 1996/12/04 10:11:23 swick $ */
/* $XFree86: xc/config/makedepend/main.c,v 3.11.2.1 1997/05/11 05:04:07 dawes Exp $ */
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

#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "def.h"

#ifdef hpux
#define sigvec sigvector
#endif /* hpux */

#ifdef X_POSIX_C_SOURCE
#define _POSIX_C_SOURCE X_POSIX_C_SOURCE
#include <signal.h>
#undef _POSIX_C_SOURCE
#else
#if defined(X_NOT_POSIX) || defined(_POSIX_SOURCE)
#include <signal.h>
#else
#define _POSIX_SOURCE
#include <signal.h>
#undef _POSIX_SOURCE
#endif
#endif

#ifdef MINIX
#define USE_CHMOD	1
#endif

#ifdef CS_DEBUG
int _debugmask;

#endif

char *ProgramName;
const char *ProgramVersion = "0.1.0";

const char *directives[] =
{
    "if",
    "ifdef",
    "ifndef",
    "else",
    "endif",
    "define",
    "undef",
    "include",
    "line",
    "pragma",
    "error",
    "ident",
    "sccs",
    "elif",
    "eject",
    "warning",
    NULL
};

#define MAKEDEPEND
#include "imakemdp.h"	/* from config sources */
#undef MAKEDEPEND

struct inclist inclist[MAXFILES];
struct inclist *inclistp = inclist;
struct inclist maininclist;

char *filelist[MAXFILES];
char *includedirs[MAXDIRS + 1];
char *notdotdot[MAXDIRS];
const char *objprefix = "";
const char *objsuffix = OBJSUFFIX;
const char *startat = "# DO NOT DELETE";
int width = 78;
bool opt_append = false;
bool opt_backup = false;
bool opt_create = false;
bool opt_printed = false;
bool opt_verbose = false;
bool opt_remove_prefix = false;
bool show_where_not = false;
bool opt_warn_multiple = false;	/* Warn on multiple includes of same file */
bool opt_sysincludes = false;

void redirect (const char *line, const char *makefile);

static
#ifdef SIGNALRETURNSINT
int
#else
void
#endif
sighandler (int sig)
{
    fflush (stdout);
    fatalerr ("got signal %d\n", sig);
}

#if defined(USG) || defined(_WIN32) || defined(OS_OS2) || defined(OS_NEXT) || defined(OS_BE) || defined (OS_DOS) || defined (OS_SOLARIS)
#define USGISH
#endif

#ifndef USGISH
#ifndef _POSIX_SOURCE
#define sigaction sigvec
#define sa_handler sv_handler
#define sa_mask sv_mask
#define sa_flags sv_flags
#endif
struct sigaction sig_act;

#endif /* USGISH */

static void display_version ()
{
    printf ("makedepend  Copyright (c) X Consortium  Version %s\n", ProgramVersion);
}

static void display_help ()
{
    printf ("makedepend                                Copyright (c) 1993, 1994 X Consortium\n\n");
    printf ("makedepend [-Dname[=def]] [-Iincludedir] [-Yincludedir] [-a]\n");
    printf ("   [-fmakefile] [-oobjsuffix] [-pobjprefix] [-sstring] [-wwidth]\n");
    printf ("   [-v] [-m] [-- ... otheroptions ... --] sourcefile ...\n\n");
    printf ("-Dname[=def] Define a macro. Without =def the symbol becomes defined as `1'.\n");
    printf ("-Iincludedir Tells makedepend to search for include files in this directory.\n");
    printf ("-Yincludedir Replace standard include dirs with the specified directory.\n");
    printf ("-V           Display makedep version number.\n");
    printf ("-S           Output dependencies on system include files as well (default: no).\n");
    printf ("-a           Append dependencies to the end of the file instead of replacing.\n");
    printf ("-b           Create a backup of makefile before modifying it.\n");
    printf ("-c           Create makefile if it does not exist.\n");
    printf ("-fmakefile   Set output filename. If '-' sends the output to stdout.\n");
    printf ("-oobjsuffix  Object file suffix. Ex: -o.b for '.b' or -o:obj for ':obj'\n");
    printf ("-pobjprefix  Object file prefix.\n");
    printf ("-r           Remove directory from object filename.\n");
    printf ("-sstring     Starting string delimiter. Default value is \"# DO NOT DELETE\".\n");
    printf ("-wwidth      Line width. Default value is 78 characters.\n");
    printf ("-v           Verbose operation.\n");
    printf ("-m           Warn about multiple inclusion.\n");
    printf ("-- ... otheroptions ... --\n");
    printf ("makedepend silently skips any options it encounters between double hyphens.\n");
}

static void add_include (const char *inc, char **incp)
{
    char *beg, *end;

    if (!inc) return;

#if defined (OS_OS2) || defined (OS_DOS) || defined (_WIN32)
#  define PATH_SEPARATOR	';'
#else
#  define PATH_SEPARATOR	':'
#endif

    /* can have more than one component */
    beg = (char *) strdup (inc);
    for (;;)
    {
        end = (char *) strchr (beg, PATH_SEPARATOR);
        if (end)
            *end = 0;
        if (incp >= includedirs + MAXDIRS)
            fatalerr ("Too many include dirs\n");
        *incp++ = beg;
        if (!end)
            break;
        beg = end + 1;
    }
#undef PATH_SEPARATOR
}

#undef main

int main (int argc, char **argv)
{
    register char **fp = filelist;
    register char **incp = includedirs;
    register char *p;
    register struct inclist *ip;
    char *makefile = NULL;
    struct filepointer *filecontent;
    struct symtab *psymp = predefs;
    const char *endmarker = NULL;
    char *defincdir = NULL;
    char **undeflist = NULL;
    int i, numundefs = 0;

#if defined (INITIALIZE)	// initialization code
    INITIALIZE;
#endif

    ProgramName = argv[0];

    while (psymp->s_name)
    {
        define2 (psymp->s_name, psymp->s_value, &maininclist);
        psymp++;
    }
    if (argc == 2 && argv[1][0] == '@')
    {
        struct stat ast;
        int afd;
        char *args;
        char **nargv;
        int nargc;
        char quotechar = '\0';

        nargc = 1;
        if ((afd = open (argv[1] + 1, O_RDONLY)) < 0)
            fatalerr ("cannot open \"%s\"\n", argv[1] + 1);
        fstat (afd, &ast);
        args = (char *) malloc (ast.st_size + 1);
        if ((ast.st_size = read (afd, args, ast.st_size)) < 0)
            fatalerr ("failed to read %s\n", argv[1] + 1);
        args[ast.st_size] = '\0';
        close (afd);
        for (p = args; *p; p++)
        {
            if (quotechar)
            {
                if (quotechar == '\\' ||
                    (*p == quotechar && p[-1] != '\\'))
                    quotechar = '\0';
                continue;
            }
            switch (*p)
            {
                case '\\':
                case '"':
                case '\'':
                    quotechar = *p;
                    break;
                case ' ':
                case '\n':
                    *p = '\0';
                    if (p > args && p[-1])
                        nargc++;
                    break;
            }
        }
        if (p[-1])
            nargc++;
        nargv = (char **) malloc (nargc * sizeof (char *));

        nargv[0] = argv[0];
        argc = 1;
        for (p = args; argc < nargc; p += strlen (p) + 1)
            if (*p)
                nargv[argc++] = p;
        argv = nargv;
    }
    for (argc--, argv++; argc; argc--, argv++)
    {
        /* if looking for endmarker then check before parsing */
        if (endmarker && strcmp (endmarker, *argv) == 0)
        {
            endmarker = NULL;
            continue;
        }
        if (**argv != '-')
        {
            /* treat +thing as an option for C++ */
            if (endmarker && **argv == '+')
                continue;
            *fp++ = argv[0];
            continue;
        }
        switch (argv[0][1])
        {
            case '-':
                endmarker = &argv[0][2];
                if (endmarker[0] == '\0')
                    endmarker = "--";
                break;
            case 'D':
                if (argv[0][2] == '\0')
                {
                    argv++;
                    argc--;
                }
                for (p = argv[0] + 2; *p; p++)
                    if (*p == '=')
                    {
                        *p = ' ';
                        break;
                    }
                define (argv[0] + 2, &maininclist);
                break;
            case 'I':
                if (incp >= includedirs + MAXDIRS)
                    fatalerr ("Too many -I flags.\n");
                *incp++ = argv[0] + 2;
                if (**(incp - 1) == '\0')
                {
                    *(incp - 1) = *(++argv);
                    argc--;
                }
                break;
            case 'U':
                /* Undef's override all -D's so save them up */
                numundefs++;
                if (numundefs == 1)
                    undeflist = (char **)malloc (sizeof (char *));
                else
                    undeflist = (char **)realloc (undeflist, numundefs * sizeof (char *));

                if (argv[0][2] == '\0')
                {
                    argv++;
                    argc--;
                }
                undeflist[numundefs - 1] = argv[0] + 2;
                break;
            case 'Y':
                defincdir = argv[0] + 2;
                break;
                /* do not use if endmarker processing */
            case 'a':
                if (endmarker)
                    break;
                opt_append = true;
                break;
            case 'b':
                if (endmarker)
                    break;
                opt_backup = true;
                break;
            case 'c':
                if (endmarker)
                    break;
                opt_create = true;
                break;
            case 'r':
                if (endmarker)
                    break;
                opt_remove_prefix = true;
                break;
            case 'w':
                if (endmarker)
                    break;
                if (argv[0][2] == '\0')
                {
                    argv++;
                    argc--;
                    width = atoi (argv[0]);
                }
                else
                    width = atoi (argv[0] + 2);
                break;
            case 'o':
                if (endmarker)
                    break;
                if (argv[0][2] == '\0')
                {
                    argv++;
                    argc--;
                    objsuffix = argv[0];
                }
                else
                    objsuffix = argv[0] + 2;
                break;
            case 'p':
                if (endmarker)
                    break;
                if (argv[0][2] == '\0')
                {
                    argv++;
                    argc--;
                    objprefix = argv[0];
                }
                else
                    objprefix = argv[0] + 2;
                break;
            case 'S':
                if (endmarker)
                    break;
                opt_sysincludes = true;
                break;
            case 'v':
                if (endmarker)
                    break;
                opt_verbose = true;
#ifdef CS_DEBUG
                if (argv[0][2])
                    _debugmask = atoi (argv[0] + 2);
#endif
                break;
            case 's':
                if (endmarker)
                    break;
                startat = argv[0] + 2;
                if (*startat == '\0')
                {
                    startat = *(++argv);
                    argc--;
                }
                if (*startat != '#')
                    fatalerr ("-s flag's value should start %s\n",
                              "with '#'.");
                break;
            case 'f':
                if (endmarker)
                    break;
                makefile = argv[0] + 2;
                if (*makefile == '\0')
                {
                    makefile = *(++argv);
                    argc--;
                }
                break;

            case 'm':
                opt_warn_multiple = true;
                break;

                /* Ignore -O, -g so we can just pass ${CFLAGS} to
                 makedepend
                 */
            case 'O':
            case 'g':
                break;
            case 'h':
                if (endmarker)
                    break;
                display_help ();
                return 0;
            case 'V':
                if (endmarker)
                    break;
                display_version ();
                return 0;
            default:
                if (endmarker)
                    break;
                /* fatalerr("unknown opt = %s\n", argv[0]); */
                warning ("ignoring option %s\n", argv[0]);
        }
    }
    /* Now do the undefs from the command line */
    for (i = 0; i < numundefs; i++)
        undefine (undeflist[i], &maininclist);
    if (numundefs > 0)
        free (undeflist);

    if (!defincdir)
    {
#ifdef PREINCDIR
        add_include (PREINCDIR, incp);
#endif
#if defined (__GNUC__) || defined (_WIN32)
        add_include (
#  if defined (__GNUC__)
       getenv ("C_INCLUDE_PATH"),
#  else
       getenv ("INCLUDE"),
#  endif
       incp);
#else /* !__GNUC__, does not use INCLUDEDIR at all */
        if (incp >= includedirs + MAXDIRS)
            fatalerr ("Too many -I flags.\n");
        *incp++ = INCLUDEDIR;
#endif

#ifdef POSTINCDIR
        add_include (POSTINCDIR, incp);
#endif
    }
    else if (*defincdir)
        add_include (defincdir, incp);

    /* if nothing to do, abort */
    if (!*filelist)
        fatalerr ("No files specified, try \"makedep -h\"\n");

    redirect (startat, makefile);

    /* catch signals */
#ifdef USGISH
    /*  should really reset SIGINT to SIG_IGN if it was.  */
#ifdef SIGHUP
    signal (SIGHUP, sighandler);
#endif
    signal (SIGINT, sighandler);
#ifdef SIGQUIT
    signal (SIGQUIT, sighandler);
#endif
    signal (SIGILL, sighandler);
#ifdef SIGBUS
    signal (SIGBUS, sighandler);
#endif
    signal (SIGSEGV, sighandler);
#ifdef SIGSYS
    signal (SIGSYS, sighandler);
#endif
#else
    sig_act.sa_handler = sighandler;
#ifdef _POSIX_SOURCE
    sigemptyset (&sig_act.sa_mask);
    sigaddset (&sig_act.sa_mask, SIGINT);
    sigaddset (&sig_act.sa_mask, SIGQUIT);
#ifdef SIGBUS
    sigaddset (&sig_act.sa_mask, SIGBUS);
#endif
    sigaddset (&sig_act.sa_mask, SIGILL);
    sigaddset (&sig_act.sa_mask, SIGSEGV);
    sigaddset (&sig_act.sa_mask, SIGHUP);
    sigaddset (&sig_act.sa_mask, SIGPIPE);
#ifdef SIGSYS
    sigaddset (&sig_act.sa_mask, SIGSYS);
#endif
#else
    sig_act.sa_mask = ((1 << (SIGINT - 1))
#ifdef SIGQUIT
                       | (1 << (SIGQUIT - 1))
#endif
#ifdef SIGBUS
                       | (1 << (SIGBUS - 1))
#endif
                       | (1 << (SIGILL - 1))
                       | (1 << (SIGSEGV - 1))
#ifdef SIGHUP
                       | (1 << (SIGHUP - 1))
#endif
#ifdef SIGPIPE
                       | (1 << (SIGPIPE - 1))
#endif
#ifdef SIGSYS
                       | (1 << (SIGSYS - 1))
#endif
                      );
#endif /* _POSIX_SOURCE */
    sig_act.sa_flags = 0;
#ifdef SIGHUP
    sigaction (SIGHUP, &sig_act, (struct sigaction *) 0);
#endif
    sigaction (SIGINT, &sig_act, (struct sigaction *) 0);
#ifdef SIGQUIT
    sigaction (SIGQUIT, &sig_act, (struct sigaction *) 0);
#endif
    sigaction (SIGILL, &sig_act, (struct sigaction *) 0);
#ifdef SIGBUS
    sigaction (SIGBUS, &sig_act, (struct sigaction *) 0);
#endif
    sigaction (SIGSEGV, &sig_act, (struct sigaction *) 0);
#ifdef SIGSYS
    sigaction (SIGSYS, &sig_act, (struct sigaction *) 0);
#endif
#endif /* USGISH */

    /*
     * now peruse through the list of files.
     */
    for (fp = filelist; *fp; fp++)
    {
        filecontent = getfile (*fp);
        ip = newinclude (*fp, (char *) NULL);

        find_includes (filecontent, ip, ip, 0, false);
        freefile (filecontent);
        recursive_pr_include (ip, ip->i_file, base_name (*fp));
        inc_clean ();
    }
    if (opt_printed)
        printf ("\n");
    return 0;
}

#ifdef __EMX__
/*
 * eliminate \r chars from file
 */
static int elim_cr (char *buf, int sz)
{
    int i,
    wp;

    for (i = wp = 0; i < sz; i++)
    {
        if (buf[i] != '\r')
            buf[wp++] = buf[i];
    }
    return wp;
}
#endif

struct filepointer *getfile (char *file)
{
    register int fd;
    struct filepointer *content;

    content = (struct filepointer *) malloc (sizeof (struct filepointer));

    if ((fd = open (file, O_RDONLY)) < 0)
    {
        warning ("cannot open \"%s\"\n", file);
        content->f_p = content->f_base = content->f_end = (char *) malloc (1);
        *content->f_p = '\0';
        return (content);
    }
#ifdef __DJGPP__
    // For some unknown reason fstat() often fails on DJGPP
    size_t fsize = lseek (fd, 0, SEEK_END);
    lseek (fd, 0, SEEK_SET);
#else
    struct stat st;
    fstat (fd, &st);
    size_t fsize = st.st_size;
#endif
    content->f_base = (char *) malloc (fsize + 1);
    if (content->f_base == NULL)
        fatalerr ("cannot allocate mem\n");
    if ((long)(fsize = read (fd, content->f_base, fsize)) < 0)
        fatalerr ("failed to read %s\n", file);
#ifdef __EMX__
    fsize = elim_cr (content->f_base, fsize);
#endif
    close (fd);
    content->f_len = fsize + 1;
    content->f_p = content->f_base;
    content->f_end = content->f_base + fsize;
    *content->f_end = '\0';
    content->f_line = 0;
    return (content);
}

void freefile (struct filepointer *fp)
{
    free (fp->f_base);
    free (fp);
}

char *copy (const char *str)
{
    register char *p = (char *) malloc (strlen (str) + 1);

    strcpy (p, str);
    return (p);
}

int match (const char *str, const char **list)
{
    register int i;

    for (i = 0; *list; i++, list++)
        if (strcmp (str, *list) == 0)
            return (i);
    return (-1);
}

/*
 * Get the next line.  We only return lines beginning with '#' since that
 * is all this program is ever interested in.
 */
char *getline (struct filepointer *filep)
{
    register char *p,		/* walking pointer */
    *eof,			/* end of file pointer */
    *bol;			/* beginning of line pointer */
    register int lineno;		/* line number */

    p = filep->f_p;
    eof = filep->f_end;
    if (p >= eof)
        return ((char *) NULL);
    lineno = filep->f_line;

    for (bol = p; p < eof; p++)
        if (p [0] == '/' && p [1] == '*')
        {
            /* consume C-style comments */
            *p++ = ' ', *p++ = ' ';
            while (*p)
            {
                if (p [0] == '*' && p [1] == '/')
                {
                    *p++ = ' ', *p = ' ';
                    break;
                }
                else if (*p == '\n')
                    lineno++;
                *p++ = ' ';
            }
        }
        else if (p [0] == '/' && p [1] == '/')
        {
            /* consume C++-style comments */
            *p++ = ' ', *p = ' ';
            while (*p && p [1] != '\n')
                *p++ = ' ';
            if (*p)
                *p = ' ';
        }
        else if (*p == '\\')
        {
            if (p [1] == '\n')
            {
                *p = ' ';
                p [1] = ' ';
                lineno++;
            }
        }
        else if (*p == '\n')
        {
            lineno++;
            /* Skip spaces at beginning of line */
            while ((bol < p) && isspace (*bol))
                bol++;
            if (*bol == '#')
            {
                register char *cp;

                *p++ = '\0';
                /* punt lines with just # (yacc generated) */
                for (cp = bol + 1; *cp && (*cp == ' ' || *cp == '\t'); cp++);
                if (*cp)
                    goto done;
            }
            bol = p + 1;
            /* Skip CR/LFs */
            if (*bol == '\r')
                bol++;
        }
    if (*bol != '#')
        bol = NULL;
    done:
    filep->f_p = p;
    filep->f_line = lineno;
    return (bol);
}

/*
 * Strip the file name down to what we want to see in the Makefile.
 * It will have objprefix and objsuffix around it.
 */
char *base_name (char *file)
{
    register char *p;

    file = copy (file);
    for (p = file + strlen (file); p > file && *p != '.'; p--);

    if (*p == '.')
        *p = '\0';
    return (file);
}

#if defined(USG) && !defined(CRAY) && !defined(SVR4) && !defined(__EMX__) && !defined(clipper) && !defined(__clipper__) && !defined(__DJGPP__)
int rename (char *from, char *to)
{
    (void) unlink (to);
    if (link (from, to) == 0)
    {
        unlink (from);
        return 0;
    }
    else
    {
        return -1;
    }
}
#endif /* USGISH */

void redirect (const char *line, const char *makefile)
{
    struct stat st;
    FILE *fdin, *fdout;
    char backup[BUFSIZ], buf[BUFSIZ];
    bool found = false;
    int len;

    /* if makefile is "-" then let it pour onto stdout. */
    if (makefile && *makefile == '-' && *(makefile + 1) == '\0')
    {
        puts (line);
        return;
    }

    /* use a default makefile is not specified. */
    if (!makefile)
    {
        if (stat ("Makefile", &st) == 0)
            makefile = "Makefile";
        else if (stat ("makefile", &st) == 0)
            makefile = "makefile";
        else
            fatalerr ("[mM]akefile is not present\n");
    }
    else
        if (stat (makefile, &st))
            st.st_mode = 0640;

    if ((fdin = fopen (makefile, "r")) == NULL)
    {
        /* Try to create file if it does not exist */
        if (opt_create)
        {
            fdin = fopen (makefile, "w");
            if (fdin)
            {
                fclose (fdin);
                fdin = fopen (makefile, "r");
            }
        }
        if (!fdin)
            fatalerr ("cannot open \"%s\"\n", makefile);
    }

#if defined (__DJGPP__)
    // DOS cannot handle long filenames
    {
        char *src = makefile;
        char *dst = backup;
        while (*src && (*src != '.'))
            *dst++ = *src++;
        strcpy (dst, ".bak");
    }
#else
    sprintf (backup, "%s.bak", makefile);
#endif
    unlink (backup);
#if defined(WIN32) || defined(__EMX__) || defined (__DJGPP__)
    fclose (fdin);
#endif
    if (rename (makefile, backup) < 0)
        fatalerr ("cannot rename %s to %s\n", makefile, backup);
#if defined(WIN32) || defined(__EMX__) || defined (__DJGPP__)
    if ((fdin = fopen (backup, "r")) == NULL)
        fatalerr ("cannot open \"%s\"\n", backup);
#endif
    if ((fdout = freopen (makefile, "w", stdout)) == NULL)
        fatalerr ("cannot open \"%s\"\n", backup);
    len = strlen (line);
    while (!found && fgets (buf, BUFSIZ, fdin))
    {
        if (*buf == '#' && strncmp (line, buf, len) == 0)
            found = true;
        fputs (buf, fdout);
    }
    if (!found)
    {
        if (opt_verbose)
            warning ("Adding new delimiting line \"%s\" and dependencies...\n",
                     line);
        puts (line);	/* same as fputs(fdout); but with newline */
    }
    else if (opt_append)
    {
        while (fgets (buf, BUFSIZ, fdin))
        {
            fputs (buf, fdout);
        }
    }
    fflush (fdout);
#if defined(USGISH) || defined(_SEQUENT_) || defined(USE_CHMOD)
    chmod (makefile, st.st_mode);
#else
    fchmod (fileno (fdout), st.st_mode);
#endif /* USGISH */

    fclose (fdin);
    if (!opt_backup)
        unlink (backup);
}

void fatalerr (const char *msg, ...)
{
    va_list args;

    fprintf (stderr, "%s: error:  ", ProgramName);
    va_start (args, msg);
    vfprintf (stderr, msg, args);
    va_end (args);
    exit (1);
}

void warning (const char *msg,...)
{
    va_list args;

    fprintf (stderr, "%s: warning:  ", ProgramName);
    va_start (args, msg);
    vfprintf (stderr, msg, args);
    va_end (args);
}

void warning1 (const char *msg,...)
{
    va_list args;

    va_start (args, msg);
    vfprintf (stderr, msg, args);
    va_end (args);
}
