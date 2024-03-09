/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Function object interface */

#ifndef PY_FUNCOBJECT_H
#define PY_FUNCOBJECT_H

struct py_func {
	struct py_object ob;
	struct py_object* code;
	struct py_object* globals;
};

extern struct py_type py_func_type;

int py_is_func(const void*);

struct py_object* py_func_new(struct py_object*, struct py_object*);

#endif
