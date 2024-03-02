/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Frame object implementation */

#include <python/std.h>
#include <python/compile.h>
#include <python/errors.h>
#include <python/opcode.h>
#include <python/structmember.h>

#include <python/frameobject.h>
#include <python/dictobject.h>

#define OFF(x) offsetof(struct py_frame, x)

static struct py_structmember frame_memberlist[] = {
		{ "back",    PY_TYPE_OBJECT, OFF(back),    PY_READWRITE },
		{ "code",    PY_TYPE_OBJECT, OFF(code),    PY_READWRITE },
		{ "globals", PY_TYPE_OBJECT, OFF(globals), PY_READWRITE },
		{ "locals",  PY_TYPE_OBJECT, OFF(locals),  PY_READWRITE },
		{ NULL,      0, 0,                         0 }  /* Sentinel */
};

static struct py_object* frame_getattr(f, name)struct py_frame* f;
											   char* name;
{
	return py_struct_get(f, frame_memberlist, name);
}

static void frame_dealloc(f)struct py_frame* f;
{
	py_object_decref(f->back);
	py_object_decref(f->code);
	py_object_decref(f->globals);
	py_object_decref(f->locals);
	free(f->valuestack);
	free(f->blockstack);
	free(f);
}

struct py_type py_frame_type = {
		{ 1, &py_type_type, 0 }, "frame", sizeof(struct py_frame), 0,
		frame_dealloc, /* dealloc */
		0, /* print */
		frame_getattr, /* get_attr */
		0, /* set_attr */
		0, /* cmp */
		0, /* repr */
		0, /* numbermethods */
		0, /* sequencemethods */
		0, /* mappingmethods */
};

struct py_frame* py_frame_new(back, code, globals, locals, nvalues, nblocks)
		struct py_frame* back;
		struct py_code* code;
		struct py_object* globals;
		struct py_object* locals;
		int nvalues;
		int nblocks;
{
	struct py_frame* f;
	if((back != NULL && !py_is_frame(back)) || code == NULL ||
	   !py_is_code(code) || globals == NULL || !py_is_dict(globals) ||
	   locals == NULL || !py_is_dict(locals) || nvalues < 0 || nblocks < 0) {
		py_error_set_badcall();
		return NULL;
	}
	f = py_object_new(&py_frame_type);
	if(f != NULL) {
		if(back)
			py_object_incref(back);
		f->back = back;
		py_object_incref(code);
		f->code = code;
		py_object_incref(globals);
		f->globals = globals;
		py_object_incref(locals);
		f->locals = locals;
		f->valuestack = malloc((nvalues + 1) * sizeof(struct py_object*));
		f->blockstack = malloc((nblocks + 1) * sizeof(struct py_block));
		f->nvalues = nvalues;
		f->nblocks = nblocks;
		f->iblock = 0;
		if(f->valuestack == NULL || f->blockstack == NULL) {
			py_error_set_nomem();
			py_object_decref(f);
			f = NULL;
		}
	}
	return f;
}

/* Block management */

void py_block_setup(f, type, handler, level)struct py_frame* f;
											int type;
											int handler;
											int level;
{
	struct py_block* b;
	if(f->iblock >= f->nblocks) {
		fprintf(stderr, "XXX block stack overflow\n");
		abort();
	}
	b = &f->blockstack[f->iblock++];
	b->type = type;
	b->level = level;
	b->handler = handler;
}

struct py_block* py_block_pop(f)struct py_frame* f;
{
	struct py_block* b;
	if(f->iblock <= 0) {
		fprintf(stderr, "XXX block stack underflow\n");
		abort();
	}
	b = &f->blockstack[--f->iblock];
	return b;
}
