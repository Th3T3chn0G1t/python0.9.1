/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Method object implementation */

#include <python/errors.h>
#include <python/methodobject.h>

struct py_object* py_method_new(py_method_t meth, struct py_object* self) {
	struct py_method* op = py_object_new(&py_method_type);
	if(op != NULL) {
		op->method = meth;
		op->self = self;

		if(self != NULL) py_object_incref(self);
	}

	return (struct py_object*) op;
}

py_method_t py_method_get(struct py_object* op) {
	if(!py_is_method(op)) {
		py_error_set_badcall();
		return NULL;
	}

	return ((struct py_method*) op)->method;
}

struct py_object* py_method_get_self(struct py_object* op) {
	if(!py_is_method(op)) {
		py_error_set_badcall();
		return NULL;
	}

	return ((struct py_method*) op)->self;
}

/* Methods (the standard built-in methods, that is) */

static void py_method_dealloc(struct py_object* op) {
	struct py_method* m = (struct py_method*) op;

	py_object_decref(m->self);
}

struct py_type py_method_type = {
		{ 1, &py_type_type }, sizeof(struct py_method),
		py_method_dealloc, /* dealloc */
		0, /* cmp */
		0, /* sequencemethods */
};
