/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module definition and import interface */

#ifndef PY_IMPORT_H
#define PY_IMPORT_H

#include <python/object.h>

void py_import_done(struct py_env*);

struct py_object* py_module_add(struct py_env* env, const char*);
struct py_object* py_import_module(struct py_env* env, const char*);

#endif
