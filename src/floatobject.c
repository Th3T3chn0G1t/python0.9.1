/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Float object implementation */

#include <python/std.h>
#include <python/errors.h>

#include <python/floatobject.h>
#include <python/stringobject.h>

struct py_object* py_float_new(double fval) {
	struct py_float* op = py_object_new(PY_TYPE_FLOAT);
	op->value = fval;

	return (struct py_object*) op;
}

double py_float_get(const struct py_object* op) {
	if(!(op->type == PY_TYPE_FLOAT)) {
		/*
		 * TODO: This EH mechanism just doesn't work - make caller do the
		 * 		 Check.
		 */
		py_error_set_badarg();
		return -1;
	}
	else return ((struct py_float*) op)->value;
}

/* Methods */

int py_float_cmp(const struct py_object* v, const struct py_object* w) {
	double i = py_float_get(v);
	double j = py_float_get(w);

	return (i < j) ? -1 : (i > j) ? 1 : 0;
}
