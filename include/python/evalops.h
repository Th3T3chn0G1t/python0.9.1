/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

#ifndef PY_EVALOPS_H
#define PY_EVALOPS_H

#include <python/object.h>

int py_object_truthy(struct py_object*);

struct py_object* py_object_add(struct py_object*, struct py_object*);
struct py_object* py_object_sub(struct py_object*, struct py_object*);

#endif
