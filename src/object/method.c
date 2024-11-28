/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Method object implementation */

#include <python/object/method.h>

struct py_object* py_method_new(py_method_t method, struct py_object* self) {
	struct py_method* op;

	if(!(op = py_object_new(PY_TYPE_METHOD))) return 0;

	op->method = method;
	op->self = py_object_incref(self);

	return (void*) op;
}

/* Methods (the standard built-in methods, that is) */

void py_method_dealloc(struct py_object* op) {
	py_object_decref(((struct py_method*) op)->self);

	free(op);
}
