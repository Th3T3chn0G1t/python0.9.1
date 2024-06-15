/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Function object implementation */

#include <python/std.h>

#include <python/object.h>
#include <python/object/func.h>

struct py_object* py_func_new(
		struct py_object* code, struct py_object* globals) {

	struct py_func* op;

	if(!(op = py_object_new(PY_TYPE_FUNC))) return 0;

	op->code = py_object_incref(code);
	op->globals = py_object_incref(globals);

	return (void*) op;
}

void py_func_dealloc(struct py_object* op) {
	struct py_func* fp = (void*) op;

	py_object_decref(fp->code);
	py_object_decref(fp->globals);

	free(op);
}
