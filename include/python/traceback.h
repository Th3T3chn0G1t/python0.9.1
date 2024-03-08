/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Traceback interface */

#ifndef PY_TRACEBACK_H
#define PY_TRACEBACK_H

#include <python/frameobject.h>

struct py_traceback {
	struct py_object ob;

	struct py_traceback* next;
	struct py_frame* frame;
	unsigned lineno;
};

/* TODO: Python global state. */
extern struct py_type py_traceback_type;

#define py_is_traceback(op) \
	 (((struct py_object*) (op))->type == &py_traceback_type)

int py_traceback_new(struct py_frame*, unsigned);
int py_traceback_print(struct py_object*, FILE*);

struct py_object* py_traceback_get(void);

#endif
