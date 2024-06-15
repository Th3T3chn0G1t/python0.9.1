/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Built-in module interface */

#ifndef PY_BLTINMODULE_H
#define PY_BLTINMODULE_H

#include "../object.h"
#include "../result.h"

enum py_result py_builtin_init(struct py_env*);
void py_builtin_done(void);

struct py_object* py_builtin_get(const char*);

void py_errors_done(void);

#endif
