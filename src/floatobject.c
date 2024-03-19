/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Float object implementation */

/* XXX There should be overflow checks here, but it's hard to check
   for any kind of float exception without losing portability. */

#include <python/std.h>
#include <python/errors.h>

#include <python/floatobject.h>
#include <python/stringobject.h>

struct py_object* py_float_new(double fval) {
	/* TODO: Is this useful? */
	/* For efficiency, this code is copied from py_object_new() */
	struct py_float* op = malloc(sizeof(struct py_float));
	if(op == NULL) return py_error_set_nomem();

	py_object_newref(op);
	op->ob.type = PY_TYPE_FLOAT;
	op->value = fval;

	return (struct py_object*) op;
}

double py_float_get(const struct py_object* op) {
	if(!(op->type == PY_TYPE_FLOAT)) {
		py_error_set_badarg();
		return -1; /* TODO: This EH mechanism just doesn't work - use NaN? */
	}
	else return ((struct py_float*) op)->value;
}

/* Methods */

int py_float_cmp(const struct py_object* v, const struct py_object* w) {
	double i = py_float_get(v);
	double j = py_float_get(w);

	return (i < j) ? -1 : (i > j) ? 1 : 0;
}

/*
 * TODO This is not enough. Need:
 * - automatic casts for mixed arithmetic (3.1 * 4)
 * - mixed comparisons (!)
 * - look at other uses of ints that could be extended to floats
 */
