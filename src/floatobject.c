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

int py_is_float(const void* op) {
	return ((struct py_object*) op)->type == &py_float_type;
}

struct py_object* py_float_new(double fval) {
	/* TODO: Is this useful? */
	/* For efficiency, this code is copied from py_object_new() */
	struct py_float* op = malloc(sizeof(struct py_float));
	if(op == NULL) return py_error_set_nomem();

	py_object_newref(op);
	op->ob.type = &py_float_type;
	op->value = fval;

	return (struct py_object*) op;
}

double py_float_get(const struct py_object* op) {
	if(!py_is_float(op)) {
		py_error_set_badarg();
		return -1; /* TODO: This EH mechanism just doesn't work - use NaN? */
	}
	else return ((struct py_float*) op)->value;
}

/* Methods */

static int float_compare(const struct py_object* v, const struct py_object* w) {
	double i = py_float_get(v);
	double j = py_float_get(w);

	return (i < j) ? -1 : (i > j) ? 1 : 0;
}

struct py_type py_float_type = {
		{ &py_type_type, 1 }, sizeof(struct py_float),
		py_object_delete, /* dealloc */
		float_compare, /* cmp */
		0, /* sequencemethods */
};

/*
XXX This is not enough. Need:
- automatic casts for mixed arithmetic (3.1 * 4)
- mixed comparisons (!)
- look at other uses of ints that could be extended to floats
*/
