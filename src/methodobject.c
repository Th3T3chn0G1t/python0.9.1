/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Method object implementation */

#include <python/errors.h>
#include <python/methodobject.h>

int py_is_method(const void* op) {
	return ((struct py_object*) op)->type == &py_method_type;
}

struct py_object* py_method_new(py_method_t meth, struct py_object* self) {
	struct py_method* op = py_object_new(&py_method_type);
	if(op != NULL) {
		op->method = meth;
		op->self = self;

		if(self != NULL) py_object_incref(self);
	}

	return (struct py_object*) op;
}

/* Methods (the standard built-in methods, that is) */

static void py_method_dealloc(struct py_object* op) {
	struct py_method* m = (struct py_method*) op;

	py_object_decref(m->self);
}

struct py_type py_method_type = {
		{ &py_type_type, 1 }, sizeof(struct py_method),
		py_method_dealloc, /* dealloc */
		0, /* cmp */
		0, /* sequencemethods */
};
