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

struct py_object* py_float_new(double);
void py_float_dealloc(struct py_object*);
double py_float_get(const struct py_object*);
int py_float_cmp(const struct py_object*, const struct py_object*);

#endif
