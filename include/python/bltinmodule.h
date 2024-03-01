/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Built-in module interface */

#ifndef PY_BLTINMODULE_H
#define PY_BLTINMODULE_H

#include <python/object.h>

void py_builtin_init(void);
void py_builtin_done(void);
struct py_object* py_builtin_get(const char*);

void py_errors_done(void);

#endif
