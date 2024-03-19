/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Method object interface */

#ifndef PY_METHODOBJECT_H
#define PY_METHODOBJECT_H

#include <python/object.h>

typedef struct py_object* (*py_method_t)(struct py_object*, struct py_object*);

struct py_method {
	struct py_object ob;

	py_method_t method;
	struct py_object* self;
};

struct py_methodlist {
	char* name;
	py_method_t method;
};

struct py_object* py_method_new(py_method_t, struct py_object*);
void py_method_dealloc(struct py_object*);

#endif
