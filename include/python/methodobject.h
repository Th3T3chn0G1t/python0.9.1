/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Method object interface */

#ifndef PY_METHODOBJECT_H
#define PY_METHODOBJECT_H

#include <python/object.h>

/* TODO: Python global state */
extern struct py_type py_method_type;

#define py_is_method(op) ((op)->type == &py_method_type)

typedef struct py_object* (*py_method_t)(struct py_object*, struct py_object*);

struct py_methodlist {
	char* name;
	py_method_t method;
};

struct py_object* py_method_new(char*, py_method_t, struct py_object*, unsigned int);
py_method_t py_method_get(struct py_object*);
struct py_object* py_method_get_self(struct py_object*);

struct py_object* py_methodlist_find(struct py_methodlist*, struct py_object*, const char*);

#endif
