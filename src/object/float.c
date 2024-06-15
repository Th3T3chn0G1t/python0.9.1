/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Float object implementation */

#include <python/errors.h>

#include <python/object/float.h>

struct py_object* py_float_new(double value) {
	struct py_float* op;

	if(!(op = py_object_new(PY_TYPE_FLOAT))) return 0;

	op->value = value;

	return (void*) op;
}

double py_float_get(const struct py_object* op) {
	return ((struct py_float*) op)->value;
}

/* Methods */

int py_float_cmp(const struct py_object* v, const struct py_object* w) {
	double i = py_float_get(v);
	double j = py_float_get(w);

	return (i < j) ? -1 : (i > j) ? 1 : 0;
}
