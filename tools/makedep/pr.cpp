/* $XConsortium: pr.c /main/20 1996/12/04 10:11:41 swick $ */
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

#include "def.h"
#include <string.h>

extern struct inclist inclist[MAXFILES], *inclistp;
extern const char *objprefix;
extern const char *objsuffix;
extern int width;
extern bool opt_printed;
extern bool opt_verbose;
extern bool show_where_not;
extern bool opt_sysincludes;
extern bool opt_remove_prefix;

void add_include (struct filepointer *filep, struct inclist *file,
                  struct inclist *file_red, char *include, bool dot, bool failOK)
{
    register struct inclist *newfile;
    register struct filepointer *content;

    /* First decide what the pathname of this include file really is. */
    newfile = inc_path (file->i_file, include, dot);
    if (newfile == NULL)
    {
        if (failOK || (!opt_sysincludes && !dot))
            return;
        if (file != file_red)
            warning ("%s (reading %s, line %d): ",
                     file_red->i_file, file->i_file, filep->f_line);
        else
            warning ("%s, line %d: ", file->i_file, filep->f_line);
        warning1 ("cannot find include file \"%s\"\n", include);
        show_where_not = true;
        newfile = inc_path (file->i_file, include, dot);
        show_where_not = false;
    }

    if (newfile)
    {
        included_by (file, newfile);
        if (!(newfile->i_flags & SEARCHED))
        {
            newfile->i_flags |= SEARCHED;
            content = getfile (newfile->i_file);
            find_includes (content, newfile, file_red, 0, failOK);
            freefile (content);
        }
        if (!dot)
            newfile->i_flags |= SYSINCLUDE;
    }
}

void pr (struct inclist *ip, char *file, char *base)
{
    static char *lastfile;
    static int current_len;
    register int len, i;
    char buf[BUFSIZ];
    char *name = base;

    if (opt_remove_prefix)
        for (name += strlen (name); (name > base) && (name [-1] != '/')
#if defined (__EMX__) || defined (__DJGPP__) || defined (WIN32)
             && (name [-1] != '\\')
#endif
             ; name--)

            opt_printed = true;
    len = strlen (ip->i_file) + 1;
    if (current_len + len > width || file != lastfile)
    {
        if (file != lastfile)
            sprintf (buf, "\n%s%s%s: %s", objprefix, name, objsuffix, ip->i_file);
        else
            sprintf (buf, " \\\n  %s", ip->i_file);
        lastfile = file;
        len = current_len = strlen (buf);
    }
    else
    {
        buf[0] = ' ';
        strcpy (buf + 1, ip->i_file);
        current_len += len;
    }
    if (fwrite (buf, len, 1, stdout))
        ;

    /* If opt_verbose is set, then print out what this file includes. */
    if (!opt_verbose || ip->i_list == NULL || ip->i_flags & NOTIFIED)
        return;
    ip->i_flags |= NOTIFIED;
    lastfile = NULL;
    printf ("\n# %s includes:", ip->i_file);
    for (i = 0; i < ip->i_listlen; i++)
        printf ("\n#\t%s", ip->i_list[i]->i_incstring);
}

void recursive_pr_include (struct inclist *head, char *file, char *base)
{
    register int i;

    if ((head->i_flags & MARKED)
        || (!opt_sysincludes && (head->i_flags & SYSINCLUDE)))
        return;
    head->i_flags |= MARKED;
    if (head->i_file != file)
        pr (head, file, base);
    for (i = 0; i < head->i_listlen; i++)
        recursive_pr_include (head->i_list[i], file, base);
}
