/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Traceback interface */

#ifndef PY_TRACEBACK_H
#define PY_TRACEBACK_H

#include <python/frameobject.h>

int py_traceback_new(struct py_frame*, int, int);
int py_traceback_print(struct py_object*, FILE*);

struct py_object* py_traceback_get(void);
int py_traceback_set(struct py_object*);

#endif
