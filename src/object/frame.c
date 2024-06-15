/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Frame object implementation */

#include <python/std.h>
#include <python/compile.h>
#include <python/errors.h>
#include <python/opcode.h>

#include <python/object/frame.h>
#include <python/object/dict.h>

/* TODO: This function is kind of a mess for such an important component. */
struct py_frame* py_frame_new(
		struct py_frame* back, struct py_code* code, struct py_object* globals,
		struct py_object* locals, unsigned nvalues, unsigned nblocks) {

	struct py_frame* f;

	if((back != NULL && back->ob.type != PY_TYPE_FRAME) || code == NULL ||
		code->ob.type != PY_TYPE_CODE || globals == NULL ||
		globals->type != PY_TYPE_DICT || locals == NULL ||
		locals->type != PY_TYPE_DICT) {

		py_error_set_badcall();
		return NULL;
	}
	f = py_object_new(PY_TYPE_FRAME);
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

void py_block_setup(
		struct py_frame* f, enum py_opcode type, unsigned handler, unsigned level) {

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

struct py_block* py_block_pop(struct py_frame* f) {
	struct py_block* b;

	if(f->iblock <= 0) {
		/* TODO: Better EH. */
		fprintf(stderr, "XXX block stack underflow\n");
		abort();
	}

	b = &f->blockstack[--f->iblock];

	return b;
}

void py_frame_dealloc(struct py_object* op) {
	struct py_frame* f = (struct py_frame*) op;

	py_object_decref(f->back);
	py_object_decref(f->code);
	py_object_decref(f->globals);
	py_object_decref(f->locals);
	free(f->valuestack);
	free(f->blockstack);
}
