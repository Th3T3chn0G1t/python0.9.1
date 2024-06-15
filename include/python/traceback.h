/*
 * Copyright 1991 by Stichting Mathematisch Centrum
 * See `LICENCE' for more information.
 */

/* Traceback interface */

#ifndef PY_TRACEBACK_H
#define PY_TRACEBACK_H

#include <python/object/frame.h>

struct py_traceback {
	struct py_object ob;

	struct py_traceback* next;
	struct py_frame* frame;
	unsigned lineno;
};

int py_traceback_new(struct py_frame*, unsigned);
int py_traceback_print(struct py_object*, FILE*);
void py_traceback_dealloc(struct py_object*);
struct py_object* py_traceback_get(void);

#endif
