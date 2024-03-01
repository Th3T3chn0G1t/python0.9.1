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

/* Error handling definitions */

void err_set(object*);

void err_setval(object*, object*);

void err_setstr(object*, char*);

int err_occurred(void);

void err_get(object**, object**);

void err_clear(void);

/* Predefined exceptions */

extern object* RuntimeError;
extern object* EOFError;
extern object* TypeError;
extern object* MemoryError;
extern object* NameError;
extern object* SystemError;
extern object* KeyboardInterrupt;

/* Some more planned for the future */

#define IndexError             RuntimeError
#define KeyError               RuntimeError
#define ZeroDivisionError      RuntimeError
#define OverflowError          RuntimeError

/* Convenience functions */

int err_badarg(void);

object* err_nomem(void);

object* err_errno(object*);

void err_input(int);

void err_badcall(void);

void fatal(char*);
