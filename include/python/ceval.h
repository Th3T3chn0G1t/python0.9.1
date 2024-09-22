/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Interface to execute compiled code */

#ifndef PY_CEVAL_H
#define PY_CEVAL_H

#include <python/std.h>
#include <python/object.h>
#include <python/object/frame.h>

struct py_env;

struct py_object* py_code_eval(
		struct py_env*, struct py_code*, struct py_object*, struct py_object*,
		struct py_object*);

#endif
