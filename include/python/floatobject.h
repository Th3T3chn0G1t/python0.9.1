/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Float object interface */

#ifndef PY_FLOATOBJECT_H
#define PY_FLOATOBJECT_H

#include <python/object.h>

/*
 * struct py_float represents a (double precision) floating point number.
 */

struct py_float {
	struct py_object ob;
	double value;
};

/* TODO: Python global state. */
extern struct py_type py_float_type;

#define py_is_float(op) ((op)->type == &py_float_type)

struct py_object* py_float_new(double);
double py_float_get(const struct py_object*);

#endif
