/***********************************************************
Copyright 1991 by Stichting Mathematisch Centrum, Amsterdam, The
Netherlands.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the names of Stichting Mathematisch
Centrum or CWI not be used in advertising or publicity pertaining to
distribution of the software without specific, written prior permission.

STICHTING MATHEMATISCH CENTRUM DISCLAIMS ALL WARRANTIES WITH REGARD TO
THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
FITNESS, IN NO EVENT SHALL STICHTING MATHEMATISCH CENTRUM BE LIABLE
FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

******************************************************************/

/* Interface to map C struct members to Python object attributes */

#include <stddef.h>

/* An array of memberlist structures defines the name, type and offset
   of selected members of a C structure.  These can be read by
   getmember() and set by setmember() (except if their READONLY flag
   is set).  The array must be terminated with an entry whose name
   pointer is NULL. */

struct memberlist {
	char* name;
	int type;
	int offset;
	int readonly;
};

/* Types */
#define T_SHORT                0
#define T_INT          1
#define T_LONG         2
#define T_FLOAT                3
#define T_DOUBLE       4
#define T_STRING       5
#define T_OBJECT       6

/* Readonly flag */
#define READONLY       1
#define RO             READONLY                /* Shorthand */

object* getmember(char *, struct memberlist *, char *);
int setmember(char *, struct memberlist *, char *, object *);
