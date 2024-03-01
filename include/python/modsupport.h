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

/* Module support interface */

#ifndef PY_MODSUPPORT_H
#define PY_MODSUPPORT_H

object* initmodule(char*, struct methodlist*);

int getnoarg(object* v);
int getintarg(object* v, int* a);
int getintintarg(object* v, int* a, int* b);
int getlongarg(object* v, long* a);
int getlonglongargs(object* v, long* a, long* b);
int getlonglongobjectargs(object* v, long* a, long* b, object** c);
int getstrarg(object* v, object** a);
int getstrstrarg(object* v, object** a, object** b);
int getstrstrintarg(object* v, object** a, object** b, int* c);
int getstrintarg(object* v, object** a, int* b);
int getintstrarg(object* v, int* a, object** b);
int getpointarg(object* v, int* a /* [2] */);
int get3pointarg(object* v, int* a);
int getrectarg(object* v, int* a);
int getrectintarg(object* v, int* a /* [4+1] */);
int getpointintarg(object* v, int* a /* [2+1] */);
int getpointstrarg(object* v, int* a /* [2] */, object** b);
int getstrintintarg(object* v, object* a, int* b, int* c);
int getrectpointarg(object* v, int* a /* [4+2] */);
int getlongtuplearg(object* args, long* a /* [n] */, int n);
int getshorttuplearg(object* args, short* a /* [n] */, int n);
int getlonglistarg(object* args, long* a /* [n] */, int n);
int getshortlistarg(object* args, short* a /* [n] */, int n);

#endif
