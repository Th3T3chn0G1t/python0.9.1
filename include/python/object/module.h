/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module object interface */

#ifndef PY_MODULEOBJECT_H
#define PY_MODULEOBJECT_H

#include "../object.h"

struct py_module {
	struct py_object ob;
	struct py_object* name;
	struct py_object* attr;
};

struct py_object* py_module_new(const char*);
struct py_object* py_module_get_attr(struct py_object*, const char*);
void py_module_dealloc(struct py_object*);

#endif
