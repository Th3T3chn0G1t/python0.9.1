/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module object interface */

#ifndef PY_MODULEOBJECT_H
#define PY_MODULEOBJECT_H

#include <python/object.h>

struct py_module {
	struct py_object ob;
	struct py_object* name;
	struct py_object* attr;
};

extern struct py_type py_module_type;

#define py_is_module(op) ((op)->type == &py_module_type)

struct py_object* py_module_new(const char*);
struct py_object* py_module_get_dict(struct py_object*);
const char* py_module_get_name(struct py_object*);

#endif
