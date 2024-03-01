/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Function object interface */

#ifndef PY_FUNCOBJECT_H
#define PY_FUNCOBJECT_H

extern struct py_type py_func_type;

#define py_is_func(op) ((op)->type == &py_func_type)

struct py_object* py_func_new(struct py_object*, struct py_object*);

struct py_object* py_func_get_code(struct py_object*);
struct py_object* py_func_get_globals(struct py_object*);

#endif
