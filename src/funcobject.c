/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Function object implementation */

#include <python/std.h>
#include <python/errors.h>

#include <python/object.h>
#include <python/funcobject.h>

int py_is_func(const void* op) {
	return ((struct py_object*) op)->type == &py_func_type;
}

struct py_object* py_func_new(
		struct py_object* code, struct py_object* globals) {

	struct py_func* op = py_object_new(&py_func_type);
	if(op != NULL) {
		py_object_incref(code);
		op->code = code;
		py_object_incref(globals);
		op->globals = globals;
	}

	return (struct py_object*) op;
}

static void func_dealloc(struct py_object* op) {
	struct py_func* fp = (struct py_func*) op;

	py_object_decref(fp->code);
	py_object_decref(fp->globals);
	free(op);
}

struct py_type py_func_type = {
		{ &py_type_type, 1 }, sizeof(struct py_func),
		func_dealloc, /* dealloc */
		0, /* cmp */
		0 };
