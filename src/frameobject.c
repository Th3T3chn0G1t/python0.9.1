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

/* Frame object implementation */

#include <stdlib.h>

#include <python/allobjects.h>
#include <python/compile.h>
#include <python/frameobject.h>
#include <python/opcode.h>
#include <python/structmember.h>

#define OFF(x) offsetof(frameobject, x)

static struct memberlist frame_memberlist[] = {
		{ "f_back",    T_OBJECT, OFF(f_back), 0 },
		{ "f_code",    T_OBJECT, OFF(f_code), 0 },
		{ "f_globals", T_OBJECT, OFF(f_globals), 0 },
		{ "f_locals",  T_OBJECT, OFF(f_locals), 0 },
		{ NULL, 0, 0, 0 }  /* Sentinel */
};

static object* frame_getattr(f, name)frameobject* f;
									 char* name;
{
	return getmember((char*) f, frame_memberlist, name);
}

static void frame_dealloc(f)frameobject* f;
{
	XDECREF(f->f_back);
	XDECREF(f->f_code);
	XDECREF(f->f_globals);
	XDECREF(f->f_locals);
	free(f->f_valuestack);
	free(f->f_blockstack);
	free(f);
}

typeobject Frametype = {
		OB_HEAD_INIT(&Typetype) 0, "frame", sizeof(frameobject), 0,
		frame_dealloc,  /*tp_dealloc*/
		0,              /*tp_print*/
		frame_getattr,  /*tp_getattr*/
		0,              /*tp_setattr*/
		0,              /*tp_compare*/
		0,              /*tp_repr*/
		0,              /*tp_as_number*/
		0,              /*tp_as_sequence*/
		0,              /*tp_as_mapping*/
};

frameobject* newframeobject(back, code, globals, locals, nvalues, nblocks)
		frameobject* back;
		codeobject* code;
		object* globals;
		object* locals;
		int nvalues;
		int nblocks;
{
	frameobject* f;
	if((back != NULL && !is_frameobject(back)) || code == NULL ||
	   !is_codeobject(code) || globals == NULL || !is_dictobject(globals) ||
	   locals == NULL || !is_dictobject(locals) || nvalues < 0 || nblocks < 0) {
		err_badcall();
		return NULL;
	}
	f = NEWOBJ(frameobject, &Frametype);
	if(f != NULL) {
		if(back)
			PY_INCREF(back);
		f->f_back = back;
		PY_INCREF(code);
		f->f_code = code;
		PY_INCREF(globals);
		f->f_globals = globals;
		PY_INCREF(locals);
		f->f_locals = locals;
		f->f_valuestack = malloc((nvalues + 1) * sizeof(object*));
		f->f_blockstack = malloc((nblocks + 1) * sizeof(block));
		f->f_nvalues = nvalues;
		f->f_nblocks = nblocks;
		f->f_iblock = 0;
		if(f->f_valuestack == NULL || f->f_blockstack == NULL) {
			err_nomem();
			PY_DECREF(f);
			f = NULL;
		}
	}
	return f;
}

/* Block management */

void setup_block(f, type, handler, level)frameobject* f;
										 int type;
										 int handler;
										 int level;
{
	block* b;
	if(f->f_iblock >= f->f_nblocks) {
		fprintf(stderr, "XXX block stack overflow\n");
		abort();
	}
	b = &f->f_blockstack[f->f_iblock++];
	b->b_type = type;
	b->b_level = level;
	b->b_handler = handler;
}

block* pop_block(f)frameobject* f;
{
	block* b;
	if(f->f_iblock <= 0) {
		fprintf(stderr, "XXX block stack underflow\n");
		abort();
	}
	b = &f->f_blockstack[--f->f_iblock];
	return b;
}
