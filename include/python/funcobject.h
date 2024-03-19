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

struct py_object* py_func_new(struct py_object*, struct py_object*);
void py_func_dealloc(struct py_object*);

#endif
