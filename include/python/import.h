/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Module definition and import interface */

#ifndef PY_IMPORT_H
#define PY_IMPORT_H

#include <python/object.h>

/* TODO: Python global state. */
extern struct py_object* py_path;

struct py_object* py_path_new(const char*);

void py_import_init(void);
void py_import_done(void);

struct py_object* py_add_module(const char*);
struct py_object* py_import_module(const char*);

#endif
