/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module support interface */

#ifndef PY_MODSUPPORT_H
#define PY_MODSUPPORT_H

#include <python/object.h>
#include <python/methodobject.h>

struct py_object* py_module_new_methods(
		struct py_env*, const char*, const struct py_methodlist*);

/* TODO: There's probably a more modular way to do this. */
int py_arg_none(struct py_object*);
int py_arg_int(struct py_object*, int*);
int py_arg_int_int(struct py_object*, int*, int*);
int py_arg_long(struct py_object*, long*);
int py_arg_long_long(struct py_object*, long*, long*);
int py_arg_str(struct py_object*, struct py_object**);
int py_arg_str_str(struct py_object*, struct py_object**, struct py_object**);
int py_arg_str_int(struct py_object*, struct py_object**, int*);

#endif
