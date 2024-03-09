/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Integer object interface */

#ifndef PY_INTOBJECT_H
#define PY_INTOBJECT_H

#include <python/object.h>

/*
 * struct py_int represents a (long) integer. This is an immutable object;
 * an integer cannot change its value after creation.
 *
 * There are functions to create new integer objects, to test an object
 * for integer-ness, and to get the integer value. The latter functions
 * returns -1 and sets errno to EBADF if the object is not an struct py_int.
 * PY_NONE of the functions should be applied to nil objects.
 *
 * The type struct py_int is (unfortunately) exposed here so we can declare
 * py_true_object and py_false_object below; don't use this.
 */

struct py_int {
	struct py_object ob;
	long value;
};

/* TODO: Python global state */
extern struct py_type py_int_type;

int py_is_int(const void*);

struct py_object* py_int_new(long);
long py_int_get(const struct py_object*);

/*
 * False and True are special intobjects used by Boolean expressions.
 * All values of type Boolean must point to either of these; but in
 * contexts where integers are required they are integers (valued 0 and 1).
 *
 * Don't forget to apply py_object_incref() when returning True or False!!!
 */

/* TODO: Python global state */
/* Don't use these directly */
extern struct py_int py_false_object, py_true_object;

#define PY_TRUE ((struct py_object*) &py_true_object)
#define PY_FALSE ((struct py_object*) &py_false_object)

#endif
