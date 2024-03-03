/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Interface to execute compiled code */

#ifndef PY_CEVAL_H
#define PY_CEVAL_H

#include <python/std.h>
#include <python/object.h>
#include <python/frameobject.h>

/* TODO: Python global state. */
extern struct py_frame* py_frame_current;

struct py_object* py_code_eval(
		struct py_code*, struct py_object*, struct py_object*,
		struct py_object*);

struct py_object* py_get_globals(void);

struct py_object* py_get_locals(void);

int py_object_truthy(struct py_object* v);

struct py_object* py_call_function(
		struct py_object* func, struct py_object* arg);

#endif
