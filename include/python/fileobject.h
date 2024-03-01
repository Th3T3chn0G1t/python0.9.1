/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* File object interface */

#ifndef PY_FILEOBJECT_H
#define PY_FILEOBJECT_H

#include <python/object.h>
#include <python/std.h>

/* TODO: Remove this. */

/* TODO: Python global state. */
extern struct py_type py_file_type;

#define py_is_file(op) ((op)->type == &py_file_type)

struct py_object* py_file_new(char*, char*);
FILE* py_file_get(struct py_object*);

struct py_object* py_openfile_new(FILE*, char*, char*);

#endif
